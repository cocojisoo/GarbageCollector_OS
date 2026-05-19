# Mini Agent OS — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Context:** Team name is **GarbageCollector** (the repo name `GarbageCollector_OS` reflects the team, not the topic — this project is *not* about garbage collection). This plan assumes a single developer executing it end-to-end; no task division is included.

**Goal:** Build an OS-inspired runtime that schedules LLM agents like processes and executes LLM-generated Python code inside Docker-based sandboxes, satisfying the Week 9 project requirements for Direction A (OS-for-LLM).

**Architecture:** A FastAPI backend exposes agent CRUD and scheduling endpoints. A scheduler (FCFS / Priority) pulls READY agents from a queue and hands them to an Executor. LLM agents call Upstage Solar Pro 3 through the OpenAI-compatible SDK. Code-execution agents take generated code and run it in an isolated Docker container (`docker run --rm --network=none --memory=128m --cpus=0.5 --pids-limit=64 --read-only`). A `QuotaManager` (threading.Lock) gates a shared API quota. An in-process `asyncio.Queue` chains agents (IPC). A `Logger` records every transition. A minimal static dashboard shows the agent table and logs.

**Tech Stack:** Python 3.11, FastAPI, Uvicorn, Pydantic v2, `openai` SDK (pointed at Solar Pro 3), Docker Engine (subprocess via `docker` CLI), pytest, vanilla HTML/CSS/JS frontend.

**OS Concepts Mapped to Code:**

| OS Concept | Where It Lives |
|---|---|
| Process / PCB | `app/agent.py` — `AgentTask` dataclass |
| Process state | `AgentState` enum (READY / RUNNING / DONE / TIMEOUT / ERROR) |
| Scheduler (FCFS, Priority) | `app/scheduler.py` |
| Process isolation | `app/sandbox.py` — Docker container per execution |
| System call restriction | sandbox: `--cap-drop=ALL`, `--network=none`, `--read-only`, `--security-opt=no-new-privileges` |
| File permission | sandbox: read-only rootfs, no host mounts |
| Resource limit | sandbox: `--memory`, `--cpus`, `--pids-limit` |
| Timeout | parent-side `subprocess.TimeoutExpired` → container is killed on cleanup |
| Synchronization | `app/quota_manager.py` — `threading.Lock` |
| IPC | `app/ipc.py` — `asyncio.Queue` between agents; stdin/stdout pipe to sandbox |
| Trace log | `app/logger.py` |

---

## File Structure

```
GarbageCollector_OS/                # branch: parkcheolwon
├── docker/
│   ├── Dockerfile                  # main app image
│   └── sandbox.Dockerfile          # restricted python runtime
├── docker-compose.yml              # one-command bring-up
├── app/
│   ├── __init__.py
│   ├── main.py                     # FastAPI app + endpoints
│   ├── agent.py                    # AgentTask + AgentState
│   ├── scheduler.py                # fcfs_schedule, priority_schedule
│   ├── quota_manager.py            # thread-safe quota gate
│   ├── llm_client.py               # Solar Pro 3 wrapper
│   ├── sandbox.py                  # docker subprocess runner
│   ├── ipc.py                      # agent message queue
│   ├── executor.py                 # orchestrates LLM + sandbox + quota
│   └── logger.py                   # file logger
├── static/
│   ├── index.html                  # dashboard
│   ├── style.css
│   └── script.js
├── tests/
│   ├── __init__.py
│   ├── test_agent.py
│   ├── test_scheduler.py
│   ├── test_quota_manager.py
│   ├── test_logger.py
│   ├── test_llm_client.py
│   ├── test_sandbox.py
│   ├── test_ipc.py
│   ├── test_executor.py
│   └── test_main.py
├── docs/
│   ├── superpowers/plans/2026-05-20-mini-agent-os.md   # this file
│   ├── technical-report.md
│   ├── development-process.md
│   ├── demo-script.md
│   └── presentation/               # slides
├── logs/
│   └── .gitkeep
├── .env.example                    # UPSTAGE_API_KEY=
├── .gitignore
├── .gitattributes                  # force LF for cross-platform
├── pytest.ini
├── requirements.txt
└── README.md
```

---

## Task 1: Project Scaffold

**Files:**
- Create: `.gitignore`
- Create: `.gitattributes`
- Create: `.env.example`
- Create: `README.md`
- Create: `requirements.txt`
- Create: `pytest.ini`
- Create: `app/__init__.py` (empty)
- Create: `tests/__init__.py` (empty)
- Create: `logs/.gitkeep` (empty)

- [ ] **Step 1: Write `.gitignore`**

```gitignore
# Python
__pycache__/
*.py[cod]
*.egg-info/
.pytest_cache/
.coverage
htmlcov/

# Env
.env
.venv/
venv/

# IDE
.vscode/
.idea/
*.swp

# OS
.DS_Store
Thumbs.db

# Project
logs/*.txt
!logs/.gitkeep
```

- [ ] **Step 2: Write `.gitattributes`** (forces LF endings for Windows + macOS parity)

```gitattributes
* text=auto eol=lf
*.png binary
*.jpg binary
*.ico binary
```

- [ ] **Step 3: Write `.env.example`**

```bash
# Upstage Solar Pro 3 (OpenAI-compatible)
UPSTAGE_API_KEY=replace_me
UPSTAGE_BASE_URL=https://api.upstage.ai/v1
UPSTAGE_MODEL=solar-pro-3

# Sandbox
SANDBOX_IMAGE=mini-agent-os-sandbox:latest
SANDBOX_MEMORY=128m
SANDBOX_CPUS=0.5
SANDBOX_TIMEOUT_DEFAULT=5

# Shared API quota (synchronization demo)
GLOBAL_QUOTA=1000
```

- [ ] **Step 4: Write `requirements.txt`**

