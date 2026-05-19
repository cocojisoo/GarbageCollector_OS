import queue
import threading
import time
import pytest
from app.message_bus import MessageBus


def test_put_get_round_trip():
    bus = MessageBus(capacity=5)
    bus.send("topic", "hello")
    assert bus.receive("topic", timeout=1) == "hello"


def test_get_raises_empty_on_timeout():
    bus = MessageBus(capacity=5)
    with pytest.raises(queue.Empty):
        bus.receive("nobody", timeout=0.1)


def test_bounded_capacity_blocks_producer():
    bus = MessageBus(capacity=2)
    bus.send("t", 1)
    bus.send("t", 2)
    # third send must block until something is consumed
    blocked = threading.Event()

    def producer():
        bus.send("t", 3)
        blocked.set()

    t = threading.Thread(target=producer)
    t.start()
    time.sleep(0.1)
    assert not blocked.is_set()  # producer is blocked

    assert bus.receive("t", timeout=1) == 1
    t.join(timeout=1)
    assert blocked.is_set()


def test_topics_are_independent():
    bus = MessageBus(capacity=5)
    bus.send("a", 1)
    bus.send("b", 2)
    assert bus.receive("b", timeout=1) == 2
    assert bus.receive("a", timeout=1) == 1
