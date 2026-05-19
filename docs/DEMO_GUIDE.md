# Demo Guide — Mini Agent OS

This guide walks you end-to-end from a fresh clone to running every demo scenario. For a concise scenario reference, see [`demo-script.md`](demo-script.md).

---

## 1. Prerequisites

You need the following tools installed and on your `PATH`:

| Tool | Version | Verify |
|---|---|---|
| **Docker Desktop** (or Engine on Linux) | 20.10+ | `docker --version` |
| **docker compose** | v2 (built into Docker Desktop) | `docker compose version` |
| **uv** (Python package manager) | 0.5+ | `uv --version` |
| **git** | any modern version | `git --version` |
| A modern web browser | Chrome / Edge / Safari / Firefox | — |

> **Install uv** if you don't have it:
> - macOS / Linux: `curl -LsSf https://astral.sh/uv/install.sh | sh`
> - Windows (PowerShell): `powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"`

> **Start Docker Desktop** before running any demo step. The app spawns sandbox containers via the Docker socket, so the daemon must be running.

---

## 2. Get an Upstage Solar Pro 3 API key

1. Sign up at <https://console.upstage.ai/>
2. Create an API key.
3. Keep the key handy — you'll paste it into `.env` in step 4.

The `code` and `llm` agents will not run without a valid key. The dashboard and the scheduler simulator still work without a key (the simulator is pure math).

---

## 3. Clone and enter the repo

```bash
git clone https://github.com/cocojisoo/GarbageCollector_OS.git
cd GarbageCollector_OS
git checkout parkcheolwon
```

---

## 4. Configure `.env`

```bash
cp .env.example .env
```

Open `.env` and replace `replace_me` with your real key:

```
UPSTAGE_API_KEY=up_xxxxxxxxxxxxxxxxxxxx
```

Optional knobs you can tweak per scenario (defaults are fine for first run):

| Variable | Default | What it controls |
|---|---|---|
| `WORKER_COUNT` | `4` | Concurrent worker threads in the pool |
| `GLOBAL_QUOTA` | `1000` | Total LLM calls allowed before exhaustion |
| `MESSAGE_BUS_CAPACITY` | `100` | Per-topic bounded buffer size |
| `PIPELINE_WAIT_TIMEOUT` | `30` | Seconds a pipeline consumer waits for upstream |
| `SANDBOX_MEMORY` | `128m` | Memory cap for sandbox containers |
| `SANDBOX_CPUS` | `0.5` | CPU cap for sandbox containers |
| `SANDBOX_TIMEOUT_DEFAULT` | `5` | Default sandbox execution timeout (seconds) |

---

## 5. Build the Docker images

```bash
docker compose build
```

This builds two images:
- `mini-agent-os:latest` — the FastAPI app (Python 3.11 + uv + docker CLI)
- `mini-agent-os-sandbox:latest` — the hardened Python runtime that executes untrusted code

First-time build takes 1–3 minutes (downloads base images). Subsequent builds are cached.

Verify:

```bash
docker images | grep mini-agent-os
```

Both images should appear.

---

## 6. Start the app

```bash
docker compose up
```

Logs will stream to your terminal. Look for:

```
INFO:     Uvicorn running on http://0.0.0.0:8000 (Press CTRL+C to quit)
INFO:     Application startup complete.
```

Leave this terminal running. Open a second terminal for any further commands.

---

## 7. Open the dashboard

Open <http://localhost:8000> in your browser.

You should see:

- **Header** — "Mini Agent OS — Team GarbageCollector"
- **Scheduling policy** dropdown (FCFS / Priority)
- **Create agent** form
- **Agents** table (empty initially)
- **Logs** pane (empty initially)
- **Scheduler simulator** section at the bottom

**Important:** the dashboard subscribes to live state via SSE (`EventSource("/events")`). Always open the dashboard **before** creating agents — events fired before the dashboard connects are dropped (no replay buffer).

---

## 8. Run the demo scenarios

Each scenario takes 10–60 seconds. Run them in order or pick the ones you want to show.

> All nine scenarios are also listed concisely in [`demo-script.md`](demo-script.md). The detailed walkthroughs below add "what to point out" notes for a live audience.

### Scenario A — Concurrent execution (worker pool)

**Goal:** show that 4 worker threads run agents in parallel, not sequentially.

1. With `WORKER_COUNT=4` (default), open the dashboard.
2. Quickly create 4 LLM agents in a row. Suggested prompts:
   - `Tell me one fact about the Roman Empire.`
   - `Tell me one fact about the Renaissance.`
   - `Tell me one fact about World War I.`
   - `Tell me one fact about the Cold War.`
