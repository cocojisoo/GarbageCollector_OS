import threading


class QuotaManager:
    """Thread-safe shared API call quota — demonstrates mutex synchronization."""

    def __init__(self, total: int):
        self._total = total
        self._used = 0
        self._lock = threading.Lock()

    def try_acquire(self, n: int) -> bool:
        with self._lock:
            if self._used + n > self._total:
                return False
            self._used += n
            return True

    def remaining(self) -> int:
        with self._lock:
            return self._total - self._used

    def reset(self) -> None:
        with self._lock:
            self._used = 0
