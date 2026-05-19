import queue
import threading
from unittest.mock import MagicMock
from app.agent import AgentTask, AgentState, AgentKind
from app.executor import Executor, INPUT_PLACEHOLDER
from app.message_bus import MessageBus
from app.quota_manager import QuotaManager
from app.sandbox import SandboxResult


def _task(kind: AgentKind, prompt="hi", agent_id=1, pipe_to=None):
    return AgentTask(agent_id=agent_id, name="t", prompt=prompt, kind=kind,
                     priority=1, timeout=3, pipe_to=pipe_to)


def _make_executor(llm=None, sandbox=None, quota=None, bus=None):
    return Executor(
        llm=llm or MagicMock(),
        sandbox=sandbox or MagicMock(),
        quota=quota or QuotaManager(total=10),
        logger=MagicMock(),
        events=MagicMock(),
        bus=bus or MessageBus(capacity=10),
        pipeline_timeout=2.0,
    )


def test_llm_agent_marks_done_and_stores_result():
    llm = MagicMock()
    llm.complete.return_value = "answer"
    ex = _make_executor(llm=llm)

    task = _task(AgentKind.LLM)
    ex.execute(task)

    assert task.state == AgentState.DONE
    assert task.result == "answer"


def test_quota_exhaustion_marks_error_without_calling_llm():
    llm = MagicMock()
    ex = _make_executor(llm=llm, quota=QuotaManager(total=0))

    task = _task(AgentKind.LLM)
    ex.execute(task)

    assert task.state == AgentState.ERROR
    assert "quota" in task.error_message.lower()
    llm.complete.assert_not_called()


def test_code_agent_runs_in_sandbox_and_marks_done():
    llm = MagicMock()
    llm.complete.return_value = "print('hello')"
    sandbox = MagicMock()
    sandbox.run.return_value = SandboxResult(
        exit_code=0, stdout="hello\n", stderr="", timed_out=False
    )
    ex = _make_executor(llm=llm, sandbox=sandbox)

    task = _task(AgentKind.CODE, prompt="generate hello")
    ex.execute(task)

    assert task.state == AgentState.DONE
    assert "hello" in task.result
    sandbox.run.assert_called_once_with("print('hello')")


def test_code_agent_timeout_marks_timeout_state():
    llm = MagicMock()
    llm.complete.return_value = "while True: pass"
    sandbox = MagicMock()
    sandbox.run.return_value = SandboxResult(
        exit_code=-1, stdout="", stderr="", timed_out=True
    )
    ex = _make_executor(llm=llm, sandbox=sandbox)

    task = _task(AgentKind.CODE)
    ex.execute(task)

    assert task.state == AgentState.TIMEOUT


def test_pipeline_publishes_result_to_downstream_topic():
    llm = MagicMock()
    llm.complete.return_value = "upstream result"
    bus = MessageBus(capacity=5)
    ex = _make_executor(llm=llm, bus=bus)

    task = _task(AgentKind.LLM, agent_id=1, pipe_to=2)
    ex.execute(task)

    assert task.state == AgentState.DONE
    # downstream agent (id=2) can now read it
    assert bus.receive("pipe-2", timeout=1) == "upstream result"


def test_pipeline_consumer_blocks_for_input_then_substitutes_placeholder():
    llm = MagicMock()
    llm.complete.return_value = "summary done"
    bus = MessageBus(capacity=5)
    ex = _make_executor(llm=llm, bus=bus)

    # consumer waits for pipe-2; producer sends 0.2s later
    task = _task(AgentKind.LLM, agent_id=2,
                 prompt=f"Summarize this: {INPUT_PLACEHOLDER}")

    def producer():
        import time
        time.sleep(0.2)
        bus.send("pipe-2", "raw text from upstream")

    threading.Thread(target=producer, daemon=True).start()
    ex.execute(task)

    assert task.state == AgentState.DONE
    # the LLM should have been called with the substituted prompt
    actual_prompt = llm.complete.call_args.args[0]
    assert "raw text from upstream" in actual_prompt
    assert INPUT_PLACEHOLDER not in actual_prompt


def test_pipeline_consumer_times_out_when_no_producer():
    llm = MagicMock()
    bus = MessageBus(capacity=5)
    ex = _make_executor(llm=llm, bus=bus)

    task = _task(AgentKind.LLM, agent_id=2,
                 prompt=f"Use {INPUT_PLACEHOLDER}")
    ex.execute(task)

    assert task.state == AgentState.ERROR
    assert "pipeline input" in task.error_message.lower()
    llm.complete.assert_not_called()
