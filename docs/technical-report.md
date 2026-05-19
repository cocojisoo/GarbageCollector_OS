# Mini Agent OS — Technical Report

**Team:** GarbageCollector
**Direction:** A — OS-for-LLM

## 1. Project Overview
Mini Agent OS is a user-level runtime that treats each LLM agent as a managed process: it creates them, queues them in a synchronized ready queue, hands them off to a fixed-size worker pool, gates them through a mutex-guarded API quota, and — for code-generating agents — executes the LLM's output inside a hardened Docker sandbox. The motivation is observational: once more than one LLM agent runs in the same address space (for summarization, code generation, retrieval, chained tool use), they exhibit the same contention problems operating systems solved decades ago — *which one runs next*, *who gets to use the shared resource*, *how do we stop one from monopolizing CPU or breaking out of its lane*. Direction A (OS-for-LLM) lets us treat those problems on their own terms rather than embedding an LLM into an existing kernel. The result is a fourteen-module Python application that implements thirteen distinct OS primitives — process state machine, ready queue with switchable scheduling, mutex, condition variable, bounded buffer, two forms of IPC, sandbox isolation via Linux namespaces and cgroups, and an SSE-based event-notification bridge between threading and asyncio — and passes fifty tests including real Docker integration tests for the sandbox.

## 2. System Architecture
The runtime is organized around two concurrency layers (asyncio for HTTP, threading for execution) connected by a thread-safe event-bus bridge.

```
                 Browser
                    │  EventSource("/events")
                    ▼
        ┌─────────────────────┐
        │  FastAPI (asyncio)  │  app/main.py
        │  /agents, /policy,  │
        │  /simulate, /events │
        │  /health, /logs     │
        └──────────┬──────────┘
                   │ ReadyQueue.put(task)
                   ▼
        ┌─────────────────────┐  app/ready_queue.py
        │     ReadyQueue      │  threading.Condition (wait / notify)
        │  switchable policy  │  app/scheduler.py: FCFS | Priority
        └──────────┬──────────┘
                   │ pop_next()  (blocks until READY)
                   ▼
        ┌─────────────────────────────────────┐  app/worker_pool.py
        │   WorkerPool (N daemon threads)     │
        └──────────┬──────────────────────────┘
                   │ Executor.execute(task)
                   ▼
        ┌─────────────────────────────────────────────────┐  app/executor.py
        │                  Executor                       │
        │  1. {INPUT}? → MessageBus.receive  (block)      │  app/message_bus.py
        │  2. QuotaManager.try_acquire       (mutex)      │  app/quota_manager.py
        │  3. LLMClient.complete             (Solar Pro 3)│  app/llm_client.py
        │     ↑ on exception: QuotaManager.release(1)     │
        │  4. if kind=CODE: Sandbox.run      (docker run) │  app/sandbox.py
        │  5. pipe_to? → MessageBus.send     (back-pressure)
        └──────────┬──────────────────────────────────────┘
                   │ events.publish(state_dict)
                   ▼
        ┌─────────────────────┐  app/event_bus.py
        │      EventBus       │  call_soon_threadsafe ─→ asyncio Queue
        │  (thread → async)   │
        └──────────┬──────────┘
                   │
                   ▼
        SSE multicast ──→ Browser updates DOM (static/script.js)
```

Each module has a single, focused responsibility; see § 6 for the exact concept-to-file mapping. The dashboard (`static/index.html` + `script.js`) is a single-page client that subscribes to `/events` via the browser's native `EventSource` API — no polling, no framework.

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
| Mutex | `app/quota_manager.py` | `threading.Lock` guards shared API quota; `release(n)` refunds the unit on LLM failure |
| Condition (wait/notify) | `app/ready_queue.py` | Workers wait when empty, notified on put |
| Bounded buffer (producer/consumer) | `app/message_bus.py` | `queue.Queue(maxsize=N)` per topic |
| IPC — agent → agent pipeline | `app/executor.py` + `app/message_bus.py` | `{INPUT}` placeholder + `pipe_to` field |
| IPC — parent ↔ sandbox | `app/sandbox.py` | stdin/stdout pipes |
| Process isolation | `app/sandbox.py` | Docker container per execution |
| System call restriction | `app/sandbox.py` | `--cap-drop=ALL`, `--security-opt=no-new-privileges` |
| Network isolation | `app/sandbox.py` | `--network=none` |
| File permission | `app/sandbox.py` | `--read-only` rootfs + `--tmpfs /tmp:rw,noexec,nosuid,size=32m` |
| Resource limit | `app/sandbox.py` | `--memory`, `--cpus`, `--pids-limit` (cgroups) |
| Timeout / forced termination | `app/sandbox.py` | `subprocess.TimeoutExpired` → `docker kill` via `--cidfile` (container reaped, not orphaned) |
| Event notification | `app/event_bus.py` | Thread-safe fan-out to async subscribers |
| Trace log | `app/logger.py` | File-backed event log |

