import pytest
from app.simulator import simulate_fcfs, simulate_priority, simulate_rr


def _job(jid, burst, arrival=0, priority=1):
    return {"id": jid, "burst": burst, "arrival": arrival, "priority": priority}


# ---------- FCFS ----------

def test_fcfs_runs_in_arrival_order():
    jobs = [_job("A", 4, arrival=0), _job("B", 2, arrival=1), _job("C", 3, arrival=2)]
    gantt = simulate_fcfs(jobs)
    assert gantt == [
        {"id": "A", "start": 0, "end": 4},
        {"id": "B", "start": 4, "end": 6},
        {"id": "C", "start": 6, "end": 9},
    ]


def test_fcfs_idle_gap_when_no_job_has_arrived():
    jobs = [_job("A", 2, arrival=5)]
    gantt = simulate_fcfs(jobs)
    assert gantt == [{"id": "A", "start": 5, "end": 7}]


# ---------- Priority (non-preemptive, higher value = higher priority) ----------

def test_priority_picks_highest_priority_when_multiple_available():
    jobs = [_job("A", 3, arrival=0, priority=1),
            _job("B", 2, arrival=0, priority=5),
            _job("C", 4, arrival=0, priority=3)]
    gantt = simulate_priority(jobs)
    assert [g["id"] for g in gantt] == ["B", "C", "A"]


def test_priority_falls_back_to_fcfs_when_only_one_available():
    # At t=0, only A is available; B (priority 9) arrives at t=5 but A already running
    jobs = [_job("A", 4, arrival=0, priority=1),
            _job("B", 2, arrival=5, priority=9)]
    gantt = simulate_priority(jobs)
    assert gantt == [
        {"id": "A", "start": 0, "end": 4},
        # idle gap [4,5] then B
        {"id": "B", "start": 5, "end": 7},
    ]


# ---------- Round Robin (preemptive, time quantum) ----------

def test_rr_slices_jobs_by_quantum():
    jobs = [_job("A", 5, arrival=0), _job("B", 3, arrival=0)]
    gantt = simulate_rr(jobs, quantum=2)
    # A(0-2), B(2-4), A(4-6), B(6-7), A(7-8)
    assert gantt == [
        {"id": "A", "start": 0, "end": 2},
        {"id": "B", "start": 2, "end": 4},
        {"id": "A", "start": 4, "end": 6},
        {"id": "B", "start": 6, "end": 7},
        {"id": "A", "start": 7, "end": 8},
    ]


def test_rr_handles_late_arrivals():
    jobs = [_job("A", 4, arrival=0), _job("B", 2, arrival=3)]
    gantt = simulate_rr(jobs, quantum=2)
    # A(0-2), A(2-4)  ← B arrives at 3 and is queued before A re-enters
    # then B(4-6), A(6-8)? Let's trace:
    #   t=0: ready=[A]; pop A; run 2 → A(0-2); A has 2 left.
    #     during this slice, B arrives at t=3? no, slice ends at 2.
    #     re-queue A. ready=[A].
    #   t=2: ready=[A]; pop A; run 2 → A(2-4); A done.
    #     B arrived at t=3 → enter ready during this slice. ready=[B] after pop.
    #   t=4: ready=[B]; pop B; run 2 → B(4-6); B done.
    assert gantt == [
        {"id": "A", "start": 0, "end": 2},
        {"id": "A", "start": 2, "end": 4},
        {"id": "B", "start": 4, "end": 6},
    ]


def test_rr_idle_gap_then_resumes():
    jobs = [_job("A", 2, arrival=5)]
    gantt = simulate_rr(jobs, quantum=3)
    assert gantt == [{"id": "A", "start": 5, "end": 7}]


def test_invalid_quantum_raises():
    with pytest.raises(ValueError):
        simulate_rr([_job("A", 1)], quantum=0)
