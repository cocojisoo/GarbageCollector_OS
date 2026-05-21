# Portable TUI Architecture

GCOS is now a single console binary:

```text
gcos-tui
  -> TUI command loop
  -> AgentRuntime process table
  -> FCFS / priority scheduler
  -> quota and timeout simulation
  -> execution log ring buffer
  -> optional typed policy layer
```

The TUI replaces both the upstream FastAPI dashboard and the later macOS GUI.
This keeps the implementation easy to run from a terminal and makes Windows
support realistic.

## Build Surface

- no SDL
- no Pango/Cairo
- no `.app`
- no code signing
- no browser server required

The default target is `build/gcos-tui` or `build/gcos-tui.exe` when
`EXEEXT=.exe` is passed.

## Commands

The TUI commands mirror the upstream API endpoints:

| Upstream endpoint | TUI command |
| --- | --- |
| `POST /agents` | `create` |
| `GET /agents` | `list` |
| `POST /run/fcfs` | `run fcfs` |
| `POST /run/priority` | `run priority` |
| `GET /logs` | `logs` |
| `DELETE /agents` | `clear` |

## Windows Notes

The primary console path avoids POSIX-only UI dependencies. Windows builds
should use MinGW/MSYS2:

```sh
mingw32-make CC=gcc EXEEXT=.exe
```

Optional broker/network checks are not required for the TUI assignment demo.
Normal verification stays local with `make check`. Actual Windows build
verification is still pending until a MinGW/MSYS2 environment runs it.