```
fastapi==0.115.*
uvicorn[standard]==0.32.*
pydantic==2.*
openai==1.*
python-dotenv==1.*
pytest==8.*
pytest-asyncio==0.24.*
httpx==0.27.*
```

- [ ] **Step 5: Write `pytest.ini`**

```ini
[pytest]
testpaths = tests
asyncio_mode = auto
addopts = -v
markers =
    docker: requires Docker daemon and sandbox image
```

- [ ] **Step 6: Write `README.md`**

```markdown
# Mini Agent OS

> Team **GarbageCollector** · Week 9 Project, Direction A (OS-for-LLM)

OS-inspired runtime that schedules LLM agents like processes and executes generated code inside Docker sandboxes.

## Quickstart

```bash
cp .env.example .env   # add your UPSTAGE_API_KEY
docker compose build
docker compose up
open http://localhost:8000
```

## Documentation

- Implementation plan — `docs/superpowers/plans/2026-05-20-mini-agent-os.md`
- Technical report — `docs/technical-report.md`
- Development process — `docs/development-process.md`
- Demo script — `docs/demo-script.md`
```

- [ ] **Step 7: Create empty package files**

```bash
mkdir -p app tests static docker logs docs/presentation
touch app/__init__.py tests/__init__.py logs/.gitkeep
```

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "chore: scaffold project structure and tooling"
```

---

## Task 2: Docker Setup

**Files:**
- Create: `docker/Dockerfile`
- Create: `docker/sandbox.Dockerfile`
- Create: `docker-compose.yml`

- [ ] **Step 1: Write main app `docker/Dockerfile`**

```dockerfile
FROM python:3.11-slim

WORKDIR /app

# docker CLI so the app can spawn sandbox containers via the mounted socket
RUN apt-get update && apt-get install -y --no-install-recommends \
    docker.io \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app
COPY static ./static

ENV PYTHONUNBUFFERED=1

EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
```

- [ ] **Step 2: Write `docker/sandbox.Dockerfile`**

```dockerfile
FROM python:3.11-slim

# Non-root user for defense-in-depth
RUN useradd -m -u 1000 sandboxuser

WORKDIR /sandbox
USER sandboxuser

# Code is piped in on stdin; no source baked into image
# -I = isolated mode (ignore env vars, user site-packages, $PYTHONPATH)
ENTRYPOINT ["python", "-I", "-"]
```

- [ ] **Step 3: Write `docker-compose.yml`**

```yaml
services:
  app:
    build:
      context: .
      dockerfile: docker/Dockerfile
    image: mini-agent-os:latest
    ports:
      - "8000:8000"
    env_file:
      - .env
    volumes:
      # Docker-out-of-Docker: app spawns sibling sandbox containers
      - /var/run/docker.sock:/var/run/docker.sock
      - ./logs:/app/logs
    depends_on:
      sandbox-build:
        condition: service_completed_successfully

  # Build-only sidecar so the sandbox image exists before the app runs
  sandbox-build:
    build:
      context: .
      dockerfile: docker/sandbox.Dockerfile
    image: mini-agent-os-sandbox:latest
    command: ["echo", "sandbox image built"]
```

- [ ] **Step 4: Verify build**

Run: `docker compose build`
Expected: both `mini-agent-os:latest` and `mini-agent-os-sandbox:latest` build with no errors.

- [ ] **Step 5: Verify the sandbox image executes Python**

Run: `echo "print(1+1)" | docker run --rm -i mini-agent-os-sandbox:latest`
Expected: prints `2`.

- [ ] **Step 6: Commit**

```bash
git add docker/ docker-compose.yml
git commit -m "feat: docker setup for app and sandbox runtime"
```

---

## Task 3: Agent Model (AgentTask + AgentState)

**Files:**
- Create: `app/agent.py`
- Create: `tests/test_agent.py`

- [ ] **Step 1: Write the failing test `tests/test_agent.py`**

```python
from datetime import datetime
from app.agent import AgentTask, AgentState


def test_new_agent_is_ready():
    a = AgentTask(agent_id=1, name="x", prompt="p", priority=5, timeout=3, quota=2)
    assert a.state == AgentState.READY
    assert a.used_quota == 0
    assert a.result is None
    assert isinstance(a.created_time, datetime)


def test_to_dict_serializes_state_as_string():
    a = AgentTask(agent_id=1, name="x", prompt="p", priority=5, timeout=3, quota=2)
    d = a.to_dict()
    assert d["state"] == "READY"
    assert d["agent_id"] == 1
    assert d["start_time"] is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_agent.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.agent'`

- [ ] **Step 3: Write `app/agent.py`**

```python
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Optional


class AgentState(str, Enum):
    READY = "READY"
    RUNNING = "RUNNING"
    DONE = "DONE"
    TIMEOUT = "TIMEOUT"
    ERROR = "ERROR"


@dataclass
class AgentTask:
    agent_id: int
    name: str
    prompt: str
    priority: int
    timeout: int
    quota: int

    state: AgentState = AgentState.READY
    used_quota: int = 0
    result: Optional[str] = None
    error_message: Optional[str] = None

    created_time: datetime = field(default_factory=datetime.now)
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None

    def to_dict(self) -> dict:
        fmt = "%Y-%m-%d %H:%M:%S"
        return {
            "agent_id": self.agent_id,
            "name": self.name,
            "prompt": self.prompt,
            "priority": self.priority,
            "timeout": self.timeout,
            "quota": self.quota,
            "used_quota": self.used_quota,
            "state": self.state.value,
            "result": self.result,
            "error_message": self.error_message,
            "created_time": self.created_time.strftime(fmt),
            "start_time": self.start_time.strftime(fmt) if self.start_time else None,
            "end_time": self.end_time.strftime(fmt) if self.end_time else None,
        }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_agent.py -v`
