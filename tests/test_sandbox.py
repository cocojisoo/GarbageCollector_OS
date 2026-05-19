import pytest
from app.sandbox import Sandbox, SandboxResult


pytestmark = pytest.mark.docker


def test_runs_simple_python_and_captures_stdout():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=3)
    r: SandboxResult = sb.run("print(2 + 2)")
    assert r.exit_code == 0
    assert r.stdout.strip() == "4"
    assert r.timed_out is False


def test_timeout_kills_long_running_code():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=2)
    r = sb.run("import time; time.sleep(10); print('done')")
    assert r.timed_out is True
    assert "done" not in r.stdout


def test_network_is_disabled():
    sb = Sandbox(image="mini-agent-os-sandbox:latest", memory="64m",
                 cpus="0.5", timeout=5)
    code = (
        "import urllib.request\n"
        "try:\n"
        "    urllib.request.urlopen('http://example.com', timeout=2)\n"
        "    print('NET_OK')\n"
        "except Exception as e:\n"
        "    print('NET_BLOCKED', type(e).__name__)\n"
    )
    r = sb.run(code)
    assert "NET_BLOCKED" in r.stdout
