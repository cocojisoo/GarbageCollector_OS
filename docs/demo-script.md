# Demo Script — Mini Agent OS (Team GarbageCollector)

## Setup
1. `cp .env.example .env` and add `UPSTAGE_API_KEY`.
2. `docker compose build`
3. `docker compose up`
4. Open `http://localhost:8000`.

## Scenarios

### Scenario A — Concurrent execution (worker pool)
1. Open the dashboard so the SSE stream is connected.
2. Quickly create 4 LLM agents in a row.
3. Expected: all four turn RUNNING (yellow, pulsing) **simultaneously**, then complete one by one. Demonstrates the 4-thread worker pool.

### Scenario B — FCFS scheduling
1. Set `WORKER_COUNT=1` in `.env` and restart (one worker → serialized order is observable).
2. Switch policy dropdown to "FCFS".
3. Create three LLM agents named `a`, `b`, `c` quickly.
4. Expected: execution order is `a → b → c` (creation order).

### Scenario C — Priority scheduling (non-preemptive)
1. Still single worker. Switch policy to "Priority".
2. Clear, then create three agents named `low` (priority 1), `mid` (5), `high` (9), in that order.
3. Expected: even though `low` was created first, the order is `high → mid → low`. Note that priority does not interrupt a running agent — it only orders the queue.

### Scenario D — Sandboxed code execution
1. Restore `WORKER_COUNT=4`, restart.
2. Create a `code` agent with prompt: `Write a Python program that prints the first 10 Fibonacci numbers.`
3. Expected: state goes RUNNING → DONE; result includes generated code and stdout `0 1 1 2 3 5 8 13 21 34`.

### Scenario E — Timeout (sandbox kill)
1. Create a `code` agent, timeout=2, prompt: `Write Python that sleeps for 60 seconds.`
2. Expected: state becomes TIMEOUT within ~2s.

### Scenario F — Quota exhaustion (mutex synchronization)
1. Set `GLOBAL_QUOTA=2` in `.env` and restart.
2. Create 5 LLM agents and wait.
3. Expected: first two reach DONE, remaining three become ERROR with "API quota exhausted". Because the QuotaManager uses a mutex, the count is always exactly 2 even with concurrent workers.

### Scenario G — Network isolation (system-call restriction)
1. Create a `code` agent with prompt: `Write Python that uses urllib to fetch https://example.com and prints the status code.`
2. Expected: code runs but reports a network error — `--network=none` blocks egress at the container namespace boundary.

### Scenario H — Pipeline (inter-agent IPC via bounded MessageBus)
1. Restore default `.env`.
2. Create agent **#1** first (note its assigned id, e.g. 5):
   - name = `gen`, kind = `llm`, prompt = `List 3 interesting facts about pelicans.`
   - pipe_to = the id that will be assigned to the next agent (one greater than #1's id; if #1 got id 5, set pipe_to = 6).
3. Immediately create agent **#2**:
   - name = `summary`, kind = `llm`, prompt = `Summarize the following into a single sentence: {INPUT}`
   - leave pipe_to blank.
4. Expected: `gen` runs and DONE; `summary` was RUNNING (blocked on `bus.receive("pipe-6")`) and the moment `gen` finishes, `summary`'s prompt gets `{INPUT}` substituted and the agent completes. This demonstrates the bounded MessageBus + producer/consumer pattern wiring two agents.

### Scenario I — Round Robin scheduling (simulator)
1. In the "Scheduler simulator" section, paste:

```
A 8 0 3
B 4 0 1
C 9 0 2
D 5 0 4
```

2. Choose **Round Robin**, quantum = `3`, click **Run simulation**.
3. Expected: a Gantt bar chart showing repeated A/B/C/D quanta in arrival order until each job's burst is consumed. Switch policy to **FCFS** or **Priority** and compare the orderings on the same jobs.
