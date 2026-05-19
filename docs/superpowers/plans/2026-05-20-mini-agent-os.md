# Mini Agent OS — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Context:** Team name is **GarbageCollector** (the repo name `GarbageCollector_OS` reflects the team, not the topic — this project is *not* about garbage collection). This plan assumes a single developer executing it end-to-end; no task division is included. The plan starts from a clean orphan branch and is not constrained by anything that was previously in `main`.

**Goal:** Build an OS-inspired runtime that schedules LLM agents like processes, executes them concurrently on a worker pool, runs LLM-generated Python code inside Docker-based sandboxes, and chains agents into pipelines through a bounded message bus — satisfying the Week 9 Direction A (OS-for-LLM) project requirements with substantive OS concept implementation. The runtime also ships a built-in **scheduler simulator** that computes Gantt charts for FCFS, non-preemptive Priority, and Round Robin so all three classroom-standard policies can be demonstrated even though real LLM calls cannot be preempted.

**Architecture:** A FastAPI backend exposes agent CRUD, scheduling, and simulator endpoints. Created agents enter a `ReadyQueue` synchronized by `threading.Condition` (wait/notify). A worker pool (`N` threads) blocks on the queue and concurrently executes agents. Each worker calls the `Executor`, which (a) optionally blocks on a bounded `MessageBus` to receive pipeline input from a predecessor agent, (b) gates on a `QuotaManager` (mutex), (c) invokes the Solar Pro 3 LLM, (d) for code-execution agents pipes the generated code into a hardened Docker sandbox, and (e) optionally publishes the result to a successor agent over the same bus. An `EventBus` bridges worker threads to the asyncio event loop so the FastAPI Server-Sent Events stream pushes live state changes to the dashboard. A pure-function `Simulator` module computes Gantt timelines for FCFS / Priority / Round Robin and powers a separate dashboard panel for textbook scheduling demos.

**Tech Stack:**
- **Python 3.11**, managed by **uv** (modern fast package manager with lockfile)
- **FastAPI** + **Pydantic v2** + **sse-starlette** for SSE
- **openai** SDK pointed at Upstage Solar Pro 3 (OpenAI-compatible)
- **Docker Engine** for sandbox isolation (cross-platform: Windows / macOS via Docker Desktop)
- **threading** (workers, Lock, Condition) + **asyncio** (FastAPI, SSE) bridged through a thread-safe `queue.Queue`
- Vanilla HTML / CSS / JS dashboard (no build step, SSE-driven)
- **pytest** + **pytest-asyncio**

**Concurrency Model:** This runtime intentionally layers two concurrency models — exactly mirroring how modern operating systems and applications combine them:

| Layer | Model | Why |
|---|---|---|
| HTTP server | asyncio (uvicorn / FastAPI) | Event-loop for many concurrent I/O-bound HTTP clients — analog of `epoll`/`kqueue`-based reactor |
| Agent execution | threading (worker pool) | Long-running blocking work (LLM HTTP call, `docker run` subprocess) — analog of OS process pool |
| Bridge | `queue.Queue` + `loop.run_in_executor` / `call_soon_threadsafe` | Thread-safe handoff between the two — the "system call boundary" of this app |

**Scheduling Note (non-preemptive):** Real agents execute LLM calls and Docker subprocesses that cannot be preempted mid-flight. Therefore the live runtime supports two **non-preemptive** policies (FCFS, Priority) that decide *which agent runs next* when a worker becomes free, but never interrupt a running one. To still demonstrate preemptive Round Robin (a classroom staple), the project ships a separate **simulator** that consumes hypothetical burst times and returns the textbook Gantt chart for all three policies. This split is intentional and explained in the technical report.

**OS Concepts Mapped to Code:**

| OS Concept | Where It Lives |
|---|---|
| Process / PCB | `app/agent.py` — `AgentTask` dataclass |
| Process state | `AgentState` enum: READY → RUNNING → DONE / TIMEOUT / ERROR |
| Ready queue | `app/ready_queue.py` — synchronized queue |
| Scheduler (FCFS, Priority — live, non-preemptive) | `app/scheduler.py` — pluggable policy keys |
| Scheduler simulator (FCFS, Priority, **RR** — preemptive Gantt) | `app/simulator.py` |
| Worker pool (CPU analog) | `app/worker_pool.py` — N daemon threads |
| Synchronization — Mutex | `app/quota_manager.py` — `threading.Lock` |
| Synchronization — Condition (wait/notify) | `app/ready_queue.py` — `threading.Condition` |
| Synchronization — Bounded buffer (producer/consumer) | `app/message_bus.py` — `queue.Queue(maxsize=N)` |
| IPC — agent → agent pipeline (`pipe_to` / `{INPUT}`) | `app/executor.py` + `app/message_bus.py` |
| IPC — parent ↔ sandbox | `app/sandbox.py` — stdin/stdout pipes |
| Process isolation | `app/sandbox.py` — Docker container per execution |
| System call restriction | sandbox: `--cap-drop=ALL`, `--security-opt=no-new-privileges` |
| File permission | sandbox: `--read-only` rootfs, no host mounts |
| Resource limit | sandbox: `--memory`, `--cpus`, `--pids-limit` |
| Timeout / forced termination | sandbox: parent-side `subprocess.TimeoutExpired` → container killed |
| Event notification (SSE) | `app/event_bus.py` — thread→asyncio bridge |
| Trace log | `app/logger.py` |

---

## File Structure

```
GarbageCollector_OS/                 # branch: parkcheolwon
├── docker/
│   ├── Dockerfile                   # main app image
│   └── sandbox.Dockerfile           # restricted python runtime
├── docker-compose.yml               # one-command bring-up
├── app/
│   ├── __init__.py
│   ├── main.py                      # FastAPI app + SSE + startup hook
│   ├── agent.py                     # AgentTask + AgentState + AgentKind
│   ├── scheduler.py                 # SchedulingPolicy + key functions (live)
│   ├── simulator.py                 # FCFS / Priority / RR Gantt simulator
│   ├── ready_queue.py               # synchronized ready queue (Condition)
│   ├── quota_manager.py             # thread-safe quota gate (Lock)
│   ├── message_bus.py               # bounded queue for inter-agent IPC
│   ├── event_bus.py                 # thread→asyncio bridge for SSE
│   ├── llm_client.py                # Solar Pro 3 wrapper
│   ├── sandbox.py                   # docker subprocess runner
│   ├── executor.py                  # orchestrates a single agent
│   ├── worker_pool.py               # N threads pulling from ready queue
│   └── logger.py                    # file logger
├── static/
│   ├── index.html                   # dashboard
│   ├── style.css
│   └── script.js                    # EventSource client + simulator UI
├── tests/
│   ├── __init__.py
│   ├── test_agent.py
│   ├── test_scheduler.py
│   ├── test_simulator.py
│   ├── test_ready_queue.py
│   ├── test_quota_manager.py
│   ├── test_message_bus.py
│   ├── test_event_bus.py
│   ├── test_logger.py
│   ├── test_llm_client.py
│   ├── test_sandbox.py
│   ├── test_executor.py
│   ├── test_worker_pool.py
│   └── test_main.py
├── docs/
│   ├── superpowers/plans/2026-05-20-mini-agent-os.md   # this file
│   ├── technical-report.md
│   ├── development-process.md
│   ├── demo-script.md
│   └── presentation/                # slides
├── logs/
│   └── .gitkeep
├── .env.example                     # UPSTAGE_API_KEY=
├── .gitignore
├── .gitattributes                   # force LF for cross-platform
├── .python-version                  # 3.11 (uv reads this)
├── pyproject.toml                   # uv-managed
├── uv.lock                          # generated by `uv lock`
└── README.md
```

---

## Task 1: Project Scaffold (uv-based)

**Files:**
- Create: `.gitignore`
- Create: `.gitattributes`
- Create: `.env.example`
- Create: `README.md`
- Create: `pyproject.toml`
- Create: `.python-version`
- Create: `app/__init__.py` (empty)
- Create: `tests/__init__.py` (empty)
- Create: `logs/.gitkeep` (empty)

- [ ] **Step 1: Install uv if not present**

Run (one-time):
- macOS / Linux: `curl -LsSf https://astral.sh/uv/install.sh | sh`
- Windows: `powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"`

Verify: `uv --version` → prints version.

- [ ] **Step 2: Write `.gitignore`**

```gitignore
# Python
__pycache__/
*.py[cod]
*.egg-info/
.pytest_cache/
.coverage
htmlcov/

# uv / venv
.venv/
venv/

# Env
.env

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

- [ ] **Step 3: Write `.gitattributes`** (forces LF endings — Windows + macOS parity)

```gitattributes
* text=auto eol=lf
*.png binary
*.jpg binary
*.ico binary
```

- [ ] **Step 4: Write `.python-version`**

```
3.11
```

- [ ] **Step 5: Write `pyproject.toml`**

```toml
[project]
name = "mini-agent-os"
version = "0.1.0"
description = "OS-inspired runtime for scheduling LLM agents (Team GarbageCollector)"
requires-python = ">=3.11,<3.13"
dependencies = [
    "fastapi>=0.115,<0.116",
    "uvicorn[standard]>=0.32,<0.33",
    "pydantic>=2.7,<3",
    "pydantic-settings>=2.5,<3",
    "sse-starlette>=2.1,<3",
    "openai>=1.50,<2",
    "python-dotenv>=1.0,<2",
]

[dependency-groups]
dev = [
    "pytest>=8.3,<9",
    "pytest-asyncio>=0.24,<0.25",
    "httpx>=0.27,<0.28",
]

[tool.pytest.ini_options]
testpaths = ["tests"]
asyncio_mode = "auto"
addopts = "-v"
markers = [
    "docker: requires Docker daemon and sandbox image",
    "slow: timing-sensitive tests that may flake on slow CI",
]
```

- [ ] **Step 6: Write `.env.example`**

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

# Runtime
WORKER_COUNT=4
GLOBAL_QUOTA=1000
MESSAGE_BUS_CAPACITY=100
PIPELINE_WAIT_TIMEOUT=30
```

- [ ] **Step 7: Write `README.md`**

