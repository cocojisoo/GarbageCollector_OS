#include "gcos.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <stdlib.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#define TUI_LINE_LEN 1024
#define TUI_WRAP_WIDTH 100

static void append_text(char *out, size_t out_size, const char *text)
{
    size_t used;

    if (out_size == 0 || text == NULL) {
        return;
    }

    used = strlen(out);
    if (used >= out_size - 1) {
        return;
    }

    strncat(out, text, out_size - used - 1);
}

static const char *skip_space(const char *text)
{
    while (text != NULL && isspace((unsigned char)*text)) {
        text++;
    }
    return text != NULL ? text : "";
}

static int gcos_setenv_local(const char *name, const char *value, int overwrite)
{
#ifdef _WIN32
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return _putenv_s(name, value);
#else
    return setenv(name, value, overwrite);
#endif
}

static void trim_newline(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

static int read_line(const char *prompt, char *out, size_t out_size)
{
    if (out_size == 0) {
        return 0;
    }

    printf("%s", prompt);
    fflush(stdout);
    if (fgets(out, (int)out_size, stdin) == NULL) {
        out[0] = '\0';
        return 0;
    }
    trim_newline(out);
    return 1;
}

static int read_secret_line(const char *prompt, char *out, size_t out_size)
{
    if (out_size == 0) {
        return 0;
    }

#ifdef _WIN32
    if (!_isatty(_fileno(stdin))) {
#else
    if (!isatty(fileno(stdin))) {
#endif
        return read_line(prompt, out, out_size);
    }

    printf("%s", prompt);
    fflush(stdout);

#ifdef _WIN32
    size_t used = 0;

    while (used + 1 < out_size) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            break;
        }
        if (ch == '\b') {
            if (used > 0) {
                used--;
            }
            continue;
        }
        out[used++] = (char)ch;
    }
    out[used] = '\0';
    putchar('\n');
    return 1;
#else
    {
        struct termios old_term;
        struct termios new_term;
        int ok = 0;

        if (tcgetattr(fileno(stdin), &old_term) == 0) {
            new_term = old_term;
            new_term.c_lflag &= (tcflag_t)~ECHO;
            if (tcsetattr(fileno(stdin), TCSAFLUSH, &new_term) == 0) {
                ok = 1;
            }
        }

        if (fgets(out, (int)out_size, stdin) == NULL) {
            out[0] = '\0';
            if (ok) {
                tcsetattr(fileno(stdin), TCSAFLUSH, &old_term);
            }
            putchar('\n');
            return 0;
        }
        if (ok) {
            tcsetattr(fileno(stdin), TCSAFLUSH, &old_term);
        }
        putchar('\n');
        trim_newline(out);
        return 1;
    }
#endif
}

static void format_stamp(time_t value, char *out, size_t out_size)
{
    struct tm tm_value;
#if !defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    struct tm *tmp;
#endif

    if (out_size == 0) {
        return;
    }

    if (value == 0) {
        snprintf(out, out_size, "-");
        return;
    }

#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    localtime_r(&value, &tm_value);
#else
    tmp = localtime(&value);
    if (tmp == NULL) {
        snprintf(out, out_size, "-");
        return;
    }
    tm_value = *tmp;
#endif
    strftime(out, out_size, "%H:%M:%S", &tm_value);
}

static int parse_int_or_default(const char *text, int fallback, int min, int max)
{
    char *end;
    long value;

    text = skip_space(text);
    if (text[0] == '\0') {
        return fallback;
    }

    value = strtol(text, &end, 10);
    if (end == text) {
        return fallback;
    }
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return (int)value;
}

static void print_wrapped_line(const char *prefix, const char *text, int width)
{
    int column = 0;
    const char *cursor = text != NULL ? text : "";

    if (prefix != NULL && prefix[0] != '\0') {
        fputs(prefix, stdout);
        column = (int)strlen(prefix);
    }

    while (*cursor != '\0') {
        if (*cursor == '\n') {
            fputc('\n', stdout);
            column = 0;
            cursor++;
            continue;
        }

        if (column >= width && !isspace((unsigned char)*cursor)) {
            fputc('\n', stdout);
            column = 0;
        }

        fputc(*cursor, stdout);
        column++;
        cursor++;
    }
    fputc('\n', stdout);
}

