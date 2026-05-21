# Mini Agent OS

팀 GarbageCollector · 9주차 프로젝트 · 방향 A: LLM을 위한 운영체제

LLM 에이전트를 운영체제의 프로세스처럼 다루는 작은 런타임이다. 각
에이전트는 id, 상태, 우선순위, timeout, quota, 결과, 에러, 생성/시작/종료
시간을 가진다. TUI에서 에이전트를 만들고, ready 상태인 에이전트를 스케줄러가
고른 뒤 실행한다.

GUI 앱으로 묶지 않고 콘솔 프로그램으로 만들었다. macOS/Linux에서는 `make`로
바로 빌드하고, Windows에서는 MSYS2/MinGW 환경을 기준으로 같은 TUI를 실행한다.

## 지금 들어간 기능

- Agent Control Block: id, name, prompt, priority, timeout, quota, state,
  result, error, timestamp
- 상태 전이: `READY -> RUNNING -> DONE/TIMEOUT/ERROR`
- 스케줄러: FCFS, Priority, Round Robin
- ready queue: 별도 큐 객체 대신 `AgentRuntime.agents[]`에서 `READY`만 골라 사용
- quota: `[CALL]` 개수로 API 사용량 계산
- timeout: `[SLOW]` 프롬프트로 초과 실행 상황 재현
- LLM 실행: 시작할 때 API key를 입력하면 agent 실행 중 broker를 통해 실제 모델 호출
- policy gate: `[SHELL:...]`, `[READ:...]`, `[ROOT:...]`, `[KERNEL:...]`,
  `[CODEX:...]` 형태의 요청을 분류하고 위험한 요청은 차단
- execution log: 생성, 스케줄링, 실행, quota error, timeout, broker 호출 기록
- TUI dashboard: agent table, result/error, log를 터미널에서 확인

## 구조

```text
User Terminal
    |
    v
TUI command loop
    |  create / list / run fcfs / run priority / run rr / logs
    v
AgentRuntime
    |  agent table, log buffer, llm_enabled
    v
Ready selection
    |  state == READY
    +--------------------+--------------------+--------------------+
    |                    |                    |
    v                    v                    v
FCFS scheduler      Priority scheduler    Round-robin scheduler
id order            priority desc         quantum = 1
    |                    |                    |
    +--------------------+--------------------+
                         |
                         v
Executor
    |  state update
    |  quota check
    |  timeout check
    |  policy check
    |  LLM broker call if enabled
    v
Agent result / error / runtime log
```

## 빠른 실행

```sh
git clone https://github.com/cocojisoo/GarbageCollector_OS.git
cd GarbageCollector_OS
git checkout codex/tui-mini-agent-os

make
make run
```

실행하면 먼저 API key를 묻는다.

```text
api key:
```

Upstage/OpenAI 호환 키를 넣으면 agent가 실제 LLM broker를 호출한다. Enter만
누르면 네트워크 호출 없이 스케줄러와 상태 전이만 확인하는 모드로 실행된다. 입력한 키는
프로세스 환경 변수에만 넣고 파일로 저장하지 않는다.

Windows 쪽은 MSYS2/MinGW 기준이다.

```sh
mingw32-make CC=gcc EXEEXT=.exe
build\gcos-tui.exe
```

## TUI 명령

| 명령 | 동작 |
| --- | --- |
| `create` | agent 생성 |
| `list` | agent table 출력 |
| `run fcfs` | 생성 순서대로 실행 |
| `run priority` | priority 높은 순서대로 실행 |
| `run rr` | quantum 1 기준 round-robin 실행 |
| `demo` | scheduling/quota/timeout 예시 agent 생성 |
| `logs` | execution log 출력 |
| `clear` | agent와 log 초기화 |
| `help` | 도움말 |
| `quit` | 종료 |

## 데모 흐름

### Priority

```text
gcos> demo
gcos> run priority
```

`AgentB`의 priority가 가장 높아서 먼저 실행된다. quota를 넘긴 agent는 `ERROR`,
`[SLOW]`가 붙은 agent는 `TIMEOUT`으로 끝난다.

### FCFS

```text
gcos> clear
gcos> demo
gcos> run fcfs
```

priority와 상관없이 생성 id 순서대로 실행된다.

