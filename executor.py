import time
from datetime import datetime

from logger import log_event


def calculate_required_api_calls(prompt: str) -> int:
    """
    [CALL] 키워드 개수를 simulated API call 횟수로 사용한다.
    [CALL]이 없으면 기본 1회 호출로 처리한다.
    """
    call_count = prompt.count("[CALL]")
    return call_count if call_count > 0 else 1


def calculate_execution_time(prompt: str, timeout: int) -> int:
    """
    prompt 길이에 따라 실행 시간을 간단히 simulation한다.
    [SLOW] 키워드가 있으면 timeout을 초과하도록 만든다.
    """
    if "[SLOW]" in prompt:
        return timeout + 1

    return 1


def execute_agent(agent):
    """
    Agent를 실행하는 함수.
    실제 LLM API를 호출하지 않고, 실행 시간과 API quota를 simulation한다.
    """
    agent.state = "RUNNING"
    agent.start_time = datetime.now()

    log_event(f"Agent {agent.agent_id} ({agent.name}) started")

    try:
        required_calls = calculate_required_api_calls(agent.prompt)

        if required_calls > agent.quota:
            agent.used_quota = agent.quota
            agent.state = "ERROR"
            agent.error_message = (
                f"API quota exceeded: required {required_calls}, quota {agent.quota}"
            )
            agent.end_time = datetime.now()

            log_event(
                f"Agent {agent.agent_id} ({agent.name}) failed - quota exceeded"
            )
            return agent

        execution_time = calculate_execution_time(agent.prompt, agent.timeout)

        if execution_time > agent.timeout:
            time.sleep(agent.timeout)
            agent.used_quota = required_calls
            agent.state = "TIMEOUT"
            agent.error_message = (
                f"Execution timeout: required {execution_time}s, timeout {agent.timeout}s"
            )
            agent.end_time = datetime.now()

            log_event(
                f"Agent {agent.agent_id} ({agent.name}) timeout"
            )
            return agent

        time.sleep(execution_time)

        agent.used_quota = required_calls
        agent.state = "DONE"
        agent.result = (
            f"Agent '{agent.name}' completed successfully. "
            f"Simulated execution time: {execution_time}s, "
            f"API calls used: {required_calls}."
        )
        agent.end_time = datetime.now()

        log_event(f"Agent {agent.agent_id} ({agent.name}) completed")
        return agent

    except Exception as e:
        agent.state = "ERROR"
        agent.error_message = str(e)
        agent.end_time = datetime.now()

        log_event(f"Agent {agent.agent_id} ({agent.name}) error: {e}")
        return agent