## 7. LLM Integration
The runtime calls **Upstage Solar Pro 3** through the `openai` SDK pointed at the OpenAI-compatible base URL `https://api.upstage.ai/v1`. `LLMClient` (`app/llm_client.py`) is a thin synchronous wrapper that constructs an `OpenAI(api_key, base_url)` client at startup and exposes a single `complete(prompt) -> str` method built on `chat.completions.create(model='solar-pro-3', messages=[{'role':'user','content':prompt}])`.

A synchronous interface is intentional: it lets the call run on a worker thread without spinning up an event loop, while the FastAPI HTTP server keeps running on its own asyncio loop independently. The Executor calls `LLMClient.complete()` *after* passing the quota gate (so a successful acquire always precedes an attempted API call). If the call raises — network error, rate limit, malformed response — the Executor catches the exception, refunds the quota unit via `QuotaManager.release(1)` (so subsequent agents are not penalised for an upstream failure), records the exception in the agent's `error_message`, and transitions the agent to `ERROR`. All configuration (`UPSTAGE_API_KEY`, `UPSTAGE_BASE_URL`, `UPSTAGE_MODEL`) is read from environment variables at app startup, so the same binary works against any OpenAI-compatible endpoint.

## 8. Walkthrough — A Code-Execution Agent
A code-execution agent travels through six stages, each crossing a recognisable OS boundary.

1. **Submission (HTTP → ready queue).** A client `POST`s to `/agents` with `kind=code` and a prompt like *Write a Python program that prints the first 10 Fibonacci numbers.* The FastAPI handler constructs an `AgentTask`, appends it to the in-memory `_agents` table under `_agents_lock`, publishes the initial state via `EventBus`, and calls `ReadyQueue.put(task)` — which acquires the condition's underlying lock, appends to the queue, and `notify()`s one blocked worker.
2. **Dispatch (Condition → worker).** One of the `N=4` worker threads has been blocked inside `ReadyQueue.pop_next()` on `Condition.wait()`. The notify wakes it; it re-takes the condition lock, sorts the queue by the active policy (FCFS or Priority), pops the next-to-run agent, and calls `Executor.execute(task)`.
3. **Quota gate (mutex).** The executor sets the agent's state to `RUNNING`, publishes the change, then calls `QuotaManager.try_acquire(1)`. The QuotaManager holds its `threading.Lock` for the entire read-check-modify sequence, so no two workers can ever drive `_used` past `_total` — even under heavy contention, our 500-thread test confirms exactly N successes when total=N.
4. **LLM call (synchronous HTTP, ~1–3 s).** The executor calls `LLMClient.complete(prompt)`. The worker thread blocks on the network round-trip — exactly the kind of blocking I/O that justifies a thread pool over a single async loop. Solar Pro 3 returns a Python source program as plain text.
5. **Sandbox execution (process isolation).** Because `task.kind == AgentKind.CODE`, the executor calls `Sandbox.run(generated_code)`. The Sandbox writes a unique `--cidfile`, then spawns `docker run --rm -i --cidfile=… --network=none --read-only --tmpfs=/tmp:rw,noexec,nosuid,size=32m --cap-drop=ALL --security-opt=no-new-privileges --memory=128m --cpus=0.5 --pids-limit=64 mini-agent-os-sandbox:latest` and pipes the source into stdin. The container runs in its own PID, network, and mount namespaces; its rootfs is read-only with a writable tmpfs at `/tmp`; its CPU and memory are bounded by Linux cgroups; all Linux capabilities are dropped. `subprocess.run` enforces a wall-clock timeout (5 s by default); on `TimeoutExpired` the Sandbox reads the container ID from the cidfile and calls `docker kill` explicitly, so the container is reaped rather than orphaned when the docker CLI process is SIGKILLed.
6. **Result and notification (event bus → SSE).** The executor formats the result (`code:\n<generated source>\n---\nstdout:\n<captured output>`), transitions to `DONE`, sets `end_time`, logs the completion, and publishes the final state dict via `EventBus.publish()`. The EventBus uses `loop.call_soon_threadsafe(queue.put_nowait, event)` — the only thread-safe way to inject an item into an asyncio queue from a sync thread. The `/events` SSE handler drains its per-subscriber queue and yields `data: <json>\n\n` to every connected EventSource. The dashboard receives the event, updates its in-memory `agentMap`, and re-renders the row from grey READY → yellow pulsing RUNNING → green DONE without a manual refresh.

