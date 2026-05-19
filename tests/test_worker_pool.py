import threading
import time
import pytest
from unittest.mock import MagicMock
from app.agent import AgentTask, AgentState, AgentKind
from app.ready_queue import ReadyQueue
from app.scheduler import SchedulingPolicy
from app.worker_pool import WorkerPool


def _agent(agent_id):
    return AgentTask(agent_id=agent_id, name=f"a{agent_id}", prompt="p",
                     kind=AgentKind.LLM, priority=1, timeout=3)


def test_workers_drain_queue_and_call_executor():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    done = threading.Event()
    seen_ids = []
    lock = threading.Lock()

    class FakeExecutor:
        def execute(self, task):
            with lock:
                seen_ids.append(task.agent_id)
                task.state = AgentState.DONE
                if len(seen_ids) == 3:
                    done.set()

    pool = WorkerPool(ready_queue=q, executor=FakeExecutor(), workers=2)
    pool.start()
    for i in [1, 2, 3]:
        q.put(_agent(i))
    assert done.wait(timeout=3)
    pool.stop()
    assert sorted(seen_ids) == [1, 2, 3]


def test_stop_shuts_workers_down():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    pool = WorkerPool(ready_queue=q, executor=MagicMock(), workers=2)
    pool.start()
    pool.stop()
    assert all(not t.is_alive() for t in pool.threads())


@pytest.mark.slow
def test_concurrent_workers_actually_run_in_parallel():
    """Two workers should process two slow tasks in less than 2x the serial time."""
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    barrier = threading.Barrier(parties=2)
    done_count = {"n": 0}
    done_lock = threading.Lock()

    class SlowExecutor:
        def execute(self, task):
            barrier.wait(timeout=2)
            time.sleep(0.3)
            task.state = AgentState.DONE
            with done_lock:
                done_count["n"] += 1

    pool = WorkerPool(ready_queue=q, executor=SlowExecutor(), workers=2)
    pool.start()
    start = time.monotonic()
    q.put(_agent(1))
    q.put(_agent(2))
    while done_count["n"] < 2 and time.monotonic() - start < 2:
        time.sleep(0.02)
    elapsed = time.monotonic() - start
    pool.stop()
    assert done_count["n"] == 2
    # Serial would take ~0.6s; parallel should be well under 0.55s.
    assert elapsed < 0.55
