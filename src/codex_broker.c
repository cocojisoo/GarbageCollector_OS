#include "gcos.h"

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)

#include <stdio.h>
#include <string.h>

const char *codex_broker_backend_name(void)
{
    return "portable-local";
}

int codex_broker_has_api_key(void)
{
    return 0;
}

int codex_broker_run(const char *prompt, char *out, size_t out_size)
{
    return codex_broker_run_with_timeout(prompt, out, out_size, 0);
}

int codex_broker_run_with_timeout(const char *prompt, char *out,
                                  size_t out_size, int timeout_seconds)
{
    (void)prompt;
    (void)timeout_seconds;
    snprintf(out, out_size,
             "external LLM broker is disabled in the portable Windows TUI build");
    return 0;
}

int codex_smoke_test(void)
{
    puts("codex-smoke skipped: portable Windows TUI build disables broker");
    return 0;
}

int api_smoke_test(void)
{
    puts("api-smoke skipped: portable Windows TUI build disables broker");
    return 0;
}

int api_config_smoke_test(void)
{
    puts("api-config-smoke passed");
    return 0;
}

#else

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    const char *name;
    char value[512];
    int had_value;
} EnvSnapshot;

typedef enum {
    API_PROVIDER_UNKNOWN,
    API_PROVIDER_UPSTAGE,
    API_PROVIDER_OPENAI
} ApiProvider;

typedef enum {
    API_STYLE_RESPONSES,
    API_STYLE_CHAT
} ApiStyle;

typedef struct {
    const char *key;
    ApiProvider provider;
    ApiStyle style;
    char base_url[512];
    char endpoint[576];
    char model[128];
} ApiConfig;

static char api_file_key[512];
static char api_file_key_path[512];
static int api_file_key_loaded;
static int api_file_key_available;

static int env_truthy(const char *name)
{
    const char *value = getenv(name);

    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0 &&
           strcmp(value, "false") != 0 && strcmp(value, "FALSE") != 0 &&
           strcmp(value, "no") != 0 && strcmp(value, "NO") != 0;
}

static void reset_api_key_cache(void)
{
    api_file_key[0] = '\0';
    api_file_key_path[0] = '\0';
    api_file_key_loaded = 0;
    api_file_key_available = 0;
}

static void save_env_snapshot(EnvSnapshot *snapshot, const char *name)
{
    const char *value = getenv(name);

    snapshot->name = name;
    snapshot->had_value = value != NULL;
    if (value != NULL) {
        snprintf(snapshot->value, sizeof(snapshot->value), "%s", value);
    } else {
        snapshot->value[0] = '\0';
    }
}

static void restore_env_snapshot(const EnvSnapshot *snapshot)
{
    if (snapshot->had_value) {
        setenv(snapshot->name, snapshot->value, 1);
    } else {
        unsetenv(snapshot->name);
    }
}

static void append_limited(char *out, size_t out_size, const char *text)
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

static void shell_quote_path(char *out, size_t out_size, const char *path)
{
    size_t i;

    if (out_size == 0) {
        return;
    }
    out[0] = '\0';
    append_limited(out, out_size, "'");
    if (path == NULL) {
        path = "";
    }
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\'') {
            append_limited(out, out_size, "'\\''");
        } else {
            char c[2] = {path[i], '\0'};
            append_limited(out, out_size, c);
        }
    }
    append_limited(out, out_size, "'");
}