Expected: 2 passed.

- [ ] **Step 5: Commit**

```bash
git add app/agent.py tests/test_agent.py
git commit -m "feat: AgentTask dataclass with AgentState enum"
```

---

## Task 4: Scheduler (FCFS + Priority)

**Files:**
- Create: `app/scheduler.py`
- Create: `tests/test_scheduler.py`

- [ ] **Step 1: Write the failing test `tests/test_scheduler.py`**

```python
from datetime import datetime, timedelta
from app.agent import AgentTask, AgentState
from app.scheduler import fcfs_schedule, priority_schedule


def _agent(agent_id, priority, created_offset_sec, state=AgentState.READY):
    a = AgentTask(agent_id=agent_id, name=f"a{agent_id}", prompt="p",
                  priority=priority, timeout=3, quota=1)
    a.state = state
    a.created_time = datetime.now() + timedelta(seconds=created_offset_sec)
    return a


def test_fcfs_orders_by_created_time():
    agents = [_agent(1, 1, 2), _agent(2, 9, 0), _agent(3, 5, 1)]
    order = [a.agent_id for a in fcfs_schedule(agents)]
    assert order == [2, 3, 1]


def test_fcfs_ignores_non_ready():
    agents = [_agent(1, 1, 0, state=AgentState.DONE), _agent(2, 1, 1)]
    order = [a.agent_id for a in fcfs_schedule(agents)]
    assert order == [2]


def test_priority_orders_high_first_then_fcfs_tiebreak():
    agents = [_agent(1, 5, 0), _agent(2, 9, 1), _agent(3, 5, 2)]
    order = [a.agent_id for a in priority_schedule(agents)]
    assert order == [2, 1, 3]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_scheduler.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.scheduler'`

- [ ] **Step 3: Write `app/scheduler.py`**

```python
from typing import List
from app.agent import AgentTask, AgentState


def _ready(agents: List[AgentTask]) -> List[AgentTask]:
    return [a for a in agents if a.state == AgentState.READY]


def fcfs_schedule(agents: List[AgentTask]) -> List[AgentTask]:
    """First-Come, First-Served: execute in creation order."""
    return sorted(_ready(agents), key=lambda a: a.created_time)


def priority_schedule(agents: List[AgentTask]) -> List[AgentTask]:
    """Higher priority value runs first; FCFS as tiebreaker."""
    return sorted(_ready(agents), key=lambda a: (-a.priority, a.created_time))
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_scheduler.py -v`
Expected: 3 passed.

- [ ] **Step 5: Commit**

```bash
git add app/scheduler.py tests/test_scheduler.py
git commit -m "feat: FCFS and Priority schedulers"
```

---

## Task 5: Quota Manager (Synchronization)

**Files:**
- Create: `app/quota_manager.py`
- Create: `tests/test_quota_manager.py`

- [ ] **Step 1: Write the failing test `tests/test_quota_manager.py`**

```python
import threading
from app.quota_manager import QuotaManager


def test_acquire_within_limit_succeeds():
    qm = QuotaManager(total=10)
    assert qm.try_acquire(3) is True
    assert qm.remaining() == 7


def test_acquire_over_limit_fails_atomically():
    qm = QuotaManager(total=5)
    assert qm.try_acquire(6) is False
    assert qm.remaining() == 5  # not partially consumed


def test_concurrent_acquires_never_exceed_total():
    qm = QuotaManager(total=100)
    successes = []

    def worker():
        if qm.try_acquire(1):
            successes.append(1)

    threads = [threading.Thread(target=worker) for _ in range(500)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    assert sum(successes) == 100
    assert qm.remaining() == 0
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_quota_manager.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.quota_manager'`

- [ ] **Step 3: Write `app/quota_manager.py`**

```python
import threading


class QuotaManager:
    """Thread-safe shared API call quota — demonstrates synchronization (mutex)."""

    def __init__(self, total: int):
        self._total = total
        self._used = 0
        self._lock = threading.Lock()

    def try_acquire(self, n: int) -> bool:
        with self._lock:
            if self._used + n > self._total:
                return False
            self._used += n
            return True

    def remaining(self) -> int:
        with self._lock:
            return self._total - self._used

    def reset(self) -> None:
        with self._lock:
            self._used = 0
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_quota_manager.py -v`
Expected: 3 passed.

- [ ] **Step 5: Commit**

```bash
git add app/quota_manager.py tests/test_quota_manager.py
git commit -m "feat: thread-safe QuotaManager with mutex (synchronization)"
```

---

## Task 6: Logger

**Files:**
- Create: `app/logger.py`
- Create: `tests/test_logger.py`

- [ ] **Step 1: Write the failing test `tests/test_logger.py`**

```python
from pathlib import Path
from app.logger import Logger


def test_log_event_appends_line(tmp_path: Path):
    log_file = tmp_path / "exec.log"
    lg = Logger(log_file)
    lg.log("agent 1 started")
    lg.log("agent 1 done")
    lines = lg.read_all()
    assert len(lines) == 2
    assert "agent 1 started" in lines[0]
    assert "agent 1 done" in lines[1]


def test_clear_empties_log(tmp_path: Path):
    log_file = tmp_path / "exec.log"
    lg = Logger(log_file)
    lg.log("x")
    lg.clear()
    assert lg.read_all() == []
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_logger.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.logger'`

- [ ] **Step 3: Write `app/logger.py`**

```python
from datetime import datetime
from pathlib import Path
from typing import List


class Logger:
    def __init__(self, path: Path):
        self._path = Path(path)
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._path.touch(exist_ok=True)

    def log(self, message: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        line = f"[{ts}] {message}"
        with self._path.open("a", encoding="utf-8") as f:
            f.write(line + "\n")
        print(line)

    def read_all(self) -> List[str]:
        return self._path.read_text(encoding="utf-8").splitlines()

    def clear(self) -> None:
        self._path.write_text("", encoding="utf-8")
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_logger.py -v`
Expected: 2 passed.

