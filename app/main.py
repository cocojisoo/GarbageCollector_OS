import asyncio
import json
import os
from contextlib import asynccontextmanager
from enum import Enum
from pathlib import Path
from typing import List, Optional

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from sse_starlette.sse import EventSourceResponse
from dotenv import load_dotenv

from app.agent import AgentTask, AgentKind
from app.event_bus import EventBus
from app.executor import Executor
from app.llm_client import LLMClient
from app.logger import Logger
from app.message_bus import MessageBus
from app.quota_manager import QuotaManager
from app.ready_queue import ReadyQueue
from app.sandbox import Sandbox
from app.scheduler import SchedulingPolicy
from app.simulator import simulate_fcfs, simulate_priority, simulate_rr
from app.worker_pool import WorkerPool


load_dotenv()


def _build_llm() -> Optional[LLMClient]:
    key = os.environ.get("UPSTAGE_API_KEY")
    if not key or key == "replace_me":
        return None
    return LLMClient(
        api_key=key,
        base_url=os.environ.get("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1"),
        model=os.environ.get("UPSTAGE_MODEL", "solar-pro-3"),
    )


def _build_sandbox() -> Sandbox:
    return Sandbox(
        image=os.environ.get("SANDBOX_IMAGE", "mini-agent-os-sandbox:latest"),
        memory=os.environ.get("SANDBOX_MEMORY", "128m"),
        cpus=os.environ.get("SANDBOX_CPUS", "0.5"),
        timeout=int(os.environ.get("SANDBOX_TIMEOUT_DEFAULT", "5")),
    )


# ---------- module-level runtime state ----------

_agents: List[AgentTask] = []
_next_id = 1
_logger = Logger(Path("logs/execution_log.txt"))
_quota = QuotaManager(total=int(os.environ.get("GLOBAL_QUOTA", "1000")))
_message_bus = MessageBus(capacity=int(os.environ.get("MESSAGE_BUS_CAPACITY", "100")))
_event_bus = EventBus()
_ready_queue = ReadyQueue(policy=SchedulingPolicy.FCFS)
_llm = _build_llm()
_sandbox = _build_sandbox()
_executor = Executor(
    llm=_llm, sandbox=_sandbox, quota=_quota,
    logger=_logger, events=_event_bus, bus=_message_bus,
    pipeline_timeout=float(os.environ.get("PIPELINE_WAIT_TIMEOUT", "30")),
)
_worker_pool = WorkerPool(
    ready_queue=_ready_queue,
    executor=_executor,
    workers=int(os.environ.get("WORKER_COUNT", "4")),
)


def _reset_for_tests() -> None:
    """Used only by the test suite to wipe per-test state."""
    global _agents, _next_id
    _agents = []
    _next_id = 1
    _quota.reset()
    _logger.clear()


# ---------- FastAPI lifespan ----------

@asynccontextmanager
async def lifespan(app: FastAPI):
    _event_bus.bind_loop(asyncio.get_running_loop())
    _worker_pool.start()
    try:
        yield
    finally:
        _worker_pool.stop()


app = FastAPI(title="Mini Agent OS", version="0.1.0", lifespan=lifespan)
app.mount("/static", StaticFiles(directory="static"), name="static")


# ---------- request models ----------

class AgentCreate(BaseModel):
    name: str = Field(..., examples=["summary"])
    prompt: str
    kind: AgentKind = AgentKind.LLM
    priority: int = Field(1, ge=1, le=10)
    timeout: int = Field(3, ge=1, le=30)
    pipe_to: Optional[int] = None


class PolicyChange(BaseModel):
    policy: SchedulingPolicy


class SimulatePolicy(str, Enum):
    FCFS = "fcfs"
    PRIORITY = "priority"
    RR = "rr"


class SimulateJob(BaseModel):
    id: str
    burst: int = Field(..., ge=1)
    arrival: int = Field(0, ge=0)
    priority: int = Field(1, ge=1, le=10)


class SimulateRequest(BaseModel):
    policy: SimulatePolicy
    jobs: List[SimulateJob]
    quantum: Optional[int] = Field(None, ge=1)


# ---------- endpoints ----------

@app.get("/")
def root():
    return FileResponse("static/index.html")


@app.post("/agents")
def create_agent(req: AgentCreate):
    global _next_id
    task = AgentTask(
        agent_id=_next_id, name=req.name, prompt=req.prompt, kind=req.kind,
        priority=req.priority, timeout=req.timeout, pipe_to=req.pipe_to,
    )
    _agents.append(task)
    _next_id += 1
    _logger.log(
        f"Agent {task.agent_id} ({task.name}) created "
        f"kind={req.kind.value} pipe_to={req.pipe_to}"
    )
    try:
        _event_bus.publish(task.to_dict())
    except RuntimeError:
        pass  # EventBus not yet bound to a loop (e.g. during tests without lifespan)
    _ready_queue.put(task)
    return {"message": "created", "agent": task.to_dict()}


@app.get("/agents")
def list_agents():
    return {"count": len(_agents), "agents": [a.to_dict() for a in _agents]}


@app.get("/policy")
def get_policy():
    return {"policy": _ready_queue.get_policy().value}


@app.post("/policy")
def set_policy(req: PolicyChange):
    _ready_queue.set_policy(req.policy)
    _logger.log(f"Scheduling policy switched to {req.policy.value}")
    return {"policy": req.policy.value}


@app.get("/logs")
def get_logs():
    return {"logs": _logger.read_all()}


@app.delete("/agents")
def clear():
    global _agents, _next_id
    _agents = []
    _next_id = 1
    _quota.reset()
    _logger.clear()
    _logger.log("State cleared")
    return {"message": "cleared"}


@app.post("/simulate")
def simulate(req: SimulateRequest):
    jobs = [j.model_dump() for j in req.jobs]
    if req.policy == SimulatePolicy.FCFS:
        gantt = simulate_fcfs(jobs)
    elif req.policy == SimulatePolicy.PRIORITY:
        gantt = simulate_priority(jobs)
    else:  # RR
        if req.quantum is None:
            raise HTTPException(400, "quantum is required for RR")
        gantt = simulate_rr(jobs, quantum=req.quantum)
    return {"policy": req.policy.value, "gantt": gantt}


@app.get("/events")
async def sse_events():
    async def stream():
        async for event in _event_bus.subscribe():
            yield {"data": json.dumps(event)}
    return EventSourceResponse(stream())
