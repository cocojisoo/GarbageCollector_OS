# Mini Agent OS

Team GarbageCollector · Week 09 Project · Direction A: Operating System for LLM

Mini Agent OS is an OS-inspired user-level runtime that manages LLM-style agent
tasks like processes. Each agent is treated as a process with its own Agent
Control Block, state, priority, timeout, quota, result, error message, and
execution log.

The current implementation is a portable terminal TUI. It intentionally avoids
GUI app packaging so the core project can run as a normal console program while
still demonstrating the runtime/kernel side of the assignment: scheduling,
state transitions, quota checks, timeout handling, policy gates, logs, and LLM
execution through an API-key broker.

## Branch Scope

This branch uses the presentation structure of a full Mini Agent OS project, but
the content below describes only this repository's C11 terminal implementation.

Implemented in this branch:

- C11 terminal TUI runtime.
- Agent table modeled as a process table.
- FCFS and priority scheduling.
- Round-robin scheduling for local time-slice simulation.
- READY/RUNNING/DONE/TIMEOUT/ERROR state transitions.
- Startup API-key input for real LLM-backed agent execution.
- `[CALL]` quota accounting for LLM/API usage.
- `[SLOW]` timeout handling.
- Local execution logs and smoke tests.
- Policy checks for typed local actions such as `[SHELL:...]`, `[READ:...]`,
  `[ROOT:...]`, `[KERNEL:...]`, and `[CODEX:...]`.

Out of scope for this C TUI branch:

- Docker sandbox execution.
- Browser/SSE dashboard.
- Threaded worker pool.
- IPC pipeline or bounded message bus.

## Highlights

- Agent tasks are managed like OS processes.
- Each agent has an Agent Control Block with id, state, priority, timeout,
  quota, timestamps, result, and error fields.
- READY agents are selected from the runtime table as a ready queue.
- Three scheduling policies are implemented: FCFS, Priority, and Round Robin.
- Agent state transitions are visible: READY -> RUNNING -> DONE, TIMEOUT, or
  ERROR.
- The TUI asks for an Upstage/OpenAI-compatible API key on startup.
- When a key is entered or already configured, ordinary agent prompts call the
  LLM broker and store the model answer as the agent result.
- Pressing Enter at the API-key prompt keeps an offline scheduler-test mode for
  smoke tests and classroom dry runs.
- `[CALL]` markers account for API-call quota usage.
- `[SLOW]` markers simulate timeout behavior.
- A runtime log records creation, scheduling, execution, timeout, quota errors,
  LLM broker calls, and policy blocks.
- The terminal TUI acts as the dashboard: it displays the process table, logs,
  scheduler output, and demo scenario results.
- The code is written in C11 with a small Makefile build surface and no SDL,
  Pango, Cairo, browser server, or `.app` bundle.

## Architecture

```text
                    User Terminal
                         |
                         v
              +---------------------+
              |      TUI Loop       |
              |  create/list/run    |
              |  logs/demo/help     |
              +----------+----------+
                         |
                         v
              +---------------------+
              |   AgentRuntime      |
              | process table       |
              | log ring buffer     |
              | llm_enabled flag    |
              | next agent id       |
              +----------+----------+
                         |
             READY agents are selected
                         |
          +--------------+--------------+--------------+
          |                             |              |
          v                             v              v
 +-----------------+           +------------------+  +------------------+
 | FCFS Scheduler  |           | Priority Scheduler|  | RR Scheduler     |
 | id ascending    |           | priority desc     |  | quantum=1 slice  |
 +--------+--------+           +---------+---------+  +--------+---------+
          |                              |                     |
          +--------------+---------------+---------------------+
                         |
                         v
              +---------------------+
              |      Executor       |
              | state transition    |
              | quota check         |
              | timeout simulation  |
              | policy gate         |
              | LLM broker call     |
              +----------+----------+
                         |
                         v
              +---------------------------+
              | Result / Error / Log      |
              | model output when enabled |
              +---------------------------+
```

