def fcfs_schedule(agents):
    """
    FCFS: First-Come, First-Served
    READY 상태인 agent를 생성 시간 순서대로 실행한다.
    """
    ready_agents = [agent for agent in agents if agent.state == "READY"]
    return sorted(ready_agents, key=lambda agent: agent.created_time)


def priority_schedule(agents):
    """
    Priority Scheduling
    priority 값이 높은 agent를 먼저 실행한다.
    """
    ready_agents = [agent for agent in agents if agent.state == "READY"]
    return sorted(
        ready_agents,
        key=lambda agent: (-agent.priority, agent.created_time)
    )