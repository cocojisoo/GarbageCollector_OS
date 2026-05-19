import threading
from typing import List, Protocol
from app.agent import AgentTask
from app.ready_queue import ReadyQueue


class ExecutorLike(Protocol):
    def execute(self, task: AgentTask) -> AgentTask: ...


class WorkerPool:
    """Fixed-size pool of daemon threads pulling agents from a ReadyQueue.

    Each worker loops: pop_next() (blocks on Condition) -> executor.execute(task).
    A None from pop_next() means the queue was closed; the worker exits.
    """

    def __init__(self, ready_queue: ReadyQueue, executor: ExecutorLike,
                 workers: int):
        self._queue = ready_queue
        self._executor = executor
        self._n = workers
        self._threads: List[threading.Thread] = []

    def start(self) -> None:
        for i in range(self._n):
            t = threading.Thread(
                target=self._worker_loop,
                name=f"agent-worker-{i}",
                daemon=True,
            )
            t.start()
            self._threads.append(t)

    def stop(self, join_timeout: float = 2.0) -> None:
        self._queue.close()
        for t in self._threads:
            t.join(timeout=join_timeout)

    def threads(self) -> List[threading.Thread]:
        return list(self._threads)

    def _worker_loop(self) -> None:
        while True:
            task = self._queue.pop_next()
            if task is None:
                return
            try:
                self._executor.execute(task)
            except Exception:
                # Executor handles its own errors; if anything leaks, drop it
                # so the worker stays alive.
                pass