```markdown
# Mini Agent OS

> Team **GarbageCollector** · Week 9 Project, Direction A (OS-for-LLM)

OS-inspired runtime that schedules LLM agents like processes on a worker pool, executes generated code inside Docker sandboxes, and chains agents into pipelines through a bounded message bus.

## Quickstart

```bash
uv sync                          # install deps from uv.lock
cp .env.example .env             # add your UPSTAGE_API_KEY
docker compose build             # build app + sandbox images
docker compose up                # start
open http://localhost:8000
```

## Local development (without Docker for the app)

```bash
uv sync
docker compose build sandbox-build   # sandbox image must exist
uv run uvicorn app.main:app --reload
uv run pytest -m "not docker"        # fast tests
uv run pytest                        # all tests (needs docker)
```

## Documentation
- Implementation plan — `docs/superpowers/plans/2026-05-20-mini-agent-os.md`
- Technical report — `docs/technical-report.md`
- Development process — `docs/development-process.md`
- Demo script — `docs/demo-script.md`
```

- [ ] **Step 8: Create empty package files and lock dependencies**

```bash
mkdir -p app tests static docker logs docs/presentation docs/superpowers/plans
touch app/__init__.py tests/__init__.py logs/.gitkeep
uv lock
uv sync
```

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "chore: scaffold project with uv and pyproject.toml"
```

---

## Task 2: Docker Setup

**Files:**
- Create: `docker/Dockerfile`
- Create: `docker/sandbox.Dockerfile`
- Create: `docker-compose.yml`

- [ ] **Step 1: Write main app `docker/Dockerfile`** (uv-based multi-stage)

```dockerfile
FROM python:3.11-slim AS base

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv

# Install docker CLI so the app can spawn sandbox containers via mounted socket
RUN apt-get update && apt-get install -y --no-install-recommends \
    docker.io \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install deps (cached layer)
COPY pyproject.toml uv.lock ./
RUN uv sync --frozen --no-dev

COPY app ./app
COPY static ./static

ENV PYTHONUNBUFFERED=1 \
    PATH="/app/.venv/bin:${PATH}"

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

# Code is piped in on stdin; nothing baked into the image
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

- [ ] **Step 5: Verify sandbox executes Python**

Run: `echo "print(1+1)" | docker run --rm -i mini-agent-os-sandbox:latest`
Expected: prints `2`.

- [ ] **Step 6: Commit**

```bash
git add docker/ docker-compose.yml
git commit -m "feat: docker setup for app and sandbox runtime"
```

---

## Task 3: Agent Model

**Files:**
- Create: `app/agent.py`
- Create: `tests/test_agent.py`

- [ ] **Step 1: Write the failing test `tests/test_agent.py`**

```python
from datetime import datetime
from app.agent import AgentTask, AgentState, AgentKind


def test_new_agent_is_ready():
    a = AgentTask(agent_id=1, name="x", prompt="p", kind=AgentKind.LLM,
                  priority=5, timeout=3)
    assert a.state == AgentState.READY
    assert a.result is None
    assert a.pipe_to is None
    assert isinstance(a.created_time, datetime)


def test_to_dict_serializes_enums_and_pipe():
    a = AgentTask(agent_id=1, name="x", prompt="p", kind=AgentKind.CODE,
                  priority=5, timeout=3, pipe_to=7)
    d = a.to_dict()
    assert d["state"] == "READY"
    assert d["kind"] == "code"
    assert d["pipe_to"] == 7
    assert d["start_time"] is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_agent.py -v`
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


class AgentKind(str, Enum):
    LLM = "llm"     # plain LLM completion
    CODE = "code"   # LLM generates code → sandbox executes


@dataclass
class AgentTask:
    agent_id: int
    name: str
    prompt: str
    kind: AgentKind
    priority: int
    timeout: int
    pipe_to: Optional[int] = None  # downstream agent id; result is published to pipe-{pipe_to}

    state: AgentState = AgentState.READY
    result: Optional[str] = None
    error_message: Optional[str] = None

    created_time: datetime = field(default_factory=datetime.now)
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None

    def to_dict(self) -> dict:
        fmt = "%Y-%m-%d %H:%M:%S.%f"
        return {
            "agent_id": self.agent_id,
            "name": self.name,
            "prompt": self.prompt,
            "kind": self.kind.value,
            "priority": self.priority,
            "timeout": self.timeout,
            "pipe_to": self.pipe_to,
            "state": self.state.value,
            "result": self.result,
            "error_message": self.error_message,
            "created_time": self.created_time.strftime(fmt),
            "start_time": self.start_time.strftime(fmt) if self.start_time else None,
            "end_time": self.end_time.strftime(fmt) if self.end_time else None,
        }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_agent.py -v`
Expected: 2 passed.

- [ ] **Step 5: Commit**

```bash
git add app/agent.py tests/test_agent.py
git commit -m "feat: AgentTask with state, kind, and pipeline link"
```

---

## Task 4: Scheduler (policy keys)

**Files:**
- Create: `app/scheduler.py`
- Create: `tests/test_scheduler.py`

- [ ] **Step 1: Write the failing test `tests/test_scheduler.py`**

```python
from datetime import datetime, timedelta
from app.agent import AgentTask, AgentKind
from app.scheduler import SchedulingPolicy, sort_key_for


def _agent(agent_id, priority, created_offset_sec):
    a = AgentTask(agent_id=agent_id, name=f"a{agent_id}", prompt="p",
                  kind=AgentKind.LLM, priority=priority, timeout=3)
    a.created_time = datetime(2026, 1, 1) + timedelta(seconds=created_offset_sec)
    return a


def test_fcfs_orders_by_created_time():
    agents = [_agent(1, 1, 2), _agent(2, 9, 0), _agent(3, 5, 1)]
    fn = sort_key_for(SchedulingPolicy.FCFS)
    ordered = sorted(agents, key=fn)
    assert [a.agent_id for a in ordered] == [2, 3, 1]


def test_priority_orders_high_first_then_fcfs_tiebreak():
    agents = [_agent(1, 5, 0), _agent(2, 9, 1), _agent(3, 5, 2)]
    fn = sort_key_for(SchedulingPolicy.PRIORITY)
    ordered = sorted(agents, key=fn)
    assert [a.agent_id for a in ordered] == [2, 1, 3]


def test_unknown_policy_raises():
    import pytest
    with pytest.raises(ValueError):
        sort_key_for("nope")  # type: ignore[arg-type]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_scheduler.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'app.scheduler'`

- [ ] **Step 3: Write `app/scheduler.py`**

```python
from enum import Enum
from typing import Callable
from app.agent import AgentTask


class SchedulingPolicy(str, Enum):
    FCFS = "fcfs"
    PRIORITY = "priority"


def sort_key_for(policy: SchedulingPolicy) -> Callable[[AgentTask], tuple]:
    """Return a sort-key function that orders ready agents per the policy.

    The earliest-ranked agent (sort ascending) is the next to run.

    Note: live scheduling is **non-preemptive** — the policy only decides
    which agent a free worker picks up next. Already-running agents are not
    interrupted (LLM API calls and Docker subprocesses cannot be preempted
    cleanly). For preemptive Round Robin, see `app/simulator.py`.
    """
    if policy == SchedulingPolicy.FCFS:
        return lambda a: (a.created_time,)
    if policy == SchedulingPolicy.PRIORITY:
        return lambda a: (-a.priority, a.created_time)
    raise ValueError(f"unknown policy: {policy!r}")
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_scheduler.py -v`
Expected: 3 passed.

- [ ] **Step 5: Commit**

```bash
git add app/scheduler.py tests/test_scheduler.py
git commit -m "feat: live scheduling policy keys (FCFS, Priority)"
```

---

## Task 5: ReadyQueue (Condition-synchronized)

**Files:**
- Create: `app/ready_queue.py`
- Create: `tests/test_ready_queue.py`

- [ ] **Step 1: Write the failing test `tests/test_ready_queue.py`**

```python
import threading
import time
from datetime import datetime, timedelta
from app.agent import AgentTask, AgentKind
from app.scheduler import SchedulingPolicy
from app.ready_queue import ReadyQueue


def _agent(agent_id, priority, offset_sec):
    a = AgentTask(agent_id=agent_id, name=f"a{agent_id}", prompt="p",
                  kind=AgentKind.LLM, priority=priority, timeout=3)
    a.created_time = datetime(2026, 1, 1) + timedelta(seconds=offset_sec)
    return a


def test_put_and_pop_under_fcfs():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    q.put(_agent(1, 1, 2))
    q.put(_agent(2, 9, 0))
    q.put(_agent(3, 5, 1))
    assert q.pop_next().agent_id == 2
    assert q.pop_next().agent_id == 3
    assert q.pop_next().agent_id == 1


def test_pop_blocks_until_put():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    got = []

    def consumer():
        got.append(q.pop_next().agent_id)

    t = threading.Thread(target=consumer)
    t.start()
    time.sleep(0.1)  # consumer is blocked on Condition.wait
    assert got == []
    q.put(_agent(42, 1, 0))
    t.join(timeout=1)
    assert got == [42]


def test_policy_can_be_switched_at_runtime():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    q.put(_agent(1, 1, 0))
    q.put(_agent(2, 9, 1))
    q.set_policy(SchedulingPolicy.PRIORITY)
    assert q.pop_next().agent_id == 2   # priority wins now
    assert q.pop_next().agent_id == 1


def test_close_unblocks_waiters_with_none():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    results = []

    def consumer():
        results.append(q.pop_next())

    t = threading.Thread(target=consumer)
    t.start()
    time.sleep(0.1)
    q.close()
    t.join(timeout=1)
    assert results == [None]   # sentinel returned when closed
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_ready_queue.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/ready_queue.py`**

