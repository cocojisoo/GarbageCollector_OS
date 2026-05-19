import queue
import threading
from typing import Any, Dict


class MessageBus:
    """Per-topic bounded queues — classic producer/consumer pattern.

    `send` blocks when the topic queue is full; `receive` blocks (with timeout)
    when the topic queue is empty. Demonstrates the bounded-buffer synchronization
    problem covered in OS textbooks.

    Used by the Executor to wire agents into pipelines: an upstream agent
    publishes its result to topic `pipe-{downstream_id}`; the downstream
    agent's executor blocks on `receive("pipe-{my_id}")` when its prompt
    contains the `{INPUT}` placeholder.
    """

    def __init__(self, capacity: int):
        self._capacity = capacity
        self._queues: Dict[str, queue.Queue] = {}
        self._lock = threading.Lock()

    def _q(self, topic: str) -> queue.Queue:
        with self._lock:
            q = self._queues.get(topic)
            if q is None:
                q = queue.Queue(maxsize=self._capacity)
                self._queues[topic] = q
            return q

    def send(self, topic: str, payload: Any, timeout: float | None = None) -> None:
        self._q(topic).put(payload, timeout=timeout)

    def receive(self, topic: str, timeout: float) -> Any:
        return self._q(topic).get(timeout=timeout)
