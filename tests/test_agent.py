from datetime import datetime
from app.agent import AgentTask, AgentState, AgentKind


def test_new_agent_is_ready():
    a = AgentTask(agent_id=1, name="x", prompt="p", kind=AgentKind.LLM,
                  priority=5, timeout=3)
    assert a.state == AgentState.READY
    assert a.result is None
    assert a.pipe_to is None
    assert isinstance(a.created_time, datetime)


def test_to_dict_serializes_enums_and_pipe():
    a = AgentTask(agent_id=1, name="x", prompt="p", kind=AgentKind.CODE,
                  priority=5, timeout=3, pipe_to=7)
    d = a.to_dict()
    assert d["state"] == "READY"
    assert d["kind"] == "code"
    assert d["pipe_to"] == 7
    assert d["start_time"] is None
