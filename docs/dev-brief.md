# GCOS Dev Brief

처음 읽을 개발 메모.

## 핵심 파일

- `src/tui.c`: TUI, API key 입력, command loop
- `src/agent.c`: agent table, FCFS/Priority/RR scheduler, quota, timeout, log
- `src/policy.c`: action marker 파싱과 위험 요청 차단
- `src/codex_broker.c`: Upstage/OpenAI 호환 LLM API 호출
- `include/gcos.h`: shared struct, enum, function declaration

## 과제 요구와 연결

- agent 생성/관리
- ready 상태 agent 관리
- FCFS, Priority, Round Robin scheduling
- READY/RUNNING/DONE/TIMEOUT/ERROR 상태 추적
- `[CALL]` 기반 quota 계산
- `[SLOW]` 기반 timeout 처리
- execution log
- TUI dashboard
- API key 기반 LLM agent 실행

## LLM 실행

TUI 시작 시 API key를 입력받는다. 키가 있으면 agent 실행 중 broker를 호출하고,
응답을 agent result로 저장한다.

사용 가능한 환경 변수:

- `GCOS_LLM_API_KEY`
- `UPSTAGE_API_KEY`
- `OPENAI_API_KEY`

Enter만 치면 offline scheduler test 모드로 실행된다.

## 자주 쓰는 명령

```sh
make context
make check
make portable-check
make run
```