## Quick Start

### macOS / Linux

```sh
git clone https://github.com/cocojisoo/GarbageCollector_OS.git
cd GarbageCollector_OS
git checkout codex/tui-mini-agent-os

make
make run
# paste API key at startup, or press Enter for offline scheduler tests
```

### Windows with MinGW/MSYS2

```sh
mingw32-make CC=gcc EXEEXT=.exe
build\\gcos-tui.exe
```

The Windows path targets MinGW/MSYS2 and uses the same console/TUI flow. The API
broker expects a curl-compatible environment.

## TUI Commands

Run `make run` and type commands at the `gcos>` prompt.

| Command | Purpose |
| --- | --- |
| `create` | Create a new agent task interactively |
| `list` | Show the current process/agent table |
| `run fcfs` | Run READY agents in creation order |
| `run priority` | Run READY agents by priority, high value first |
| `run rr` | Run READY agents with round-robin quantum scheduling |
| `demo` | Load sample agents for scheduling, quota, and timeout demos |
| `logs` | Show execution logs |
| `clear` | Clear all agents and logs |
| `help` | Show command help |
| `quit` | Exit the TUI |

## Demo Scenarios

### 1. Priority Scheduling Demo

```text
gcos> demo
gcos> run priority
```

The demo creates four agents:

| Agent | Priority | Prompt | Expected Result |
| --- | ---: | --- | --- |
| AgentA | 3 | `Short summary task [CALL]` | DONE |
| AgentB | 9 | `Urgent analysis task [CALL]` | DONE first |
| AgentC | 5 | `Quota pressure [CALL] [CALL] [CALL]` | ERROR |
| SlowOne | 4 | `Timeout demo [SLOW]` | TIMEOUT |

Because priority scheduling runs the highest priority first, AgentB runs before
AgentC, SlowOne, and AgentA.

### 2. FCFS Scheduling Demo

```text
gcos> clear
gcos> demo
gcos> run fcfs
```

FCFS executes agents by creation order, so AgentA runs before AgentB even though
AgentB has a higher priority.

### 3. Round-Robin Scheduling Demo

```text
gcos> clear
gcos> demo
gcos> run rr
```

Round Robin gives each READY agent one scheduling quantum at a time. In offline
mode, each `[CALL]` marker is treated as one unit of remaining work, so a
multi-call agent yields back to READY until its units are complete. If LLM mode
is enabled, the scheduler still chooses the next agent by RR order, then the
LLM HTTP call runs atomically for that dispatch.

### 4. LLM-Backed Agent Demo

```text
api key: up_... or sk-...
gcos> create
name: Explainer
prompt: llm: explain FCFS scheduling in two Korean sentences
priority 1-10 [5]: 5
timeout seconds 1-30 [3]: 10
quota 1-20 [2]: 1
gcos> run priority
```

The executor records the agent as RUNNING, consumes quota, calls the configured
LLM broker, then stores the model response in `result`.

### 5. Quota Error Demo

AgentC contains three `[CALL]` markers but has quota 2. The runtime detects that
required calls exceed the quota and moves AgentC to ERROR.

```text
quota exceeded: required=3 quota=2
```

### 6. Timeout Demo

SlowOne contains `[SLOW]`. The executor simulates work that exceeds the timeout
and moves the agent to TIMEOUT.

```text
timeout: required=2 timeout=1
```

### 7. Log Demo

```text
gcos> logs
```

The log shows runtime events such as agent creation, scheduler start, agent
start, LLM broker call, completion, quota error, timeout, and scheduler finish.

## Operating System Concepts -> Code