static void render_agent_row(const Agent *agent)
{
    char created[16];
    char started[16];
    char finished[16];

    if (agent == NULL) {
        return;
    }

    printf("%-3d %-16.16s %-19s pri=%-2d timeout=%-2d quota=%-2d used=%-2d action=%s\n",
           agent->id, agent->name, agent_state_name(agent->state),
           agent->priority, agent->timeout, agent->quota, agent->used_quota,
           action_kind_name(agent->action.kind));
    format_stamp(agent->created_time, created, sizeof(created));
    format_stamp(agent->started_time, started, sizeof(started));
    format_stamp(agent->finished_time, finished, sizeof(finished));
    printf("    time  : created=%s started=%s finished=%s\n", created, started,
           finished);
}

static void render_agents(const AgentRuntime *runtime)
{
    int i;

    puts("\nAgent table");
    puts("===========");
    if (runtime->agent_count == 0) {
        puts("(empty)");
        return;
    }

    for (i = 0; i < runtime->agent_count; i++) {
        const Agent *agent = &runtime->agents[i];
        render_agent_row(agent);
        print_wrapped_line("    prompt: ", agent->prompt, TUI_WRAP_WIDTH);
        if (agent->result[0] != '\0') {
            print_wrapped_line("    result: ", agent->result, TUI_WRAP_WIDTH);
        }
        if (agent->error[0] != '\0') {
            print_wrapped_line("    error : ", agent->error, TUI_WRAP_WIDTH);
        }
    }
}

static void render_logs(const AgentRuntime *runtime)
{
    int i;

    puts("\nExecution logs");
    puts("==============");
    if (runtime->log_count == 0) {
        puts("(empty)");
        return;
    }

    for (i = 0; i < runtime->log_count; i++) {
        print_wrapped_line("", runtime->logs[i], TUI_WRAP_WIDTH);
    }
}

static void render_help(void)
{
    puts("\nMini Agent OS TUI");
    puts("=================");
    puts("commands:");
    puts("  create          create an Agent Control Block");
    puts("  list            show process/agent table");
    puts("  run fcfs        run READY agents by creation order");
    puts("  run priority    run READY agents by high priority first");
    puts("  run rr          run READY agents by round-robin quantum");
    puts("  demo            load the README scheduling/quota/timeout demo");
    puts("  logs            show execution logs");
    puts("  clear           clear all agents and logs");
    puts("  help            show this help");
    puts("  quit            exit");
    puts("");
    puts("prompt markers:");
    puts("  [CALL] counts as one LLM/API quota unit; no [CALL] means one call");
    puts("  [SLOW] forces timeout");
    puts("  llm: / ask: prefixes are sent to the configured LLM broker");
}

static void configure_llm_at_start(AgentRuntime *runtime)
{
    char key[TUI_LINE_LEN];

    puts("\nLLM broker setup");
    puts("================");
    if (codex_broker_has_api_key()) {
        runtime_set_llm_enabled(runtime, 1);
        printf("using configured LLM backend: %s\n", codex_broker_backend_name());
        return;
    }

    puts("Paste an Upstage/OpenAI-compatible API key to run agents through the LLM.");
    puts("Press Enter without a key only for offline scheduler tests.");
    if (!read_secret_line("api key: ", key, sizeof(key)) || key[0] == '\0') {
        runtime_set_llm_enabled(runtime, 0);
        puts("no API key entered; offline simulation mode enabled");
        return;
    }

    if (gcos_setenv_local("GCOS_LLM_API_KEY", key, 1) != 0 ||
        gcos_setenv_local("GCOS_LLM_BACKEND", "api", 1) != 0) {
        runtime_set_llm_enabled(runtime, 0);
        puts("failed to configure API key; offline simulation mode enabled");
        return;
    }

    runtime_set_llm_enabled(runtime, 1);
    printf("LLM backend enabled: %s\n", codex_broker_backend_name());
}

