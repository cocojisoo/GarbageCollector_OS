#include "gcos.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static void gcos_sleep_seconds(int seconds)
{
    Sleep((DWORD)seconds * 1000U);
}

static int gcos_mkdir(const char *path, int mode)
{
    (void)mode;
    return _mkdir(path);
}

static int gcos_rmdir(const char *path)
{
    return _rmdir(path);
}

static int gcos_unlink(const char *path)
{
    return _unlink(path);
}

static char *gcos_mkdtemp(char *path)
{
    if (_mktemp_s(path, strlen(path) + 1) != 0) {
        return NULL;
    }
    if (_mkdir(path) != 0) {
        return NULL;
    }
    return path;
}

static int gcos_setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return _putenv_s(name, value);
}

static int gcos_unsetenv(const char *name)
{
    return _putenv_s(name, "");
}
#else
static void gcos_sleep_seconds(int seconds)
{
    sleep((unsigned int)seconds);
}

static int gcos_mkdir(const char *path, int mode)
{
    return mkdir(path, (mode_t)mode);
}

static int gcos_rmdir(const char *path)
{
    return rmdir(path);
}

static int gcos_unlink(const char *path)
{
    return unlink(path);
}

static char *gcos_mkdtemp(char *path)
{
    return mkdtemp(path);
}

static int gcos_setenv(const char *name, const char *value, int overwrite)
{
    return setenv(name, value, overwrite);
}

static int gcos_unsetenv(const char *name)
{
    return unsetenv(name);
}
#endif

