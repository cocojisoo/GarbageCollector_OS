from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Optional


class AgentState(str, Enum):
    READY = "READY"
    RUNNING = "RUNNING"
    DONE = "DONE"
    TIMEOUT = "TIMEOUT"
    ERROR = "ERROR"


class AgentKind(str, Enum):
    LLM = "llm"     # plain LLM completion
    CODE = "code"   # LLM generates code → sandbox executes


@dataclass
class AgentTask:
    agent_id: int
    name: str
    prompt: str
    kind: AgentKind
    priority: int
    timeout: int
    pipe_to: Optional[int] = None  # downstream agent id; result is published to pipe-{pipe_to}

    state: AgentState = AgentState.READY
    result: Optional[str] = None
    error_message: Optional[str] = None

    created_time: datetime = field(default_factory=datetime.now)
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None

    def to_dict(self) -> dict:
        fmt = "%Y-%m-%d %H:%M:%S.%f"
        return {
            "agent_id": self.agent_id,
            "name": self.name,
            "prompt": self.prompt,
            "kind": self.kind.value,
            "priority": self.priority,
            "timeout": self.timeout,
            "pipe_to": self.pipe_to,
            "state": self.state.value,
            "result": self.result,
            "error_message": self.error_message,
            "created_time": self.created_time.strftime(fmt),
            "start_time": self.start_time.strftime(fmt) if self.start_time else None,
            "end_time": self.end_time.strftime(fmt) if self.end_time else None,
        }