- [ ] **Step 5: Commit**

```bash
git add app/logger.py tests/test_logger.py
git commit -m "feat: file-based execution Logger"
```

---

## Task 7: LLM Client (Solar Pro 3 wrapper)

**Files:**
- Create: `app/llm_client.py`
- Create: `tests/test_llm_client.py`

- [ ] **Step 1: Write the failing test `tests/test_llm_client.py`**

```python
from unittest.mock import MagicMock, patch
from app.llm_client import LLMClient


def test_complete_calls_openai_with_configured_model():
    fake_response = MagicMock()
    fake_response.choices = [MagicMock(message=MagicMock(content="hello world"))]

    with patch("app.llm_client.OpenAI") as mock_openai:
        mock_client = MagicMock()
        mock_client.chat.completions.create.return_value = fake_response
        mock_openai.return_value = mock_client

        llm = LLMClient(api_key="k", base_url="u", model="solar-pro-3")
        out = llm.complete("say hi")

        assert out == "hello world"
        mock_client.chat.completions.create.assert_called_once()
        kwargs = mock_client.chat.completions.create.call_args.kwargs
        assert kwargs["model"] == "solar-pro-3"
        assert kwargs["messages"][0]["content"] == "say hi"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_llm_client.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.llm_client'`

- [ ] **Step 3: Write `app/llm_client.py`**

```python
from openai import OpenAI


class LLMClient:
    """Thin wrapper over Upstage Solar Pro 3 (OpenAI-compatible endpoint)."""

    def __init__(self, api_key: str, base_url: str, model: str):
        self._client = OpenAI(api_key=api_key, base_url=base_url)
        self._model = model

    def complete(self, prompt: str) -> str:
        resp = self._client.chat.completions.create(
            model=self._model,
            messages=[{"role": "user", "content": prompt}],
        )
        return resp.choices[0].message.content or ""
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_llm_client.py -v`
Expected: 1 passed.

- [ ] **Step 5: Commit**

```bash
git add app/llm_client.py tests/test_llm_client.py
git commit -m "feat: Solar Pro 3 LLM client wrapper"
```

---

## Task 8: Sandbox (Docker subprocess)

**Files:**
- Create: `app/sandbox.py`
- Create: `tests/test_sandbox.py`

> **Note:** Tests in this task require Docker to be running locally and the `mini-agent-os-sandbox:latest` image to be built (`docker compose build sandbox-build`). Skip with `pytest -m "not docker"` when running fast unit suites.

- [ ] **Step 1: Write the failing test `tests/test_sandbox.py`**

```python
import pytest
from app.sandbox import Sandbox, SandboxResult


pytestmark = pytest.mark.docker


def test_runs_simple_python_and_captures_stdout():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=3)
    result: SandboxResult = sb.run("print(2 + 2)")
    assert result.exit_code == 0
    assert result.stdout.strip() == "4"
    assert result.timed_out is False


def test_timeout_kills_long_running_code():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=2)
    result = sb.run("import time; time.sleep(10); print('done')")
    assert result.timed_out is True
    assert "done" not in result.stdout


def test_network_is_disabled():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=5)
    code = (
        "import urllib.request\n"
        "try:\n"
        "    urllib.request.urlopen('http://example.com', timeout=2)\n"
        "    print('NET_OK')\n"
        "except Exception as e:\n"
        "    print('NET_BLOCKED', type(e).__name__)\n"
    )
    result = sb.run(code)
    assert "NET_BLOCKED" in result.stdout
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_sandbox.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.sandbox'`

- [ ] **Step 3: Write `app/sandbox.py`**

```python
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
      - Process isolation: each run = its own container
      - System call restriction: --cap-drop=ALL, no network, read-only fs
      - Resource limit: --memory, --cpus, --pids-limit
      - Timeout: parent-side kill via subprocess timeout
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
                stdout=e.stdout.decode() if e.stdout else "",
                stderr=e.stderr.decode() if e.stderr else "",
                timed_out=True,
            )
```

- [ ] **Step 4: Build sandbox image (one-time)**

Run: `docker compose build sandbox-build`
Expected: image `mini-agent-os-sandbox:latest` exists (verify with `docker images`).

- [ ] **Step 5: Run test to verify it passes**

Run: `pytest tests/test_sandbox.py -v`
Expected: 3 passed (slow — each test spawns a real container).

- [ ] **Step 6: Commit**

```bash
git add app/sandbox.py tests/test_sandbox.py
git commit -m "feat: Docker-based Sandbox with isolation and resource limits"
```

---

## Task 9: IPC (Inter-agent message queue)

**Files:**
- Create: `app/ipc.py`
- Create: `tests/test_ipc.py`

- [ ] **Step 1: Write the failing test `tests/test_ipc.py`**

```python
import asyncio
import pytest
from app.ipc import MessageBus


@pytest.mark.asyncio
async def test_send_and_receive_passes_payload():
    bus = MessageBus()
    await bus.send("topic-a", {"x": 1})
    msg = await bus.receive("topic-a", timeout=1)
    assert msg == {"x": 1}


@pytest.mark.asyncio
async def test_receive_times_out_when_no_message():
    bus = MessageBus()
    with pytest.raises(asyncio.TimeoutError):
        await bus.receive("nobody", timeout=0.2)


@pytest.mark.asyncio
async def test_messages_are_per_topic():
    bus = MessageBus()
    await bus.send("a", 1)
    await bus.send("b", 2)
    assert await bus.receive("b", timeout=1) == 2
    assert await bus.receive("a", timeout=1) == 1
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_ipc.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.ipc'`