### Round Robin

```text
gcos> clear
gcos> demo
gcos> run rr
```

offline 모드에서는 `[CALL]` 하나를 작업 단위 하나로 보고, agent가 한 quantum을
쓴 뒤 아직 일이 남아 있으면 다시 `READY`로 돌아간다. LLM 모드에서는 HTTP 호출을
중간에 끊을 수 없으므로 RR이 agent 선택 순서를 정하고, 선택된 agent의 LLM 호출은
한 번에 끝낸다.

### LLM 실행

```text
api key: up_... 또는 sk-...
gcos> create
name: Explainer
prompt: llm: FCFS scheduling을 한국어 두 문장으로 설명해줘
priority 1-10 [5]: 5
timeout seconds 1-30 [3]: 10
quota 1-20 [2]: 1
gcos> run priority
```

실행이 끝나면 `result`에 모델 응답이 들어간다.

## OS 개념 매핑

| OS 개념 | 코드 | 구현 방식 |
| --- | --- | --- |
| Process | `include/gcos.h` | `Agent` 구조체 |
| PID | `include/gcos.h` | `Agent.id` |
| PCB | `include/gcos.h` | state, priority, timeout, quota, result, error |
| Process state | `include/gcos.h`, `src/agent.c` | READY/RUNNING/DONE/TIMEOUT/ERROR |
| Ready queue | `src/agent.c` | `READY` agent만 골라 scheduler input 구성 |
| FCFS | `src/agent.c` | id 오름차순 |
| Priority | `src/agent.c` | priority 내림차순, 동률이면 id 오름차순 |
| Round Robin | `src/agent.c` | quantum 1 기준 반복 실행 |
| Resource quota | `src/agent.c` | `[CALL]` count와 `Agent.quota` 비교 |
| Timeout | `src/agent.c` | `[SLOW]`로 timeout 경로 재현 |
| LLM broker | `src/agent.c`, `src/codex_broker.c` | API key가 있으면 agent 실행 중 모델 호출 |
| Policy gate | `src/policy.c` | action marker 파싱 후 위험 요청 차단 |
| Trace log | `src/agent.c` | ring buffer log |
| Dashboard | `src/tui.c` | terminal table과 logs |

## 파일 구조

```text
GarbageCollector_OS/
├── include/
│   └── gcos.h
├── src/
│   ├── main.c
│   ├── tui.c
│   ├── agent.c
│   ├── policy.c
│   └── codex_broker.c
├── docs/
│   ├── dev-brief.md
│   ├── project-alignment.md
│   ├── tui-architecture.md
│   └── gcos-roadmap.md
├── scripts/
│   └── gcos-context
├── Makefile
└── README.md
```

## 빌드와 테스트

| 명령 | 내용 |
| --- | --- |
| `make` | `build/gcos-tui` 빌드 |
| `make run` | TUI 실행 |
| `make check` | 기본 smoke test 실행 |
| `make portable-check` | C source syntax check |
| `make api-check` | 실제 API key로 LLM 호출 확인 |
| `make context` | 프로젝트 요약 출력 |
| `make clean` | 빌드 산출물 삭제 |

`make check`에 들어있는 항목:

| 항목 | 확인 내용 |
| --- | --- |
| `--self-test` | policy guard, 민감 경로 차단, 잘못된 broker backend |
| `--os-demo-smoke` | priority order, quota error, kernel policy block |
| `--tui-smoke` | TUI scheduling 상태 |
| `--rr-smoke` | round-robin scheduling과 quota |
| `--input-smoke` | 입력 경로 기본 동작 |
| `--wrap-smoke` | 긴 텍스트 출력 준비 |
| `--api-config-smoke` | API 설정 파싱 |

## 발표 때 잡을 포인트

이 프로젝트에서 중요한 건 “운영체제 위에서 실행된다”가 아니라, agent runtime 안에
운영체제 개념을 직접 대응시켰다는 점이다.

- agent = process
- id = PID
- `Agent` 구조체 = PCB
- `READY` 필터링 = ready queue
- FCFS/Priority/RR = scheduler policy
- `[CALL]` = resource quota
- `[SLOW]` = timeout
- API key broker = LLM agent execution
- runtime log = trace log
- TUI = process table/dashboard
