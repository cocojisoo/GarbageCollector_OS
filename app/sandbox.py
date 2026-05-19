import subprocess
from dataclasses import dataclass


@dataclass
class SandboxResult:
    exit_code: int
    stdout: str
    stderr: str
    timed_out: bool


class Sandbox:
    """Runs untrusted Python in an isolated Docker container.

    OS concepts demonstrated:
      - Process isolation: each run = its own container (separate namespaces)
      - System call restriction: --cap-drop=ALL, --security-opt=no-new-privileges
      - Network isolation: --network=none
      - File permission: --read-only rootfs, no host mounts
      - Resource limits: --memory, --cpus, --pids-limit (cgroups)
      - Timeout: parent-side kill via subprocess timeout
      - IPC: stdin/stdout pipes carry code in and results out
    """

    def __init__(self, image: str, memory: str = "128m", cpus: str = "0.5",
                 timeout: int = 5, pids_limit: int = 64):
        self.image = image
        self.memory = memory
        self.cpus = cpus
        self.timeout = timeout
        self.pids_limit = pids_limit

    def run(self, code: str) -> SandboxResult:
        cmd = [
            "docker", "run", "--rm", "-i",
            "--network=none",
            "--read-only",
            "--cap-drop=ALL",
            "--security-opt=no-new-privileges",
            f"--memory={self.memory}",
            f"--cpus={self.cpus}",
            f"--pids-limit={self.pids_limit}",
            self.image,
        ]
        try:
            proc = subprocess.run(
                cmd,
                input=code,
                capture_output=True,
                text=True,
                timeout=self.timeout,
            )
            return SandboxResult(
                exit_code=proc.returncode,
                stdout=proc.stdout,
                stderr=proc.stderr,
                timed_out=False,
            )
        except subprocess.TimeoutExpired as e:
            return SandboxResult(
                exit_code=-1,
                stdout=e.stdout if e.stdout else "",
                stderr=e.stderr if e.stderr else "",
                timed_out=True,
            )
