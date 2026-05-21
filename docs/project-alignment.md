# Project Alignment

원본 `cocojisoo/GarbageCollector_OS`의 Mini Agent OS 방향을 C TUI로 맞춘
내용이다. 핵심은 LLM agent를 process처럼 다루는 runtime이다.

## 요구사항 대응

| 요구 | 현재 코드 |
| --- | --- |
| agent 생성/관리 | `runtime_create_agent` |
| ready queue | `AgentRuntime.agents[]`에서 `READY`만 선택 |
| FCFS | `runtime_run_fcfs` |
| Priority | `runtime_run_priority` |
| Round Robin | `runtime_run_round_robin` |
| 상태 추적 | `AgentState` |
| timeout | `[SLOW]` 처리 |
| quota | `[CALL]` count |
| 실제 LLM 실행 | TUI API key 입력 + `codex_broker_run_with_timeout` |
| execution log | `runtime_log` |
| dashboard | `src/tui.c` |

## 데모 순서

1. `make run`
2. API key 입력. 네트워크 없이 보여줄 때는 Enter.
3. `demo`
4. `run priority`, `run fcfs`, `run rr` 비교
5. `logs`로 실행 기록 확인

설명할 때는 Agent=Process, id=PID, `Agent`=PCB, READY 필터링=ready queue,
scheduler=FCFS/Priority/RR, `[CALL]`=quota, `[SLOW]`=timeout, broker=LLM 실행
경로로 잡으면 된다.

## 선 긋기

이 브랜치는 C TUI 런타임이다. Docker sandbox, SSE dashboard, threaded worker
pool, IPC message bus는 이 코드의 기능으로 설명하지 않는다.
