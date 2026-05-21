# GCOS Dev Brief

Use this file as the first context read.

## Core Shape

- Portable C terminal TUI, not a macOS GUI app.
- TUI and dashboard commands: `src/tui.c`
- Runtime/scheduler/quota/timeout/state: `src/agent.c`
- Policy/action markers: `src/policy.c`
- Optional API/Codex bridge: `src/codex_broker.c`
- Shared contracts: `include/gcos.h`

## Upstream Contract

Implement the original `cocojisoo/GarbageCollector_OS` Mini Agent OS brief:

- create/manage LLM-style agent tasks
- keep agents in a ready queue/process table
- run FCFS and priority scheduling
- track READY/RUNNING/DONE/TIMEOUT/ERROR
- simulate API quota with `[CALL]`
- simulate timeout with `[SLOW]`
- record execution logs
- show the process table through a simple dashboard, now TUI

## Token Rule

The TUI is local simulation only. It must not spend external LLM/API tokens for
ordinary use. Raw `[CODEX:...]`, `llm:`, and `ask:` are blocked in TUI mode.

## Fast Commands

```sh
make context
make check
make portable-check
make run
```
