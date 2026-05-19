# Mini Agent OS

> Team **GarbageCollector** · Week 9 Project, Direction A (OS-for-LLM)

OS-inspired runtime that schedules LLM agents like processes on a worker pool, executes generated code inside Docker sandboxes, and chains agents into pipelines through a bounded message bus.

## Quickstart

```bash
uv sync                          # install deps from uv.lock
cp .env.example .env             # add your UPSTAGE_API_KEY
docker compose build             # build app + sandbox images
docker compose up                # start
# Then open http://localhost:8000 in your browser
```

## Local development (without Docker for the app)

```bash
uv sync
docker compose build sandbox-build   # sandbox image must exist
uv run uvicorn app.main:app --reload
uv run pytest -m "not docker"        # fast tests
uv run pytest                        # all tests (needs docker)
```

## Documentation
- Implementation plan — `docs/superpowers/plans/2026-05-20-mini-agent-os.md`
- Technical report — `docs/technical-report.md`
- Development process — `docs/development-process.md`
- Demo script — `docs/demo-script.md`
