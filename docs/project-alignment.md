# Project Alignment

This document pins GCOS to the original `cocojisoo/GarbageCollector_OS`
README while respecting the current TUI requirement.

## Source Of Truth

The upstream project says Mini Agent OS is an OS-inspired user-level runtime
that manages multiple LLM agent tasks like processes in an operating system.
The implementation here keeps that exact product identity.

## Upstream README Mapping

| Upstream requirement | Current implementation |
| --- | --- |
| Create and manage multiple LLM agent tasks | `runtime_create_agent` |
| Store agents in a ready queue | `AgentRuntime.agents[]` filtered by `READY` |
| FCFS scheduling | `runtime_run_fcfs` |
| Priority scheduling | `runtime_run_priority` |
| Track states | `AgentState` |
| Timeout handling | `[SLOW]` branch in `agent_execute` |
| Resource quota | `[CALL]` counting in `agent_execute` |
| Execution logs | `runtime_log` ring buffer |
| Dashboard | terminal TUI in `src/tui.c` |

## Demo Contract

1. Run `make run`.
2. Type `demo`.
3. Type `run priority` or `run fcfs`.
4. Show agent table states, quota error, timeout behavior, and logs.
5. Explain the OS mapping: Agent=Process, ID=PID, table=PCB/ready queue,
   scheduler=FCFS/Priority, quota=resource limit, timeout=process timeout.

## What Not To Claim

- Do not claim this is a real kernel-level OS.
- Do not claim the TUI calls a real LLM during normal local simulation.
- Do not reintroduce a GUI/app bundle as the primary interface.