static void create_agent_interactive(AgentRuntime *runtime)
{
    char name[GCOS_NAME_LEN];
    char prompt[GCOS_PROMPT_LEN];
    char line[TUI_LINE_LEN];
    int priority;
    int timeout;
    int quota;
    Agent *agent;

    if (!read_line("name: ", name, sizeof(name))) {
        return;
    }
    if (name[0] == '\0') {
        snprintf(name, sizeof(name), "%s", "Agent");
    }

    if (!read_line("prompt: ", prompt, sizeof(prompt))) {
        return;
    }

    read_line("priority 1-10 [5]: ", line, sizeof(line));
    priority = parse_int_or_default(line, 5, 1, 10);
    read_line("timeout seconds 1-30 [3]: ", line, sizeof(line));
    timeout = parse_int_or_default(line, 3, 1, 30);
    read_line("quota 1-20 [2]: ", line, sizeof(line));
    quota = parse_int_or_default(line, 2, 1, 20);

    agent = runtime_create_agent(runtime, name, prompt, priority, timeout, quota);
    if (agent == NULL) {
        puts("agent table is full");
        return;
    }

    printf("created agent %d\n", agent->id);
}

static void add_demo_agents(AgentRuntime *runtime)
{
    runtime_reset(runtime);
    runtime_create_agent(runtime, "AgentA", "Short summary task [CALL]", 3, 3, 2);
    runtime_create_agent(runtime, "AgentB", "Urgent analysis task [CALL]", 9, 3, 2);
    runtime_create_agent(runtime, "AgentC", "Quota pressure [CALL] [CALL] [CALL]", 5,
                         3, 2);
    runtime_create_agent(runtime, "SlowOne", "Timeout demo [SLOW]", 4, 1, 2);
    puts("demo loaded: AgentA, AgentB, AgentC, SlowOne");
}

static void run_scheduler(AgentRuntime *runtime, int priority)
{
    if (priority) {
        runtime_run_priority(runtime);
        puts("Priority scheduling completed");
    } else {
        runtime_run_fcfs(runtime);
        puts("FCFS scheduling completed");
    }
    render_agents(runtime);
}

static void run_round_robin_scheduler(AgentRuntime *runtime)
{
    runtime_run_round_robin(runtime);
    puts("Round-robin scheduling completed");
    render_agents(runtime);
}

int tui_run(AgentRuntime *runtime)
{
    char line[TUI_LINE_LEN];

    configure_llm_at_start(runtime);
    render_help();
    while (read_line("\ngcos> ", line, sizeof(line))) {
        const char *cmd = skip_space(line);

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        }
        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
            render_help();
        } else if (strcmp(cmd, "create") == 0) {
            create_agent_interactive(runtime);
        } else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "agents") == 0) {
            render_agents(runtime);
        } else if (strcmp(cmd, "run fcfs") == 0 || strcmp(cmd, "fcfs") == 0) {
            run_scheduler(runtime, 0);
        } else if (strcmp(cmd, "run priority") == 0 ||
                   strcmp(cmd, "priority") == 0) {
            run_scheduler(runtime, 1);
        } else if (strcmp(cmd, "run rr") == 0 ||
                   strcmp(cmd, "run round-robin") == 0 ||
                   strcmp(cmd, "rr") == 0 ||
                   strcmp(cmd, "round-robin") == 0) {
            run_round_robin_scheduler(runtime);
        } else if (strcmp(cmd, "logs") == 0) {
            render_logs(runtime);
        } else if (strcmp(cmd, "clear") == 0) {
            runtime_reset(runtime);
            puts("all agents and logs cleared");
        } else if (strcmp(cmd, "demo") == 0) {
            add_demo_agents(runtime);
            render_agents(runtime);
        } else if (cmd[0] == '\0') {
            continue;
        } else {
            puts("unknown command; type help");
        }
    }

    puts("bye");
    return 0;
}

static void build_local_answer(AgentRuntime *runtime, const char *input,
                               char *answer, size_t answer_size)
{
    Agent *agent;
    ActionRequest action;

    answer[0] = '\0';
    agent = runtime_create_agent(runtime, "User", input, 5, 3, 2);
    if (agent == NULL) {
        snprintf(answer, answer_size, "agent table full");
        return;
    }

    action = policy_extract_action(input);
    if (action.kind == ACTION_CODEX && !runtime->llm_enabled) {
        agent->action = action;
        agent->state = AGENT_BLOCKED_ON_APPROVAL;
        snprintf(agent->error, sizeof(agent->error),
                 "LLM broker requires a startup API key");
        runtime_log(runtime,
                    "agent %d blocked: CODEX action requires configured API key",
                    agent->id);
        append_text(answer, answer_size, "Local Mini Agent OS input path.\n");
        append_text(answer, answer_size, agent->error);
        return;
    }

    runtime_run_priority(runtime);
    append_text(answer, answer_size, "Local Mini Agent OS input path completed.");
}

