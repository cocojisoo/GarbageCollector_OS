from enum import Enum
from typing import Callable
from app.agent import AgentTask


class SchedulingPolicy(str, Enum):
    FCFS = "fcfs"
    PRIORITY = "priority"


def sort_key_for(policy: SchedulingPolicy) -> Callable[[AgentTask], tuple]:
    """Return a sort-key function that orders ready agents per the policy.

    The earliest-ranked agent (sort ascending) is the next to run.

    Note: live scheduling is **non-preemptive** — the policy only decides
    which agent a free worker picks up next. Already-running agents are not
    interrupted (LLM API calls and Docker subprocesses cannot be preempted
    cleanly). For preemptive Round Robin, see `app/simulator.py`.
    """
    if policy == SchedulingPolicy.FCFS:
        return lambda a: (a.created_time,)
    if policy == SchedulingPolicy.PRIORITY:
        return lambda a: (-a.priority, a.created_time)
    raise ValueError(f"unknown policy: {policy!r}")