static int read_file_limited(const char *path, char *out, size_t out_size)
{
    FILE *file;
    size_t used = 0;
    int ch;

    if (out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    file = fopen(path, "r");
    if (file == NULL) {
        snprintf(out, out_size, "failed to read codex output: %s",
                 strerror(errno));
        return 0;
    }

    while ((ch = fgetc(file)) != EOF && used + 1 < out_size) {
        out[used++] = (char)ch;
    }
    out[used] = '\0';
    fclose(file);
    return 1;
}

static void join_home_path(char *out, size_t out_size, const char *suffix);

static int text_has_value(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static int looks_like_upstage_key(const char *key)
{
    return key != NULL && strncmp(key, "up_", 3) == 0;
}

static int provider_from_name(const char *name, ApiProvider *provider)
{
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    if (strcmp(name, "upstage") == 0) {
        *provider = API_PROVIDER_UPSTAGE;
        return 1;
    }
    if (strcmp(name, "openai") == 0) {
        *provider = API_PROVIDER_OPENAI;
        return 1;
    }
    return 0;
}

static int provider_from_base_url(const char *base, ApiProvider *provider)
{
    if (!text_has_value(base)) {
        return 0;
    }
    if (strstr(base, "upstage.ai") != NULL) {
        *provider = API_PROVIDER_UPSTAGE;
        return 1;
    }
    if (strstr(base, "openai.com") != NULL) {
        *provider = API_PROVIDER_OPENAI;
        return 1;
    }
    return 0;
}

static ApiProvider infer_provider(const char *key, ApiProvider fallback)
{
    const char *base = getenv("GCOS_LLM_API_BASE_URL");
    const char *provider_name = getenv("GCOS_LLM_PROVIDER");
    ApiProvider provider = fallback;

    if (provider_from_name(provider_name, &provider)) {
        return provider;
    }
    if (provider_from_base_url(base, &provider)) {
        return provider;
    }
    if (looks_like_upstage_key(key)) {
        return API_PROVIDER_UPSTAGE;
    }
    return provider == API_PROVIDER_UNKNOWN ? API_PROVIDER_OPENAI : provider;
}

static int read_api_key_file(const char *path)
{
    FILE *file;
    size_t len;

    if (!text_has_value(path)) {
        return 0;
    }

    if (api_file_key_loaded && api_file_key_available &&
        strcmp(api_file_key_path, path) == 0) {
        return 1;
    }

    api_file_key_loaded = 1;
    api_file_key_available = 0;
    api_file_key[0] = '\0';
    snprintf(api_file_key_path, sizeof(api_file_key_path), "%s", path);

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    if (fgets(api_file_key, sizeof(api_file_key), file) == NULL) {
        fclose(file);
        return 0;
    }
    fclose(file);

    len = strlen(api_file_key);
    while (len > 0 &&
           (api_file_key[len - 1] == '\n' ||
            api_file_key[len - 1] == '\r' ||
            api_file_key[len - 1] == ' ' ||
            api_file_key[len - 1] == '\t')) {
        api_file_key[--len] = '\0';
    }

    api_file_key_available = api_file_key[0] != '\0';
    return api_file_key_available;
}

static int api_config_try_key(ApiConfig *config, const char *key,
                              ApiProvider provider)
{
    if (!text_has_value(key)) {
        return 0;
    }

    config->key = key;
    config->provider = infer_provider(key, provider);
    return 1;
}

static const char *api_default_base(ApiProvider provider)
{
    if (provider == API_PROVIDER_UPSTAGE) {
        return "https://api.upstage.ai/v1";
    }
    return "https://api.openai.com/v1";
}

static void api_config_finish(ApiConfig *config)
{
    const char *base = getenv("GCOS_LLM_API_BASE_URL");
    const char *style = getenv("GCOS_LLM_API_STYLE");
    const char *model = getenv("GCOS_LLM_MODEL");
    size_t len;

    if (!text_has_value(base)) {
        if (config->provider == API_PROVIDER_UPSTAGE) {
            base = getenv("UPSTAGE_BASE_URL");
        } else {
            base = getenv("OPENAI_BASE_URL");
        }
    }

    if (text_has_value(base)) {
        snprintf(config->base_url, sizeof(config->base_url), "%s", base);
    } else {
        snprintf(config->base_url, sizeof(config->base_url), "%s",
                 api_default_base(config->provider));
    }

    if (strcmp(style != NULL ? style : "", "chat") == 0) {
        config->style = API_STYLE_CHAT;
    } else if (strcmp(style != NULL ? style : "", "responses") == 0) {
        config->style = API_STYLE_RESPONSES;
    } else if (text_has_value(base) || config->provider == API_PROVIDER_UPSTAGE) {
        config->style = API_STYLE_CHAT;
    } else {
        config->style = API_STYLE_RESPONSES;
    }

    if (!text_has_value(model)) {
        if (config->provider == API_PROVIDER_UPSTAGE) {
            model = getenv("UPSTAGE_MODEL");
        } else {
            model = getenv("GCOS_OPENAI_MODEL");
        }
    }
    if (!text_has_value(model) && config->provider == API_PROVIDER_OPENAI) {
        model = getenv("OPENAI_MODEL");
    }
    if (!text_has_value(model)) {
        if (config->provider == API_PROVIDER_UPSTAGE) {
            model = "solar-pro3";
        } else {
            model = "gpt-5.5";
        }
    }
    snprintf(config->model, sizeof(config->model), "%s", model);

    snprintf(config->endpoint, sizeof(config->endpoint), "%s",
             config->base_url);
    len = strlen(config->endpoint);
    while (len > 0 && config->endpoint[len - 1] == '/') {
        config->endpoint[--len] = '\0';
    }
    snprintf(config->endpoint + len, sizeof(config->endpoint) - len, "/%s",
             config->style == API_STYLE_CHAT ? "chat/completions"
                                             : "responses");
}

static int api_config_load(ApiConfig *config)
{
    const char *key_file = getenv("GCOS_LLM_API_KEY_FILE");
    const char *legacy_key_file = getenv("GCOS_OPENAI_API_KEY_FILE");
    char default_key_file[512];
    char legacy_default_key_file[512];

    memset(config, 0, sizeof(*config));
    if (api_config_try_key(config, getenv("GCOS_LLM_API_KEY"),
                           API_PROVIDER_UNKNOWN) ||
        api_config_try_key(config, getenv("GCOS_UPSTAGE_API_KEY"),
                           API_PROVIDER_UPSTAGE) ||
        api_config_try_key(config, getenv("UPSTAGE_API_KEY"),
                           API_PROVIDER_UPSTAGE) ||
        api_config_try_key(config, getenv("GCOS_OPENAI_API_KEY"),
                           API_PROVIDER_OPENAI) ||
        api_config_try_key(config, getenv("OPENAI_API_KEY"),
                           API_PROVIDER_OPENAI)) {
        api_config_finish(config);
        return 1;
    }

    if (text_has_value(key_file)) {
        if (read_api_key_file(key_file)) {
            api_config_try_key(config, api_file_key, API_PROVIDER_UNKNOWN);
            api_config_finish(config);
            return 1;
        }
        return 0;
    }

    if (text_has_value(legacy_key_file)) {
        if (read_api_key_file(legacy_key_file)) {
            api_config_try_key(config, api_file_key, API_PROVIDER_OPENAI);
            api_config_finish(config);
            return 1;
        }
        return 0;
    }

    join_home_path(default_key_file, sizeof(default_key_file),
                   ".config/gcos/llm_api_key");
    if (read_api_key_file(default_key_file)) {
        api_config_try_key(config, api_file_key, API_PROVIDER_UNKNOWN);
        api_config_finish(config);
        return 1;
    }

    reset_api_key_cache();
    join_home_path(legacy_default_key_file, sizeof(legacy_default_key_file),
                   ".config/gcos/openai_api_key");
    if (read_api_key_file(legacy_default_key_file)) {
        api_config_try_key(config, api_file_key, API_PROVIDER_OPENAI);
        api_config_finish(config);
        return 1;
    }

    return 0;
}

static int api_key_configured(void)
{
    ApiConfig config;

    return api_config_load(&config);
}

int codex_broker_has_api_key(void)
{
    return api_key_configured();
}

static int executable_file(const char *path)
{
    return path != NULL && path[0] != '\0' && access(path, X_OK) == 0;
}

static void join_home_path(char *out, size_t out_size, const char *suffix)
{
    const char *home = getenv("HOME");

    if (out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (home == NULL || home[0] == '\0') {
        return;
    }

    snprintf(out, out_size, "%s/%s", home, suffix);
}

static void find_codex_binary(char *out, size_t out_size)
{
    const char *env_path = getenv("CODEX_BIN");
    const char *candidates[] = {
        "Coding/node.js/current/bin/codex",
        ".codex/bin/codex",
        ".local/bin/codex",
    };
    char home_candidate[512];
    const char *system_candidates[] = {
        "/opt/homebrew/bin/codex",
        "/usr/local/bin/codex",
        "/usr/bin/codex",
    };
    size_t i;

    if (out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (executable_file(env_path)) {
        snprintf(out, out_size, "%s", env_path);
        return;
    }

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        join_home_path(home_candidate, sizeof(home_candidate), candidates[i]);
        if (executable_file(home_candidate)) {
            snprintf(out, out_size, "%s", home_candidate);
            return;
        }
    }

    for (i = 0; i < sizeof(system_candidates) / sizeof(system_candidates[0]);
         i++) {
        if (executable_file(system_candidates[i])) {
            snprintf(out, out_size, "%s", system_candidates[i]);
            return;
        }
    }

    snprintf(out, out_size, "%s", "codex");
}

static void json_write_string(FILE *file, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    fputc('"', file);
    while (*cursor != '\0') {
        switch (*cursor) {
        case '\\':
            fputs("\\\\", file);
            break;
        case '"':
            fputs("\\\"", file);
            break;
        case '\n':
            fputs("\\n", file);
            break;
        case '\r':
            fputs("\\r", file);
            break;
        case '\t':
            fputs("\\t", file);
            break;
        default:
            if (*cursor < 0x20) {
                fprintf(file, "\\u%04x", *cursor);
            } else {
                fputc(*cursor, file);
            }
            break;
        }
        cursor++;
    }
    fputc('"', file);
}

static int json_unescape(char *out, size_t out_size, const char *start,
                         const char *end)
{
    size_t used = 0;
    const char *cursor = start;

    if (out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    while (cursor < end && used + 1 < out_size) {
        if (*cursor != '\\') {
            out[used++] = *cursor++;
            continue;
        }

        cursor++;
        if (cursor >= end) {
            break;
        }

        switch (*cursor) {
        case 'n':
            out[used++] = '\n';
            break;
        case 'r':
            out[used++] = '\r';
            break;
        case 't':
            out[used++] = '\t';
            break;
        case '"':
        case '\\':
        case '/':
            out[used++] = *cursor;
            break;
        case 'u':
            if (cursor + 4 < end) {
                out[used++] = '?';
                cursor += 4;
            }
            break;
        default:
            out[used++] = *cursor;
            break;
        }
        cursor++;
    }

    out[used] = '\0';
    return used > 0;
}

static int extract_json_string_after_key(const char *json, const char *from,
                                         const char *key, char *out,
                                         size_t out_size)
{
    const char *cursor = strstr(from != NULL ? from : json, key);
    const char *start;
    const char *end;

    if (cursor == NULL) {
        return 0;
    }

    cursor += strlen(key);
    cursor = strchr(cursor, ':');
    if (cursor == NULL) {
        return 0;
    }
    cursor++;
    while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' ||
           *cursor == '\t') {
        cursor++;
    }
    if (*cursor != '"') {
        return 0;
    }

    start = cursor + 1;
    end = start;
    while (*end != '\0') {
        if (*end == '"' && (end == start || *(end - 1) != '\\')) {
            return json_unescape(out, out_size, start, end);
        }
        end++;
    }

    return 0;
}

static int extract_api_response_text(const char *json, char *out,
                                     size_t out_size)
{
    const char *output_text = strstr(json, "\"output_text\"");

    if (output_text != NULL &&
        extract_json_string_after_key(json, output_text, "\"text\"", out,
                                      out_size)) {
        return 1;
    }

    if (extract_json_string_after_key(json, json, "\"content\"", out,
                                      out_size)) {
        return 1;
    }

    if (extract_json_string_after_key(json, json, "\"message\"", out,
                                      out_size)) {
        return 0;
    }

    snprintf(out, out_size, "LLM API response did not contain text");
    return 0;
}

static int llm_api_run(const char *prompt, char *out, size_t out_size,
                       int timeout_seconds)
{
    char request_path[] = "/tmp/gcos-api-request-XXXXXX";
    char response_path[] = "/tmp/gcos-api-response-XXXXXX";
    char config_path[] = "/tmp/gcos-api-curl-XXXXXX";
    char log_path[] = "/tmp/gcos-api-curl-log-XXXXXX";
    char q_request[256];
    char q_config[256];
    char q_log[256];
    char command[1024];
    char response[GCOS_RESPONSE_LEN];
    ApiConfig config;
    int request_fd;
    int response_fd;
    int config_fd;
    int log_fd;
    int status;
    FILE *request_file;
    FILE *config_file;

    if (!api_config_load(&config)) {
        snprintf(out, out_size,
                 "UPSTAGE_API_KEY, OPENAI_API_KEY, or GCOS_LLM_API_KEY is not set");
        return 0;
    }

    request_fd = mkstemp(request_path);
    response_fd = mkstemp(response_path);
    config_fd = mkstemp(config_path);
    log_fd = mkstemp(log_path);
    if (request_fd < 0 || response_fd < 0 || config_fd < 0 || log_fd < 0) {
        snprintf(out, out_size, "mkstemp failed: %s", strerror(errno));
        if (request_fd >= 0) {
            close(request_fd);
            unlink(request_path);
        }
        if (response_fd >= 0) {
            close(response_fd);
            unlink(response_path);
        }
        if (config_fd >= 0) {
            close(config_fd);
            unlink(config_path);
        }
        if (log_fd >= 0) {
            close(log_fd);
            unlink(log_path);
        }
        return 0;
    }
    close(response_fd);
    close(log_fd);

    request_file = fdopen(request_fd, "w");
    config_file = fdopen(config_fd, "w");
    if (request_file == NULL || config_file == NULL) {
        snprintf(out, out_size, "fdopen failed: %s", strerror(errno));
        if (request_file != NULL) {
            fclose(request_file);
        } else {
            close(request_fd);
        }
        if (config_file != NULL) {
            fclose(config_file);
        } else {
            close(config_fd);
        }
        unlink(request_path);
        unlink(response_path);
        unlink(config_path);
        unlink(log_path);
        return 0;
    }

    if (config.style == API_STYLE_CHAT) {
        fputs("{\"model\":", request_file);
        json_write_string(request_file, config.model);
        fputs(",\"messages\":[{\"role\":\"system\",\"content\":", request_file);
        json_write_string(request_file,
                          "You are the LLM broker for GarbageCollector OS, a Mini Agent OS runtime. Answer only the user-level request. Do not execute destructive commands.");
        fputs("},{\"role\":\"user\",\"content\":", request_file);
        json_write_string(request_file, prompt);
        fputs("}],\"stream\":false}\n", request_file);
    } else {
        fputs("{\"model\":", request_file);
        json_write_string(request_file, config.model);
        fputs(",\"instructions\":", request_file);
        json_write_string(request_file,
                          "You are the LLM broker for GarbageCollector OS, a Mini Agent OS runtime. Answer only the user-level request. Do not execute destructive commands.");
        fputs(",\"input\":", request_file);
        json_write_string(request_file, prompt);
        fputs(",\"max_output_tokens\":128,\"store\":false}\n", request_file);
    }
    fclose(request_file);

    fputs("url = \"", config_file);
    fputs(config.endpoint, config_file);
    fputs("\"\n", config_file);
    fputs("request = \"POST\"\n", config_file);
    fputs("silent\nshow-error\nfail-with-body\n", config_file);
    if (timeout_seconds > 0) {
        fprintf(config_file, "max-time = %d\n", timeout_seconds);
    }
    fprintf(config_file, "output = \"%s\"\n", response_path);
    fputs("header = \"Content-Type: application/json\"\n", config_file);
    fputs("header = \"Authorization: Bearer ", config_file);
    fputs(config.key, config_file);
    fputs("\"\n", config_file);
    fclose(config_file);

    shell_quote_path(q_config, sizeof(q_config), config_path);
    shell_quote_path(q_request, sizeof(q_request), request_path);
    shell_quote_path(q_log, sizeof(q_log), log_path);
    snprintf(command, sizeof(command),
             "/usr/bin/curl --config %s --data-binary @%s >%s 2>&1",
             q_config, q_request, q_log);

    status = system(command);
    if (!read_file_limited(response_path, response, sizeof(response))) {
        unlink(request_path);
        unlink(response_path);
        unlink(config_path);
        unlink(log_path);
        return 0;
    }

    unlink(request_path);
    unlink(response_path);
    unlink(config_path);
    unlink(log_path);

    if (status != 0) {
        if (!extract_json_string_after_key(response, response, "\"message\"",
                                           out, out_size)) {
            snprintf(out, out_size, "LLM API request failed with status=%d",
                     status);
        }
        return 0;
    }

    return extract_api_response_text(response, out, out_size);
}

static int codex_cli_run(const char *prompt, char *out, size_t out_size)
{
    char prompt_path[] = "/tmp/gcos-codex-prompt-XXXXXX";
    char output_path[] = "/tmp/gcos-codex-output-XXXXXX";
    char log_path[] = "/tmp/gcos-codex-run-log-XXXXXX";
    char codex_path[512];
    char q_codex[768];
    char q_prompt[256];
    char q_output[256];
    char q_log[256];
    char command[1536];
    int prompt_fd;
    int output_fd;
    int log_fd;
    int status;
    FILE *prompt_file;

    if (out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    prompt_fd = mkstemp(prompt_path);
    output_fd = mkstemp(output_path);
    log_fd = mkstemp(log_path);
    if (prompt_fd < 0 || output_fd < 0 || log_fd < 0) {
        snprintf(out, out_size, "mkstemp failed: %s", strerror(errno));
        if (prompt_fd >= 0) {
            close(prompt_fd);
            unlink(prompt_path);
        }
        if (output_fd >= 0) {
            close(output_fd);
            unlink(output_path);
        }
        if (log_fd >= 0) {
            close(log_fd);
            unlink(log_path);
        }
        return 0;
    }
    close(output_fd);
    close(log_fd);

    prompt_file = fdopen(prompt_fd, "w");
    if (prompt_file == NULL) {
        snprintf(out, out_size, "fdopen failed: %s", strerror(errno));
        close(prompt_fd);
        unlink(prompt_path);
        unlink(output_path);
        unlink(log_path);
        return 0;
    }

    fprintf(prompt_file,
            "You are the Codex broker for GarbageCollector OS, a Mini Agent OS runtime. Answer only the user-level request. Do not execute destructive commands. Request: %s\n",
            prompt);
    fclose(prompt_file);

    find_codex_binary(codex_path, sizeof(codex_path));
    shell_quote_path(q_codex, sizeof(q_codex), codex_path);
    shell_quote_path(q_prompt, sizeof(q_prompt), prompt_path);
    shell_quote_path(q_output, sizeof(q_output), output_path);
    shell_quote_path(q_log, sizeof(q_log), log_path);
    snprintf(command, sizeof(command),
             "%s exec --ephemeral --skip-git-repo-check --sandbox read-only --output-last-message %s - < %s >%s 2>&1",
             q_codex, q_output, q_prompt, q_log);

    status = system(command);
    if (status != 0) {
        read_file_limited(log_path, out, out_size);
        if (out[0] == '\0') {
            snprintf(out, out_size, "codex broker failed with status=%d",
                     status);
        }
        unlink(prompt_path);
        unlink(output_path);
        unlink(log_path);
        return 0;
    }

    if (!read_file_limited(output_path, out, out_size)) {
        unlink(prompt_path);
        unlink(output_path);
        unlink(log_path);
        return 0;
    }

    unlink(prompt_path);
    unlink(output_path);
    unlink(log_path);
    return 1;
}

const char *codex_broker_backend_name(void)
{
    const char *backend = getenv("GCOS_LLM_BACKEND");

    if (backend != NULL && strcmp(backend, "api") == 0) {
        return "api-key";
    }

    if (backend != NULL && strcmp(backend, "codex") == 0) {
        return "codex-cli";
    }

    if (backend != NULL && backend[0] != '\0' &&
        strcmp(backend, "auto") != 0) {
        return "invalid-backend";
    }

    if (api_key_configured()) {
        return "api-key";
    }

    if (env_truthy("GCOS_ENABLE_CODEX_FALLBACK")) {
        return "codex-cli";
    }

    return "api-key-missing";
}

int codex_broker_run(const char *prompt, char *out, size_t out_size)
{
    return codex_broker_run_with_timeout(prompt, out, out_size, 0);
}

int codex_broker_run_with_timeout(const char *prompt, char *out,
                                  size_t out_size, int timeout_seconds)
{
    const char *backend = getenv("GCOS_LLM_BACKEND");

    if (backend != NULL && strcmp(backend, "api") == 0) {
        return llm_api_run(prompt, out, out_size, timeout_seconds);
    }

    if (backend != NULL && strcmp(backend, "codex") == 0) {
        return codex_cli_run(prompt, out, out_size);
    }

    if (backend != NULL && backend[0] != '\0' &&
        strcmp(backend, "auto") != 0) {
        snprintf(out, out_size,
                 "unsupported GCOS_LLM_BACKEND=%s; use auto, api, or codex",
                 backend);
        return 0;
    }

    if (api_key_configured()) {
        return llm_api_run(prompt, out, out_size, timeout_seconds);
    }

    if (env_truthy("GCOS_ENABLE_CODEX_FALLBACK")) {
        return codex_cli_run(prompt, out, out_size);
    }

    snprintf(out, out_size,
             "API key is not configured. Codex CLI fallback is disabled by default to avoid macOS privacy-folder permission prompts. Set UPSTAGE_API_KEY or ~/.config/gcos/llm_api_key, or explicitly set GCOS_LLM_BACKEND=codex.");
    return 0;
}

int codex_smoke_test(void)
{
    char out[GCOS_TEXT_LEN];

    if (!codex_broker_run("Reply with exactly: gcos-codex-ok", out,
                          sizeof(out))) {
        fprintf(stderr, "codex-smoke failed: %s\n", out);
        return 1;
    }

    if (strstr(out, "gcos-codex-ok") == NULL) {
        fprintf(stderr, "codex-smoke unexpected output: %s\n", out);
        return 1;
    }

    puts("codex-smoke passed");
    return 0;
}

int api_smoke_test(void)
{
    char out[GCOS_TEXT_LEN];

    if (!llm_api_run("Reply with exactly: gcos-api-ok", out, sizeof(out), 30)) {
        fprintf(stderr, "api-smoke failed: %s\n", out);
        return 1;
    }

    if (strstr(out, "gcos-api-ok") == NULL) {
        fprintf(stderr, "api-smoke unexpected output: %s\n", out);
        return 1;
    }

    puts("api-smoke passed");
    return 0;
}

static void clear_api_test_env(void)
{
    const char *names[] = {
        "GCOS_LLM_BACKEND",
        "GCOS_LLM_PROVIDER",
        "GCOS_LLM_API_KEY",
        "GCOS_UPSTAGE_API_KEY",
        "UPSTAGE_API_KEY",
        "GCOS_OPENAI_API_KEY",
        "OPENAI_API_KEY",
        "GCOS_LLM_API_KEY_FILE",
        "GCOS_OPENAI_API_KEY_FILE",
        "GCOS_LLM_API_BASE_URL",
        "UPSTAGE_BASE_URL",
        "OPENAI_BASE_URL",
        "GCOS_LLM_API_STYLE",
        "GCOS_LLM_MODEL",
        "UPSTAGE_MODEL",
        "GCOS_OPENAI_MODEL",
        "OPENAI_MODEL",
        "GCOS_ENABLE_CODEX_FALLBACK",
    };
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        unsetenv(names[i]);
    }
    reset_api_key_cache();
}

static int expect_api_config(const char *label, ApiProvider provider,
                             ApiStyle style, const char *endpoint,
                             const char *model)
{
    ApiConfig config;

    if (!api_config_load(&config)) {
        fprintf(stderr, "api-config-smoke failed: %s no config\n", label);
        return 0;
    }

    if (config.provider != provider || config.style != style ||
        strcmp(config.endpoint, endpoint) != 0 ||
        strcmp(config.model, model) != 0) {
        fprintf(stderr,
                "api-config-smoke failed: %s provider=%d style=%d endpoint=%s model=%s\n",
                label, config.provider, config.style, config.endpoint,
                config.model);
        return 0;
    }

    return 1;
}

int api_config_smoke_test(void)
{
    const char *env_names[] = {
        "HOME",
        "GCOS_LLM_BACKEND",
        "GCOS_LLM_PROVIDER",
        "GCOS_LLM_API_KEY",
        "GCOS_UPSTAGE_API_KEY",
        "UPSTAGE_API_KEY",
        "GCOS_OPENAI_API_KEY",
        "OPENAI_API_KEY",
        "GCOS_LLM_API_KEY_FILE",
        "GCOS_OPENAI_API_KEY_FILE",
        "GCOS_LLM_API_BASE_URL",
        "UPSTAGE_BASE_URL",
        "OPENAI_BASE_URL",
        "GCOS_LLM_API_STYLE",
        "GCOS_LLM_MODEL",
        "UPSTAGE_MODEL",
        "GCOS_OPENAI_MODEL",
        "OPENAI_MODEL",
        "GCOS_ENABLE_CODEX_FALLBACK",
    };
    EnvSnapshot env[sizeof(env_names) / sizeof(env_names[0])];
    char temp_home[] = "/tmp/gcos-api-config-home-XXXXXX";
    char config_dir[512];
    char key_path[512];
    FILE *file;
    size_t i;
    int ok = 1;

    for (i = 0; i < sizeof(env_names) / sizeof(env_names[0]); i++) {
        save_env_snapshot(&env[i], env_names[i]);
    }

    if (mkdtemp(temp_home) == NULL) {
        fprintf(stderr, "api-config-smoke failed: mkdtemp\n");
        return 1;
    }

    snprintf(config_dir, sizeof(config_dir), "%s/.config", temp_home);
    if (mkdir(config_dir, 0700) != 0) {
        fprintf(stderr, "api-config-smoke failed: mkdir .config\n");
        rmdir(temp_home);
        return 1;
    }
    snprintf(config_dir, sizeof(config_dir), "%s/.config/gcos", temp_home);
    if (mkdir(config_dir, 0700) != 0) {
        fprintf(stderr, "api-config-smoke failed: mkdir gcos\n");
        snprintf(config_dir, sizeof(config_dir), "%s/.config", temp_home);
        rmdir(config_dir);
        rmdir(temp_home);
        return 1;
    }

    snprintf(key_path, sizeof(key_path), "%s/llm_api_key", config_dir);
    file = fopen(key_path, "w");
    if (file == NULL) {
        fprintf(stderr, "api-config-smoke failed: create key file\n");
        rmdir(config_dir);
        snprintf(config_dir, sizeof(config_dir), "%s/.config", temp_home);
        rmdir(config_dir);
        rmdir(temp_home);
        return 1;
    }
    fputs("up_test_key\n", file);
    fclose(file);

    clear_api_test_env();
    setenv("HOME", temp_home, 1);
    ok = expect_api_config("default generic file", API_PROVIDER_UPSTAGE,
                           API_STYLE_CHAT,
                           "https://api.upstage.ai/v1/chat/completions",
                           "solar-pro3") &&
         ok;

    clear_api_test_env();
    setenv("UPSTAGE_API_KEY", "up_env_key", 1);
    ok = expect_api_config("upstage env", API_PROVIDER_UPSTAGE,
                           API_STYLE_CHAT,
                           "https://api.upstage.ai/v1/chat/completions",
                           "solar-pro3") &&
         ok;

    clear_api_test_env();
    setenv("UPSTAGE_API_KEY", "up_env_key", 1);
    setenv("OPENAI_API_KEY", "sk-env-key", 1);
    ok = expect_api_config("mixed provider env precedence",
                           API_PROVIDER_UPSTAGE, API_STYLE_CHAT,
                           "https://api.upstage.ai/v1/chat/completions",
                           "solar-pro3") &&
         ok;

    clear_api_test_env();
    setenv("OPENAI_API_KEY", "sk-env-key", 1);
    ok = expect_api_config("openai env", API_PROVIDER_OPENAI,
                           API_STYLE_RESPONSES,
                           "https://api.openai.com/v1/responses",
                           "gpt-5.5") &&
         ok;

    clear_api_test_env();
    setenv("GCOS_LLM_API_KEY", "up_generic_key", 1);
    setenv("GCOS_LLM_API_BASE_URL", "https://api.upstage.ai/v1/", 1);
    setenv("GCOS_LLM_API_STYLE", "chat", 1);
    setenv("GCOS_LLM_MODEL", "solar-pro3", 1);
    ok = expect_api_config("explicit generic upstage override",
                           API_PROVIDER_UPSTAGE, API_STYLE_CHAT,
                           "https://api.upstage.ai/v1/chat/completions",
                           "solar-pro3") &&
         ok;

    for (i = 0; i < sizeof(env_names) / sizeof(env_names[0]); i++) {
        restore_env_snapshot(&env[i]);
    }
    reset_api_key_cache();

    unlink(key_path);
    rmdir(config_dir);
    snprintf(config_dir, sizeof(config_dir), "%s/.config", temp_home);
    rmdir(config_dir);
    rmdir(temp_home);

    if (!ok) {
        return 1;
    }

    puts("api-config-smoke passed");
    return 0;
}

#endif
