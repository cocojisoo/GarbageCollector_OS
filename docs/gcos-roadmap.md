# GCOS Roadmap

GCOS의 중심은 Mini Agent OS다. LLM을 붙인 shell이 아니라, LLM agent를
process처럼 관리하는 runtime으로 잡는다.

## 현재 단계

```text
GCOS C TUI
  -> AgentRuntime
  -> FCFS / Priority / Round Robin
  -> quota / timeout
  -> policy gate
  -> LLM broker
```

현재 브랜치에서 보여줄 수 있는 것:

- agent 생성과 상태 전이
- ready agent 선택
- 세 가지 스케줄링 정책
- quota 초과와 timeout
- 위험 action 차단
- API key와 model name 기반 LLM 실행
- execution log

## 다음 단계

1. TUI 사용성 정리
   - 긴 이름/한글 출력 폭 보정
   - demo case를 발표 순서에 맞게 정리

2. action broker 정리
   - `read`, `list`, `shell`, `kernel`, `root`, `codex` action별 결과 문구 정리
   - 위험 action은 계속 approval/block 상태로 유지

3. system observation
   - process snapshot
   - file activity watcher
   - network summary

4. kernel-adjacent prototype
   - Linux: eBPF/LSM/auditd/cgroups 중 하나로 작은 관찰 레이어 실험
   - macOS: Endpoint Security나 System Extension은 별도 단계로 분리

5. 배포 형태
   - VM 또는 작은 Linux image
   - GCOS를 shell/session manager처럼 띄우는 방식 검토

## 지킬 것

- LLM이 root shell을 마음대로 실행하게 두지 않는다.
- token, key, auth 파일을 runtime이 직접 긁어오지 않는다.
- 위험 작업은 typed action, log, approval을 거친다.
- 과제 설명에서는 Mini Agent OS 구조를 중심에 둔다.
