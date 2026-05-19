# Mini Agent OS — Technical Report

**Team:** GarbageCollector
**Direction:** A — OS-for-LLM

## 1. Project Overview
This project explores how classical OS abstractions—process states, ready queues, scheduling policies, synchronization primitives, and sandbox isolation—apply to coordinating LLM-based agents. We built a Python runtime that hosts multiple agents, each with an independent lifecycle, concurrency model, and execution sandbox. The headline result: a minimal but honest OS scheduler (non-preemptive FCFS/Priority for live execution, plus a simulator covering preemptive Round Robin) that manages LLM calls and Docker-sandboxed code side by side, driving agents through their state machine and rendering their behavior on a live dashboard.

## 2. System Architecture

```
User Browser (EventSource)
    ↓
Dashboard (HTML/CSS/JS)
    ↓
SSE Stream (StateUpdates)
    ↓
    ┌──────────────────────────────────────────────────────────┐
    │                    FastAPI (uvicorn)                     │
    │                   HTTP / asyncio reactor                 │
    └──────┬───────────────────────────────────────────────────┘
           │
           ├─ POST /agents (create AgentTask)
           │   ↓
           │ ReadyQueue (FCFS / Priority)
           │   ↓
           ├──────────────────────────────────────────────────┐
           │     WorkerPool (N blocking threads)               │
           │  ┌─────────────────────────────────────────────┐ │
           │  │ Worker[0]  Worker[1]  ...  Worker[N-1]     │ │
           │  │    ↓          ↓                  ↓         │ │
           │  │  pop()      pop()              pop()       │ │
           │  └─────────────┬───────────────────┬──────────┘ │
           │                │ (AgentTask)       │            │
           │                ↓                   ↓            │
           │  ┌──────────────────────────────────────────┐  │
           │  │         Executor                         │  │
           │  │  • Quota check (Lock)                    │  │
           │  │  • LLM call → Solar Pro 3               │  │
           │  │  • State transitions + emit EventBus     │  │
           │  └──────────────┬───────────────────────────┘  │
           │                 │                               │
           │                 ├─ (LLMClient)                  │
           │                 │   ↓                           │
           │                 │  openai.ChatCompletion       │
           │                 │                               │
           │                 └─ (Sandbox)                    │
           │                     ↓                           │
           │                    docker run (isolated env)    │
           │                                                  │
           │  EventBus (Queue + call_soon_threadsafe)       │
           │    Thread-safe bridge ↓ asyncio.Event          │
           └──────────────────────┬──────────────────────────┘
                                  │
                        ┌─────────┴─────────┐
                        ↓                   ↓
                   SSE multicast        Logger (file)
                   → Dashboard          → trace log
```

**Key files:**
- `app/agent.py` — AgentTask, AgentState enum
- `app/ready_queue.py` — Synchronized ready queue with pluggable policy
- `app/scheduler.py` — Policy dispatch (FCFS, Priority)
- `app/simulator.py` — Gantt timelines for simulated scheduling
- `app/worker_pool.py` — N daemon threads, blocking pop loop
- `app/quota_manager.py` — Mutex-guarded LLM API quota
- `app/message_bus.py` — Topic-based bounded queues for IPC
- `app/event_bus.py` — Thread-safe event fan-out to asyncio subscribers
- `app/executor.py` — State machine executor; bridges agents ↔ LLM ↔ Sandbox
- `app/sandbox.py` — Docker container with strict isolation and cgroup limits
- `app/logger.py` — Trace log event writer
- `app/main.py` — FastAPI routes, SSE endpoints
- `static/dashboard.html` — Client-side EventSource listener and Gantt renderer

## 3. Tech Stack
- **Python 3.11**, managed by `uv` (lockfile-based, cross-platform reproducible)
- **FastAPI + Pydantic v2 + sse-starlette** — HTTP API, SSE streaming
- **Upstage Solar Pro 3** via `openai` SDK (compatible endpoint)
- **Docker Engine** — sandbox isolation; macOS + Windows via Docker Desktop
- **threading** (worker pool, Lock, Condition, Queue) + **asyncio** (HTTP, SSE)
- **Vanilla HTML / CSS / JavaScript**, EventSource API (no frontend framework)

## 4. Concurrency Model
This runtime intentionally layers two concurrency models — exactly like a real OS layers preemptive kernel threads under an asynchronous user-space reactor:

