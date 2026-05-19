from datetime import datetime, timedelta
from app.agent import AgentTask, AgentKind
from app.scheduler import SchedulingPolicy, sort_key_for


def _agent(agent_id, priority, created_offset_sec):
    a = AgentTask(agent_id=agent_id, name=f"a{agent_id}", prompt="p",
                  kind=AgentKind.LLM, priority=priority, timeout=3)
    a.created_time = datetime(2026, 1, 1) + timedelta(seconds=created_offset_sec)
    return a


def test_fcfs_orders_by_created_time():
    agents = [_agent(1, 1, 2), _agent(2, 9, 0), _agent(3, 5, 1)]
    fn = sort_key_for(SchedulingPolicy.FCFS)
    ordered = sorted(agents, key=fn)
    assert [a.agent_id for a in ordered] == [2, 3, 1]


def test_priority_orders_high_first_then_fcfs_tiebreak():
    agents = [_agent(1, 5, 0), _agent(2, 9, 1), _agent(3, 5, 2)]
    fn = sort_key_for(SchedulingPolicy.PRIORITY)
    ordered = sorted(agents, key=fn)
    assert [a.agent_id for a in ordered] == [2, 1, 3]


def test_unknown_policy_raises():
    import pytest
    with pytest.raises(ValueError):
        sort_key_for("nope")  # type: ignore[arg-type]