| OS Concept | File | Implementation |
| --- | --- | --- |
| Process | `include/gcos.h` | `Agent` struct |
| PID | `include/gcos.h` | `Agent.id` |
| PCB / ACB | `include/gcos.h` | state, priority, timeout, quota, timestamps, result, error |
| Process State | `include/gcos.h` / `src/agent.c` | READY, RUNNING, DONE, TIMEOUT, ERROR |
| Ready Queue | `src/agent.c` | READY agents are copied into a scheduler input array |
| FCFS Scheduling | `src/agent.c` | `runtime_run_fcfs`, sorted by creation id |
| Priority Scheduling | `src/agent.c` | `runtime_run_priority`, sorted by priority then id |
| Round-Robin Scheduling | `src/agent.c` | `runtime_run_round_robin`, quantum=1 local time-slice simulation |
| CPU Burst Simulation | `src/agent.c` | normal prompt runs for one simulated second |
| Timeout | `src/agent.c` | `[SLOW]` forces timeout path |
| Resource Quota | `src/agent.c` | `[CALL]` count checked against `Agent.quota` |
| LLM Broker | `src/agent.c` + `src/codex_broker.c` | TUI startup API key enables model calls during execution |
| Trace Log | `src/agent.c` | `runtime_log` ring buffer |
| Dashboard | `src/tui.c` | process table, logs, help, demo output |
| Policy Gate | `src/policy.c` | typed action parser and sensitive action blocking |

## Technical Stack

- C11
- Makefile build
- Terminal TUI using standard input/output
- POSIX-compatible code path for macOS/Linux
- MinGW/MSYS2-targeted Windows code path
- Upstage/OpenAI-compatible API broker through `curl`
- Startup API key input; keys are kept in process environment and not written to
  generated files
- No GUI framework
- No browser dashboard
- No generated API key files

## Repository Structure

```text
GarbageCollector_OS/
├── include/
│   └── gcos.h              # shared structs, enums, function declarations
├── src/
│   ├── main.c              # program entry point and smoke-test flags
│   ├── tui.c               # terminal UI, API-key setup, command loop
│   ├── agent.c             # agent table, schedulers, executor, logs
│   ├── policy.c            # action marker parser and safety policy
│   └── codex_broker.c      # Upstage/OpenAI-compatible LLM API broker
├── docs/
│   ├── dev-brief.md
│   ├── project-alignment.md
│   ├── tui-architecture.md
│   └── gcos-roadmap.md
├── scripts/
│   └── gcos-context        # quick project summary
├── Makefile
├── README.md
├── .gitignore
└── .ignore
```

## Build Targets

| Target | Purpose |
| --- | --- |
| `make` | Build `build/gcos-tui` |
| `make run` | Build and start the TUI |
| `make check` | Run local smoke tests |
| `make portable-check` | Syntax-check all C sources |
| `make context` | Print a short project summary |
| `make clean` | Remove build outputs |

## Test Coverage

`make check` currently runs:

| Test Flag | What It Checks |
| --- | --- |
| `--self-test` | policy guard, sensitive paths, invalid broker backend |
| `--os-demo-smoke` | priority order, quota error, kernel policy block |
| `--tui-smoke` | priority scheduling states for sample agents |
| `--rr-smoke` | round-robin scheduling, yield, quota behavior |
| `--input-smoke` | TUI input path without live API calls |
| `--wrap-smoke` | long text setup for terminal wrapping |
| `--api-config-smoke` | offline API config parsing without real API calls |

## Notes for Presentation

When presenting this project, the main point is not that it is "just running on
an OS." The point is that the project maps OS concepts into individually
testable runtime components:

- agent as process
- id as PID
- Agent Control Block as PCB
- runtime table as process table
- READY filtering as ready queue
- FCFS, priority, and round-robin scheduling as scheduler policies
- `[CALL]` quota as resource management
- `[SLOW]` timeout as process timeout
- startup API key plus broker call as LLM execution path
- runtime logs as trace logs
- TUI output as dashboard/process table

The core assignment demo should focus on scheduling, state transitions, quota,
timeout, LLM execution, and logs.
