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
