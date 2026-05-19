import os
import subprocess
import tempfile
import uuid
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
      - File permission: --read-only rootfs + writable tmpfs at /tmp
      - Resource limits: --memory, --cpus, --pids-limit (cgroups)
      - Timeout: parent-side kill via subprocess timeout, then explicit
        `docker kill` of the container (so the container is reaped, not orphaned
        when the docker CLI process is killed).
      - IPC: stdin/stdout pipes carry code in and results out
    """

    def __init__(self, image: str, memory: str = "128m", cpus: str = "0.5",
                 timeout: int = 5, pids_limit: int = 64,
                 tmpfs_size: str = "32m"):
        self.image = image
        self.memory = memory
        self.cpus = cpus
        self.timeout = timeout
        self.pids_limit = pids_limit
        self.tmpfs_size = tmpfs_size

    def run(self, code: str) -> SandboxResult:
        # `--cidfile` writes the container ID before the entrypoint runs, so on
        # timeout we can `docker kill` the container by ID instead of leaving
        # it orphaned when the parent `docker run` process is SIGKILLed.
        cid_path = os.path.join(
            tempfile.gettempdir(), f"miniagentos-{uuid.uuid4().hex}.cid"
        )

        cmd = [
            "docker", "run", "--rm", "-i",
            f"--cidfile={cid_path}",
            "--network=none",
            "--read-only",
            f"--tmpfs=/tmp:rw,noexec,nosuid,size={self.tmpfs_size}",
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
            # Reap the orphaned container so it does not keep running.
            self._kill_by_cidfile(cid_path)
            return SandboxResult(
                exit_code=-1,
                stdout=e.stdout if e.stdout else "",
                stderr=e.stderr if e.stderr else "",
                timed_out=True,
            )
        finally:
            try:
                if os.path.exists(cid_path):
                    os.unlink(cid_path)
            except OSError:
                pass

    @staticmethod
    def _kill_by_cidfile(cid_path: str) -> None:
        try:
            with open(cid_path) as f:
                cid = f.read().strip()
        except OSError:
            return
        if not cid:
            return
        # Best-effort; --rm will cleanup once the container exits.
        subprocess.run(
            ["docker", "kill", cid],
            capture_output=True, timeout=5,
        )
