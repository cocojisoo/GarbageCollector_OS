# GCOS Dev Brief

Use this file as the first context read.

## Core Shape

- Portable C terminal TUI, not a macOS GUI app.
- TUI and dashboard commands: `src/tui.c`
- Runtime/scheduler/quota/timeout/state: `src/agent.c`
- Policy/action markers: `src/policy.c`
- Upstage/OpenAI-compatible LLM broker: `src/codex_broker.c`
- Shared contracts: `include/gcos.h`

## Upstream Contract

Implement the original `cocojisoo/GarbageCollector_OS` Mini Agent OS brief:

- create/manage LLM-style agent tasks
- keep agents in a ready queue/process table
- run FCFS, priority, and round-robin scheduling
- track READY/RUNNING/DONE/TIMEOUT/ERROR
- account API quota with `[CALL]`
- simulate timeout with `[SLOW]`
- record execution logs
- show the process table through a simple dashboard, now TUI

## LLM Execution Rule

The TUI asks for an API key at startup. If a key is entered or already present
in `GCOS_LLM_API_KEY`, `UPSTAGE_API_KEY`, or `OPENAI_API_KEY`, ordinary agents
execute through the LLM broker and store the model output in the agent result.
Pressing Enter at startup keeps an offline scheduler-test mode for smoke tests.

## Fast Commands

```sh
make context
make check
make portable-check
make run
```