- [ ] **Step 3: Write `app/ipc.py`**

```python
import asyncio
from typing import Any, Dict


class MessageBus:
    """Per-topic asyncio queues — IPC between agents in a pipeline."""

    def __init__(self):
        self._queues: Dict[str, asyncio.Queue] = {}

    def _queue(self, topic: str) -> asyncio.Queue:
        if topic not in self._queues:
            self._queues[topic] = asyncio.Queue()
        return self._queues[topic]

    async def send(self, topic: str, payload: Any) -> None:
        await self._queue(topic).put(payload)

    async def receive(self, topic: str, timeout: float) -> Any:
        return await asyncio.wait_for(self._queue(topic).get(), timeout=timeout)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_ipc.py -v`
Expected: 3 passed.

- [ ] **Step 5: Commit**

```bash
git add app/ipc.py tests/test_ipc.py
git commit -m "feat: per-topic MessageBus for inter-agent IPC"
```

---

## Task 10: Executor (orchestration)

**Files:**
- Create: `app/executor.py`
- Create: `tests/test_executor.py`

- [ ] **Step 1: Write the failing test `tests/test_executor.py`**

```python
from unittest.mock import MagicMock
from app.agent import AgentTask, AgentState
from app.executor import Executor, AgentKind
from app.quota_manager import QuotaManager
from app.sandbox import SandboxResult


def _task(kind: AgentKind, prompt="hi", quota=2):
    t = AgentTask(agent_id=1, name="t", prompt=prompt,
                  priority=1, timeout=3, quota=quota)
    t.kind = kind
    return t


def test_llm_agent_marks_done_and_stores_result():
    llm = MagicMock()
    llm.complete.return_value = "answer"
    qm = QuotaManager(total=10)
    ex = Executor(llm=llm, sandbox=MagicMock(), quota=qm, logger=MagicMock())

    task = _task(AgentKind.LLM)
    ex.execute(task)

    assert task.state == AgentState.DONE
    assert task.result == "answer"
    assert task.used_quota == 1


def test_quota_exhaustion_marks_error():
    llm = MagicMock()
    qm = QuotaManager(total=0)
    ex = Executor(llm=llm, sandbox=MagicMock(), quota=qm, logger=MagicMock())

    task = _task(AgentKind.LLM)
    ex.execute(task)

    assert task.state == AgentState.ERROR
    assert "quota" in task.error_message.lower()
    llm.complete.assert_not_called()


def test_code_agent_runs_in_sandbox_and_marks_done():
    llm = MagicMock()
    llm.complete.return_value = "print('hello')"
    sandbox = MagicMock()
    sandbox.run.return_value = SandboxResult(
        exit_code=0, stdout="hello\n", stderr="", timed_out=False
    )
    qm = QuotaManager(total=10)
    ex = Executor(llm=llm, sandbox=sandbox, quota=qm, logger=MagicMock())

    task = _task(AgentKind.CODE, prompt="generate hello")
    ex.execute(task)

    assert task.state == AgentState.DONE
    assert "hello" in task.result
    sandbox.run.assert_called_once_with("print('hello')")


def test_code_agent_timeout_marks_timeout_state():
    llm = MagicMock()
    llm.complete.return_value = "while True: pass"
    sandbox = MagicMock()
    sandbox.run.return_value = SandboxResult(
        exit_code=-1, stdout="", stderr="", timed_out=True
    )
    qm = QuotaManager(total=10)
    ex = Executor(llm=llm, sandbox=sandbox, quota=qm, logger=MagicMock())

    task = _task(AgentKind.CODE)
    ex.execute(task)

    assert task.state == AgentState.TIMEOUT
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_executor.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.executor'`

- [ ] **Step 3: Write `app/executor.py`**

```python
from datetime import datetime
from enum import Enum
from app.agent import AgentTask, AgentState


class AgentKind(str, Enum):
    LLM = "llm"     # plain LLM completion
    CODE = "code"   # LLM generates code → sandbox executes


class Executor:
    """Orchestrates a single agent: quota gate → LLM → optional sandbox → state."""

    def __init__(self, llm, sandbox, quota, logger):
        self._llm = llm
        self._sandbox = sandbox
        self._quota = quota
        self._log = logger

    def execute(self, task: AgentTask) -> AgentTask:
        task.state = AgentState.RUNNING
        task.start_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) started")

        if not self._quota.try_acquire(1):
            self._fail(task, AgentState.ERROR, "API quota exhausted")
            return task
        task.used_quota = 1

        try:
            llm_output = self._llm.complete(task.prompt)
        except Exception as e:
            self._fail(task, AgentState.ERROR, f"LLM error: {e}")
            return task

        kind = getattr(task, "kind", AgentKind.LLM)
        if kind == AgentKind.LLM:
            task.result = llm_output
            self._done(task)
            return task

        # CODE: run LLM-generated code in sandbox
        sb_result = self._sandbox.run(llm_output)
        if sb_result.timed_out:
            self._fail(task, AgentState.TIMEOUT, "Sandbox execution timed out")
            return task
        if sb_result.exit_code != 0:
            self._fail(task, AgentState.ERROR,
                       f"Sandbox exit {sb_result.exit_code}: {sb_result.stderr.strip()}")
            return task

        task.result = f"code:\n{llm_output}\n---\nstdout:\n{sb_result.stdout}"
        self._done(task)
        return task

    def _done(self, task: AgentTask) -> None:
        task.state = AgentState.DONE
        task.end_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) completed")

    def _fail(self, task: AgentTask, state: AgentState, message: str) -> None:
        task.state = state
        task.error_message = message
        task.end_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) {state.value}: {message}")
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_executor.py -v`
Expected: 4 passed.

- [ ] **Step 5: Commit**