3. **Watch for:** all four rows turn **RUNNING** (yellow, pulsing) at roughly the same time, then transition to **DONE** (green) as their LLM calls return.
4. **What to point out:** the worker pool is genuinely concurrent — this is real `threading` parallelism, not asyncio cooperative scheduling. Each worker is blocked on a separate LLM HTTP call.

### Scenario B — FCFS scheduling

**Goal:** show ordering deterministically; needs one worker.

1. Edit `.env`: set `WORKER_COUNT=1`.
2. Restart: in the `docker compose up` terminal press `Ctrl+C`, then `docker compose up` again.
3. Reload the dashboard.
4. In the **Scheduling policy** dropdown choose **FCFS**.
5. Quickly create three LLM agents named `a`, `b`, `c` (in that order).
6. **Watch for:** rows turn RUNNING → DONE strictly in order `a → b → c`.
7. **What to point out:** since there is only one worker, the scheduler's choice is observable. FCFS picks the oldest READY agent.

### Scenario C — Priority scheduling (non-preemptive)

**Goal:** show priority overrides arrival order — but does not interrupt running agents.

1. Still single worker. Click **Clear all**.
2. Switch policy to **Priority (non-preemptive)**.
3. Create three agents:
   - name = `low`, priority = **1**, prompt = `Say "low".`
   - name = `mid`, priority = **5**, prompt = `Say "mid".`
   - name = `high`, priority = **9**, prompt = `Say "high".`
4. **Watch for:** order is `high → mid → low`, even though `low` was created first.
5. **What to point out:** the scheduler only chooses the **next** agent for a free worker. If a low-priority agent is already RUNNING when a high-priority one arrives, the low one runs to completion (no preemption — LLM calls are atomic).

### Scenario D — Sandboxed code execution

**Goal:** show LLM-generated code running inside an isolated Docker container.

1. Restore `WORKER_COUNT=4`, restart.
2. Create one agent:
   - kind = **code (sandboxed)**
   - timeout = **5**
   - prompt = `Write a Python program that prints the first 10 Fibonacci numbers, one per line.`
3. **Watch for:** state goes READY → RUNNING → **DONE**. The Result column shows two parts: the generated code, then the sandbox stdout. Expect `0 1 1 2 3 5 8 13 21 34` (or similar formatting).
4. **What to point out:** each `code` agent spawns its own Docker container with `--network=none --read-only --cap-drop=ALL --memory=128m --cpus=0.5`. The container is destroyed (`--rm`) after each run.

### Scenario E — Timeout (sandbox kill)

**Goal:** show the parent-side timeout killing an unresponsive sandbox.

1. Create a `code` agent:
   - timeout = **2**
   - prompt = `Write Python that calls time.sleep(60) and then prints "done".`
2. **Watch for:** state becomes **TIMEOUT** within ~2 seconds. The Result column shows the error message "Sandbox execution timed out".
3. **What to point out:** `subprocess.run(..., timeout=2)` raises `TimeoutExpired`, which causes Docker's `--rm` cleanup to terminate the orphaned container. This is the OS analog of process termination on burst overrun.

### Scenario F — Quota exhaustion (mutex synchronization)

**Goal:** show the QuotaManager mutex enforcing exact-N successes under concurrency.

1. Edit `.env`: set `GLOBAL_QUOTA=2` and `WORKER_COUNT=4`. Restart.
2. Reload dashboard.
3. Create **5** LLM agents quickly (any prompts).
4. **Watch for:** the first two reach **DONE**; the remaining three become **ERROR** with `API quota exhausted` in the Result column.
5. **What to point out:** even though 4 workers race for the quota concurrently, the count is exactly 2 — `threading.Lock` guarantees the read-modify-write of `_used` is atomic. Without the mutex you would see anywhere from 2 to 5 successes.

### Scenario G — Network isolation (system-call restriction)

**Goal:** show the sandbox blocks network egress at the container namespace boundary.

1. Restore `GLOBAL_QUOTA=1000`, restart.
2. Create a `code` agent:
   - timeout = **8**
   - prompt = `Write Python that uses urllib.request to fetch https://example.com and prints the HTTP status. Catch and print any exception class name on failure.`
3. **Watch for:** state goes DONE; the Result stdout includes an exception name like `URLError` or `gaierror` — not a status code.
4. **What to point out:** `--network=none` puts the container in a network namespace with no interfaces. The Python `socket` syscall fails immediately because there's no route to anywhere. The host has internet, the container does not.

### Scenario H — Pipeline (inter-agent IPC via bounded MessageBus)

**Goal:** show two agents wired together via a bounded queue (`pipe-{id}` topic).

