"""Pure-function scheduler simulator.

Inputs are plain dicts so this module is trivially testable and exposable as
an HTTP endpoint without coupling to AgentTask.

Job shape: {"id": str, "burst": int, "arrival": int, "priority": int}
Output:    list of {"id": str, "start": int, "end": int}
"""
from collections import deque
from typing import List, Dict


Job = Dict[str, int]
Slice = Dict[str, int]


def simulate_fcfs(jobs: List[Job]) -> List[Slice]:
    """First-Come, First-Served: run in order of arrival to completion."""
    ordered = sorted(jobs, key=lambda j: (j["arrival"], j["id"]))
    timeline: List[Slice] = []
    clock = 0
    for j in ordered:
        start = max(clock, j["arrival"])
        end = start + j["burst"]
        timeline.append({"id": j["id"], "start": start, "end": end})
        clock = end
    return timeline


def simulate_priority(jobs: List[Job]) -> List[Slice]:
    """Non-preemptive priority: among arrived jobs, pick the highest priority value.

    Tiebreak: earlier arrival, then lexicographic id.
    """
    remaining = [dict(j) for j in jobs]
    timeline: List[Slice] = []
    clock = 0
    while remaining:
        available = [j for j in remaining if j["arrival"] <= clock]
        if not available:
            clock = min(j["arrival"] for j in remaining)
            continue
        chosen = max(available, key=lambda j: (j["priority"], -j["arrival"], -ord(j["id"][0])))
        start = clock
        end = start + chosen["burst"]
        timeline.append({"id": chosen["id"], "start": start, "end": end})
        clock = end
        remaining.remove(chosen)
    return timeline


def simulate_rr(jobs: List[Job], quantum: int) -> List[Slice]:
    """Round Robin with the given time quantum (preemptive).

    Consecutive same-id slices are NOT merged; each one represents a separate
    quantum so the Gantt chart faithfully shows preemption points.
    """
    if quantum <= 0:
        raise ValueError("quantum must be a positive integer")

    pending = sorted(jobs, key=lambda j: (j["arrival"], j["id"]))
    remaining_burst = {j["id"]: j["burst"] for j in jobs}
    ready: deque = deque()
    timeline: List[Slice] = []
    clock = 0

    def admit_arrivals(up_to_time: int) -> None:
        nonlocal pending
        new_pending = []
        for j in pending:
            if j["arrival"] <= up_to_time:
                ready.append(j["id"])
            else:
                new_pending.append(j)
        pending = new_pending

    admit_arrivals(clock)
    if not ready and pending:
        clock = pending[0]["arrival"]
        admit_arrivals(clock)

    while ready or pending:
        if not ready:
            clock = pending[0]["arrival"]
            admit_arrivals(clock)
            continue

        jid = ready.popleft()
        run = min(quantum, remaining_burst[jid])
        timeline.append({"id": jid, "start": clock, "end": clock + run})
        clock += run
        remaining_burst[jid] -= run

        # Admit any jobs that arrived during this quantum BEFORE re-queuing self,
        # which matches standard RR convention.
        admit_arrivals(clock)

        if remaining_burst[jid] > 0:
            ready.append(jid)

    return timeline
