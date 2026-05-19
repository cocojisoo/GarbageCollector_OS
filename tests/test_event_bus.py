import asyncio
import threading
import pytest
from app.event_bus import EventBus


@pytest.mark.asyncio
async def test_published_event_reaches_async_subscriber():
    bus = EventBus()
    bus.bind_loop(asyncio.get_running_loop())

    async def collect_one():
        async for event in bus.subscribe():
            return event

    task = asyncio.create_task(collect_one())
    await asyncio.sleep(0.05)  # let the subscriber attach
    # publish from a worker thread
    threading.Thread(target=lambda: bus.publish({"agent_id": 7, "state": "DONE"})).start()
    result = await asyncio.wait_for(task, timeout=1)
    assert result == {"agent_id": 7, "state": "DONE"}


@pytest.mark.asyncio
async def test_multiple_subscribers_each_receive_event():
    bus = EventBus()
    bus.bind_loop(asyncio.get_running_loop())

    async def collect():
        async for event in bus.subscribe():
            return event

    t1 = asyncio.create_task(collect())
    t2 = asyncio.create_task(collect())
    await asyncio.sleep(0.05)
    bus.publish("ping")
    a, b = await asyncio.gather(asyncio.wait_for(t1, timeout=1),
                                asyncio.wait_for(t2, timeout=1))
    assert a == "ping" and b == "ping"
