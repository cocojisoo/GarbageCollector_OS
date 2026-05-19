# Mini Agent OS

> Team **GarbageCollector** · Week 9 Project, Direction A (OS-for-LLM)

An OS-inspired runtime that schedules LLM agents like processes on a worker pool, executes generated Python code inside hardened Docker sandboxes, and chains agents into pipelines through a bounded message bus. Treats each LLM agent as a process with a state machine; uses real OS synchronization primitives (mutex, condition variable, bounded buffer) to coordinate them.

---

## Highlights

- **13 distinct OS concepts** implemented as separate, testable modules — not "runs on Linux" hand-waving.
- **3 scheduling policies** — FCFS and Priority (live, non-preemptive) plus Round Robin (in the simulator, since LLM HTTP calls cannot be cleanly preempted).
- **Real concurrency:** N threading worker pool executes agents in parallel; a `threading.Condition` blocks idle workers; a `threading.Lock` guards a shared API quota.
- **Real sandbox isolation:** every LLM-generated code agent runs in its own Docker container with `--network=none --read-only --cap-drop=ALL --memory=128m --cpus=0.5 --pids-limit=64 --security-opt=no-new-privileges`.
- **Real LLM integration** via Upstage Solar Pro 3 (OpenAI-compatible) — agents call the API behind the scheduler/quota/sandbox stack.
- **Live SSE dashboard** — agent state changes (READY → RUNNING → DONE/TIMEOUT/ERROR) are pushed to the browser in real time; no polling.
- **Pipeline IPC** — agents can be chained via a `pipe_to` field and `{INPUT}` placeholder, with bounded-buffer back-pressure semantics.
- **Cross-platform** — Windows (Docker Desktop + WSL2) and macOS (Docker Desktop) both supported; `uv` lockfile makes installs reproducible.
- **47 tests pass**, including Docker integration tests for the sandbox.

---

## Architecture

```
                 Browser
                    │  EventSource("/events")
                    ▼
        ┌─────────────────────┐
        │  FastAPI (asyncio)  │── /agents, /policy, /simulate, /events
        └──────────┬──────────┘
                   │ put(task)
                   ▼
        ┌─────────────────────┐
        │     ReadyQueue      │  threading.Condition (wait / notify)
        │  switchable policy  │  scheduler.py: FCFS | Priority
        └──────────┬──────────┘
                   │ pop_next()
                   ▼
        ┌─────────────────────────────────────┐
        │   WorkerPool (N threading workers)  │
        └──────────┬──────────────────────────┘
                   │ executor.execute(task)
                   ▼
        ┌─────────────────────────────────────────────────┐
        │                  Executor                       │
        │  1. {INPUT}? → MessageBus.receive  (block)      │
        │  2. QuotaManager.try_acquire       (mutex)      │
        │  3. LLMClient.complete             (Solar Pro 3)│
        │  4. if kind=CODE: Sandbox.run      (docker run) │
        │  5. pipe_to? → MessageBus.send                  │
        └──────────┬──────────────────────────────────────┘
                   │ events.publish(state_dict)
                   ▼
        ┌─────────────────────┐
        │      EventBus       │  call_soon_threadsafe ─→ asyncio Queue
        │  (thread → async)   │
        └──────────┬──────────┘
                   │
                   ▼
        SSE multicast ──→ Browser updates DOM
```

---

## Quickstart

> Prerequisites: Docker Desktop running, `uv` installed, an `UPSTAGE_API_KEY` from <https://console.upstage.ai/>.

```bash
git clone https://github.com/cocojisoo/GarbageCollector_OS.git
cd GarbageCollector_OS
git checkout parkcheolwon

cp .env.example .env             # then edit and add your UPSTAGE_API_KEY
docker compose build             # ~1-3 min on first run
docker compose up
# Then open http://localhost:8000 in your browser
```

For a complete end-to-end walkthrough — including each of the 9 demo scenarios with what to point out to an audience — see **[`docs/DEMO_GUIDE.md`](docs/DEMO_GUIDE.md)**.

---

## OS Concepts → Code

| Concept | File | Mechanism |
|---|---|---|
| Process / PCB | `app/agent.py` | `AgentTask` dataclass |
| Process state | `app/agent.py` | `AgentState` enum: READY / RUNNING / DONE / TIMEOUT / ERROR |
| Ready queue | `app/ready_queue.py` | List + `threading.Condition`; blocking `pop_next()` |
| Live scheduler (FCFS / Priority, non-preemptive) | `app/scheduler.py` | Sort-key strategy functions, runtime-switchable |
| Simulated scheduler (FCFS / Priority / **RR**) | `app/simulator.py` | Pure functions producing Gantt timelines |
| Worker pool (CPU analog) | `app/worker_pool.py` | N daemon threads, blocking pop loop |
| Mutex | `app/quota_manager.py` | `threading.Lock` guards shared API quota |
| Condition variable (wait/notify) | `app/ready_queue.py` | Workers block on empty; producers `notify()` |
| Bounded buffer (producer/consumer) | `app/message_bus.py` | `queue.Queue(maxsize=N)` per topic |
| IPC — agent → agent | `app/executor.py` + `app/message_bus.py` | `pipe_to` field + `{INPUT}` placeholder |
| IPC — parent ↔ sandbox | `app/sandbox.py` | `subprocess` stdin/stdout pipes |
| Process isolation | `app/sandbox.py` | One Docker container per execution |
| System call restriction | `app/sandbox.py` | `--cap-drop=ALL`, `--security-opt=no-new-privileges` |
| Network isolation | `app/sandbox.py` | `--network=none` |
| File permission | `app/sandbox.py` | `--read-only` rootfs, no host mounts |
| Resource limit | `app/sandbox.py` | `--memory`, `--cpus`, `--pids-limit` (cgroups) |
| Timeout / forced termination | `app/sandbox.py` | `subprocess.TimeoutExpired` → container reaped |
| Event notification | `app/event_bus.py` | Thread-safe fan-out to async SSE subscribers |
| Trace log | `app/logger.py` | Thread-safe file-backed event log |

