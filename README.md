# Mini Agent OS

Team GarbageCollector · Week 09 Project · Direction A: Operating System for LLM

Mini Agent OS is an OS-inspired user-level runtime that manages LLM-style agent
tasks like processes. Each agent is treated as a process with its own Agent
Control Block, state, priority, timeout, quota, result, error message, and
execution log.

The current implementation is a portable terminal TUI. It intentionally avoids
GUI app packaging so the core project can run as a normal console program on
macOS, Linux, and MinGW/MSYS2-based Windows environments.

This is not a real kernel-level operating system. It is a runtime simulation
that demonstrates operating-system concepts through agent scheduling.

## Branch Scope

This branch uses the presentation structure of a full Mini Agent OS project, but
the content below describes only this repository's C11 terminal implementation.

Implemented in this branch:

- C11 terminal TUI runtime.
- Agent table modeled as a process table.
- FCFS and priority scheduling.
- READY/RUNNING/DONE/TIMEOUT/ERROR state transitions.
- `[CALL]` quota simulation.
- `[SLOW]` timeout simulation.
- Local execution logs and smoke tests.
- Plain TUI safety checks that reject external broker prompts.

Not claimed by this branch:

- Docker sandbox execution.
- Browser/SSE dashboard.
- Threaded worker pool.
- IPC pipeline or bounded message bus.
- Runtime round-robin scheduling.
- Real LLM calls during normal TUI demos.

## Highlights

- Agent tasks are managed like OS processes.
- Each agent has an Agent Control Block with id, state, priority, timeout,
  quota, timestamps, result, and error fields.
- READY agents are selected from the runtime table as a ready queue.
- Two non-preemptive scheduling policies are implemented: FCFS and Priority.
- Agent state transitions are visible: READY -> RUNNING -> DONE, TIMEOUT, or
  ERROR.
- `[CALL]` markers simulate API-call resource usage.
- `[SLOW]` markers simulate timeout behavior.
- A runtime log records creation, scheduling, execution, timeout, quota errors,
  and policy blocks.
- The terminal TUI acts as the dashboard: it displays the process table, logs,
  scheduler output, and demo scenario results.
- Plain TUI mode is local simulation only. `llm:`, `ask:`, and raw
  `[CODEX:...]` prompts are blocked so normal demos do not spend API tokens.
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
              | next agent id       |
              +----------+----------+
                         |
             READY agents are selected
                         |
          +--------------+--------------+
          |                             |
          v                             v
 +-----------------+           +------------------+
 | FCFS Scheduler  |           | Priority Scheduler|
 | id ascending    |           | priority desc     |
 +--------+--------+           +---------+---------+
          |                              |
          +--------------+---------------+
                         |
                         v
              +---------------------+
              |      Executor       |
              | state transition    |
              | quota check         |
              | timeout simulation  |
              | policy gate         |
              +----------+----------+
                         |
                         v
              +---------------------+
              | Result / Error / Log|
              +---------------------+
```

## Quick Start

### macOS / Linux

```sh
git clone https://github.com/cocojisoo/GarbageCollector_OS.git
cd GarbageCollector_OS
git checkout codex/tui-mini-agent-os

make
make run
```

### Windows with MinGW/MSYS2

```sh
mingw32-make CC=gcc EXEEXT=.exe
build\\gcos-tui.exe
```

The Windows path is designed for MinGW/MSYS2. Native Windows build verification
has not been completed yet.

## TUI Commands

Run `make run` and type commands at the `gcos>` prompt.

| Command | Purpose |
| --- | --- |
| `create` | Create a new agent task interactively |
| `list` | Show the current process/agent table |
| `run fcfs` | Run READY agents in creation order |
| `run priority` | Run READY agents by priority, high value first |
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

### 3. Quota Error Demo

AgentC contains three `[CALL]` markers but has quota 2. The runtime detects that
required calls exceed the quota and moves AgentC to ERROR.

```text
quota exceeded: required=3 quota=2
```

### 4. Timeout Demo

SlowOne contains `[SLOW]`. The executor simulates work that exceeds the timeout
and moves the agent to TIMEOUT.

```text
timeout: required=2 timeout=1
```

### 5. Log Demo

```text
gcos> logs
```

The log shows runtime events such as agent creation, scheduler start, agent
start, completion, quota error, timeout, and scheduler finish.

### 6. Local-Only Safety Demo

Plain TUI mode blocks external broker prompts.

```text
gcos> create
prompt: [CODEX:explain this]
```

The TUI rejects the prompt and does not create an agent. This keeps normal demos
offline and token-free.

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
| CPU Burst Simulation | `src/agent.c` | normal prompt runs for one simulated second |
| Timeout | `src/agent.c` | `[SLOW]` forces timeout path |
| Resource Quota | `src/agent.c` | `[CALL]` count checked against `Agent.quota` |
| Trace Log | `src/agent.c` | `runtime_log` ring buffer |
| Dashboard | `src/tui.c` | process table, logs, help, demo output |
| Policy Gate | `src/policy.c` | typed action parser and sensitive action blocking |

## Technical Stack

- C11
- Makefile build
- Terminal TUI using standard input/output
- POSIX-compatible code path for macOS/Linux
- MinGW/MSYS2-targeted Windows code path
- Optional external broker code path kept outside normal TUI use
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
│   ├── tui.c               # terminal UI, command loop, input safety tests
│   ├── agent.c             # agent table, schedulers, executor, logs
│   ├── policy.c            # action marker parser and safety policy
│   └── codex_broker.c      # optional non-TUI broker path
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
| `--input-smoke` | local-only TUI input and `[CODEX:...]` rejection |
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
- FCFS and priority scheduling as scheduler policies
- `[CALL]` quota as resource management
- `[SLOW]` timeout as process timeout
- runtime logs as trace logs
- TUI output as dashboard/process table

The policy and broker files are secondary extensions. The core assignment demo
should focus on scheduling, state transitions, quota, timeout, and logs.

## Limitations

- This is not a kernel and does not replace an operating system.
- The runtime uses non-preemptive scheduling.
- Round-robin scheduling is not implemented in the current TUI runtime.
- Normal TUI use does not call a real LLM.
- Native Windows execution has not been verified yet; the intended Windows path
  is MinGW/MSYS2.
- The optional broker path is outside the normal local TUI demo.
