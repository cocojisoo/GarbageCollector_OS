# GCOS Roadmap

GCOS should stay grounded in the original Mini Agent OS project: an
OS-inspired runtime that manages LLM agent tasks like processes. A conversational
assistant shell can exist, but it is not the main identity of the project.

For the Week 09 project requirement, the direction is:

- primary direction: OS-for-LLM, because GCOS manages LLM agents with process
  state, scheduling, quota, timeout, and execution logs
- supporting direction: LLM-for-OS, because the broker can explain or assist
  with host OS state through typed, policy-checked actions

## Core Design

```text
GCOS portable terminal TUI
  -> AgentRuntime process table
  -> FCFS / priority scheduler
  -> timeout and quota enforcement
  -> policy engine
  -> typed action broker
  -> optional LLM/API broker
```

The LLM should not live in kernel mode. The right split is:

- GCOS runtime: owns agent lifecycle, scheduling, timeout, quota, state, logs
- LLM broker: answers prompts or explains state
- policy gate: turns risky filesystem, shell, root, or kernel requests into
  blocked/approval states
- future kernel-adjacent layer: observes events and enforces narrow signed
  operations

## What "Kernel-Level AI" Means Here

It does not mean an LLM freely executes inside the kernel. It means a future
GCOS layer can see and influence kernel-relevant events through controlled
interfaces:

- process creation and exit
- file open/read/write attempts
- network connection attempts
- device events
- permission requests
- sandbox violations
- resource pressure

On Linux this maps to eBPF, LSM hooks, fanotify/inotify, auditd, cgroups, and a
small privileged daemon. On macOS the equivalent is more constrained and should
start with Endpoint Security, System Extensions, launch services, and local
automation permissions. This is a future extension, not the current core.

## Phases

1. Portable Mini Agent OS TUI
   - one console binary
   - OS Demo scenario
   - Agent=Process table
   - FCFS / priority scheduling
   - quota, timeout, and policy-blocked kernel requests

2. Typed action broker
   - no free-form root command execution
   - typed actions such as `read_file`, `summarize_process`, `list_processes`,
     `watch_folder`, `kill_process_with_confirmation`
   - persistent audit log

3. LLM broker integration
   - Upstage Solar Pro 3 API-key path
   - explicit Codex CLI mode only when requested
   - broker output treated as agent result, not as kernel authority

4. System observation
   - process table snapshot
   - file activity watcher
   - network activity summary
   - terminal session and process context

5. Kernel-adjacent enforcement
   - Linux eBPF/LSM prototype or macOS Endpoint Security prototype
   - policy decisions cached locally
   - LLM proposes policy, deterministic layer enforces it

6. AI OS distribution
   - bootable Linux image or VM
   - GCOS starts as the shell/session manager
   - kernel hooks stream events into the broker
   - high-risk actions require typed approval and rollback plans

## Non-Negotiables

- No arbitrary LLM-generated code in kernel mode.
- No raw OAuth token parsing in the runtime.
- No free-form root shell as the privileged interface.
- Every privileged action needs a typed operation, audit entry, and rollback
  story.
- The Mini Agent OS runtime must remain visible in the product and report.