```python
import threading
from typing import List, Optional
from app.agent import AgentTask
from app.scheduler import SchedulingPolicy, sort_key_for


class ReadyQueue:
    """Thread-safe ready queue with a switchable scheduling policy.

    Producers (API handlers) call `put`. Consumers (worker threads) call
    `pop_next`, which blocks on a Condition until something is available or
    the queue is closed (returns None as a shutdown sentinel).
    """

    def __init__(self, policy: SchedulingPolicy):
        self._items: List[AgentTask] = []
        self._policy = policy
        self._cond = threading.Condition()
        self._closed = False

    def set_policy(self, policy: SchedulingPolicy) -> None:
        with self._cond:
            self._policy = policy

    def get_policy(self) -> SchedulingPolicy:
        with self._cond:
            return self._policy

    def put(self, task: AgentTask) -> None:
        with self._cond:
            self._items.append(task)
            self._cond.notify()

    def pop_next(self) -> Optional[AgentTask]:
        """Block until an agent is available or the queue is closed.

        Returns the next agent per the current policy, or None if closed.
        """
        with self._cond:
            while not self._items and not self._closed:
                self._cond.wait()
            if self._closed and not self._items:
                return None
            key = sort_key_for(self._policy)
            self._items.sort(key=key)
            return self._items.pop(0)

    def size(self) -> int:
        with self._cond:
            return len(self._items)

    def close(self) -> None:
        with self._cond:
            self._closed = True
            self._cond.notify_all()
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_ready_queue.py -v`
Expected: 4 passed.

- [ ] **Step 5: Commit**

```bash
git add app/ready_queue.py tests/test_ready_queue.py
git commit -m "feat: ReadyQueue with Condition-based wait/notify"
```

---

## Task 6: QuotaManager (Mutex)

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
    lock = threading.Lock()

    def worker():
        if qm.try_acquire(1):
            with lock:
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

Run: `uv run pytest tests/test_quota_manager.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/quota_manager.py`**

```python
import threading


class QuotaManager:
    """Thread-safe shared API call quota — demonstrates mutex synchronization."""

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

Run: `uv run pytest tests/test_quota_manager.py -v`
Expected: 3 passed.

- [ ] **Step 5: Commit**

```bash
git add app/quota_manager.py tests/test_quota_manager.py
git commit -m "feat: thread-safe QuotaManager with mutex"
```

---

## Task 7: MessageBus (Bounded buffer / producer-consumer)

**Files:**
- Create: `app/message_bus.py`
- Create: `tests/test_message_bus.py`

- [ ] **Step 1: Write the failing test `tests/test_message_bus.py`**

```python
import queue
import threading
import time
import pytest
from app.message_bus import MessageBus


def test_put_get_round_trip():
    bus = MessageBus(capacity=5)
    bus.send("topic", "hello")
    assert bus.receive("topic", timeout=1) == "hello"


def test_get_raises_empty_on_timeout():
    bus = MessageBus(capacity=5)
    with pytest.raises(queue.Empty):
        bus.receive("nobody", timeout=0.1)


def test_bounded_capacity_blocks_producer():
    bus = MessageBus(capacity=2)
    bus.send("t", 1)
    bus.send("t", 2)
    # third send must block until something is consumed
    blocked = threading.Event()

    def producer():
        bus.send("t", 3)
        blocked.set()

    t = threading.Thread(target=producer)
    t.start()
    time.sleep(0.1)
    assert not blocked.is_set()  # producer is blocked

    assert bus.receive("t", timeout=1) == 1
    t.join(timeout=1)
    assert blocked.is_set()


def test_topics_are_independent():
    bus = MessageBus(capacity=5)
    bus.send("a", 1)
    bus.send("b", 2)
    assert bus.receive("b", timeout=1) == 2
    assert bus.receive("a", timeout=1) == 1
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_message_bus.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/message_bus.py`**

```python
import queue
import threading
from typing import Any, Dict


class MessageBus:
    """Per-topic bounded queues — classic producer/consumer pattern.

    `send` blocks when the topic queue is full; `receive` blocks (with timeout)
    when the topic queue is empty. Demonstrates the bounded-buffer synchronization
    problem covered in OS textbooks.

    Used by the Executor to wire agents into pipelines: an upstream agent
    publishes its result to topic `pipe-{downstream_id}`; the downstream
    agent's executor blocks on `receive("pipe-{my_id}")` when its prompt
    contains the `{INPUT}` placeholder.
    """

    def __init__(self, capacity: int):
        self._capacity = capacity
        self._queues: Dict[str, queue.Queue] = {}
        self._lock = threading.Lock()

    def _q(self, topic: str) -> queue.Queue:
        with self._lock:
            q = self._queues.get(topic)
            if q is None:
                q = queue.Queue(maxsize=self._capacity)
                self._queues[topic] = q
            return q

    def send(self, topic: str, payload: Any, timeout: float | None = None) -> None:
        self._q(topic).put(payload, timeout=timeout)

    def receive(self, topic: str, timeout: float) -> Any:
        return self._q(topic).get(timeout=timeout)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_message_bus.py -v`
Expected: 4 passed.

- [ ] **Step 5: Commit**

```bash
git add app/message_bus.py tests/test_message_bus.py
git commit -m "feat: bounded MessageBus for inter-agent pipelines"
```

---

## Task 8: EventBus (thread → asyncio bridge for SSE)

**Files:**
- Create: `app/event_bus.py`
- Create: `tests/test_event_bus.py`

- [ ] **Step 1: Write the failing test `tests/test_event_bus.py`**

```python
import asyncio
import threading
import pytest
from app.event_bus import EventBus


@pytest.mark.asyncio
async def test_published_event_reaches_async_subscriber():
    bus = EventBus()
    bus.bind_loop(asyncio.get_running_loop())

    async def collect_one():
        async for event in bus.subscribe():
            return event

    task = asyncio.create_task(collect_one())
    await asyncio.sleep(0.05)  # let the subscriber attach
    # publish from a worker thread
    threading.Thread(target=lambda: bus.publish({"agent_id": 7, "state": "DONE"})).start()
    result = await asyncio.wait_for(task, timeout=1)
    assert result == {"agent_id": 7, "state": "DONE"}


@pytest.mark.asyncio
async def test_multiple_subscribers_each_receive_event():
    bus = EventBus()
    bus.bind_loop(asyncio.get_running_loop())

    async def collect():
        async for event in bus.subscribe():
            return event

    t1 = asyncio.create_task(collect())
    t2 = asyncio.create_task(collect())
    await asyncio.sleep(0.05)
    bus.publish("ping")
    a, b = await asyncio.gather(asyncio.wait_for(t1, timeout=1),
                                asyncio.wait_for(t2, timeout=1))
    assert a == "ping" and b == "ping"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_event_bus.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/event_bus.py`**

```python
import asyncio
import threading
from typing import Any, AsyncIterator, List, Optional


class EventBus:
    """Fan-out bus that bridges thread-side publishers to async subscribers.

    `publish` is callable from any worker thread (no event loop required).
    `subscribe` returns an async iterator usable by an SSE endpoint.

    Note: events published while there are zero subscribers are dropped
    (there is no replay buffer). Open the dashboard before starting a demo.
    """

    def __init__(self):
        self._subscribers: List[asyncio.Queue] = []
        self._lock = threading.Lock()
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    def bind_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        """Called once from the FastAPI lifespan so publishers can schedule
        queue puts on the correct event loop."""
        self._loop = loop

    def _ensure_loop(self) -> asyncio.AbstractEventLoop:
        if self._loop is None:
            raise RuntimeError("EventBus.bind_loop() not called")
        return self._loop

    def publish(self, event: Any) -> None:
        loop = self._ensure_loop()
        with self._lock:
            subscribers = list(self._subscribers)
        for q in subscribers:
            loop.call_soon_threadsafe(q.put_nowait, event)

    async def subscribe(self) -> AsyncIterator[Any]:
        q: asyncio.Queue = asyncio.Queue()
        with self._lock:
            self._subscribers.append(q)
        try:
            while True:
                event = await q.get()
                yield event
        finally:
            with self._lock:
                if q in self._subscribers:
                    self._subscribers.remove(q)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_event_bus.py -v`
Expected: 2 passed.

- [ ] **Step 5: Commit**

```bash
git add app/event_bus.py tests/test_event_bus.py
git commit -m "feat: EventBus bridge from worker threads to async SSE clients"
```

---

## Task 9: Logger

**Files:**
- Create: `app/logger.py`
- Create: `tests/test_logger.py`

- [ ] **Step 1: Write the failing test `tests/test_logger.py`**

```python
from pathlib import Path
from app.logger import Logger


def test_log_event_appends_line(tmp_path: Path):
    lg = Logger(tmp_path / "exec.log")
    lg.log("agent 1 started")
    lg.log("agent 1 done")
    lines = lg.read_all()
    assert len(lines) == 2
    assert "agent 1 started" in lines[0]
    assert "agent 1 done" in lines[1]


def test_clear_empties_log(tmp_path: Path):
    lg = Logger(tmp_path / "exec.log")
    lg.log("x")
    lg.clear()
    assert lg.read_all() == []
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_logger.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/logger.py`**

```python
import threading
from datetime import datetime
from pathlib import Path
from typing import List


class Logger:
    """File-backed event log, safe to call from multiple worker threads."""

    def __init__(self, path: Path):
        self._path = Path(path)
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._path.touch(exist_ok=True)
        self._lock = threading.Lock()

    def log(self, message: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        line = f"[{ts}] {message}"
        with self._lock:
            with self._path.open("a", encoding="utf-8") as f:
                f.write(line + "\n")
        print(line, flush=True)

    def read_all(self) -> List[str]:
        with self._lock:
            return self._path.read_text(encoding="utf-8").splitlines()

    def clear(self) -> None:
        with self._lock:
            self._path.write_text("", encoding="utf-8")
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_logger.py -v`
Expected: 2 passed.

- [ ] **Step 5: Commit**

```bash
git add app/logger.py tests/test_logger.py
git commit -m "feat: thread-safe file Logger"
```

---

## Task 10: LLM Client (Solar Pro 3)

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

Run: `uv run pytest tests/test_llm_client.py -v`
Expected: FAIL with `ModuleNotFoundError`

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

Run: `uv run pytest tests/test_llm_client.py -v`
Expected: 1 passed.

- [ ] **Step 5: Commit**

```bash
git add app/llm_client.py tests/test_llm_client.py
git commit -m "feat: Solar Pro 3 LLM client wrapper"
```

---

## Task 11: Sandbox (Docker subprocess)

**Files:**
- Create: `app/sandbox.py`
- Create: `tests/test_sandbox.py`

> **Note:** Tests in this task require the Docker daemon running locally and the `mini-agent-os-sandbox:latest` image to be built (`docker compose build sandbox-build`). Skip with `uv run pytest -m "not docker"` when running fast unit suites.

- [ ] **Step 1: Write the failing test `tests/test_sandbox.py`**

```python
import pytest
from app.sandbox import Sandbox, SandboxResult


pytestmark = pytest.mark.docker


def test_runs_simple_python_and_captures_stdout():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=3)
    r: SandboxResult = sb.run("print(2 + 2)")
    assert r.exit_code == 0
    assert r.stdout.strip() == "4"
    assert r.timed_out is False


