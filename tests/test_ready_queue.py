import threading
import time
from datetime import datetime, timedelta
from app.agent import AgentTask, AgentKind
from app.scheduler import SchedulingPolicy
from app.ready_queue import ReadyQueue


def _agent(agent_id, priority, offset_sec):
    a = AgentTask(agent_id=agent_id, name=f"a{agent_id}", prompt="p",
                  kind=AgentKind.LLM, priority=priority, timeout=3)
    a.created_time = datetime(2026, 1, 1) + timedelta(seconds=offset_sec)
    return a


def test_put_and_pop_under_fcfs():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    q.put(_agent(1, 1, 2))
    q.put(_agent(2, 9, 0))
    q.put(_agent(3, 5, 1))
    assert q.pop_next().agent_id == 2
    assert q.pop_next().agent_id == 3
    assert q.pop_next().agent_id == 1


def test_pop_blocks_until_put():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    got = []

    def consumer():
        got.append(q.pop_next().agent_id)

    t = threading.Thread(target=consumer)
    t.start()
    time.sleep(0.1)  # consumer is blocked on Condition.wait
    assert got == []
    q.put(_agent(42, 1, 0))
    t.join(timeout=1)
    assert got == [42]


def test_policy_can_be_switched_at_runtime():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    q.put(_agent(1, 1, 0))
    q.put(_agent(2, 9, 1))
    q.set_policy(SchedulingPolicy.PRIORITY)
    assert q.pop_next().agent_id == 2   # priority wins now
    assert q.pop_next().agent_id == 1


def test_close_unblocks_waiters_with_none():
    q = ReadyQueue(policy=SchedulingPolicy.FCFS)
    results = []

    def consumer():
        results.append(q.pop_next())

    t = threading.Thread(target=consumer)
    t.start()
    time.sleep(0.1)
    q.close()
    t.join(timeout=1)
    assert results == [None]   # sentinel returned when closed
