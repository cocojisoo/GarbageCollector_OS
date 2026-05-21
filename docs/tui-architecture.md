# TUI Architecture

현재 구조는 단일 콘솔 바이너리다.

```text
gcos-tui
  -> TUI command loop
  -> AgentRuntime process table
  -> FCFS / Priority / Round Robin scheduler
  -> quota / timeout / policy check
  -> LLM broker call
  -> execution log
```

GUI 앱 번들, 브라우저 서버, SDL/Pango/Cairo 없이 터미널에서 바로 실행한다.

## 빌드

```sh
make
make run
```

Windows는 MSYS2/MinGW 기준이다.

```sh
mingw32-make CC=gcc EXEEXT=.exe
build\gcos-tui.exe
```

## 명령 매핑

| 기능 | TUI 명령 |
| --- | --- |
| agent 생성 | `create` |
| agent 목록 | `list` |
| FCFS 실행 | `run fcfs` |
| Priority 실행 | `run priority` |
| Round Robin 실행 | `run rr` |
| log 확인 | `logs` |
| 초기화 | `clear` |

## LLM 경로

TUI가 시작할 때 API key를 묻는다. 키가 있으면 runtime의 `llm_enabled`가 켜지고,
executor가 agent prompt를 broker로 넘긴다. broker는 Upstage/OpenAI 호환 API를
`curl`로 호출한다.

키를 입력하지 않으면 scheduling, quota, timeout을 offline으로 확인한다.