def test_timeout_kills_long_running_code():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=2)
    r = sb.run("import time; time.sleep(10); print('done')")
    assert r.timed_out is True
    assert "done" not in r.stdout


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
    r = sb.run(code)
    assert "NET_BLOCKED" in r.stdout
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_sandbox.py -v`
Expected: FAIL with `ModuleNotFoundError`

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
                stdout=e.stdout.decode() if e.stdout else "",
                stderr=e.stderr.decode() if e.stderr else "",
                timed_out=True,
            )
```

- [ ] **Step 4: Build sandbox image (one-time)**

Run: `docker compose build sandbox-build`
Expected: image `mini-agent-os-sandbox:latest` exists (verify with `docker images`).

- [ ] **Step 5: Run test to verify it passes**

Run: `uv run pytest tests/test_sandbox.py -v`
Expected: 3 passed (slow — each test spawns a real container).

- [ ] **Step 6: Commit**

```bash
git add app/sandbox.py tests/test_sandbox.py
git commit -m "feat: Docker sandbox with isolation and resource limits"
```

---

## Task 12: Executor (single-agent orchestration with pipeline IPC)

**Files:**
- Create: `app/executor.py`
- Create: `tests/test_executor.py`

- [ ] **Step 1: Write the failing test `tests/test_executor.py`**

```python
import queue
import threading
from unittest.mock import MagicMock
from app.agent import AgentTask, AgentState, AgentKind
from app.executor import Executor, INPUT_PLACEHOLDER
from app.message_bus import MessageBus
from app.quota_manager import QuotaManager
from app.sandbox import SandboxResult


def _task(kind: AgentKind, prompt="hi", agent_id=1, pipe_to=None):
    return AgentTask(agent_id=agent_id, name="t", prompt=prompt, kind=kind,
                     priority=1, timeout=3, pipe_to=pipe_to)


def _make_executor(llm=None, sandbox=None, quota=None, bus=None):
    return Executor(
        llm=llm or MagicMock(),
        sandbox=sandbox or MagicMock(),
        quota=quota or QuotaManager(total=10),
        logger=MagicMock(),
        events=MagicMock(),
        bus=bus or MessageBus(capacity=10),
        pipeline_timeout=2.0,
    )


def test_llm_agent_marks_done_and_stores_result():
    llm = MagicMock()
    llm.complete.return_value = "answer"
    ex = _make_executor(llm=llm)

    task = _task(AgentKind.LLM)
    ex.execute(task)

    assert task.state == AgentState.DONE
    assert task.result == "answer"


def test_quota_exhaustion_marks_error_without_calling_llm():
    llm = MagicMock()
    ex = _make_executor(llm=llm, quota=QuotaManager(total=0))

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
    ex = _make_executor(llm=llm, sandbox=sandbox)

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
    ex = _make_executor(llm=llm, sandbox=sandbox)

    task = _task(AgentKind.CODE)
    ex.execute(task)

    assert task.state == AgentState.TIMEOUT


def test_pipeline_publishes_result_to_downstream_topic():
    llm = MagicMock()
    llm.complete.return_value = "upstream result"
    bus = MessageBus(capacity=5)
    ex = _make_executor(llm=llm, bus=bus)

    task = _task(AgentKind.LLM, agent_id=1, pipe_to=2)
    ex.execute(task)

    assert task.state == AgentState.DONE
    # downstream agent (id=2) can now read it
    assert bus.receive("pipe-2", timeout=1) == "upstream result"


def test_pipeline_consumer_blocks_for_input_then_substitutes_placeholder():
    llm = MagicMock()
    llm.complete.return_value = "summary done"
    bus = MessageBus(capacity=5)
    ex = _make_executor(llm=llm, bus=bus)

    # consumer waits for pipe-2; producer sends 0.2s later
    task = _task(AgentKind.LLM, agent_id=2,
                 prompt=f"Summarize this: {INPUT_PLACEHOLDER}")

    def producer():
        import time
        time.sleep(0.2)
        bus.send("pipe-2", "raw text from upstream")

    threading.Thread(target=producer, daemon=True).start()
    ex.execute(task)

    assert task.state == AgentState.DONE
    # the LLM should have been called with the substituted prompt
    actual_prompt = llm.complete.call_args.args[0]
    assert "raw text from upstream" in actual_prompt
    assert INPUT_PLACEHOLDER not in actual_prompt


def test_pipeline_consumer_times_out_when_no_producer():
    llm = MagicMock()
    bus = MessageBus(capacity=5)
    ex = _make_executor(llm=llm, bus=bus)

    task = _task(AgentKind.LLM, agent_id=2,
                 prompt=f"Use {INPUT_PLACEHOLDER}")
    ex.execute(task)

    assert task.state == AgentState.ERROR
    assert "pipeline input" in task.error_message.lower()
    llm.complete.assert_not_called()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_executor.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/executor.py`**

```python
import queue
from datetime import datetime
from app.agent import AgentTask, AgentState, AgentKind


INPUT_PLACEHOLDER = "{INPUT}"


class Executor:
    """Executes ONE agent end-to-end:
        (pipeline input?) → quota gate → LLM → optional sandbox → (pipeline output?)

    Designed to be called by worker threads. All collaborators are passed in
    so the unit is easy to test with mocks.
    """

    def __init__(self, llm, sandbox, quota, logger, events, bus,
                 pipeline_timeout: float = 30.0):
        self._llm = llm
        self._sandbox = sandbox
        self._quota = quota
        self._log = logger
        self._events = events
        self._bus = bus
        self._pipeline_timeout = pipeline_timeout

    def execute(self, task: AgentTask) -> AgentTask:
        task.state = AgentState.RUNNING
        task.start_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) started")
        self._publish(task)

        # 1. Pipeline input: if prompt contains {INPUT}, block until upstream sends
        prompt = task.prompt
        if INPUT_PLACEHOLDER in prompt:
            try:
                upstream = self._bus.receive(
                    f"pipe-{task.agent_id}", timeout=self._pipeline_timeout
                )
            except queue.Empty:
                return self._fail(task, AgentState.ERROR,
                                  f"Pipeline input not received within "
                                  f"{self._pipeline_timeout}s")
            prompt = prompt.replace(INPUT_PLACEHOLDER, str(upstream))

        # 2. Quota gate (mutex)
        if not self._quota.try_acquire(1):
            return self._fail(task, AgentState.ERROR, "API quota exhausted")

        # 3. LLM call
        try:
            llm_output = self._llm.complete(prompt)
        except Exception as e:
            return self._fail(task, AgentState.ERROR, f"LLM error: {e}")

        # 4. Optional sandbox for CODE agents
        if task.kind == AgentKind.LLM:
            final_result = llm_output
        else:
            sb = self._sandbox.run(llm_output)
            if sb.timed_out:
                return self._fail(task, AgentState.TIMEOUT,
                                  "Sandbox execution timed out")
            if sb.exit_code != 0:
                return self._fail(task, AgentState.ERROR,
                                  f"Sandbox exit {sb.exit_code}: {sb.stderr.strip()}")
            final_result = f"code:\n{llm_output}\n---\nstdout:\n{sb.stdout}"

        task.result = final_result

        # 5. Pipeline output: publish to downstream if pipe_to is set
        if task.pipe_to is not None:
            try:
                self._bus.send(f"pipe-{task.pipe_to}", final_result, timeout=5)
            except queue.Full:
                self._log.log(
                    f"Agent {task.agent_id} pipeline output dropped: "
                    f"pipe-{task.pipe_to} full"
                )

        return self._done(task)

    def _done(self, task: AgentTask) -> AgentTask:
        task.state = AgentState.DONE
        task.end_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) completed")
        self._publish(task)
        return task

    def _fail(self, task: AgentTask, state: AgentState, message: str) -> AgentTask:
        task.state = state
        task.error_message = message
        task.end_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) {state.value}: {message}")
        self._publish(task)
        return task

    def _publish(self, task: AgentTask) -> None:
        self._events.publish(task.to_dict())
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_executor.py -v`
Expected: 7 passed.

- [ ] **Step 5: Commit**

```bash
git add app/executor.py tests/test_executor.py
git commit -m "feat: Executor with pipeline IPC (MessageBus consumer/producer)"
```

---

## Task 13: WorkerPool (concurrent execution)

**Files:**
- Create: `app/worker_pool.py`
- Create: `tests/test_worker_pool.py`

- [ ] **Step 1: Write the failing test `tests/test_worker_pool.py`**

```python
import threading
import time
import pytest
from unittest.mock import MagicMock
from app.agent import AgentTask, AgentState, AgentKind
from app.ready_queue import ReadyQueue
from app.scheduler import SchedulingPolicy
from app.worker_pool import WorkerPool


def _agent(agent_id):
    return AgentTask(agent_id=agent_id, name=f"a{agent_id}", prompt="p",
                     kind=AgentKind.LLM, priority=1, timeout=3)


def test_workers_drain_queue_and_call_executor():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    done = threading.Event()
    seen_ids = []
    lock = threading.Lock()

    class FakeExecutor:
        def execute(self, task):
            with lock:
                seen_ids.append(task.agent_id)
                task.state = AgentState.DONE
                if len(seen_ids) == 3:
                    done.set()

    pool = WorkerPool(ready_queue=q, executor=FakeExecutor(), workers=2)
    pool.start()
    for i in [1, 2, 3]:
        q.put(_agent(i))
    assert done.wait(timeout=3)
    pool.stop()
    assert sorted(seen_ids) == [1, 2, 3]


def test_stop_shuts_workers_down():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    pool = WorkerPool(ready_queue=q, executor=MagicMock(), workers=2)
    pool.start()
    pool.stop()
    assert all(not t.is_alive() for t in pool.threads())


@pytest.mark.slow
def test_concurrent_workers_actually_run_in_parallel():
    """Two workers should process two slow tasks in less than 2× the serial time."""
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    barrier = threading.Barrier(parties=2)
    done_count = {"n": 0}
    done_lock = threading.Lock()

    class SlowExecutor:
        def execute(self, task):
            barrier.wait(timeout=2)
            time.sleep(0.3)
            task.state = AgentState.DONE
            with done_lock:
                done_count["n"] += 1

    pool = WorkerPool(ready_queue=q, executor=SlowExecutor(), workers=2)
    pool.start()
    start = time.monotonic()
    q.put(_agent(1))
    q.put(_agent(2))
    while done_count["n"] < 2 and time.monotonic() - start < 2:
        time.sleep(0.02)
    elapsed = time.monotonic() - start
    pool.stop()
    assert done_count["n"] == 2
    # Serial would take ~0.6s; parallel should be well under 0.55s.
    assert elapsed < 0.55
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_worker_pool.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/worker_pool.py`**

