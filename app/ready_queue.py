import threading
from typing import List, Optional
from app.agent import AgentTask
from app.scheduler import SchedulingPolicy, sort_key_for


class ReadyQueue:
    """Thread-safe ready queue with a switchable scheduling policy.

    Producers (API handlers) call `put`. Consumers (worker threads) call
    `pop_next`, which blocks on a Condition until something is available or
    the queue is closed (returns None as a shutdown sentinel).
    """

    def __init__(self, policy: SchedulingPolicy):
        self._items: List[AgentTask] = []
        self._policy = policy
        self._cond = threading.Condition()
        self._closed = False

    def set_policy(self, policy: SchedulingPolicy) -> None:
        with self._cond:
            self._policy = policy

    def get_policy(self) -> SchedulingPolicy:
        with self._cond:
            return self._policy

    def put(self, task: AgentTask) -> None:
        with self._cond:
            self._items.append(task)
            self._cond.notify()

    def pop_next(self) -> Optional[AgentTask]:
        """Block until an agent is available or the queue is closed.

        Returns the next agent per the current policy, or None if closed.
        """
        with self._cond:
            while not self._items and not self._closed:
                self._cond.wait()
            if self._closed and not self._items:
                return None
            key = sort_key_for(self._policy)
            self._items.sort(key=key)
            return self._items.pop(0)

    def size(self) -> int:
        with self._cond:
            return len(self._items)

    def close(self) -> None:
        with self._cond:
            self._closed = True
            self._cond.notify_all()
