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
- Completed project plan and direction rationale document (`docs/DIRECTION.md`).
- Set up project scaffold: Python 3.11 via `uv`, FastAPI, Docker integration.
- Explored Direction A (OS-for-LLM) rationale and confirmed it as the primary focus.

### Week 10
- Implemented core OS abstractions:
  - `AgentTask` and `AgentState` (process / PCB).
  - `ReadyQueue` with pluggable scheduling policy (FCFS, Priority).
  - `QuotaManager` with mutex (Lock) for API quota enforcement.
  - `MessageBus` for inter-agent communication (bounded queues per topic).
  - `EventBus` for thread-safe event fan-out to asyncio.
  - `Scheduler` module with policy strategy functions.

### Week 11
- Implemented execution layer:
  - `LLMClient` wrapper around Upstage Solar Pro 3 (openai SDK).
  - `Sandbox` class for Docker-based isolation (cgroup limits, capability dropping, read-only fs).
  - `Executor` state machine (QUEUED → RUNNING → DONE/ERROR) with sandbox execution and message bus bridging.
  - `WorkerPool` (N daemon threads, blocking pop).
  - Pipeline support via `{INPUT}` substitution and `pipe_to` field.

### Week 12
- Built simulator and web layer:
  - `Simulator` module with Gantt timeline generation (FCFS, Priority, RR).
  - FastAPI routes: POST /agents, GET /agents/{id}, GET /agents (list).
  - SSE endpoint `/events` for real-time state + output streaming.
  - Dashboard (HTML/CSS/JS) with task cards, live output pane, Gantt charts.
  - Logger for trace events.

### Week 13
- Final integration and documentation:
  - End-to-end demo script (`scripts/demo.sh`) with pipeline and simulator scenarios.
  - Technical report (`docs/technical-report.md`).
  - Development process document (this file).

## Issues and Resolutions

### Issue 1: Thread-safe event publishing
**Problem:** Worker threads executing agents need to publish state changes (QUEUED → RUNNING → DONE) and streaming output to the EventBus, which is consumed by an asyncio-based SSE endpoint. Direct queue operations from threads cause issues.

**Resolution:** Implemented `EventBus` as a wrapper around `queue.Queue` with a `call_soon_threadsafe()` bridge. Worker threads call `event_bus.emit()`, which enqueues the event and injects an asyncio Task into the uvicorn event loop.

### Issue 2: Docker socket mounting and permissions
**Problem:** The app must spawn containers from within a container (or from a local dev machine). Mounting `/var/run/docker.sock` is convenient but grants effective root access.

**Resolution:** Accepted as a development trade-off. For production, a separate sandbox service (with authentication and rate limiting) would be recommended.

### Issue 3: LLM response parsing (sandbox commands)
**Problem:** The LLM generates free-form text with embedded sandbox commands, delimited by `<sandbox>` tags. Robust parsing is needed.

**Resolution:** Simple regex extraction (`<sandbox>(.*?)</sandbox>`). If the LLM doesn't produce tags or produces malformed ones, the executor logs the raw response and transitions to ERROR state.

### Issue 4: Blocking vs. async in quota acquisition
**Problem:** The `QuotaManager.acquire()` method uses `threading.Lock`, which blocks a worker thread. If the quota is exhausted, the worker spins, wasting CPU.

**Resolution:** Kept as blocking for simplicity (quota is rarely exhausted in demo). A more refined approach would use a `Semaphore` with a queue of waiting agents, or integrate quota refill as a separate background task.

### Issue 5: IPC pipeline — handling missing input
**Problem:** An agent configured with `pipe_to` waits for input from the upstream agent's message bus topic. If the upstream fails, the downstream waits forever.

**Resolution:** Added explicit error propagation. If upstream agent enters ERROR state, downstream is marked as BLOCKED_ON_UPSTREAM_ERROR and will not be dequeued.

### Issue 6: Gantt chart rendering with overlapping time scales
**Problem:** Simulated scheduling timelines can have agents spanning different time ranges (wall-clock minutes to milliseconds). Rendering them at a single pixel-per-unit scale leads to unreadable charts.

**Resolution:** Implemented auto-scaling in the dashboard: the chart finds min/max times and scales the x-axis to fit the viewport, with gridlines at sensible intervals (1s, 10s, 1m).

## Retrospective

### What went well
1. **Layered concurrency model** — Separating asyncio (HTTP) from threading (execution) with an event bus bridge made the system easy to reason about and test. Each layer has a single responsibility.
2. **OS abstraction fidelity** — Processes, PCBs, state machines, ready queues, and synchronization primitives mapped cleanly to agent scheduling. Students can recognize the concepts.
3. **Simulator as a teaching tool** — Splitting live non-preemptive behavior from simulated preemptive RR was honest and educational. It avoids false claims about preemption while still covering the algorithm.
4. **Sandbox isolation** — Docker made it trivial to sandbox arbitrary code with tight resource and security constraints (read-only FS, no new privileges, no network).
5. **Event-driven dashboard** — EventSource / SSE was a lightweight way to stream live output without WebSockets, keeping the frontend simple.

### What I would change next time
1. **Quota management refactor** — The current `QuotaManager` is a simple counter. A production system would benefit from a more sophisticated model (e.g., token budget with per-agent allocation, daily refill, or a leaky bucket for rate limiting).
2. **Persistence** — Agents are lost on restart. Adding SQLAlchemy + PostgreSQL would allow event sourcing and replay. This is essential for production but would have added complexity for a demo.
3. **Distributed scheduling** — The current design is single-host. A multi-agent platform would benefit from a task broker (e.g., Celery, RabbitMQ) and a central scheduler.
4. **Streaming LLM output** — The current implementation collects the full LLM response and then parses it. Streaming tokens from the LLM and preempting at token boundaries would be more realistic and would make the "preemptive scheduling" claim stronger.
5. **Polyglot sandbox images** — Currently only Python 3.11. Supporting Node.js, Go, Rust, etc., would require building or pulling multiple images. A registry + caching layer would be useful.
6. **Dashboard UX** — The current dashboard is functional but minimal. A production UI would include filtering, sorting, export, and better visualization of historical timelines and statistics.

## Key Learnings
- **OS concepts translate to LLM orchestration.** Processes, scheduling, synchronization, and isolation are not just kernel abstractions; they are fundamental patterns for managing any concurrent workload.
- **Non-preemption is honest.** Because LLM calls and Docker subprocesses cannot be interrupted mid-execution, a real-world agent system is non-preemptive by substrate. The simulator covers preemptive algorithms for completeness, but pretending the live runtime does preemption would be misleading.
- **Event buses are powerful.** A simple thread-safe queue with asyncio injection is enough to bridge threading and async, unlocking responsive system design.
- **Small is beautiful.** This project fits in ~2k lines of code (including the dashboard) and demonstrates all the major OS concepts. Minimalism forces clarity.