```python
import threading
from typing import List, Protocol
from app.agent import AgentTask
from app.ready_queue import ReadyQueue


class ExecutorLike(Protocol):
    def execute(self, task: AgentTask) -> AgentTask: ...


class WorkerPool:
    """Fixed-size pool of daemon threads pulling agents from a ReadyQueue.

    Each worker loops: pop_next() (blocks on Condition) → executor.execute(task).
    A None from pop_next() means the queue was closed; the worker exits.
    """

    def __init__(self, ready_queue: ReadyQueue, executor: ExecutorLike,
                 workers: int):
        self._queue = ready_queue
        self._executor = executor
        self._n = workers
        self._threads: List[threading.Thread] = []

    def start(self) -> None:
        for i in range(self._n):
            t = threading.Thread(
                target=self._worker_loop,
                name=f"agent-worker-{i}",
                daemon=True,
            )
            t.start()
            self._threads.append(t)

    def stop(self, join_timeout: float = 2.0) -> None:
        self._queue.close()
        for t in self._threads:
            t.join(timeout=join_timeout)

    def threads(self) -> List[threading.Thread]:
        return list(self._threads)

    def _worker_loop(self) -> None:
        while True:
            task = self._queue.pop_next()
            if task is None:
                return
            try:
                self._executor.execute(task)
            except Exception:
                # Executor handles its own errors; if anything leaks, drop it
                # so the worker stays alive.
                pass
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_worker_pool.py -v`
Expected: 3 passed.

- [ ] **Step 5: Commit**

```bash
git add app/worker_pool.py tests/test_worker_pool.py
git commit -m "feat: WorkerPool with concurrent agent execution"
```

---

## Task 14: Simulator (FCFS / Priority / Round Robin Gantt)

**Files:**
- Create: `app/simulator.py`
- Create: `tests/test_simulator.py`

> **Rationale:** Real LLM-backed agents cannot be preempted, so the live scheduler is non-preemptive (Task 4). To still demonstrate Round Robin and let viewers compare textbook scheduling behavior side-by-side, the simulator takes a list of hypothetical jobs `[{id, burst, arrival, priority}]` and returns a Gantt timeline. It is a pure-function module — no threads, no LLM, no Docker — and powers a dedicated dashboard panel.

- [ ] **Step 1: Write the failing test `tests/test_simulator.py`**

```python
import pytest
from app.simulator import simulate_fcfs, simulate_priority, simulate_rr


def _job(jid, burst, arrival=0, priority=1):
    return {"id": jid, "burst": burst, "arrival": arrival, "priority": priority}


# ---------- FCFS ----------

def test_fcfs_runs_in_arrival_order():
    jobs = [_job("A", 4, arrival=0), _job("B", 2, arrival=1), _job("C", 3, arrival=2)]
    gantt = simulate_fcfs(jobs)
    assert gantt == [
        {"id": "A", "start": 0, "end": 4},
        {"id": "B", "start": 4, "end": 6},
        {"id": "C", "start": 6, "end": 9},
    ]


def test_fcfs_idle_gap_when_no_job_has_arrived():
    jobs = [_job("A", 2, arrival=5)]
    gantt = simulate_fcfs(jobs)
    assert gantt == [{"id": "A", "start": 5, "end": 7}]


# ---------- Priority (non-preemptive, higher value = higher priority) ----------

def test_priority_picks_highest_priority_when_multiple_available():
    jobs = [_job("A", 3, arrival=0, priority=1),
            _job("B", 2, arrival=0, priority=5),
            _job("C", 4, arrival=0, priority=3)]
    gantt = simulate_priority(jobs)
    assert [g["id"] for g in gantt] == ["B", "C", "A"]


def test_priority_falls_back_to_fcfs_when_only_one_available():
    # At t=0, only A is available; B (priority 9) arrives at t=5 but A already running
    jobs = [_job("A", 4, arrival=0, priority=1),
            _job("B", 2, arrival=5, priority=9)]
    gantt = simulate_priority(jobs)
    assert gantt == [
        {"id": "A", "start": 0, "end": 4},
        # idle gap [4,5] then B
        {"id": "B", "start": 5, "end": 7},
    ]


# ---------- Round Robin (preemptive, time quantum) ----------

def test_rr_slices_jobs_by_quantum():
    jobs = [_job("A", 5, arrival=0), _job("B", 3, arrival=0)]
    gantt = simulate_rr(jobs, quantum=2)
    # A(0-2), B(2-4), A(4-6), B(6-7), A(7-8)
    assert gantt == [
        {"id": "A", "start": 0, "end": 2},
        {"id": "B", "start": 2, "end": 4},
        {"id": "A", "start": 4, "end": 6},
        {"id": "B", "start": 6, "end": 7},
        {"id": "A", "start": 7, "end": 8},
    ]


def test_rr_handles_late_arrivals():
    jobs = [_job("A", 4, arrival=0), _job("B", 2, arrival=3)]
    gantt = simulate_rr(jobs, quantum=2)
    # A(0-2), A(2-4)  ← B arrives at 3 and is queued before A re-enters
    # then B(4-6), A(6-8)? Let's trace:
    #   t=0: ready=[A]; pop A; run 2 → A(0-2); A has 2 left.
    #     during this slice, B arrives at t=3? no, slice ends at 2.
    #     re-queue A. ready=[A].
    #   t=2: ready=[A]; pop A; run 2 → A(2-4); A done.
    #     B arrived at t=3 → enter ready during this slice. ready=[B] after pop.
    #   t=4: ready=[B]; pop B; run 2 → B(4-6); B done.
    assert gantt == [
        {"id": "A", "start": 0, "end": 2},
        {"id": "A", "start": 2, "end": 4},
        {"id": "B", "start": 4, "end": 6},
    ]


def test_rr_idle_gap_then_resumes():
    jobs = [_job("A", 2, arrival=5)]
    gantt = simulate_rr(jobs, quantum=3)
    assert gantt == [{"id": "A", "start": 5, "end": 7}]


def test_invalid_quantum_raises():
    with pytest.raises(ValueError):
        simulate_rr([_job("A", 1)], quantum=0)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_simulator.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/simulator.py`**

```python
"""Pure-function scheduler simulator.

Inputs are plain dicts so this module is trivially testable and exposable as
an HTTP endpoint without coupling to AgentTask.

Job shape: {"id": str, "burst": int, "arrival": int, "priority": int}
Output:    list of {"id": str, "start": int, "end": int}
"""
from collections import deque
from typing import List, Dict


Job = Dict[str, int]
Slice = Dict[str, int]


def simulate_fcfs(jobs: List[Job]) -> List[Slice]:
    """First-Come, First-Served: run in order of arrival to completion."""
    ordered = sorted(jobs, key=lambda j: (j["arrival"], j["id"]))
    timeline: List[Slice] = []
    clock = 0
    for j in ordered:
        start = max(clock, j["arrival"])
        end = start + j["burst"]
        timeline.append({"id": j["id"], "start": start, "end": end})
        clock = end
    return timeline


def simulate_priority(jobs: List[Job]) -> List[Slice]:
    """Non-preemptive priority: among arrived jobs, pick the highest priority value.

    Tiebreak: earlier arrival, then lexicographic id.
    """
    remaining = [dict(j) for j in jobs]
    timeline: List[Slice] = []
    clock = 0
    while remaining:
        available = [j for j in remaining if j["arrival"] <= clock]
        if not available:
            clock = min(j["arrival"] for j in remaining)
            continue
        chosen = max(available, key=lambda j: (j["priority"], -j["arrival"], -ord(j["id"][0])))
        start = clock
        end = start + chosen["burst"]
        timeline.append({"id": chosen["id"], "start": start, "end": end})
        clock = end
        remaining.remove(chosen)
    return timeline


def simulate_rr(jobs: List[Job], quantum: int) -> List[Slice]:
    """Round Robin with the given time quantum (preemptive).

    Consecutive same-id slices are NOT merged; each one represents a separate
    quantum so the Gantt chart faithfully shows preemption points.
    """
    if quantum <= 0:
        raise ValueError("quantum must be a positive integer")

    pending = sorted(jobs, key=lambda j: (j["arrival"], j["id"]))
    remaining_burst = {j["id"]: j["burst"] for j in jobs}
    ready: deque = deque()
    timeline: List[Slice] = []
    clock = 0

    def admit_arrivals(up_to_time: int) -> None:
        nonlocal pending
        new_pending = []
        for j in pending:
            if j["arrival"] <= up_to_time:
                ready.append(j["id"])
            else:
                new_pending.append(j)
        pending = new_pending

    admit_arrivals(clock)
    if not ready and pending:
        clock = pending[0]["arrival"]
        admit_arrivals(clock)

    while ready or pending:
        if not ready:
            clock = pending[0]["arrival"]
            admit_arrivals(clock)
            continue

        jid = ready.popleft()
        run = min(quantum, remaining_burst[jid])
        timeline.append({"id": jid, "start": clock, "end": clock + run})
        clock += run
        remaining_burst[jid] -= run

        # Admit any jobs that arrived during this quantum BEFORE re-queuing self,
        # which matches standard RR convention.
        admit_arrivals(clock)

        if remaining_burst[jid] > 0:
            ready.append(jid)

    return timeline
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_simulator.py -v`
Expected: 7 passed.

