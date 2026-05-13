from datetime import datetime
from pathlib import Path

LOG_DIR = Path("logs")
LOG_FILE = LOG_DIR / "execution_log.txt"


def init_log():
    LOG_DIR.mkdir(exist_ok=True)
    if not LOG_FILE.exists():
        LOG_FILE.write_text("", encoding="utf-8")


def log_event(message: str):
    init_log()

    now = datetime.now().strftime("%H:%M:%S")
    log_line = f"[{now}] {message}"

    with LOG_FILE.open("a", encoding="utf-8") as f:
        f.write(log_line + "\n")

    print(log_line)


def get_logs():
    init_log()
    return LOG_FILE.read_text(encoding="utf-8").splitlines()


def clear_logs():
    init_log()
    LOG_FILE.write_text("", encoding="utf-8")