static int clamp_int(int value, int min, int max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static int count_marker(const char *text, const char *marker)
{
    int count = 0;
    const char *cursor = text;
    size_t len = strlen(marker);

    while ((cursor = strstr(cursor, marker)) != NULL) {
        count++;
        cursor += len;
    }

    return count;
}

const char *agent_state_name(AgentState state)
{
    switch (state) {
    case AGENT_READY:
        return "READY";
    case AGENT_RUNNING:
        return "RUNNING";
    case AGENT_BLOCKED_ON_APPROVAL:
        return "BLOCKED_ON_APPROVAL";
    case AGENT_DONE:
        return "DONE";
    case AGENT_TIMEOUT:
        return "TIMEOUT";
    case AGENT_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

const char *action_kind_name(ActionKind kind)
{
    switch (kind) {
    case ACTION_NONE:
        return "NONE";
    case ACTION_LIST:
        return "LIST";
    case ACTION_READ:
        return "READ";
    case ACTION_SHELL:
        return "SHELL";
    case ACTION_ROOT:
        return "ROOT";
    case ACTION_KERNEL:
        return "KERNEL";
    case ACTION_CODEX:
        return "CODEX";
    default:
        return "UNKNOWN";
    }
}

void runtime_init(AgentRuntime *runtime)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->next_agent_id = 1;
}

void runtime_log(AgentRuntime *runtime, const char *fmt, ...)
{
    char line[GCOS_TEXT_LEN];
    char stamp[32];
    time_t now = time(NULL);
    struct tm tm_now;
    va_list args;

#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    localtime_r(&now, &tm_now);
#else
    tm_now = *localtime(&now);
#endif
    strftime(stamp, sizeof(stamp), "%H:%M:%S", &tm_now);

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (runtime->log_count >= GCOS_MAX_LOGS) {
        memmove(runtime->logs, runtime->logs + 1,
                sizeof(runtime->logs[0]) * (GCOS_MAX_LOGS - 1));
        runtime->log_count = GCOS_MAX_LOGS - 1;
    }

    snprintf(runtime->logs[runtime->log_count], GCOS_TEXT_LEN, "[%s] %s", stamp,
             line);
    runtime->log_count++;
}

Agent *runtime_create_agent(AgentRuntime *runtime, const char *name,
                            const char *prompt, int priority, int timeout,
                            int quota)
{
    Agent *agent;

    if (runtime->agent_count >= GCOS_MAX_AGENTS) {
        runtime_log(runtime, "agent table full; create rejected");
        return NULL;
    }

    agent = &runtime->agents[runtime->agent_count++];
    memset(agent, 0, sizeof(*agent));
    agent->id = runtime->next_agent_id++;
    snprintf(agent->name, sizeof(agent->name), "%s",
             name && name[0] ? name : "Agent");
    snprintf(agent->prompt, sizeof(agent->prompt), "%s", prompt ? prompt : "");
    agent->priority = clamp_int(priority, 1, 10);
    agent->timeout = clamp_int(timeout, 1, 30);
    agent->quota = clamp_int(quota, 1, 20);
    agent->created_time = time(NULL);
    agent->state = AGENT_READY;
    agent->action.kind = ACTION_NONE;

    runtime_log(runtime, "created agent id=%d name=%s priority=%d timeout=%d quota=%d",
                agent->id, agent->name, agent->priority, agent->timeout,
                agent->quota);
    return agent;
}

Agent *runtime_find_agent(AgentRuntime *runtime, int id)
{
    int i;

    for (i = 0; i < runtime->agent_count; i++) {
        if (runtime->agents[i].id == id) {
            return &runtime->agents[i];
        }
    }

    return NULL;
}

void runtime_reset(AgentRuntime *runtime)
{
    runtime_init(runtime);
    runtime_log(runtime, "runtime reset");
}

void agent_execute(AgentRuntime *runtime, Agent *agent)
{
    ActionRequest action;
    PolicyDecision decision;
    int required_calls;
    int execution_time;

    if (agent == NULL || agent->state != AGENT_READY) {
        return;
    }

    agent->state = AGENT_RUNNING;
    agent->started_time = time(NULL);
    agent->finished_time = 0;
    agent->error[0] = '\0';
    agent->result[0] = '\0';
    runtime_log(runtime, "agent %d started", agent->id);

    action = policy_extract_action(agent->prompt);
    agent->action = action;
    if (action.kind != ACTION_NONE) {
        decision = policy_decide(&action, agent->approval_granted);
        agent->decision = decision;

        if (!decision.allowed) {
            agent->state = decision.state;
            agent->finished_time = time(NULL);
            snprintf(agent->error, sizeof(agent->error), "%s", decision.reason);
            runtime_log(runtime, "agent %d blocked: %s", agent->id,
                        decision.reason);
            return;
        }

        if (!policy_run_action(&action, agent->result, sizeof(agent->result))) {
            agent->state = AGENT_ERROR;
            agent->finished_time = time(NULL);
            snprintf(agent->error, sizeof(agent->error), "%s", agent->result);
            runtime_log(runtime, "agent %d action failed: %s", agent->id,
                        agent->error);
            return;
        }

        agent->used_quota = 1;
        agent->state = AGENT_DONE;
        agent->finished_time = time(NULL);
        runtime_log(runtime, "agent %d completed %s action", agent->id,
                    action_kind_name(action.kind));
        return;
    }

    required_calls = count_marker(agent->prompt, "[CALL]");
    if (required_calls == 0) {
        required_calls = 1;
    }

    if (required_calls > agent->quota) {
        agent->used_quota = agent->quota;
        agent->state = AGENT_ERROR;
        agent->finished_time = time(NULL);
        snprintf(agent->error, sizeof(agent->error),
                 "quota exceeded: required=%d quota=%d", required_calls,
                 agent->quota);
        runtime_log(runtime, "agent %d failed: %s", agent->id, agent->error);
        return;
    }

    execution_time = strstr(agent->prompt, "[SLOW]") ? agent->timeout + 1 : 1;
    if (execution_time > agent->timeout) {
        gcos_sleep_seconds(agent->timeout);
        agent->used_quota = required_calls;
        agent->state = AGENT_TIMEOUT;
        agent->finished_time = time(NULL);
        snprintf(agent->error, sizeof(agent->error),
                 "timeout: required=%d timeout=%d", execution_time,
                 agent->timeout);
        runtime_log(runtime, "agent %d timed out", agent->id);
        return;
    }

    gcos_sleep_seconds(execution_time);
    agent->used_quota = required_calls;
    agent->state = AGENT_DONE;
    agent->finished_time = time(NULL);
    snprintf(agent->result, sizeof(agent->result),
             "simulated LLM agent completed; calls=%d runtime=%ds",
             required_calls, execution_time);
    runtime_log(runtime, "agent %d completed simulated LLM work", agent->id);
}

static int by_fcfs(const void *left, const void *right)
{
    const Agent *a = *(const Agent *const *)left;
    const Agent *b = *(const Agent *const *)right;
    return a->id - b->id;
}

static int by_priority(const void *left, const void *right)
{
    const Agent *a = *(const Agent *const *)left;
    const Agent *b = *(const Agent *const *)right;

    if (a->priority != b->priority) {
        return b->priority - a->priority;
    }

    return a->id - b->id;
}

static void runtime_run(AgentRuntime *runtime, int priority)
{
    Agent *ready[GCOS_MAX_AGENTS];
    int count = 0;
    int i;

    for (i = 0; i < runtime->agent_count; i++) {
        if (runtime->agents[i].state == AGENT_READY) {
            ready[count++] = &runtime->agents[i];
        }
    }

    qsort(ready, (size_t)count, sizeof(ready[0]),
          priority ? by_priority : by_fcfs);

    runtime_log(runtime, "%s scheduler started",
                priority ? "priority" : "fcfs");
    for (i = 0; i < count; i++) {
        agent_execute(runtime, ready[i]);
    }
    runtime_log(runtime, "%s scheduler finished",
                priority ? "priority" : "fcfs");
}

void runtime_run_fcfs(AgentRuntime *runtime)
{
    runtime_run(runtime, 0);
}

void runtime_run_priority(AgentRuntime *runtime)
{
    runtime_run(runtime, 1);
}

void runtime_approve_agent(AgentRuntime *runtime, int id)
{
    Agent *agent = runtime_find_agent(runtime, id);

    if (agent == NULL) {
        runtime_log(runtime, "approve failed; missing agent id=%d", id);
        return;
    }

    if (agent->state != AGENT_BLOCKED_ON_APPROVAL) {
        runtime_log(runtime, "approve ignored; agent %d is %s", id,
                    agent_state_name(agent->state));
        return;
    }

    agent->approval_granted = 1;
    agent->state = AGENT_READY;
    runtime_log(runtime, "agent %d approved by human", id);
    agent_execute(runtime, agent);
}

int self_test(void)
{
    AgentRuntime runtime;
    Agent *safe_agent;
    Agent *blocked_agent;
    ActionRequest sensitive_action;
    PolicyDecision sensitive_decision;
    const char *saved_backend = getenv("GCOS_LLM_BACKEND");
    char saved_backend_copy[128];
    int had_backend = saved_backend != NULL;
    char broker_error[GCOS_TEXT_LEN];
#ifdef _WIN32
    char temp_dir[] = "gcos-policy-test-XXXXXX";
#else
    char temp_dir[] = "/tmp/gcos-policy-test-XXXXXX";
#endif
    char sensitive_dir[512];
    char sensitive_file[512];
    char sensitive_link[512];
    char ssh_dir[512];
    char ssh_link[512];
    char *made_dir;

    runtime_init(&runtime);
    safe_agent = runtime_create_agent(&runtime, "Probe",
                                      "Check kernel [SHELL:uname -a]", 4, 3, 1);
    blocked_agent = runtime_create_agent(
        &runtime, "Root", "Restart daemon [ROOT:systemctl restart sshd]", 9, 3,
        1);

    runtime_run_priority(&runtime);

    if (safe_agent == NULL || blocked_agent == NULL) {
        fprintf(stderr, "self-test failed: agent creation\n");
        return 1;
    }

    if (safe_agent->state != AGENT_DONE) {
        fprintf(stderr, "self-test failed: safe agent state=%s\n",
                agent_state_name(safe_agent->state));
        return 1;
    }

    if (blocked_agent->state != AGENT_BLOCKED_ON_APPROVAL) {
        fprintf(stderr, "self-test failed: blocked agent state=%s\n",
                agent_state_name(blocked_agent->state));
        return 1;
    }

    memset(&sensitive_action, 0, sizeof(sensitive_action));
    sensitive_action.kind = ACTION_READ;
    snprintf(sensitive_action.target, sizeof(sensitive_action.target),
             "/Users/example/.codex/auth.json");
    sensitive_decision = policy_decide(&sensitive_action, 0);
    if (sensitive_decision.allowed ||
        sensitive_decision.state != AGENT_BLOCKED_ON_APPROVAL) {
        fprintf(stderr, "self-test failed: sensitive Codex auth path allowed\n");
        return 1;
    }

    snprintf(sensitive_action.target, sizeof(sensitive_action.target),
             "/Users/example/Desktop/photo.jpg");
    sensitive_decision = policy_decide(&sensitive_action, 0);
    if (sensitive_decision.allowed ||
        sensitive_decision.state != AGENT_BLOCKED_ON_APPROVAL) {
        fprintf(stderr, "self-test failed: macOS Desktop path allowed\n");
        return 1;
    }

    sensitive_action.kind = ACTION_LIST;
    snprintf(sensitive_action.target, sizeof(sensitive_action.target),
             "~/Music");
    sensitive_decision = policy_decide(&sensitive_action, 0);
    if (sensitive_decision.allowed ||
        sensitive_decision.state != AGENT_BLOCKED_ON_APPROVAL) {
        fprintf(stderr, "self-test failed: macOS Music path allowed\n");
        return 1;
    }

    if (policy_run_action(&sensitive_action, broker_error,
                          sizeof(broker_error)) ||
        strstr(broker_error, "macOS privacy path rejected") == NULL) {
        fprintf(stderr,
                "self-test failed: final policy allowed macOS Music path: %s\n",
                broker_error);
        return 1;
    }

    sensitive_action.kind = ACTION_READ;
    snprintf(sensitive_action.target, sizeof(sensitive_action.target),
             "/Users/example/Desktop/photo.jpg");
    if (policy_run_action(&sensitive_action, broker_error,
                          sizeof(broker_error)) ||
        strstr(broker_error, "macOS privacy path rejected") == NULL) {
        fprintf(stderr,
                "self-test failed: final policy allowed macOS Desktop read: %s\n",
                broker_error);
        return 1;
    }

    made_dir = gcos_mkdtemp(temp_dir);
    if (made_dir == NULL) {
        fprintf(stderr, "self-test failed: mkdtemp\n");
        return 1;
    }
    snprintf(sensitive_dir, sizeof(sensitive_dir), "%s/.codex", made_dir);
    snprintf(sensitive_file, sizeof(sensitive_file), "%s/auth.json",
             sensitive_dir);
    snprintf(sensitive_link, sizeof(sensitive_link), "%s/link-to-auth",
             made_dir);
    if (gcos_mkdir(sensitive_dir, 0700) != 0) {
        fprintf(stderr, "self-test failed: mkdir sensitive dir\n");
        gcos_rmdir(made_dir);
        return 1;
    }
    {
        FILE *file = fopen(sensitive_file, "w");
        if (file == NULL) {
            fprintf(stderr, "self-test failed: create sensitive file\n");
            gcos_rmdir(sensitive_dir);
            gcos_rmdir(made_dir);
            return 1;
        }
        fputs("{}", file);
        fclose(file);
    }
#ifndef _WIN32
    if (symlink(sensitive_file, sensitive_link) != 0) {
        fprintf(stderr, "self-test failed: symlink sensitive file\n");
        gcos_unlink(sensitive_file);
        gcos_rmdir(sensitive_dir);
        gcos_rmdir(made_dir);
        return 1;
    }
    snprintf(sensitive_action.target, sizeof(sensitive_action.target), "%s",
             sensitive_link);
    sensitive_decision = policy_decide(&sensitive_action, 0);
    if (policy_run_action(&sensitive_action, broker_error,
                          sizeof(broker_error))) {
        fprintf(stderr,
                "self-test failed: final policy followed sensitive symlink\n");
        gcos_unlink(sensitive_link);
        gcos_unlink(sensitive_file);
        gcos_rmdir(sensitive_dir);
        gcos_rmdir(made_dir);
        return 1;
    }
    gcos_unlink(sensitive_link);
#endif
    gcos_unlink(sensitive_file);
    gcos_rmdir(sensitive_dir);
#ifndef _WIN32
    if (sensitive_decision.allowed ||
        sensitive_decision.state != AGENT_BLOCKED_ON_APPROVAL) {
        fprintf(stderr,
                "self-test failed: symlinked sensitive Codex auth path allowed\n");
        gcos_rmdir(made_dir);
        return 1;
    }
#endif

    snprintf(ssh_dir, sizeof(ssh_dir), "%s/.ssh", made_dir);
    snprintf(ssh_link, sizeof(ssh_link), "%s/link-to-ssh", made_dir);
    if (gcos_mkdir(ssh_dir, 0700) != 0) {
        fprintf(stderr, "self-test failed: mkdir ssh dir\n");
        gcos_rmdir(made_dir);
        return 1;
    }
    sensitive_action.kind = ACTION_LIST;
    snprintf(sensitive_action.target, sizeof(sensitive_action.target), "%s",
             ssh_dir);
    sensitive_decision = policy_decide(&sensitive_action, 0);
    if (sensitive_decision.allowed ||
        sensitive_decision.state != AGENT_BLOCKED_ON_APPROVAL) {
        fprintf(stderr, "self-test failed: exact .ssh directory allowed\n");
        gcos_rmdir(ssh_dir);
        gcos_rmdir(made_dir);
        return 1;
    }
#ifndef _WIN32
    if (symlink(ssh_dir, ssh_link) != 0) {
        fprintf(stderr, "self-test failed: symlink ssh dir\n");
        gcos_rmdir(ssh_dir);
        gcos_rmdir(made_dir);
        return 1;
    }
    snprintf(sensitive_action.target, sizeof(sensitive_action.target), "%s",
             ssh_link);
    sensitive_decision = policy_decide(&sensitive_action, 0);
    if (policy_run_action(&sensitive_action, broker_error,
                          sizeof(broker_error))) {
        fprintf(stderr,
                "self-test failed: final policy followed .ssh symlink\n");
        gcos_unlink(ssh_link);
        gcos_rmdir(ssh_dir);
        gcos_rmdir(made_dir);
        return 1;
    }
    gcos_unlink(ssh_link);
#endif
    gcos_rmdir(ssh_dir);
    gcos_rmdir(made_dir);
#ifndef _WIN32
    if (sensitive_decision.allowed ||
        sensitive_decision.state != AGENT_BLOCKED_ON_APPROVAL) {
        fprintf(stderr, "self-test failed: symlinked .ssh directory allowed\n");
        return 1;
    }
#endif

    if (had_backend) {
        snprintf(saved_backend_copy, sizeof(saved_backend_copy), "%s",
                 saved_backend);
    }
    gcos_setenv("GCOS_LLM_BACKEND", "invalid-test-backend", 1);
    if (codex_broker_run("this should not reach a broker", broker_error,
                         sizeof(broker_error))) {
        fprintf(stderr, "self-test failed: invalid broker backend allowed\n");
        if (had_backend) {
            gcos_setenv("GCOS_LLM_BACKEND", saved_backend_copy, 1);
        } else {
            gcos_unsetenv("GCOS_LLM_BACKEND");
        }
        return 1;
    }
    if (strstr(broker_error, "unsupported GCOS_LLM_BACKEND") == NULL) {
        fprintf(stderr, "self-test failed: invalid broker backend message=%s\n",
                broker_error);
        if (had_backend) {
            gcos_setenv("GCOS_LLM_BACKEND", saved_backend_copy, 1);
        } else {
            gcos_unsetenv("GCOS_LLM_BACKEND");
        }
        return 1;
    }
    if (had_backend) {
        gcos_setenv("GCOS_LLM_BACKEND", saved_backend_copy, 1);
    } else {
        gcos_unsetenv("GCOS_LLM_BACKEND");
    }

    puts("self-test passed");
    return 0;
}

int os_demo_smoke_test(void)
{
    AgentRuntime runtime;
    Agent *probe;
    Agent *quota;
    Agent *kernel;

    runtime_init(&runtime);
    probe = runtime_create_agent(&runtime, "Probe", "Check host [SHELL:uname -a]",
                                 4, 3, 1);
    quota = runtime_create_agent(&runtime, "Quota", "Needs [CALL] [CALL] [CALL]",
                                 6, 3, 2);
    kernel = runtime_create_agent(&runtime, "KernelGuard",
                                  "Touch kernel [KERNEL:load unsigned module]",
                                  9, 3, 1);

    if (probe == NULL || quota == NULL || kernel == NULL) {
        fprintf(stderr, "os-demo-smoke failed: agent creation\n");
        return 1;
    }

    runtime_run_priority(&runtime);

    if (kernel->state != AGENT_BLOCKED_ON_APPROVAL ||
        kernel->action.kind != ACTION_KERNEL) {
        fprintf(stderr, "os-demo-smoke failed: kernel guard state=%s action=%s\n",
                agent_state_name(kernel->state),
                action_kind_name(kernel->action.kind));
        return 1;
    }

    if (quota->state != AGENT_ERROR || quota->used_quota != quota->quota) {
        fprintf(stderr, "os-demo-smoke failed: quota state=%s used=%d quota=%d\n",
                agent_state_name(quota->state), quota->used_quota,
                quota->quota);
        return 1;
    }

    if (probe->state != AGENT_DONE || probe->action.kind != ACTION_SHELL) {
        fprintf(stderr, "os-demo-smoke failed: probe state=%s action=%s\n",
                agent_state_name(probe->state),
                action_kind_name(probe->action.kind));
        return 1;
    }

    if (runtime.log_count < 7 ||
        strstr(runtime.logs[4], "agent 3 started") == NULL ||
        strstr(runtime.logs[6], "agent 2 started") == NULL) {
        fprintf(stderr, "os-demo-smoke failed: priority execution log missing\n");
        return 1;
    }

    puts("os-demo-smoke passed");
    return 0;
}
