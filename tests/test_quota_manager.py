import threading
from app.quota_manager import QuotaManager


def test_acquire_within_limit_succeeds():
    qm = QuotaManager(total=10)
    assert qm.try_acquire(3) is True
    assert qm.remaining() == 7


def test_acquire_over_limit_fails_atomically():
    qm = QuotaManager(total=5)
    assert qm.try_acquire(6) is False
    assert qm.remaining() == 5  # not partially consumed


def test_release_refunds_consumed_units():
    qm = QuotaManager(total=10)
    assert qm.try_acquire(4) is True
    assert qm.remaining() == 6
    qm.release(3)
    assert qm.remaining() == 9


def test_release_clamps_at_zero_so_double_release_is_safe():
    qm = QuotaManager(total=5)
    qm.try_acquire(1)
    qm.release(10)            # over-release does not underflow
    assert qm.remaining() == 5
    qm.release(1)             # still safe when nothing is in use
    assert qm.remaining() == 5


def test_concurrent_acquires_never_exceed_total():
    qm = QuotaManager(total=100)
    successes = []
    lock = threading.Lock()

    def worker():
        if qm.try_acquire(1):
            with lock:
                successes.append(1)

    threads = [threading.Thread(target=worker) for _ in range(500)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    assert sum(successes) == 100
    assert qm.remaining() == 0
