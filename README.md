# GarbageCollector OS

### Portable TUI Mini Agent Operating System for Scheduling LLM Tasks

GCOS implements the `cocojisoo/GarbageCollector_OS` Mini Agent OS brief as a
terminal program instead of a GUI app. The upstream idea is preserved: treat
each LLM-style agent task like an operating-system process and manage it with
an Agent Control Block, ready queue, FCFS scheduling, priority scheduling,
timeout handling, API-call quota simulation, execution logs, and a dashboard.

This is not a real kernel-level operating system. It is a user-level runtime
that demonstrates OS scheduling concepts.

## What It Implements From Upstream

| Upstream file / requirement | Current implementation |
| --- | --- |
| `agent.py` / AgentTask | `Agent` struct in `include/gcos.h` |
| Agent Control Block fields | id, name, prompt, priority, timeout, quota, state, result, error, timestamps/logs |
| `scheduler.py` / FCFS | `runtime_run_fcfs` in `src/agent.c` |
| `scheduler.py` / Priority | `runtime_run_priority` in `src/agent.c` |
| `executor.py` / execution | `agent_execute` in `src/agent.c` |
| `[CALL]` API quota simulation | prompt marker counting in `src/agent.c` |
| `[SLOW]` timeout simulation | timeout branch in `src/agent.c` |
| `logger.py` | runtime log ring buffer |
| dashboard/API surface | terminal TUI in `src/tui.c` |

## Build And Run

macOS/Linux:

```sh
make
make run
```

Windows with MinGW/MSYS2:

```sh
mingw32-make CC=gcc EXEEXT=.exe
build\\gcos-tui.exe
```

There is no `.app`, SDL, Pango, Cairo, browser dashboard, or LaunchServices
wrapper. The program is a normal console binary.
The Windows path is MinGW/MSYS2-targeted; native Windows build verification is
still pending.

## TUI Commands

```text
create          create an Agent Control Block
list            show the process/agent table
run fcfs        run READY agents by creation order
run priority    run READY agents by high priority first
demo            load sample scheduling/quota/timeout agents
logs            show execution logs
clear           clear all agents and logs
help            show commands
quit            exit
```

Simulation markers:

```text
[CALL]  counts as one simulated API call; no [CALL] means one call
[SLOW]  forces execution time beyond timeout
```

Plain TUI mode does not spend LLM/API tokens. Raw `[CODEX:...]`, `llm:`, and
`ask:` inputs are blocked in the TUI local simulation path.

## OS Concept Mapping

| Operating System Concept | Mini Agent OS Concept |
| --- | --- |
| Process | Agent task |
| PID | Agent ID |
| PCB | Agent Control Block |
| Ready Queue | `AgentRuntime.agents[]` filtered by `READY` |
| Scheduler | FCFS and Priority Scheduling |
| CPU Burst | simulated execution time |
| Resource Limit | API-call quota |
| Process Timeout | timeout state |
| Process State | `READY`, `RUNNING`, `DONE`, `TIMEOUT`, `ERROR` |
| Trace Log | runtime execution log |
| Process Table | TUI agent table |

The code also keeps a small policy layer for typed local actions, but the core
project demo follows the upstream simulation model.

## Verification

```sh
make check
make portable-check
```

`make check` verifies self-test, OS demo behavior, TUI scheduling, input safety,
wrapped terminal output, and offline API config parsing. `make portable-check`
syntax-checks the C sources without SDL/app dependencies.

Live broker checks are optional and not part of normal TUI use:

```sh
make codex-check
make api-check
```
