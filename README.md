# 🧠 Mini Agent OS

### Design and Implementation of a Mini Agent Operating System for Scheduling LLM Tasks

> **Mini Agent OS** is an OS-inspired user-level runtime system that manages multiple LLM agent tasks like processes in an operating system.

---

## 📌 Project Overview

Mini Agent OS는 여러 개의 LLM Agent Task를 운영체제의 process처럼 관리하는 mini runtime system이다.

일반적인 Operating System은 process를 생성하고, ready queue에 넣은 뒤, scheduler를 통해 실행 순서를 결정한다.  
이 프로젝트는 이러한 OS 개념을 LLM agent 환경에 적용하여, agent 생성, queue 관리, scheduling, timeout handling, resource quota, execution log를 구현한다.

This project does **not** implement a real kernel-level operating system.  
Instead, it simulates core operating system concepts at the application level.

---

## 🎯 Project Goal

The goal of this project is to design and implement a small runtime system that can:

- Create and manage multiple LLM agent tasks
- Store agents in a ready queue
- Schedule agents using FCFS and Priority Scheduling
- Track agent states such as READY, RUNNING, DONE, TIMEOUT, and ERROR
- Limit execution time using timeout handling
- Simulate API quota as a resource management mechanism
- Record execution logs
- Display runtime information through a simple dashboard

---

## 💡 Motivation

Recently, LLM-based agents are increasingly used for tasks such as summarization, code generation, analysis, and decision making.

However, when multiple agents exist at the same time, we need a system that can decide:

- Which agent should run first?
- How long should an agent be allowed to run?
- How much resource can each agent use?
- How can we track the execution result?

이 문제를 운영체제 관점에서 바라보면, LLM agent는 하나의 process처럼 생각할 수 있다.  
따라서 이 프로젝트는 LLM agent를 process처럼 관리하는 **Mini Agent Operating System**을 구현한다.

---

## 🧩 Main Concept

The main idea is simple:

> Treat each LLM agent as a process and manage it using OS scheduling concepts.

Each agent has its own ID, state, priority, timeout, quota, execution result, and log information.

The Mini Agent OS stores agents in a queue and executes them according to the selected scheduling policy.

---

## 🖥️ OS Concept Mapping

| Operating System Concept | Mini Agent OS Concept |
|---|---|
| Process | Agent Task |
| PID | Agent ID |
| PCB | Agent Control Block |
| Ready Queue | Agent Queue |
| Scheduler | Agent Scheduler |
| CPU Burst | Agent Execution Time |
| FCFS Scheduling | First-created agent runs first |
| Priority Scheduling | Higher-priority agent runs first |
| Resource Limit | API Call Quota |
| Process Timeout | Agent Timeout |
| Process State | Agent State |
| Trace Log | Execution Log |
| Process Table | Dashboard Agent Table |

---

## ⚙️ Functional Specification

### 1. Agent Creation

사용자는 새로운 agent task를 생성할 수 있다.

Each agent contains the following information:

- Agent ID
- Agent name
- Prompt
- Priority
- Timeout
- API quota
- Current state
- Execution result
- Error message

When an agent is created, it is inserted into the ready queue with the `READY` state.

---

### 2. Agent State Management

Each agent has one of the following states:

| State | Description |
|---|---|
| `READY` | Agent is waiting in the queue |
| `RUNNING` | Agent is currently executing |
| `DONE` | Agent completed successfully |
| `TIMEOUT` | Agent exceeded its time limit |
| `ERROR` | Agent failed during execution |

State transition example:

```text
READY → RUNNING → DONE
READY → RUNNING → TIMEOUT
READY → RUNNING → ERROR
```

---

### 3. Agent Queue

The Mini Agent OS maintains an agent queue similar to the ready queue in an operating system.

새로 생성된 agent는 queue에 들어가고, scheduler는 이 queue에서 다음에 실행할 agent를 선택한다.

---
### 4. FCFS Scheduling

FCFS stands for First-Come, First-Served.

In FCFS scheduling, agents are executed in the order they were created.

```text
Created Order:
Agent A → Agent B → Agent C

Execution Order:
Agent A → Agent B → Agent C
```
#### Advantages
- Simple implementation
- Fair based on arrival order
- Easy to understand

#### Disadvantages
- A long-running agent can delay all later agents
- Not suitable when urgent tasks exist
---
### 5. Priority Scheduling
In Priority Scheduling, agents with higher priority values are executed first.

```text
Agent A: priority 3
Agent B: priority 9
Agent C: priority 5

Execution Order:
Agent B → Agent C → Agent A
```
#### Advantages
- Important agents can run earlier
- More flexible than FCFS

#### Disadvantages
- Low-priority agents may wait too long
- Starvation can occur
---

### Timeout Handling
Each agent has a timeout value.
If the agent does not finish within the given time limit, its state becomes TIMEOUT

This is similar to how an operating system prevents a process from monopolizing system resources.

```text
Agent starts execution
→ finishes within timeout: DONE
→ exceeds timeout: TIMEOUT
```
---
## 🏗️ System Architecture
```text
mini-agent-os/
│
├── app.py
├── agent.py
├── scheduler.py
├── executor.py
├── logger.py
│
├── static/
│   ├── index.html
│   ├── style.css
│   └── script.js
│
└── logs/
    └── execution_log.txt
```
---
## 📁 File Description
| File           | Description                               |
| -------------- | ----------------------------------------- |
| `app.py`       | FastAPI backend server and API endpoints  |
| `agent.py`     | Defines AgentTask and Agent Control Block |
| `scheduler.py` | Implements FCFS and Priority Scheduling   |
| `executor.py`  | Executes agents and handles timeout/quota |
| `logger.py`    | Saves execution logs                      |
| `index.html`   | Web dashboard                             |
| `style.css`    | Dashboard styling                         |
| `script.js`    | Frontend API requests and UI update logic |

---
## 🧠 Agent Control Block
Similar to a Process Control Block in an operating system, each agent is represented by an Agent Control Block.

```text
Agent Control Block
- agent_id
- name
- prompt
- priority
- state
- created_time
- start_time
- end_time
- timeout
- api_quota
- used_quota
- result
- error_message
```

The Agent Control Block stores all information needed to manage and monitor an agent.
---

## 🌐 API Endpoints
| Method   | Endpoint        | Description                          |
| -------- | --------------- | ------------------------------------ |
| `POST`   | `/agents`       | Create a new agent                   |
| `GET`    | `/agents`       | Get all agents                       |
| `POST`   | `/run/fcfs`     | Run agents using FCFS scheduling     |
| `POST`   | `/run/priority` | Run agents using Priority Scheduling |
| `GET`    | `/logs`         | Get execution logs                   |
| `DELETE` | `/agents`       | Clear all agents                     |
---
## 📊 Expected Result
The final system should allow users to:
1. Create multiple LLM agent tasks
2. Assign priority, timeout, and quota to each agent
3. Execute agents using FCFS scheduling
4. Execute agents using Priority Scheduling
5. Monitor agent states through the dashboard
6. Check execution logs
7. Understand how OS scheduling concepts can be applied to LLM agent management