The split between **live** non-preemptive scheduling and the **simulated** preemptive Round Robin is intentional — LLM HTTP calls cannot be cleanly preempted, so the runtime is honest about that. The dashboard's Gantt simulator panel covers RR for completeness. See `docs/technical-report.md` § 5.

---

## Tech Stack

- **Python 3.11**, managed by **uv** (lockfile-based, cross-platform reproducible)
- **FastAPI** + **Pydantic v2** + **sse-starlette**
- **Upstage Solar Pro 3** via the **openai** SDK (OpenAI-compatible endpoint)
- **Docker Engine** for sandbox isolation
- **threading** (worker pool, Lock, Condition) + **asyncio** (HTTP, SSE), bridged through `queue.Queue` + `call_soon_threadsafe`
- Vanilla HTML / CSS / JS dashboard (no build step), EventSource API
- **pytest** + **pytest-asyncio** for tests

---

## Repository Structure

```
GarbageCollector_OS/
├── app/                          # runtime modules
│   ├── agent.py                  # AgentTask + AgentState + AgentKind
│   ├── ready_queue.py            # synchronized ready queue (Condition)
│   ├── scheduler.py              # FCFS / Priority key functions
│   ├── simulator.py              # FCFS / Priority / RR Gantt simulator
│   ├── quota_manager.py          # mutex-guarded API quota
│   ├── message_bus.py            # bounded buffer for inter-agent IPC
│   ├── event_bus.py              # thread→asyncio bridge for SSE
│   ├── llm_client.py             # Solar Pro 3 wrapper
│   ├── sandbox.py                # docker-run hardened executor
│   ├── executor.py               # single-agent orchestration
│   ├── worker_pool.py            # N daemon worker threads
│   ├── logger.py                 # file event log
│   └── main.py                   # FastAPI app, SSE stream, lifespan
├── tests/                        # pytest suite (47 tests)
├── static/                       # SSE-driven dashboard (HTML/CSS/JS)
├── docker/
│   ├── Dockerfile                # app image (uv + docker CLI)
│   └── sandbox.Dockerfile        # sandbox runtime (non-root Python -I)
├── docker-compose.yml            # app + sandbox-build sidecar
├── docs/
│   ├── DEMO_GUIDE.md             # end-to-end demo walkthrough
│   ├── demo-script.md            # concise scenario reference
│   ├── technical-report.md       # report skeleton (OS concept mappings)
│   ├── development-process.md    # process / schedule / retro skeleton
│   └── superpowers/plans/2026-05-20-mini-agent-os.md   # implementation plan
├── pyproject.toml + uv.lock      # reproducible dependency pin
├── .env.example                  # UPSTAGE_API_KEY=...
└── .gitattributes                # LF enforcement (Windows + macOS)
```

---

## API Endpoints

| Method | Path | Purpose |
|---|---|---|
| `GET`    | `/` | Serve the dashboard |
| `POST`   | `/agents` | Create an agent (kind: `llm` or `code`, optional `pipe_to`) |
| `GET`    | `/agents` | List all agents |
| `DELETE` | `/agents` | Clear all agents + logs (reset state) |
| `GET`    | `/policy` | Current scheduling policy |
| `POST`   | `/policy` | Switch policy (`fcfs` or `priority`) |
| `GET`    | `/logs` | Read the trace log |
| `POST`   | `/simulate` | Compute a Gantt timeline for hypothetical jobs (FCFS / Priority / RR) |
| `GET`    | `/events` | Server-Sent Events stream of agent state changes |

OpenAPI / Swagger UI is available at <http://localhost:8000/docs>.

---

## Local Development

Without spinning up the full Docker stack for the app itself:

```bash
uv sync                                    # install deps
docker compose build sandbox-build         # build sandbox image only
uv run uvicorn app.main:app --reload       # hot-reload server

uv run pytest -m "not docker and not slow" # fast unit suite
uv run pytest                              # full suite (needs Docker)
```

You still need Docker for the sandbox image — code agents won't run without it.

---

## Documentation

| Document | Purpose |
|---|---|
| **[`docs/DEMO_GUIDE.md`](docs/DEMO_GUIDE.md)** | Step-by-step demo walkthrough with troubleshooting |
| [`docs/demo-script.md`](docs/demo-script.md) | Concise list of the 9 demo scenarios |
| [`docs/technical-report.md`](docs/technical-report.md) | Technical report skeleton (architecture, OS concept mappings, walkthroughs) |
| [`docs/development-process.md`](docs/development-process.md) | Schedule, weekly progress, issues, retrospective |
| [`docs/superpowers/plans/2026-05-20-mini-agent-os.md`](docs/superpowers/plans/2026-05-20-mini-agent-os.md) | The 18-task implementation plan this branch executed |