1. With defaults restored, click **Clear all** so agent IDs start at 1.
2. Create agent **#1** (it will get `agent_id = 1`):
   - name = `gen`
   - kind = **llm**
   - prompt = `List 3 interesting facts about pelicans.`
   - **pipe_to = 2** (the id we'll assign to the next agent)
3. *Immediately* create agent **#2** (gets `agent_id = 2`):
   - name = `summary`
   - kind = **llm**
   - prompt = `Summarize the following into a single sentence: {INPUT}`
   - leave **pipe_to** blank
4. **Watch for:**
   - Agent #1 (`gen`) runs and reaches DONE with the pelican facts in Result.
   - Agent #2 (`summary`) goes RUNNING immediately but blocks on `bus.receive("pipe-2")` until #1 finishes.
   - The moment #1 publishes its result to `pipe-2`, agent #2's `{INPUT}` placeholder is substituted and the LLM is called with the substituted prompt.
   - Agent #2 reaches DONE with a one-sentence summary.
5. **What to point out:** the `MessageBus` is a `queue.Queue(maxsize=100)` — classic bounded buffer. The downstream worker is blocked (not polling) until the upstream sends. If you started agent #2 first and #1 never came, #2 would ERROR after `PIPELINE_WAIT_TIMEOUT` seconds (default 30).

### Scenario I — Round Robin scheduling (simulator)

**Goal:** show preemptive RR via the Gantt simulator (live runtime can't preempt LLM calls).

1. Scroll to the **Scheduler simulator (Gantt)** section.
2. Replace the textarea content with:

   ```
   A 8 0 3
   B 4 0 1
   C 9 0 2
   D 5 0 4
   ```

   (Format: `id burst arrival priority`, one per line.)

3. Choose **Round Robin**, quantum = **3**, click **Run simulation**.
4. **Watch for:** a colored Gantt bar chart appears below, showing repeated slices: `A(0→3) B(3→6) C(6→9) D(9→12) A(12→15) B(15→16) C(16→19) D(19→21) A(21→23) C(23→26)` (or similar). Each slice is one quantum or the remaining burst, whichever is smaller.
5. Switch policy to **FCFS** and click **Run simulation** again — observe the ordering changes: `A(0→8) B(8→12) C(12→21) D(21→26)`.
6. Switch policy to **Priority** — order becomes `D(0→5) A(5→13) C(13→22) B(22→26)`.
7. **What to point out:** these are textbook Gantt charts, computed by pure functions in `app/simulator.py`. The split between simulator and live runtime is intentional — the live runtime cannot preempt an in-flight LLM call.

---

## 9. Verify tests pass

In a separate terminal, from the repo root:

```bash
uv run pytest -m "not docker and not slow"   # fast unit tests
uv run pytest                                # full suite incl. Docker tests
```

Expected: **47 passed**.

---

## 10. Stopping

In the `docker compose up` terminal press `Ctrl+C`, then:

```bash
docker compose down
```

To also remove the built images:

```bash
docker compose down --rmi local
docker image rm mini-agent-os-sandbox:latest
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Cannot connect to Docker daemon` | Docker Desktop not running | Start Docker Desktop, wait for the whale icon to settle |
| `docker: Error response from daemon: ... mini-agent-os-sandbox:latest: not found` | Sandbox image missing | `docker compose build sandbox-build` |
| Agent reaches ERROR with "LLM error: 401" | Invalid `UPSTAGE_API_KEY` | Check `.env` — no quotes around the key |
| Agent stays RUNNING forever after Quick demo | EventBus dropped events before dashboard connected | Reload the page after the worker started |
| `pip install`-style error on `uv sync` | `uv` not on PATH | Reinstall uv (see Prerequisites) |
| `docker compose up` fails with `bind: address already in use` | Port 8000 in use | Stop the conflicting process or change `ports` in `docker-compose.yml` |
| Pipeline downstream agent never completes | Upstream not started, or `pipe_to` id wrong | Verify the upstream agent id matches downstream's `pipe_to` |
| Windows: line-ending warnings on git commit | LF/CRLF translation | `.gitattributes` already enforces LF — warnings are informational |

---

## What to record (for slides / submission)

While running the demo, capture:

1. A screenshot of the dashboard mid-Scenario A (multiple agents RUNNING) — proves concurrency.
2. A screenshot from Scenario D showing generated code + stdout in the Result column — proves real sandbox execution.
3. A terminal screenshot from `docker compose up` showing the log lines for one full agent lifecycle.
4. A screenshot of the Gantt chart from Scenario I for each of FCFS, Priority, RR — visually compelling for the slides.
5. The output of `uv run pytest` showing 47 passed.

These five artifacts cover concurrency, sandbox isolation, OS state transitions, the three scheduling policies, and test coverage — the headline claims of the project.