| Layer | Model | Why |
|---|---|---|
| HTTP server | asyncio (uvicorn) | Many concurrent I/O-bound clients; single-threaded reactor |
| Agent execution | threading worker pool (N) | Blocking work: LLM HTTP call, `docker run` subprocess |
| Bridge | `EventBus` (queue.Queue + `call_soon_threadsafe`) | Thread-safe handoff to asyncio for SSE fan-out |

The HTTP layer remains responsive even while workers are blocked on LLM I/O or sandboxed subprocesses. Events (state transitions, stdout chunks) are queued by worker threads and safely injected back into the asyncio event loop for streaming to browsers.

## 5. Scheduling: Live vs. Simulated
A real LLM call is atomic — once an HTTP request is in flight, the only way to "preempt" it is to abandon the response. Likewise a `docker run` subprocess can only be killed wholesale. The **live runtime** therefore implements **non-preemptive FCFS and Priority**: the scheduler only chooses which READY agent a free worker picks up next, never interrupting a running one.

To still demonstrate preemptive Round Robin — the canonical OS-class algorithm — the project ships a separate **simulator module** (`app/simulator.py`) that takes hypothetical burst times and produces Gantt timelines for FCFS, non-preemptive Priority, and Round Robin. The dashboard renders these as colored bar charts. Splitting "live behavior" from "textbook behavior" is honest about what the LLM substrate allows and what the OS course expects.

## 6. OS Concepts and Where They Live

| OS Concept | File | How it's implemented |
|---|---|---|
| Process / PCB | `app/agent.py` | `AgentTask` dataclass with metadata fields |
| Process state | `app/agent.py` | `AgentState` enum; transitions logged + published |
| Ready queue | `app/ready_queue.py` | Synchronized queue with switchable policy (FCFS, Priority) |
| Live scheduler (FCFS / Priority, non-preemptive) | `app/scheduler.py` | Strategy via sort-key functions; called by workers |
| Simulated scheduler (FCFS / Priority / RR) | `app/simulator.py` | Pure functions; Gantt timeline output |
| Worker pool | `app/worker_pool.py` | N daemon threads, blocking `pop()` loop |
| Mutex | `app/quota_manager.py` | `threading.Lock` guards shared API quota |
| Condition (wait/notify) | `app/ready_queue.py` | Workers wait when empty, notified on `put()` |
| Bounded buffer (producer/consumer) | `app/message_bus.py` | `queue.Queue(maxsize=N)` per topic |
| IPC — agent → agent pipeline | `app/executor.py` + `app/message_bus.py` | `{INPUT}` placeholder substitution + `pipe_to` field |
| IPC — parent ↔ sandbox | `app/sandbox.py` | stdin/stdout pipes (async streaming) |
| Process isolation | `app/sandbox.py` | Docker container per execution |
| System call restriction | `app/sandbox.py` | `--cap-drop=ALL`, `--security-opt=no-new-privileges` |
| Network isolation | `app/sandbox.py` | `--network=none` |
| File permission | `app/sandbox.py` | `--read-only` rootfs |
| Resource limit | `app/sandbox.py` | `--memory`, `--cpus`, `--pids-limit` (cgroups) |
| Timeout / forced termination | `app/sandbox.py` | `subprocess.TimeoutExpired` → container killed |
| Event notification | `app/event_bus.py` | Thread-safe fan-out to async subscribers |
| Trace log | `app/logger.py` | File-backed event log (append-only) |

## 7. LLM Integration
The project uses Upstage Solar Pro 3 via the `openai` SDK. The `LLMClient` class wraps the completion call with:
- **Quota enforcement:** Calls acquire a lock guarding request count and token budget.
- **Prompt templating:** Agent instructions and input are formatted into a system + user message.
- **Error handling:** Rate limits, timeouts, and malformed responses are caught and logged; the agent transitions to ERROR state.
- **Token tracking:** Completion and prompt tokens are recorded in the agent's result.

The Solar Pro 3 endpoint accepts the same request format as OpenAI's API, making it a drop-in backend.

## 8. Walkthrough — A Code-Execution Agent
Here's the journey of a single agent from creation to completion:

1. **Client POSTs** to `/agents` with a task:
   ```json
   {
     "name": "python-agent-1",
     "instructions": "Run the attached code and return the output.",
     "input": "print('hello world')",
     "sandbox_image": "python:3.11"
   }
   ```

2. **FastAPI handler** creates an `AgentTask`, transitions it to `QUEUED`, emits event, and calls `ready_queue.put(task)`.

