import asyncio
import threading
from typing import Any, AsyncIterator, List, Optional


class EventBus:
    """Fan-out bus that bridges thread-side publishers to async subscribers.

    `publish` is callable from any worker thread (no event loop required).
    `subscribe` returns an async iterator usable by an SSE endpoint.

    Note: events published while there are zero subscribers are dropped
    (there is no replay buffer). Open the dashboard before starting a demo.
    """

    def __init__(self):
        self._subscribers: List[asyncio.Queue] = []
        self._lock = threading.Lock()
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    def bind_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        """Called once from the FastAPI lifespan so publishers can schedule
        queue puts on the correct event loop."""
        self._loop = loop

    def _ensure_loop(self) -> asyncio.AbstractEventLoop:
        if self._loop is None:
            raise RuntimeError("EventBus.bind_loop() not called")
        return self._loop

    def publish(self, event: Any) -> None:
        loop = self._ensure_loop()
        with self._lock:
            subscribers = list(self._subscribers)
        for q in subscribers:
            loop.call_soon_threadsafe(q.put_nowait, event)

    async def subscribe(self) -> AsyncIterator[Any]:
        q: asyncio.Queue = asyncio.Queue()
        with self._lock:
            self._subscribers.append(q)
        try:
            while True:
                event = await q.get()
                yield event
        finally:
            with self._lock:
                if q in self._subscribers:
                    self._subscribers.remove(q)
