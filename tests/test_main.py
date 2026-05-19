import time
from unittest.mock import patch, MagicMock
from fastapi.testclient import TestClient


def _make_client():
    """Patch dependency constructors so tests don't hit network or Docker."""
    with patch("app.main._build_llm") as mk_llm, \
         patch("app.main._build_sandbox") as mk_sb:
        mk_llm.return_value = MagicMock()
        mk_llm.return_value.complete.return_value = "ok"
        mk_sb.return_value = MagicMock()
        from app import main
        main._reset_for_tests()
        return TestClient(main.app)


def test_create_and_list_agents():
    client = _make_client()
    r = client.post("/agents", json={
        "name": "summary", "prompt": "do x",
        "priority": 5, "timeout": 3, "kind": "llm",
    })
    assert r.status_code == 200
    body = r.json()
    assert body["agent"]["state"] in ("READY", "RUNNING", "DONE")
    time.sleep(0.2)
    listing = client.get("/agents").json()
    assert listing["count"] == 1


def test_policy_can_be_changed():
    client = _make_client()
    r = client.post("/policy", json={"policy": "priority"})
    assert r.status_code == 200
    assert r.json()["policy"] == "priority"
    r = client.get("/policy")
    assert r.json()["policy"] == "priority"


def test_clear_resets_agents_and_logs():
    client = _make_client()
    client.post("/agents", json={
        "name": "x", "prompt": "x", "priority": 1, "timeout": 3, "kind": "llm",
    })
    time.sleep(0.2)
    r = client.delete("/agents")
    assert r.status_code == 200
    assert client.get("/agents").json()["count"] == 0


def test_simulate_returns_gantt_for_each_policy():
    client = _make_client()
    body = {
        "policy": "rr",
        "quantum": 2,
        "jobs": [
            {"id": "A", "burst": 5, "arrival": 0, "priority": 1},
            {"id": "B", "burst": 3, "arrival": 0, "priority": 1},
        ],
    }
    r = client.post("/simulate", json=body)
    assert r.status_code == 200
    gantt = r.json()["gantt"]
    assert gantt[0] == {"id": "A", "start": 0, "end": 2}
    assert gantt[-1]["end"] == 8  # total = 5 + 3


def test_simulate_fcfs_does_not_require_quantum():
    client = _make_client()
    body = {
        "policy": "fcfs",
        "jobs": [
            {"id": "A", "burst": 3, "arrival": 0, "priority": 1},
            {"id": "B", "burst": 2, "arrival": 0, "priority": 1},
        ],
    }
    r = client.post("/simulate", json=body)
    assert r.status_code == 200
    assert r.json()["gantt"] == [
        {"id": "A", "start": 0, "end": 3},
        {"id": "B", "start": 3, "end": 5},
    ]