```bash
git add app/executor.py tests/test_executor.py
git commit -m "feat: Executor orchestrates quota, LLM, and sandbox per agent"
```

---

## Task 11: FastAPI Endpoints

**Files:**
- Create: `app/main.py`
- Create: `tests/test_main.py`

- [ ] **Step 1: Write the failing test `tests/test_main.py`**

```python
from unittest.mock import patch, MagicMock
from fastapi.testclient import TestClient


def _make_client():
    # Patch construction-time deps so tests don't hit the network or Docker.
    with patch("app.main._build_llm") as mk_llm, \
         patch("app.main._build_sandbox") as mk_sb:
        mk_llm.return_value = MagicMock()
        mk_llm.return_value.complete.return_value = "ok"
        mk_sb.return_value = MagicMock()
        from app.main import app
        return TestClient(app)


def test_create_and_list_agents():
    client = _make_client()
    r = client.post("/agents", json={
        "name": "summary", "prompt": "do x",
        "priority": 5, "timeout": 3, "quota": 1, "kind": "llm"
    })
    assert r.status_code == 200
    body = r.json()
    assert body["agent"]["state"] == "READY"
    listing = client.get("/agents").json()
    assert listing["count"] == 1


def test_run_fcfs_executes_in_creation_order():
    client = _make_client()
    for name in ["a", "b", "c"]:
        client.post("/agents", json={
            "name": name, "prompt": "p", "priority": 1,
            "timeout": 3, "quota": 1, "kind": "llm"
        })
    r = client.post("/run/fcfs").json()
    assert r["execution_order"] == [1, 2, 3]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_main.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.main'`

- [ ] **Step 3: Write `app/main.py`**

```python
import os
from pathlib import Path

from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from dotenv import load_dotenv

from app.agent import AgentTask
from app.executor import Executor, AgentKind
from app.llm_client import LLMClient
from app.logger import Logger
from app.quota_manager import QuotaManager
from app.sandbox import Sandbox
from app.scheduler import fcfs_schedule, priority_schedule


load_dotenv()


def _build_llm() -> LLMClient:
    return LLMClient(
        api_key=os.environ["UPSTAGE_API_KEY"],
        base_url=os.environ.get("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1"),
        model=os.environ.get("UPSTAGE_MODEL", "solar-pro-3"),
    )


def _build_sandbox() -> Sandbox:
    return Sandbox(
        image=os.environ.get("SANDBOX_IMAGE", "mini-agent-os-sandbox:latest"),
        memory=os.environ.get("SANDBOX_MEMORY", "128m"),
        cpus=os.environ.get("SANDBOX_CPUS", "0.5"),
        timeout=int(os.environ.get("SANDBOX_TIMEOUT_DEFAULT", "5")),
    )


_agents: list[AgentTask] = []
_next_id = 1
_logger = Logger(Path("logs/execution_log.txt"))
_quota = QuotaManager(total=int(os.environ.get("GLOBAL_QUOTA", "1000")))
_llm = _build_llm() if os.environ.get("UPSTAGE_API_KEY") else None
_sandbox = _build_sandbox()
_executor = Executor(llm=_llm, sandbox=_sandbox, quota=_quota, logger=_logger)


app = FastAPI(title="Mini Agent OS", version="0.1.0")
app.mount("/static", StaticFiles(directory="static"), name="static")


class AgentCreate(BaseModel):
    name: str = Field(..., examples=["summary"])
    prompt: str
    priority: int = Field(1, ge=1, le=10)
    timeout: int = Field(3, ge=1, le=30)
    quota: int = Field(1, ge=1, le=100)
    kind: AgentKind = AgentKind.LLM


@app.get("/")
def root():
    return FileResponse("static/index.html")


@app.post("/agents")
def create_agent(req: AgentCreate):
    global _next_id
    task = AgentTask(
        agent_id=_next_id, name=req.name, prompt=req.prompt,
        priority=req.priority, timeout=req.timeout, quota=req.quota,
    )
    task.kind = req.kind
    _agents.append(task)
    _next_id += 1
    _logger.log(f"Agent {task.agent_id} ({task.name}) created kind={req.kind.value}")
    return {"message": "created", "agent": task.to_dict()}


@app.get("/agents")
def list_agents():
    return {"count": len(_agents), "agents": [a.to_dict() for a in _agents]}


def _run(order_fn):
    scheduled = order_fn(_agents)
    execution_order = []
    for task in scheduled:
        execution_order.append(task.agent_id)
        _executor.execute(task)
    return {
        "execution_order": execution_order,
        "agents": [a.to_dict() for a in _agents],
    }


@app.post("/run/fcfs")
def run_fcfs():
    _logger.log("FCFS scheduler started")
    out = _run(fcfs_schedule)
    _logger.log("FCFS scheduler finished")
    return out


@app.post("/run/priority")
def run_priority():
    _logger.log("Priority scheduler started")
    out = _run(priority_schedule)
    _logger.log("Priority scheduler finished")
    return out


@app.get("/logs")
def get_logs():
    return {"logs": _logger.read_all()}


@app.delete("/agents")
def clear():
    global _agents, _next_id
    _agents = []
    _next_id = 1
    _logger.clear()
    return {"message": "cleared"}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest tests/test_main.py -v`
Expected: 2 passed.

- [ ] **Step 5: Run full non-Docker suite**