*[Insert screenshot here: dashboard immediately after a code agent completes — Result column shows the generated Fibonacci source on top and the captured stdout `0 1 1 2 3 5 8 13 21 34` below.]*

## 9. Walkthrough — A Pipeline (IPC demo)
Pipelines are wired declaratively: an upstream agent is created with `pipe_to = <downstream_id>`, and the downstream agent's prompt contains the literal `{INPUT}` placeholder. The runtime stitches them together through `MessageBus`, a per-topic bounded queue (`queue.Queue(maxsize=100)`).

Consider this two-agent example:
- **Agent 1 (`gen`):** `kind=llm`, prompt = `List 3 interesting facts about pelicans.`, `pipe_to = 2`.
- **Agent 2 (`summary`):** `kind=llm`, prompt = `Summarize this into one sentence: {INPUT}`, no `pipe_to`.

The dance:

1. Both agents are submitted in quick succession; both land in the ready queue. With `WORKER_COUNT=4`, both are picked up almost simultaneously.
2. The worker that picks up `summary` enters the executor first. It detects `{INPUT}` in the prompt and calls `MessageBus.receive('pipe-2', timeout=30)`. The bounded queue is empty, so the worker blocks inside the queue's internal condition — it consumes no CPU, holds no quota, and has not called the LLM.
3. The worker that picks up `gen` runs its LLM call, gets the pelican facts, then — because `gen.pipe_to == 2` — calls `MessageBus.send('pipe-2', result, timeout=5)`. The send succeeds because the bounded queue has capacity.
4. The blocked `summary` worker is unblocked by the queue's notify, receives the upstream string, substitutes it for `{INPUT}`, calls its own LLM with the substituted prompt, and completes.

This is the textbook producer-consumer pattern with a bounded buffer providing back-pressure. If `gen` were to produce results faster than `summary` could consume them (impossible here, but possible in a high-fanout pipeline), `send` would itself block until the consumer drained the queue — flow-controlling the producer. If `gen` never runs at all (for example, because it errored), `summary`'s receive eventually times out at `PIPELINE_WAIT_TIMEOUT` (30 s default), and the executor fails the agent with `Pipeline input not received within 30s`.

*[Insert screenshot here: two rows in the agent table; `gen` reaches DONE first with the pelican facts in its Result column, while `summary` is RUNNING (yellow, pulsing) during the wait, then turns DONE with a one-sentence summary.]*

## 10. Limitations and Future Work
- Single-host runtime; no distributed scheduling.
- **Non-preemptive live scheduling.** LLM calls and Docker subprocesses cannot be cleanly preempted, so the live runtime never interrupts a running agent. The simulator covers preemptive RR for completeness, but a hypothetical "tokens-per-quantum" preemption of streaming LLM output would be interesting future work.
- **Docker-out-of-Docker.** The app mounts `/var/run/docker.sock` to spawn sandbox containers — convenient for demo, equivalent to host root for production. A proper deployment would use a separate sandbox service over an authenticated channel.
- **Events dropped without subscribers.** The EventBus has no replay buffer; events fired before the dashboard connects are lost.
- Sandbox image is Python-only.
- In-memory agent table — lost on restart.

## 11. Conclusion
The exercise shows that operating-system abstractions — processes, ready queues, schedulers, mutexes, condition variables, bounded buffers, IPC, namespaces, cgroups — are not artifacts of kernel implementation but reusable design patterns whose value scales down to any system that coordinates concurrent, expensive, partially-trusted work. Building Mini Agent OS forced explicit answers to questions that ad-hoc agent runtimes typically leave implicit: *which agent runs next when a worker frees up?* (the scheduler), *what stops two agents from both consuming the last unit of a shared resource?* (the mutex), *how does a consumer wait for a producer it does not know about?* (the condition variable behind the bounded buffer), *what can untrusted code actually do to the host?* (the namespace and capability boundary of the Docker sandbox). The deliberate split between a non-preemptive live runtime and a separate preemptive simulator was the project's most honest design decision: rather than pretend the LLM substrate supports preemption it cannot, we acknowledged the limit and built the simulator to cover the classroom algorithm. The result is a small but coherent microcosm — agents are created, scheduled, executed, observed, and reaped — with every transition crossing a primitive that has a name in any OS textbook.