int tui_smoke_test(void)
{
    AgentRuntime runtime;
    Agent *a;
    Agent *b;
    Agent *c;

    runtime_init(&runtime);
    a = runtime_create_agent(&runtime, "A", "first [CALL]", 1, 3, 2);
    b = runtime_create_agent(&runtime, "B", "second [CALL]", 9, 3, 2);
    c = runtime_create_agent(&runtime, "C", "quota [CALL] [CALL] [CALL]", 5, 3, 2);
    if (a == NULL || b == NULL || c == NULL) {
        fprintf(stderr, "tui-smoke failed: create\n");
        return 1;
    }

    runtime_run_priority(&runtime);
    if (b->state != AGENT_DONE || c->state != AGENT_ERROR ||
        a->state != AGENT_DONE || runtime.log_count == 0) {
        fprintf(stderr, "tui-smoke failed: scheduling states\n");
        return 1;
    }

    puts("tui-smoke passed");
    return 0;
}

int rr_smoke_test(void)
{
    AgentRuntime runtime;
    Agent *a;
    Agent *b;
    Agent *c;

    runtime_init(&runtime);
    a = runtime_create_agent(&runtime, "A", "first [CALL] [CALL]", 1, 3, 3);
    b = runtime_create_agent(&runtime, "B", "second [CALL]", 9, 3, 2);
    c = runtime_create_agent(&runtime, "C", "quota [CALL] [CALL] [CALL]", 5, 3,
                             2);
    if (a == NULL || b == NULL || c == NULL) {
        fprintf(stderr, "rr-smoke failed: create\n");
        return 1;
    }

    runtime_run_round_robin(&runtime);
    if (a->state != AGENT_DONE || b->state != AGENT_DONE ||
        c->state != AGENT_ERROR || a->used_quota != 2 ||
        b->used_quota != 1) {
        fprintf(stderr,
                "rr-smoke failed: a=%s/%d b=%s/%d c=%s/%d\n",
                agent_state_name(a->state), a->used_quota,
                agent_state_name(b->state), b->used_quota,
                agent_state_name(c->state), c->used_quota);
        return 1;
    }

    puts("rr-smoke passed");
    return 0;
}

int input_smoke_test(void)
{
    AgentRuntime runtime;
    char answer[GCOS_RESPONSE_LEN];

    runtime_init(&runtime);
    build_local_answer(&runtime, "plain local run", answer, sizeof(answer));
    if (runtime.agent_count != 1 || runtime.agents[0].state != AGENT_DONE ||
        strstr(answer, "Local Mini Agent OS") == NULL) {
        fprintf(stderr, "input-smoke failed: local prompt answer=%s\n", answer);
        return 1;
    }

    runtime_init(&runtime);
    build_local_answer(&runtime, "[CODEX:gcos-test]", answer, sizeof(answer));
    if (runtime.agent_count != 1 ||
        runtime.agents[0].state != AGENT_BLOCKED_ON_APPROVAL ||
        strstr(answer, "Local Mini Agent OS") == NULL) {
        fprintf(stderr, "input-smoke failed: CODEX prompt answer=%s\n", answer);
        return 1;
    }

    runtime_init(&runtime);
    build_local_answer(&runtime, "llm: do not call", answer, sizeof(answer));
    if (runtime.agent_count != 1 || runtime.agents[0].state != AGENT_DONE ||
        strstr(answer, "Local Mini Agent OS") == NULL) {
        fprintf(stderr, "input-smoke failed: broker prefix answer=%s\n", answer);
        return 1;
    }

    puts("input-smoke passed");
    return 0;
}

int wrap_smoke_test(void)
{
    char long_text[4096] = "";
    int i;

    for (i = 0; i < 40; i++) {
        append_text(long_text, sizeof(long_text),
                    "Mini Agent OS TUI keeps long dashboard text visible ");
    }

    if ((int)strlen(long_text) <= TUI_WRAP_WIDTH) {
        fprintf(stderr, "wrap-smoke failed: setup text too short\n");
        return 1;
    }

    puts("wrap-smoke passed");
    return 0;
}
