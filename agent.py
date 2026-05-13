from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional


@dataclass
class AgentTask:
    agent_id: int
    name: str
    prompt: str
    priority: int
    timeout: int
    quota: int

    state: str = "READY"
    used_quota: int = 0
    result: Optional[str] = None
    error_message: Optional[str] = None

    created_time: datetime = field(default_factory=datetime.now)
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None

    def to_dict(self):
        return {
            "agent_id": self.agent_id,
            "name": self.name,
            "prompt": self.prompt,
            "priority": self.priority,
            "timeout": self.timeout,
            "quota": self.quota,
            "used_quota": self.used_quota,
            "state": self.state,
            "result": self.result,
            "error_message": self.error_message,
            "created_time": self.created_time.strftime("%Y-%m-%d %H:%M:%S"),
            "start_time": self.start_time.strftime("%Y-%m-%d %H:%M:%S") if self.start_time else None,
            "end_time": self.end_time.strftime("%Y-%m-%d %H:%M:%S") if self.end_time else None,
        }