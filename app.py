from fastapi import FastAPI
from pydantic import BaseModel, Field

from agent import AgentTask
from scheduler import fcfs_schedule, priority_schedule
from executor import execute_agent
from logger import log_event, get_logs, clear_logs


app = FastAPI(
    title="Mini Agent OS",
    description="OS-inspired runtime system for scheduling LLM agent tasks",
    version="0.1.0"
)

agents = []
next_agent_id = 1


class AgentCreateRequest(BaseModel):
    name: str = Field(..., example="Summary Agent")
    prompt: str = Field(..., example="Summarize this document.")
    priority: int = Field(1, ge=1, le=10, example=5)
    timeout: int = Field(3, ge=1, le=30, example=3)
    quota: int = Field(1, ge=1, le=10, example=1)


@app.get("/")
def root():
    return {
        "message": "Mini Agent OS is running",
        "docs": "Open http://127.0.0.1:8000/docs to test the API"
    }


@app.post("/agents")
def create_agent(request: AgentCreateRequest):
    global next_agent_id

    agent = AgentTask(
        agent_id=next_agent_id,
        name=request.name,
        prompt=request.prompt,
        priority=request.priority,
        timeout=request.timeout,
        quota=request.quota
    )

    agents.append(agent)
    next_agent_id += 1

    log_event(
        f"Agent {agent.agent_id} ({agent.name}) created "
        f"with priority={agent.priority}, timeout={agent.timeout}, quota={agent.quota}"
    )

    return {
        "message": "Agent created",
        "agent": agent.to_dict()
    }


@app.get("/agents")
def get_agents():
    return {
        "count": len(agents),
        "agents": [agent.to_dict() for agent in agents]
    }


@app.post("/run/fcfs")
def run_fcfs():
    scheduled_agents = fcfs_schedule(agents)

    log_event("FCFS scheduler started")

    execution_order = []

    for agent in scheduled_agents:
        execution_order.append(agent.agent_id)
        execute_agent(agent)

    log_event("FCFS scheduler finished")

    return {
        "message": "FCFS scheduling completed",
        "execution_order": execution_order,
        "agents": [agent.to_dict() for agent in agents]
    }


@app.post("/run/priority")
def run_priority():
    scheduled_agents = priority_schedule(agents)

    log_event("Priority scheduler started")

    execution_order = []

    for agent in scheduled_agents:
        execution_order.append(agent.agent_id)
        execute_agent(agent)

    log_event("Priority scheduler finished")

    return {
        "message": "Priority scheduling completed",
        "execution_order": execution_order,
        "agents": [agent.to_dict() for agent in agents]
    }


@app.get("/logs")
def read_logs():
    return {
        "logs": get_logs()
    }


@app.delete("/agents")
def clear_agents():
    global agents, next_agent_id

    agents = []
    next_agent_id = 1
    clear_logs()

    return {
        "message": "All agents and logs cleared"
    }