- [ ] **Step 5: Commit**

```bash
git add app/simulator.py tests/test_simulator.py
git commit -m "feat: scheduler simulator (FCFS, Priority, Round Robin Gantt)"
```

---

## Task 15: FastAPI App (endpoints + SSE + simulator)

**Files:**
- Create: `app/main.py`
- Create: `tests/test_main.py`

- [ ] **Step 1: Write the failing test `tests/test_main.py`**

```python
import time
from unittest.mock import patch, MagicMock
from fastapi.testclient import TestClient


def _make_client():
    """Patch dependency constructors so tests don't hit network or Docker."""
    with patch("app.main._build_llm") as mk_llm, \
         patch("app.main._build_sandbox") as mk_sb:
        mk_llm.return_value = MagicMock()
        mk_llm.return_value.complete.return_value = "ok"
        mk_sb.return_value = MagicMock()
        from app import main
        main._reset_for_tests()
        return TestClient(main.app)


def test_create_and_list_agents():
    client = _make_client()
    r = client.post("/agents", json={
        "name": "summary", "prompt": "do x",
        "priority": 5, "timeout": 3, "kind": "llm",
    })
    assert r.status_code == 200
    body = r.json()
    assert body["agent"]["state"] in ("READY", "RUNNING", "DONE")
    time.sleep(0.2)
    listing = client.get("/agents").json()
    assert listing["count"] == 1


def test_policy_can_be_changed():
    client = _make_client()
    r = client.post("/policy", json={"policy": "priority"})
    assert r.status_code == 200
    assert r.json()["policy"] == "priority"
    r = client.get("/policy")
    assert r.json()["policy"] == "priority"


def test_clear_resets_agents_and_logs():
    client = _make_client()
    client.post("/agents", json={
        "name": "x", "prompt": "x", "priority": 1, "timeout": 3, "kind": "llm",
    })
    time.sleep(0.2)
    r = client.delete("/agents")
    assert r.status_code == 200
    assert client.get("/agents").json()["count"] == 0


def test_simulate_returns_gantt_for_each_policy():
    client = _make_client()
    body = {
        "policy": "rr",
        "quantum": 2,
        "jobs": [
            {"id": "A", "burst": 5, "arrival": 0, "priority": 1},
            {"id": "B", "burst": 3, "arrival": 0, "priority": 1},
        ],
    }
    r = client.post("/simulate", json=body)
    assert r.status_code == 200
    gantt = r.json()["gantt"]
    assert gantt[0] == {"id": "A", "start": 0, "end": 2}
    assert gantt[-1]["end"] == 8  # total = 5 + 3


def test_simulate_fcfs_does_not_require_quantum():
    client = _make_client()
    body = {
        "policy": "fcfs",
        "jobs": [
            {"id": "A", "burst": 3, "arrival": 0, "priority": 1},
            {"id": "B", "burst": 2, "arrival": 0, "priority": 1},
        ],
    }
    r = client.post("/simulate", json=body)
    assert r.status_code == 200
    assert r.json()["gantt"] == [
        {"id": "A", "start": 0, "end": 3},
        {"id": "B", "start": 3, "end": 5},
    ]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `uv run pytest tests/test_main.py -v`
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write `app/main.py`**

```python
import asyncio
import json
import os
from contextlib import asynccontextmanager
from enum import Enum
from pathlib import Path
from typing import List, Optional

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from sse_starlette.sse import EventSourceResponse
from dotenv import load_dotenv

from app.agent import AgentTask, AgentKind
from app.event_bus import EventBus
from app.executor import Executor
from app.llm_client import LLMClient
from app.logger import Logger
from app.message_bus import MessageBus
from app.quota_manager import QuotaManager
from app.ready_queue import ReadyQueue
from app.sandbox import Sandbox
from app.scheduler import SchedulingPolicy
from app.simulator import simulate_fcfs, simulate_priority, simulate_rr
from app.worker_pool import WorkerPool


load_dotenv()


