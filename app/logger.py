import threading
from datetime import datetime
from pathlib import Path
from typing import List


class Logger:
    """File-backed event log, safe to call from multiple worker threads."""

    def __init__(self, path: Path):
        self._path = Path(path)
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._path.touch(exist_ok=True)
        self._lock = threading.Lock()

    def log(self, message: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        line = f"[{ts}] {message}"
        with self._lock:
            with self._path.open("a", encoding="utf-8") as f:
                f.write(line + "\n")
        print(line, flush=True)

    def read_all(self) -> List[str]:
        with self._lock:
            return self._path.read_text(encoding="utf-8").splitlines()

    def clear(self) -> None:
        with self._lock:
            self._path.write_text("", encoding="utf-8")