Run: `pytest -v -m "not docker"`
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add app/main.py tests/test_main.py
git commit -m "feat: FastAPI endpoints for agent CRUD and scheduling"
```

---

## Task 12: Dashboard (static frontend)

**Files:**
- Create: `static/index.html`
- Create: `static/style.css`
- Create: `static/script.js`

- [ ] **Step 1: Write `static/index.html`**

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>Mini Agent OS — GarbageCollector</title>
  <link rel="stylesheet" href="/static/style.css" />
</head>
<body>
  <header>
    <h1>Mini Agent OS</h1>
    <p>OS-inspired runtime for LLM agents · sandboxed code execution · team <strong>GarbageCollector</strong></p>
  </header>

  <section class="create">
    <h2>Create Agent</h2>
    <form id="create-form">
      <input name="name" placeholder="name" required />
      <textarea name="prompt" placeholder="prompt" required></textarea>
      <select name="kind">
        <option value="llm">llm</option>
        <option value="code">code (sandboxed)</option>
      </select>
      <input name="priority" type="number" value="5" min="1" max="10" />
      <input name="timeout" type="number" value="3" min="1" max="30" />
      <input name="quota" type="number" value="1" min="1" max="100" />
      <button type="submit">Create</button>
    </form>
  </section>

  <section class="run">
    <button id="run-fcfs">Run FCFS</button>
    <button id="run-priority">Run Priority</button>
    <button id="clear">Clear all</button>
  </section>

  <section>
    <h2>Agents</h2>
    <table id="agents">
      <thead><tr>
        <th>ID</th><th>Name</th><th>State</th><th>Priority</th>
        <th>Quota</th><th>Result</th>
      </tr></thead>
      <tbody></tbody>
    </table>
  </section>

  <section>
    <h2>Logs</h2>
    <pre id="logs"></pre>
  </section>

  <script src="/static/script.js"></script>
</body>
</html>
```

- [ ] **Step 2: Write `static/style.css`**

```css
* { box-sizing: border-box; font-family: -apple-system, system-ui, sans-serif; }
body { margin: 0; padding: 2rem; background: #0f1115; color: #e6e9ef; }
h1 { margin: 0 0 .25rem 0; }
header p { color: #8a93a3; margin-top: 0; }
section { margin-top: 2rem; }
form { display: grid; gap: .5rem; max-width: 480px; }
input, textarea, select, button {
  padding: .5rem; border-radius: 6px; border: 1px solid #2a2f3a;
  background: #181c25; color: inherit;
}
button { cursor: pointer; background: #3b82f6; border-color: #3b82f6; }
button:hover { background: #2563eb; }
.run button { margin-right: .5rem; }
table { width: 100%; border-collapse: collapse; }
th, td { padding: .5rem; border-bottom: 1px solid #2a2f3a; text-align: left; }
.state-READY { color: #94a3b8; }
.state-RUNNING { color: #fbbf24; }
.state-DONE { color: #34d399; }
.state-TIMEOUT { color: #f87171; }
.state-ERROR { color: #ef4444; }
pre { background: #181c25; padding: 1rem; border-radius: 6px; max-height: 300px; overflow: auto; }
```

- [ ] **Step 3: Write `static/script.js`**

```javascript
async function refresh() {
  const a = await (await fetch("/agents")).json();
  const tbody = document.querySelector("#agents tbody");
  tbody.innerHTML = "";
  for (const ag of a.agents) {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${ag.agent_id}</td>
      <td>${ag.name}</td>
      <td class="state-${ag.state}">${ag.state}</td>
      <td>${ag.priority}</td>
      <td>${ag.used_quota}/${ag.quota}</td>
      <td><pre>${(ag.result || ag.error_message || "").slice(0, 400)}</pre></td>
    `;
    tbody.appendChild(tr);
  }
  const l = await (await fetch("/logs")).json();
  document.querySelector("#logs").textContent = l.logs.join("\n");
}

document.querySelector("#create-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(e.target);
  const body = Object.fromEntries(fd.entries());
  ["priority", "timeout", "quota"].forEach(k => body[k] = Number(body[k]));
  await fetch("/agents", {
    method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body),
  });
  e.target.reset();
  refresh();
});

document.querySelector("#run-fcfs").onclick = async () => {
  await fetch("/run/fcfs", {method: "POST"});
  refresh();
};
document.querySelector("#run-priority").onclick = async () => {
  await fetch("/run/priority", {method: "POST"});
  refresh();
};
document.querySelector("#clear").onclick = async () => {
  await fetch("/agents", {method: "DELETE"});
  refresh();
};

refresh();
setInterval(refresh, 2000);
```

- [ ] **Step 4: Manual smoke test**

Run: `docker compose up` then open `http://localhost:8000`
Expected: dashboard loads. With a real `UPSTAGE_API_KEY` set, create an `llm` agent, click Run FCFS, watch the row turn green (DONE).

- [ ] **Step 5: Commit**

```bash
git add static/
git commit -m "feat: minimal dashboard for agents, runs, and logs"
```

---

## Task 13: End-to-End Demo Script

**Files:**
- Create: `docs/demo-script.md`

- [ ] **Step 1: Write `docs/demo-script.md`**

```markdown
# Demo Script — Mini Agent OS (Team GarbageCollector)

## Setup
1. `cp .env.example .env` and add `UPSTAGE_API_KEY`.
2. `docker compose build`
3. `docker compose up`
4. Open `http://localhost:8000`.

## Scenarios

### Scenario A — FCFS scheduling (LLM agents)
1. Create three LLM agents with priorities 1, 5, 9 (in that order).
2. Click "Run FCFS". Expected: execution order 1 → 2 → 3 (creation order).

### Scenario B — Priority scheduling
1. Click "Clear all", recreate the same three agents.
2. Click "Run Priority". Expected: 3 → 2 → 1 (highest priority first).

### Scenario C — Sandboxed code execution
1. Create a `code` agent with prompt: `Write a Python program that prints the first 10 Fibonacci numbers.`
2. Run. Expected: row shows DONE; result includes generated code and stdout `0 1 1 2 3 5 8 13 21 34`.