3. **Worker thread** blocks on `ready_queue.pop()`, wakes when a task arrives. It calls `executor.run(task)`.

4. **Executor** transitions task to `RUNNING`, acquires quota lock, calls LLM:
   - System: "You are a helpful code executor. The user will provide code. Execute it in the sandbox and return the output and any error messages."
   - User: "Run: print('hello world')"

5. **LLM** responds with:
   ```
   I'll execute that code for you.
   
   <sandbox>
   python -c "print('hello world')"
   </sandbox>
   ```

6. **Executor parses** the response, detects `<sandbox>` tag, extracts command, and calls `sandbox.run()`:
   - Spawns Docker container from `python:3.11`
   - Pipes command via stdin
   - Streams stdout asynchronously
   - Each line emitted as `OUTPUT` event

7. **EventBus** receives `OUTPUT` events from the worker thread, injects them into asyncio via `call_soon_threadsafe()`, and broadcasts to all SSE subscribers.

8. **Dashboard** renders the streaming output in real-time.

9. **Sandbox** completes (return code 0), executor transitions task to `DONE`, records result (stdout, stderr, exit code), and emits final event.

10. **Dashboard** shows task in green, with full log and result.

## 9. Walkthrough — A Pipeline (IPC demo)
Suppose we have two agents wired in a pipeline:
- **Agent A** (Summarizer): "Summarize the input text in one sentence."
- **Agent B** (Translator): "Translate the input to French."
- **Configuration:** Agent A has `pipe_to = "agent-b-id"`.

1. **Agent A** is created and queued.
2. **Worker** executes Agent A, LLM produces a summary.
3. **Executor** records the summary as Agent A's result, then publishes it to `message_bus.topic(agent-b-id)` (a bounded queue).
4. **Agent B** is `READY` and waiting for input. Its worker calls `message_bus.poll(agent-b-id)`, which blocks if the queue is empty.
5. **Agent A's result** arrives in the queue → `message_bus.poll()` returns the summary.
6. **Agent B's executor** substitutes the summary for `{INPUT}` in the LLM prompt and calls the completion.
7. **LLM** translates the summary and returns French text.
8. **Agent B** transitions to `DONE`.

The message bus acts as a bounded buffer between producers and consumers, ensuring pipelining works correctly even if one agent is faster than the other. If Agent A produces output faster than Agent B consumes it, the queue fills to capacity and Agent A's worker is blocked on `put()`; once Agent B drains the queue, Agent A resumes.

**Screenshot**: The dashboard shows two task cards side by side, Agent A complete (green) with a summary, Agent B running (yellow) with the French translation appearing in real-time.

## 10. Limitations and Future Work
- **Single-host runtime** — no distributed scheduling across machines.
- **Non-preemptive live scheduling** — LLM calls and Docker subprocesses cannot be cleanly preempted, so the live runtime never interrupts a running agent. The simulator covers preemptive RR for completeness, but a hypothetical "tokens-per-quantum" preemption of streaming LLM output (truncate at N tokens, save state, resume later) would be interesting future work.
- **Docker-out-of-Docker** — The app mounts `/var/run/docker.sock` to spawn sandbox containers, which is convenient for demo but equivalent to granting host root for production. A proper deployment would use a separate sandbox service over an authenticated, rate-limited channel.
- **Events dropped without subscribers** — The EventBus has no replay buffer; events fired before the dashboard connects are lost.
- **Python-only sandbox** — the default sandbox image is `python:3.11`. Extending to polyglot runtimes (Node.js, Go, Rust, etc.) would require building or pulling per-language images.
- **In-memory agent table** — Lost on restart. A production system would persist agents to a database (e.g., PostgreSQL) with event sourcing.
- **No quota refill policy** — The quota manager is a simple counter; a real system would refill daily or tie it to an external metering service.

## 11. Conclusion
This project demonstrates that OS abstractions—processes, scheduling, synchronization, and isolation—translate directly to LLM agent orchestration. The concurrency model (asyncio HTTP + threaded execution + event bus bridge) cleanly separates concerns and keeps the system responsive. The split between live non-preemptive scheduling and simulated preemptive behavior acknowledges the limits of the LLM execution substrate while still covering classic OS algorithms. The result is a minimal but complete microcosm of an operating system: agents are born, scheduled, executed, and reaped, all while respecting quotas, deadlines, and security boundaries.
