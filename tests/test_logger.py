from pathlib import Path
from app.logger import Logger


def test_log_event_appends_line(tmp_path: Path):
    lg = Logger(tmp_path / "exec.log")
    lg.log("agent 1 started")
    lg.log("agent 1 done")
    lines = lg.read_all()
    assert len(lines) == 2
    assert "agent 1 started" in lines[0]
    assert "agent 1 done" in lines[1]


def test_clear_empties_log(tmp_path: Path):
    lg = Logger(tmp_path / "exec.log")
    lg.log("x")
    lg.clear()
    assert lg.read_all() == []