### Scenario D — Timeout (sandbox kill)
1. Create a `code` agent, timeout=2s, prompt: `Write Python that sleeps for 60 seconds.`
2. Run. Expected: state becomes TIMEOUT within ~2s.

### Scenario E — Quota exhaustion (synchronization)
1. Set `GLOBAL_QUOTA=2` in `.env` and restart.
2. Create 5 LLM agents and run.
3. Expected: first two DONE, remaining three ERROR with "API quota exhausted".

### Scenario F — Network isolation (system call restriction)
1. Create a `code` agent with prompt: `Write Python that uses urllib to fetch https://example.com and prints the status code.`
2. Run. Expected: code runs but reports network error — `--network=none` blocks egress.
```

- [ ] **Step 2: Commit**

```bash
git add docs/demo-script.md
git commit -m "docs: end-to-end demo script"
```

---

## Task 14: Technical Report and Process Skeletons

**Files:**
- Create: `docs/technical-report.md`
- Create: `docs/development-process.md`

- [ ] **Step 1: Write `docs/technical-report.md`**

```markdown
# Mini Agent OS — Technical Report

**Team:** GarbageCollector
**Direction:** A — OS-for-LLM

## 1. Project Overview
[1 paragraph: motivation, why Direction A, what was built, the headline result.]

## 2. System Architecture
[Diagram: User → FastAPI → Scheduler → Executor → (LLMClient | Sandbox) → Logger.
Reference file paths so the reader can jump to the code.]

## 3. Tech Stack
- Python 3.11, FastAPI, Pydantic v2
- Upstage Solar Pro 3 via `openai` SDK
- Docker Engine (sandbox isolation)
- Vanilla HTML / CSS / JS dashboard

## 4. OS Concepts and Where They Live

| OS Concept | Implementation File | Brief description |
|---|---|---|
| Process / PCB | `app/agent.py` | `AgentTask` dataclass holds all per-agent state |
| Process state | `app/agent.py` | `AgentState` enum: READY → RUNNING → DONE/TIMEOUT/ERROR |
| Scheduler (FCFS / Priority) | `app/scheduler.py` | Pure sort functions over the ready queue |
| Process isolation | `app/sandbox.py` | Each code run = its own Docker container |
| System call restriction | `app/sandbox.py` | `--cap-drop=ALL`, `--network=none`, `--read-only` |
| File permission | `app/sandbox.py` | Read-only rootfs, no host mounts |
| Resource limit | `app/sandbox.py` | `--memory`, `--cpus`, `--pids-limit` |
| Timeout | `app/sandbox.py` | `subprocess.run(..., timeout=N)` → container killed on cleanup |
| Synchronization (mutex) | `app/quota_manager.py` | `threading.Lock` guards shared quota |
| IPC | `app/ipc.py`, sandbox stdin/stdout | `asyncio.Queue` between agents; pipe to sandbox |
| Trace log | `app/logger.py` | File-backed event log |

## 5. LLM Integration
[How Solar Pro 3 is called, prompt format, what the wrapper does, error handling.]

## 6. Walkthrough of a Code-Execution Agent
[Step by step: API call → quota acquire → LLM completion (code text) → sandbox.run → captured stdout → DONE state. Show one real screenshot or log excerpt.]

## 7. Limitations and Future Work
- Single-host runtime; no distributed scheduling.
- No preemption — a running agent runs to completion or timeout.
- Sandbox image is Python-only.
- Dashboard polls every 2 s — no websocket push.
- In-memory agent list — lost on restart.

## 8. Conclusion
[1 paragraph: what was learned about applying OS abstractions to LLM workloads.]
```

- [ ] **Step 2: Write `docs/development-process.md`**

```markdown
# Development Process

## Planning
Implementation plan: `docs/superpowers/plans/2026-05-20-mini-agent-os.md`

## Schedule
| Week | Milestone |
|---|---|
| 9 | Plan, scaffold, Docker setup |
| 10 | Agent, Scheduler, Logger, QuotaManager |
| 11 | LLM client, Sandbox, IPC |
| 12 | Executor, FastAPI, Dashboard |
| 13 | Demo, report, slides |

## Weekly Progress
### Week 9
- ...

## Issues and Resolutions
- ...

## Retrospective
- What went well:
- What I would change next time:
```

- [ ] **Step 3: Commit**

```bash
git add docs/technical-report.md docs/development-process.md
git commit -m "docs: technical report and development process skeletons"
```

---

## Self-Review

**Spec coverage:**
- ✅ Direction A (OS-for-LLM) — Mini Agent OS as a runtime
- ✅ OS concepts substantively designed and implemented — 9 distinct concepts mapped to code, not "runs on Linux" hand-waving
- ✅ Real LLM integration (not a wrapper) — Solar Pro 3 sits behind a scheduler, quota gate, and optional sandbox
- ✅ Working app + setup instructions + demo script (Tasks 1, 11, 13)
- ✅ Technical report skeleton (Task 14)
- ✅ Development process doc skeleton (Task 14)
- ⚠️ Presentation slides (English) — not in this plan; slides are a manual creative artifact, produced from the technical report once implementation is stable

**Placeholder scan:** none in the implementation tasks — every code block is concrete. The docs in Task 14 contain bracketed "[fill in]" cues, which are intentional: those are skeletons for the author to flesh out after the implementation is done.

**Type consistency:**
- `AgentTask.kind` is attached externally (Tasks 10, 11); both tests and `main.py` set it the same way.
- `SandboxResult` fields used identically in Task 8 (definition), Task 10 (executor), and tests.
- `AgentKind` enum values `"llm"` / `"code"` used identically in Task 10, Task 11 (Pydantic field), and Task 12 (`<option value>`).

---

## Execution Handoff

Two execution options:

1. **Subagent-Driven (recommended)** — dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — execute tasks in this session using executing-plans, batch with checkpoints.
