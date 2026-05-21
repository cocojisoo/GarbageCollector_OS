#ifndef GCOS_H
#define GCOS_H

#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stddef.h>
#include <time.h>

#define GCOS_MAX_AGENTS 32
#define GCOS_MAX_LOGS 256
#define GCOS_NAME_LEN 48
#define GCOS_PROMPT_LEN 512
#define GCOS_TEXT_LEN 2048
#define GCOS_ACTION_LEN 512
#define GCOS_RESPONSE_LEN 65536

typedef enum {
    AGENT_READY,
    AGENT_RUNNING,
    AGENT_BLOCKED_ON_APPROVAL,
    AGENT_DONE,
    AGENT_TIMEOUT,
    AGENT_ERROR
} AgentState;

typedef enum {
    ACTION_NONE,
    ACTION_LIST,
    ACTION_READ,
    ACTION_SHELL,
    ACTION_ROOT,
    ACTION_KERNEL,
    ACTION_CODEX
} ActionKind;

typedef struct {
    ActionKind kind;
    char target[GCOS_ACTION_LEN];
    int requires_root;
} ActionRequest;

typedef struct {
    int allowed;
    AgentState state;
    int requires_approval;
    int requires_privileged_broker;
    char reason[GCOS_TEXT_LEN];
} PolicyDecision;

typedef struct {
    int id;
    char name[GCOS_NAME_LEN];
    char prompt[GCOS_PROMPT_LEN];
    int priority;
    int timeout;
    int quota;
    int used_quota;
    int approval_granted;
    time_t created_time;
    time_t started_time;
    time_t finished_time;
    AgentState state;
    ActionRequest action;
    PolicyDecision decision;
    char result[GCOS_TEXT_LEN];
    char error[GCOS_TEXT_LEN];
} Agent;

typedef struct {
    Agent agents[GCOS_MAX_AGENTS];
    int agent_count;
    int next_agent_id;
    char logs[GCOS_MAX_LOGS][GCOS_TEXT_LEN];
    int log_count;
} AgentRuntime;

void runtime_init(AgentRuntime *runtime);
Agent *runtime_create_agent(AgentRuntime *runtime, const char *name,
                            const char *prompt, int priority, int timeout,
                            int quota);
Agent *runtime_find_agent(AgentRuntime *runtime, int id);
void runtime_reset(AgentRuntime *runtime);
void runtime_log(AgentRuntime *runtime, const char *fmt, ...);
void runtime_run_fcfs(AgentRuntime *runtime);
void runtime_run_priority(AgentRuntime *runtime);
void runtime_approve_agent(AgentRuntime *runtime, int id);
void agent_execute(AgentRuntime *runtime, Agent *agent);

const char *agent_state_name(AgentState state);
const char *action_kind_name(ActionKind kind);

ActionRequest policy_extract_action(const char *prompt);
PolicyDecision policy_decide(const ActionRequest *action, int approval_granted);
int policy_run_action(const ActionRequest *action, char *out, size_t out_size);
const char *codex_broker_backend_name(void);
int codex_broker_run(const char *prompt, char *out, size_t out_size);

int tui_run(AgentRuntime *runtime);
int self_test(void);
int os_demo_smoke_test(void);
int tui_smoke_test(void);
int input_smoke_test(void);
int wrap_smoke_test(void);
int api_config_smoke_test(void);
int codex_smoke_test(void);
int api_smoke_test(void);

#endif