def _build_llm() -> Optional[LLMClient]:
    key = os.environ.get("UPSTAGE_API_KEY")
    if not key or key == "replace_me":
        return None
    return LLMClient(
        api_key=key,
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


# ---------- module-level runtime state ----------

_agents: List[AgentTask] = []
_next_id = 1
_logger = Logger(Path("logs/execution_log.txt"))
_quota = QuotaManager(total=int(os.environ.get("GLOBAL_QUOTA", "1000")))
_message_bus = MessageBus(capacity=int(os.environ.get("MESSAGE_BUS_CAPACITY", "100")))
_event_bus = EventBus()
_ready_queue = ReadyQueue(policy=SchedulingPolicy.FCFS)
_llm = _build_llm()
_sandbox = _build_sandbox()
_executor = Executor(
    llm=_llm, sandbox=_sandbox, quota=_quota,
    logger=_logger, events=_event_bus, bus=_message_bus,
    pipeline_timeout=float(os.environ.get("PIPELINE_WAIT_TIMEOUT", "30")),
)
_worker_pool = WorkerPool(
    ready_queue=_ready_queue,
    executor=_executor,
    workers=int(os.environ.get("WORKER_COUNT", "4")),
)


def _reset_for_tests() -> None:
    """Used only by the test suite to wipe per-test state."""
    global _agents, _next_id
    _agents = []
    _next_id = 1
    _quota.reset()
    _logger.clear()


# ---------- FastAPI lifespan ----------

@asynccontextmanager
async def lifespan(app: FastAPI):
    _event_bus.bind_loop(asyncio.get_running_loop())
    _worker_pool.start()
    try:
        yield
    finally:
        _worker_pool.stop()


app = FastAPI(title="Mini Agent OS", version="0.1.0", lifespan=lifespan)
app.mount("/static", StaticFiles(directory="static"), name="static")


# ---------- request models ----------

class AgentCreate(BaseModel):
    name: str = Field(..., examples=["summary"])
    prompt: str
    kind: AgentKind = AgentKind.LLM
    priority: int = Field(1, ge=1, le=10)
    timeout: int = Field(3, ge=1, le=30)
    pipe_to: Optional[int] = None


class PolicyChange(BaseModel):
    policy: SchedulingPolicy


class SimulatePolicy(str, Enum):
    FCFS = "fcfs"
    PRIORITY = "priority"
    RR = "rr"


class SimulateJob(BaseModel):
    id: str
    burst: int = Field(..., ge=1)
    arrival: int = Field(0, ge=0)
    priority: int = Field(1, ge=1, le=10)


class SimulateRequest(BaseModel):
    policy: SimulatePolicy
    jobs: List[SimulateJob]
    quantum: Optional[int] = Field(None, ge=1)


# ---------- endpoints ----------

@app.get("/")
def root():
    return FileResponse("static/index.html")


@app.post("/agents")
def create_agent(req: AgentCreate):
    global _next_id
    task = AgentTask(
        agent_id=_next_id, name=req.name, prompt=req.prompt, kind=req.kind,
        priority=req.priority, timeout=req.timeout, pipe_to=req.pipe_to,
    )
    _agents.append(task)
    _next_id += 1
    _logger.log(
        f"Agent {task.agent_id} ({task.name}) created "
        f"kind={req.kind.value} pipe_to={req.pipe_to}"
    )
    _event_bus.publish(task.to_dict())
    _ready_queue.put(task)
    return {"message": "created", "agent": task.to_dict()}


@app.get("/agents")
def list_agents():
    return {"count": len(_agents), "agents": [a.to_dict() for a in _agents]}


@app.get("/policy")
def get_policy():
    return {"policy": _ready_queue.get_policy().value}


@app.post("/policy")
def set_policy(req: PolicyChange):
    _ready_queue.set_policy(req.policy)
    _logger.log(f"Scheduling policy switched to {req.policy.value}")
    return {"policy": req.policy.value}


@app.get("/logs")
def get_logs():
    return {"logs": _logger.read_all()}


@app.delete("/agents")
def clear():
    global _agents, _next_id
    _agents = []
    _next_id = 1
    _quota.reset()
    _logger.clear()
    _logger.log("State cleared")
    return {"message": "cleared"}


@app.post("/simulate")
def simulate(req: SimulateRequest):
    jobs = [j.model_dump() for j in req.jobs]
    if req.policy == SimulatePolicy.FCFS:
        gantt = simulate_fcfs(jobs)
    elif req.policy == SimulatePolicy.PRIORITY:
        gantt = simulate_priority(jobs)
    else:  # RR
        if req.quantum is None:
            raise HTTPException(400, "quantum is required for RR")
        gantt = simulate_rr(jobs, quantum=req.quantum)
    return {"policy": req.policy.value, "gantt": gantt}


@app.get("/events")
async def sse_events():
    async def stream():
        async for event in _event_bus.subscribe():
            yield {"data": json.dumps(event)}
    return EventSourceResponse(stream())
```

- [ ] **Step 4: Run test to verify it passes**

Run: `uv run pytest tests/test_main.py -v`
Expected: 5 passed.

- [ ] **Step 5: Run the full non-Docker, non-slow suite**

Run: `uv run pytest -m "not docker and not slow"`
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add app/main.py tests/test_main.py
git commit -m "feat: FastAPI endpoints, SSE stream, and /simulate"
```

---

## Task 16: Dashboard (SSE-driven + Simulator UI)

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
    <p>OS-inspired runtime for LLM agents · sandboxed code execution · pipelines · team <strong>GarbageCollector</strong></p>
  </header>

  <section>
    <h2>Scheduling policy (live)</h2>
    <select id="policy">
      <option value="fcfs">FCFS</option>
      <option value="priority">Priority (non-preemptive)</option>
    </select>
    <span id="policy-status"></span>
  </section>

  <section class="create">
    <h2>Create agent</h2>
    <form id="create-form">
      <input name="name" placeholder="name" required />
      <textarea name="prompt" placeholder="prompt — use {INPUT} placeholder for pipeline input" required></textarea>
      <select name="kind">
        <option value="llm">llm</option>
        <option value="code">code (sandboxed)</option>
      </select>
      <input name="priority" type="number" value="5" min="1" max="10" />
      <input name="timeout" type="number" value="3" min="1" max="30" />
      <input name="pipe_to" type="number" placeholder="pipe_to (optional downstream id)" />
      <button type="submit">Create</button>
    </form>
  </section>

  <section class="run">
    <button id="clear">Clear all</button>
  </section>

  <section>
    <h2>Agents</h2>
    <table id="agents">
      <thead><tr>
        <th>ID</th><th>Name</th><th>Kind</th><th>State</th><th>Priority</th>
        <th>Pipe to</th><th>Result</th>
      </tr></thead>
      <tbody></tbody>
    </table>
  </section>

  <section>
    <h2>Logs</h2>
    <pre id="logs"></pre>
  </section>

  <section class="simulator">
    <h2>Scheduler simulator (Gantt)</h2>
    <p class="hint">
      Compute textbook scheduling — including preemptive Round Robin which the live runtime
      cannot do because LLM/Docker calls are non-preemptable.
    </p>
    <div class="sim-controls">
      <label>Policy
        <select id="sim-policy">
          <option value="fcfs">FCFS</option>
          <option value="priority">Priority (non-preemptive)</option>
          <option value="rr">Round Robin</option>
        </select>
      </label>
      <label>Quantum (RR only)
        <input id="sim-quantum" type="number" value="2" min="1" />
      </label>
      <button id="sim-run">Run simulation</button>
    </div>
    <p class="hint">Jobs (one per line: <code>id burst arrival priority</code>)</p>
    <textarea id="sim-jobs" rows="5">A 5 0 1
B 3 0 1
C 4 2 1</textarea>
    <div id="sim-gantt"></div>
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
  background: #181c25; color: inherit; font-size: 14px;
}
button { cursor: pointer; background: #3b82f6; border-color: #3b82f6; }
button:hover { background: #2563eb; }
.run button { margin-right: .5rem; }
table { width: 100%; border-collapse: collapse; }
th, td { padding: .5rem; border-bottom: 1px solid #2a2f3a; text-align: left; vertical-align: top; }
.state-READY   { color: #94a3b8; }
.state-RUNNING { color: #fbbf24; animation: pulse 1s infinite; }
.state-DONE    { color: #34d399; }
.state-TIMEOUT { color: #f87171; }
.state-ERROR   { color: #ef4444; }
@keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: .55; } }
pre { background: #181c25; padding: 1rem; border-radius: 6px; max-height: 300px; overflow: auto; margin: 0; }
td pre { max-height: 120px; padding: .5rem; font-size: 12px; }
#policy-status { margin-left: .5rem; color: #34d399; font-size: 13px; }
.hint { color: #8a93a3; font-size: 13px; margin: .25rem 0; }

/* Simulator UI */
.sim-controls { display: flex; gap: .5rem; align-items: end; margin-bottom: .5rem; flex-wrap: wrap; }
.sim-controls label { display: grid; gap: .25rem; font-size: 12px; color: #8a93a3; }
#sim-jobs { width: 100%; max-width: 480px; font-family: monospace; }
#sim-gantt { display: flex; gap: 2px; margin-top: 1rem; min-height: 60px; }
.sim-bar {
  background: #3b82f6; color: white; padding: .35rem .5rem;
  border-radius: 4px; font-size: 12px; font-family: monospace;
  display: flex; flex-direction: column; justify-content: center;
}
.sim-bar small { opacity: .8; font-size: 10px; }
```

- [ ] **Step 3: Write `static/script.js`**

```javascript
// ---------- agent table + SSE ----------

const agentMap = new Map();

function renderAgents() {
  const tbody = document.querySelector("#agents tbody");
  const rows = [...agentMap.values()].sort((a, b) => a.agent_id - b.agent_id);
  tbody.innerHTML = rows.map(a => `
    <tr>
      <td>${a.agent_id}</td>
      <td>${a.name}</td>
      <td>${a.kind}</td>
      <td class="state-${a.state}">${a.state}</td>
      <td>${a.priority}</td>
      <td>${a.pipe_to ?? ""}</td>
      <td><pre>${escapeHtml((a.result || a.error_message || "")).slice(0, 600)}</pre></td>
    </tr>
  `).join("");
}

function escapeHtml(s) {
  return s.replace(/[&<>"']/g, c => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;"
  }[c]));
}

async function refreshLogs() {
  const r = await fetch("/logs");
  const data = await r.json();
  document.querySelector("#logs").textContent = data.logs.join("\n");
}

async function refreshAll() {
  const r = await fetch("/agents");
  const data = await r.json();
  agentMap.clear();
  for (const a of data.agents) agentMap.set(a.agent_id, a);
  renderAgents();
  refreshLogs();
}

const es = new EventSource("/events");
es.onmessage = (ev) => {
  const a = JSON.parse(ev.data);
  agentMap.set(a.agent_id, a);
  renderAgents();
  refreshLogs();
};

// ---------- create agent ----------

document.querySelector("#create-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const body = Object.fromEntries(new FormData(e.target).entries());
  ["priority", "timeout"].forEach(k => body[k] = Number(body[k]));
  body.pipe_to = body.pipe_to ? Number(body.pipe_to) : null;
  await fetch("/agents", {
    method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body),
  });
  e.target.reset();
});

// ---------- policy switch ----------

const policySel = document.querySelector("#policy");
fetch("/policy").then(r => r.json()).then(d => { policySel.value = d.policy; });
policySel.addEventListener("change", async (e) => {
  const r = await fetch("/policy", {
    method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify({policy: e.target.value}),
  });
  const d = await r.json();
  const s = document.querySelector("#policy-status");
  s.textContent = `→ ${d.policy}`;
  setTimeout(() => s.textContent = "", 1500);
});

// ---------- clear ----------

document.querySelector("#clear").onclick = async () => {
  await fetch("/agents", {method: "DELETE"});
  agentMap.clear();
  renderAgents();
  refreshLogs();
};

// ---------- simulator ----------

function parseJobs(text) {
  return text.trim().split("\n").map(line => {
    const [id, burst, arrival = "0", priority = "1"] = line.trim().split(/\s+/);
    return {
      id, burst: Number(burst), arrival: Number(arrival), priority: Number(priority),
    };
  });
}

function renderGantt(gantt) {
  const container = document.querySelector("#sim-gantt");
  container.innerHTML = "";
  if (!gantt.length) return;
  const totalEnd = gantt[gantt.length - 1].end;
  for (const slice of gantt) {
    const dur = slice.end - slice.start;
    const bar = document.createElement("div");
    bar.className = "sim-bar";
    bar.style.flexGrow = String(dur);
    bar.innerHTML = `${slice.id}<small>${slice.start}→${slice.end}</small>`;
    container.appendChild(bar);
  }
  const totalLabel = document.createElement("div");
  totalLabel.className = "hint";
  totalLabel.textContent = `total time = ${totalEnd}`;
  container.appendChild(totalLabel);
}

document.querySelector("#sim-run").onclick = async () => {
  const policy = document.querySelector("#sim-policy").value;
  const quantum = Number(document.querySelector("#sim-quantum").value);
  const jobs = parseJobs(document.querySelector("#sim-jobs").value);
  const body = { policy, jobs };
  if (policy === "rr") body.quantum = quantum;
  const r = await fetch("/simulate", {
    method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body),
  });
  if (!r.ok) {
    alert(await r.text());
    return;
  }
  const data = await r.json();
  renderGantt(data.gantt);
};

refreshAll();
```

- [ ] **Step 4: Manual smoke test**

Run: `docker compose up` then open `http://localhost:8000`
Expected:
- Dashboard loads.
- With a real `UPSTAGE_API_KEY`, creating an `llm` agent shows READY → RUNNING (yellow pulsing) → DONE (green) without any manual refresh.
- "Run simulation" in the Scheduler simulator section produces a colored Gantt bar chart.

- [ ] **Step 5: Commit**

```bash
git add static/
git commit -m "feat: SSE dashboard with pipeline support and Gantt simulator UI"
```

---

## Task 17: End-to-End Demo Script

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

### Scenario A — Concurrent execution (worker pool)
1. Open the dashboard so the SSE stream is connected.
2. Quickly create 4 LLM agents in a row.
3. Expected: all four turn RUNNING (yellow, pulsing) **simultaneously**, then complete one by one. Demonstrates the 4-thread worker pool.

### Scenario B — FCFS scheduling
1. Set `WORKER_COUNT=1` in `.env` and restart (one worker → serialized order is observable).
2. Switch policy dropdown to "FCFS".
3. Create three LLM agents named `a`, `b`, `c` quickly.
4. Expected: execution order is `a → b → c` (creation order).

### Scenario C — Priority scheduling (non-preemptive)
1. Still single worker. Switch policy to "Priority".
2. Clear, then create three agents named `low` (priority 1), `mid` (5), `high` (9), in that order.
3. Expected: even though `low` was created first, the order is `high → mid → low`. Note that priority does not interrupt a running agent — it only orders the queue.

### Scenario D — Sandboxed code execution
1. Restore `WORKER_COUNT=4`, restart.
2. Create a `code` agent with prompt: `Write a Python program that prints the first 10 Fibonacci numbers.`
3. Expected: state goes RUNNING → DONE; result includes generated code and stdout `0 1 1 2 3 5 8 13 21 34`.

### Scenario E — Timeout (sandbox kill)
1. Create a `code` agent, timeout=2, prompt: `Write Python that sleeps for 60 seconds.`
2. Expected: state becomes TIMEOUT within ~2s.

### Scenario F — Quota exhaustion (mutex synchronization)
1. Set `GLOBAL_QUOTA=2` in `.env` and restart.
2. Create 5 LLM agents and wait.
3. Expected: first two reach DONE, remaining three become ERROR with "API quota exhausted". Because the QuotaManager uses a mutex, the count is always exactly 2 even with concurrent workers.

### Scenario G — Network isolation (system-call restriction)
1. Create a `code` agent with prompt: `Write Python that uses urllib to fetch https://example.com and prints the status code.`
2. Expected: code runs but reports a network error — `--network=none` blocks egress at the container namespace boundary.

### Scenario H — Pipeline (inter-agent IPC via bounded MessageBus)
1. Restore default `.env`.
2. Create agent **#1** first (note its assigned id, e.g. 5):
   - name = `gen`, kind = `llm`, prompt = `List 3 interesting facts about pelicans.`
   - pipe_to = the id that will be assigned to the next agent (one greater than #1's id; if #1 got id 5, set pipe_to = 6).
3. Immediately create agent **#2**:
   - name = `summary`, kind = `llm`, prompt = `Summarize the following into a single sentence: {INPUT}`
   - leave pipe_to blank.
4. Expected: `gen` runs and DONE; `summary` was RUNNING (blocked on `bus.receive("pipe-6")`) and the moment `gen` finishes, `summary`'s prompt gets `{INPUT}` substituted and the agent completes. This demonstrates the bounded MessageBus + producer/consumer pattern wiring two agents.

### Scenario I — Round Robin scheduling (simulator)
1. In the "Scheduler simulator" section, paste:

```
A 8 0 3
B 4 0 1
C 9 0 2
D 5 0 4
```

2. Choose **Round Robin**, quantum = `3`, click **Run simulation**.
3. Expected: a Gantt bar chart showing repeated A/B/C/D quanta in arrival order until each job's burst is consumed. Switch policy to **FCFS** or **Priority** and compare the orderings on the same jobs.
```

- [ ] **Step 2: Commit**

```bash
git add docs/demo-script.md
git commit -m "docs: end-to-end demo script with pipeline and simulator scenarios"
```

---

## Task 18: Technical Report and Process Skeletons

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
[Diagram: User → FastAPI → ReadyQueue → WorkerPool (N threads) → Executor → (LLMClient | Sandbox).
MessageBus connects upstream → downstream agents.
EventBus pushes state changes to SSE → Dashboard.
Reference file paths so the reader can jump straight to the code.]

## 3. Tech Stack
- Python 3.11, managed by uv (lockfile-based, cross-platform reproducible)
- FastAPI + Pydantic v2 + sse-starlette
- Upstage Solar Pro 3 via `openai` SDK
- Docker Engine (sandbox isolation; macOS + Windows via Docker Desktop)
- threading (worker pool, Lock, Condition) + asyncio (HTTP / SSE)
- Vanilla HTML / CSS / JS, EventSource API

## 4. Concurrency Model
This runtime intentionally layers two concurrency models — exactly like a real OS layers preemptive kernel threads under an asynchronous user-space reactor:

| Layer | Model | Why |
|---|---|---|
| HTTP server | asyncio (uvicorn) | Many concurrent I/O-bound clients, single-threaded reactor |
| Agent execution | threading worker pool (N) | Blocking work: LLM HTTP call, `docker run` subprocess |
| Bridge | `EventBus` (queue.Queue + `call_soon_threadsafe`) | Thread-safe handoff to asyncio for SSE fan-out |

## 5. Scheduling: Live vs. Simulated
A real LLM call is atomic — once an HTTP request is in flight, the only way to "preempt" it is to abandon the response. Likewise a `docker run` subprocess can only be killed wholesale. The live runtime therefore implements **non-preemptive FCFS and Priority**: the scheduler only chooses which READY agent a free worker picks up next, never interrupting a running one.

To still demonstrate preemptive Round Robin — the canonical OS-class algorithm — the project ships a separate **simulator module** (`app/simulator.py`) that takes hypothetical burst times and produces Gantt timelines for FCFS, non-preemptive Priority, and Round Robin. The dashboard renders these as colored bar charts. Splitting "live behavior" from "textbook behavior" is honest about what the LLM substrate allows and what the OS course expects.

## 6. OS Concepts and Where They Live

| OS Concept | File | How it's implemented |
|---|---|---|
| Process / PCB | `app/agent.py` | `AgentTask` dataclass |
| Process state | `app/agent.py` | `AgentState` enum, transitions logged + published |
| Ready queue | `app/ready_queue.py` | Synchronized queue with switchable policy |
| Live scheduler (FCFS / Priority, non-preemptive) | `app/scheduler.py` | Strategy via sort-key functions |
| Simulated scheduler (FCFS / Priority / RR) | `app/simulator.py` | Pure functions, Gantt timeline output |
| Worker pool | `app/worker_pool.py` | N daemon threads, blocking pop |
| Mutex | `app/quota_manager.py` | `threading.Lock` guards shared API quota |
| Condition (wait/notify) | `app/ready_queue.py` | Workers wait when empty, notified on put |
| Bounded buffer (producer/consumer) | `app/message_bus.py` | `queue.Queue(maxsize=N)` per topic |
| IPC — agent → agent pipeline | `app/executor.py` + `app/message_bus.py` | `{INPUT}` placeholder + `pipe_to` field |
| IPC — parent ↔ sandbox | `app/sandbox.py` | stdin/stdout pipes |
| Process isolation | `app/sandbox.py` | Docker container per execution |
| System call restriction | `app/sandbox.py` | `--cap-drop=ALL`, `--security-opt=no-new-privileges` |
| Network isolation | `app/sandbox.py` | `--network=none` |
| File permission | `app/sandbox.py` | `--read-only` rootfs |
| Resource limit | `app/sandbox.py` | `--memory`, `--cpus`, `--pids-limit` (cgroups) |
| Timeout / forced termination | `app/sandbox.py` | `subprocess.TimeoutExpired` → container killed |
| Event notification | `app/event_bus.py` | Thread-safe fan-out to async subscribers |
| Trace log | `app/logger.py` | File-backed event log |

## 7. LLM Integration
[How Solar Pro 3 is called, prompt format, what the wrapper does, error handling.]

## 8. Walkthrough — A Code-Execution Agent
[Step by step: POST /agents → ReadyQueue.put → worker wakes via Condition → quota mutex acquired → LLM completion → sandbox.run → SSE event → dashboard update → DONE state. Include one real screenshot or log excerpt.]

## 9. Walkthrough — A Pipeline (IPC demo)
[Two agents wired with pipe_to / {INPUT}; explain how the downstream worker blocks on the bounded queue and resumes when the upstream publishes its result. Include a screenshot.]

## 10. Limitations and Future Work
- Single-host runtime; no distributed scheduling.
- **Non-preemptive live scheduling.** LLM calls and Docker subprocesses cannot be cleanly preempted, so the live runtime never interrupts a running agent. The simulator covers preemptive RR for completeness, but a hypothetical "tokens-per-quantum" preemption of streaming LLM output would be interesting future work.
- **Docker-out-of-Docker.** The app mounts `/var/run/docker.sock` to spawn sandbox containers — convenient for demo, equivalent to host root for production. A proper deployment would use a separate sandbox service over an authenticated channel.
- **Events dropped without subscribers.** The EventBus has no replay buffer; events fired before the dashboard connects are lost.
- Sandbox image is Python-only.
- In-memory agent table — lost on restart.

## 11. Conclusion
[1 paragraph on what was learned about applying OS abstractions to LLM workloads.]
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
| 10 | Agent, Scheduler, ReadyQueue, QuotaManager, MessageBus, EventBus |
| 11 | LLM client, Sandbox, Executor (with pipeline), WorkerPool |
| 12 | Simulator, FastAPI + SSE, Dashboard |
| 13 | Demo, technical report, slides |

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
- ✅ Substantive OS concepts: process/PCB, state machine, ready queue, **3 scheduling policies** (FCFS, Priority live; RR via simulator), worker pool, mutex, condition, bounded buffer, IPC (2 forms — pipeline + sandbox pipes), process isolation, syscall restriction, network/file/resource limits, timeout, event bus, trace log
- ✅ Real LLM integration (Solar Pro 3) — sits behind scheduler, quota gate, optional sandbox, and pipeline IPC; not a thin wrapper
- ✅ Working app + setup instructions + demo script (Tasks 1, 15, 17)
- ✅ Technical report skeleton with explicit non-preemptive scheduling discussion (Task 18)
- ✅ Development process doc skeleton (Task 18)
- ✅ MessageBus is wired into actual runtime behavior (Tasks 12, 15, 17 Scenario H) — no dead code
- ✅ Round Robin demonstrated via simulator (Tasks 14, 15, 16, 17 Scenario I)
- ⚠️ Presentation slides (English) — not in this plan; slides are a manual creative artifact, produced from the technical report once implementation stabilizes

**Placeholder scan:** all implementation tasks contain concrete code. The `docs/technical-report.md` skeleton in Task 18 contains intentional `[fill in]` cues — that document is the author's writing, not part of the implementation, so cues belong there.

**Type consistency:**
- `AgentKind` enum values `"llm"` / `"code"` used identically in `app/agent.py`, `app/main.py` (Pydantic field), and `static/index.html` (`<option value>`).
- `SchedulingPolicy` enum values `"fcfs"` / `"priority"` used identically in scheduler, ReadyQueue, main.py, and dashboard. The simulator has its own `SimulatePolicy` enum (adds `"rr"`); the dashboard simulator section uses these values.
- `SandboxResult` fields (`exit_code`, `stdout`, `stderr`, `timed_out`) used identically in Task 11 (definition), Task 12 (Executor consumes), and tests.
- `ReadyQueue.pop_next()` returning `None` as shutdown sentinel is honored by `WorkerPool._worker_loop` (Task 13).
- `Executor` constructor parameters (`llm`, `sandbox`, `quota`, `logger`, `events`, `bus`, `pipeline_timeout`) match the call site in `app/main.py` (Task 15).
- `INPUT_PLACEHOLDER = "{INPUT}"` defined in `app/executor.py` (Task 12); the same literal `{INPUT}` appears in dashboard placeholder text and demo Scenario H — kept literal rather than imported by the frontend, which is acceptable since the dashboard is decoupled.
- Simulator Job shape `{id, burst, arrival, priority}` is consistent between `app/simulator.py` (Task 14), Pydantic model `SimulateJob` in Task 15, dashboard `parseJobs` in Task 16, and demo input in Task 17.
- `pipe_to` field is `Optional[int]` everywhere: `AgentTask`, `AgentCreate` Pydantic model, demo script, and dashboard form.

---

## Execution Handoff

Two execution options:

1. **Subagent-Driven (recommended)** — dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — execute tasks in this session using executing-plans, batch with checkpoints.
