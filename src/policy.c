#include "gcos.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#ifdef _WIN32
static char *gcos_realpath(const char *path, char *resolved)
{
    return _fullpath(resolved, path, PATH_MAX);
}
#else
static char *gcos_realpath(const char *path, char *resolved)
{
    return realpath(path, resolved);
}
#endif

static int starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static int contains(const char *text, const char *needle)
{
    return strstr(text, needle) != NULL;
}

static int contains_path_segment(const char *path, const char *segment)
{
    const char *cursor = path;
    size_t len = strlen(segment);

    while ((cursor = strstr(cursor, segment)) != NULL) {
        int starts_segment =
            cursor == path || cursor[-1] == '/' || cursor[-1] == '\\';
        int ends_segment =
            cursor[len] == '\0' || cursor[len] == '/' || cursor[len] == '\\';

        if (starts_segment && ends_segment) {
            return 1;
        }
        cursor += len;
    }

    return 0;
}

static int starts_with_protected_home_component(const char *relative)
{
    const char *protected_names[] = {
        "Desktop", "Documents", "Downloads", "Pictures",
        "Music",   "Movies",    "Library",   "CloudStorage",
    };
    size_t i;

    for (i = 0; i < sizeof(protected_names) / sizeof(protected_names[0]); i++) {
        size_t len = strlen(protected_names[i]);
        if (strncmp(relative, protected_names[i], len) == 0 &&
            (relative[len] == '\0' || relative[len] == '/')) {
            return 1;
        }
    }

    return 0;
}

static int is_macos_privacy_path_text(const char *path)
{
    const char *after_user;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (starts_with(path, "~/")) {
        return starts_with_protected_home_component(path + 2);
    }

    if (!starts_with(path, "/Users/")) {
        return starts_with(path, "/Volumes/");
    }

    after_user = strchr(path + strlen("/Users/"), '/');
    if (after_user == NULL) {
        return 0;
    }

    return starts_with_protected_home_component(after_user + 1);
}

static void copy_marker_body(const char *prompt, const char *marker, char *out,
                             size_t out_size)
{
    const char *start = strstr(prompt, marker);
    const char *end;
    size_t len;

    if (start == NULL || out_size == 0) {
        return;
    }

    start += strlen(marker);
    end = strchr(start, ']');
    if (end == NULL) {
        return;
    }

    len = (size_t)(end - start);
    if (len >= out_size) {
        len = out_size - 1;
    }

    memcpy(out, start, len);
    out[len] = '\0';

    while (isspace((unsigned char)out[0])) {
        memmove(out, out + 1, strlen(out));
    }
    while (strlen(out) > 0 &&
           isspace((unsigned char)out[strlen(out) - 1])) {
        out[strlen(out) - 1] = '\0';
    }
}

ActionRequest policy_extract_action(const char *prompt)
{
    ActionRequest action;

    memset(&action, 0, sizeof(action));
    action.kind = ACTION_NONE;

    if (strstr(prompt, "[LIST:") != NULL) {
        action.kind = ACTION_LIST;
        copy_marker_body(prompt, "[LIST:", action.target, sizeof(action.target));
    } else if (strstr(prompt, "[READ:") != NULL) {
        action.kind = ACTION_READ;
        copy_marker_body(prompt, "[READ:", action.target, sizeof(action.target));
    } else if (strstr(prompt, "[SHELL:") != NULL) {
        action.kind = ACTION_SHELL;
        copy_marker_body(prompt, "[SHELL:", action.target,
                         sizeof(action.target));
    } else if (strstr(prompt, "[ROOT:") != NULL) {
        action.kind = ACTION_ROOT;
        action.requires_root = 1;
        copy_marker_body(prompt, "[ROOT:", action.target, sizeof(action.target));
    } else if (strstr(prompt, "[KERNEL:") != NULL) {
        action.kind = ACTION_KERNEL;
        action.requires_root = 1;
        copy_marker_body(prompt, "[KERNEL:", action.target,
                         sizeof(action.target));
    } else if (strstr(prompt, "[CODEX:") != NULL) {
        action.kind = ACTION_CODEX;
        copy_marker_body(prompt, "[CODEX:", action.target,
                         sizeof(action.target));
    }

    if (action.kind != ACTION_NONE && action.target[0] == '\0') {
        action.kind = ACTION_NONE;
    }

    return action;
}

static PolicyDecision decision(int allowed, AgentState state, int approval,
                               int broker, const char *reason)
{
    PolicyDecision out;

    memset(&out, 0, sizeof(out));
    out.allowed = allowed;
    out.state = state;
    out.requires_approval = approval;
    out.requires_privileged_broker = broker;
    snprintf(out.reason, sizeof(out.reason), "%s", reason);
    return out;
}

static int is_sensitive_path_text(const char *path)
{
    return starts_with(path, "/etc/shadow") || starts_with(path, "/etc/sudoers") ||
           starts_with(path, "/dev/mem") || starts_with(path, "/proc/kcore") ||
           starts_with(path, "/private/var/db") ||
           contains(path, "/.codex/auth.json") ||
           starts_with(path, "~/.codex/auth.json") ||
           contains_path_segment(path, ".ssh") ||
           contains(path, "/.config/gcos/llm_api_key") ||
           starts_with(path, "~/.config/gcos/llm_api_key") ||
           contains(path, "/.config/gcos/openai_api_key") ||
           starts_with(path, "~/.config/gcos/openai_api_key") ||
           contains(path, "/.aws/credentials") ||
           starts_with(path, "~/.aws/credentials") ||
           contains(path, "/.git-credentials") ||
           starts_with(path, "~/.git-credentials") ||
           contains(path, "/.netrc") || starts_with(path, "~/.netrc");
}

static int is_sensitive_path(const char *path)
{
    char resolved[PATH_MAX];

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (is_sensitive_path_text(path)) {
        return 1;
    }

    if (gcos_realpath(path, resolved) != NULL &&
        is_sensitive_path_text(resolved)) {
        return 1;
    }

    return 0;
}

static int is_macos_privacy_path(const char *path)
{
    char resolved[PATH_MAX];

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (is_macos_privacy_path_text(path)) {
        return 1;
    }

    if (gcos_realpath(path, resolved) != NULL &&
        is_macos_privacy_path_text(resolved)) {
        return 1;
    }

    return 0;
}

static int is_safe_shell(const char *command)
{
    return strcmp(command, "pwd") == 0 || strcmp(command, "whoami") == 0 ||
           strcmp(command, "id") == 0 || strcmp(command, "uname") == 0 ||
           strcmp(command, "uname -a") == 0;
}

static int starts_with_destructive_command(const char *command)
{
    const char *blocked[] = {"rm",      "rmdir",  "dd",      "mkfs",
                             "mount",   "umount", "shutdown", "reboot",
                             "halt",    "chmod",  "chown",   "kill",
                             "killall", "systemctl", "launchctl"};
    size_t i;

    for (i = 0; i < sizeof(blocked) / sizeof(blocked[0]); i++) {
        size_t len = strlen(blocked[i]);
        if (strncmp(command, blocked[i], len) == 0 &&
            (command[len] == '\0' || isspace((unsigned char)command[len]))) {
            return 1;
        }
    }

    return 0;
}

PolicyDecision policy_decide(const ActionRequest *action, int approval_granted)
{
    if (action->kind == ACTION_NONE) {
        return decision(1, AGENT_READY, 0, 0, "no control-plane action");
    }

    if (action->kind == ACTION_KERNEL) {
        return decision(
            0, AGENT_BLOCKED_ON_APPROVAL, 1, 1,
            "kernel-space change requires signed module/eBPF/LSM design review");
    }

    if (action->kind == ACTION_CODEX) {
        return decision(1, AGENT_READY, 0, 0,
                        "LLM broker request allowed through API key or Codex CLI");
    }

    if (action->kind == ACTION_ROOT && !approval_granted) {
        return decision(0, AGENT_BLOCKED_ON_APPROVAL, 1, 1,
                        "root-level action requested; waiting for approval");
    }

    if (action->kind == ACTION_ROOT && approval_granted) {
        return decision(1, AGENT_READY, 0, 1,
                        "approved for privileged broker handoff");
    }

    if ((action->kind == ACTION_LIST || action->kind == ACTION_READ) &&
        is_macos_privacy_path(action->target)) {
        return decision(0, AGENT_BLOCKED_ON_APPROVAL, 1, 0,
                        "macOS privacy-protected user folder is blocked by default");
    }

    if ((action->kind == ACTION_LIST || action->kind == ACTION_READ) &&
        is_sensitive_path(action->target)) {
        return decision(0, AGENT_BLOCKED_ON_APPROVAL, 1, 0,
                        "sensitive path is protected by policy");
    }

    if (action->kind == ACTION_LIST || action->kind == ACTION_READ) {
        return decision(1, AGENT_READY, 0, 0,
                        "read-only filesystem action allowed");
    }

    if (action->kind == ACTION_SHELL) {
        if (starts_with_destructive_command(action->target)) {
            return decision(0, AGENT_BLOCKED_ON_APPROVAL, 1,
                            action->requires_root,
                            "potentially destructive command is blocked");
        }

        if (!is_safe_shell(action->target)) {
            return decision(0, AGENT_BLOCKED_ON_APPROVAL, 1,
                            action->requires_root,
                            "command is outside the safe allowlist");
        }

        return decision(1, AGENT_READY, 0, action->requires_root,
                        "safe shell command allowed");
    }

    return decision(0, AGENT_ERROR, 0, 0, "unsupported action");
}

static int append(char *out, size_t out_size, const char *text)
{
    size_t used;
    size_t available;

    if (out_size == 0 || text == NULL) {
        return 0;
    }

    used = strlen(out);
    if (used >= out_size - 1) {
        return 0;
    }

    available = out_size - used - 1;
    strncat(out, text, available);
    return 1;
}

static int reject_protected_path_before_io(const char *path, char *out,
                                           size_t out_size)
{
    if (is_macos_privacy_path(path)) {
        snprintf(out, out_size, "macOS privacy path rejected by final policy");
        return 1;
    }

    if (is_sensitive_path(path)) {
        snprintf(out, out_size, "sensitive path rejected by final policy");
        return 1;
    }

    return 0;
}

static int reject_protected_open_fd(int fd, char *out, size_t out_size)
{
#if defined(__APPLE__) && defined(F_GETPATH)
    char opened_path[PATH_MAX];

    if (fcntl(fd, F_GETPATH, opened_path) == 0 &&
        reject_protected_path_before_io(opened_path, out, out_size)) {
        return 1;
    }
#else
    (void)fd;
    (void)out;
    (void)out_size;
#endif
    return 0;
}

#ifdef _WIN32
static int list_directory_windows(const char *path, char *out, size_t out_size)
{
    WIN32_FIND_DATAA data;
    HANDLE handle;
    char pattern[PATH_MAX];
    size_t len;
    int count = 0;

    snprintf(pattern, sizeof(pattern), "%s", path);
    len = strlen(pattern);
    if (len > 0 && pattern[len - 1] != '\\' && pattern[len - 1] != '/') {
        snprintf(pattern + len, sizeof(pattern) - len, "\\*");
    } else {
        snprintf(pattern + len, sizeof(pattern) - len, "*");
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        snprintf(out, out_size, "FindFirstFile failed: %lu",
                 GetLastError());
        return 0;
    }

    do {
        append(out, out_size, data.cFileName);
        append(out, out_size, "\n");
        count++;
    } while (count < 200 && FindNextFileA(handle, &data));

    FindClose(handle);
    return 1;
}
#endif

int policy_run_action(const ActionRequest *action, char *out, size_t out_size)
{
    FILE *file;
#ifndef _WIN32
    DIR *dir;
    struct dirent *entry;
#endif
    char line[256];
    int count = 0;
#ifndef _WIN32
    int fd;
#endif

    if (out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    if (action->kind == ACTION_LIST) {
        if (reject_protected_path_before_io(action->target, out, out_size)) {
            return 0;
        }
#ifdef _WIN32
        return list_directory_windows(action->target, out, out_size);
#else
        fd = open(action->target, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        if (fd < 0) {
            snprintf(out, out_size, "open directory failed: %s",
                     strerror(errno));
            return 0;
        }
        if (reject_protected_open_fd(fd, out, out_size)) {
            close(fd);
            return 0;
        }
        dir = fdopendir(fd);
        if (dir == NULL) {
            snprintf(out, out_size, "fdopendir failed: %s", strerror(errno));
            close(fd);
            return 0;
        }

        while ((entry = readdir(dir)) != NULL && count < 200) {
            append(out, out_size, entry->d_name);
            append(out, out_size, "\n");
            count++;
        }
        closedir(dir);
        return 1;
#endif
    }

    if (action->kind == ACTION_READ) {
        if (reject_protected_path_before_io(action->target, out, out_size)) {
            return 0;
        }
#ifdef _WIN32
        file = fopen(action->target, "r");
        if (file == NULL) {
            snprintf(out, out_size, "fopen failed: %s", strerror(errno));
            return 0;
        }
#else
        fd = open(action->target, O_RDONLY | O_NOFOLLOW);
        if (fd < 0) {
            snprintf(out, out_size, "open failed: %s", strerror(errno));
            return 0;
        }
        if (reject_protected_open_fd(fd, out, out_size)) {
            close(fd);
            return 0;
        }
        file = fdopen(fd, "r");
        if (file == NULL) {
            snprintf(out, out_size, "fdopen failed: %s", strerror(errno));
            close(fd);
            return 0;
        }
#endif

        while (fgets(line, sizeof(line), file) != NULL) {
            if (!append(out, out_size, line)) {
                break;
            }
        }
        fclose(file);
        return 1;
    }

    if (action->kind == ACTION_ROOT) {
        snprintf(out, out_size, "approved privileged broker request: %s",
                 action->target);
        return 1;
    }

    if (action->kind == ACTION_CODEX) {
        return codex_broker_run(action->target, out, out_size);
    }

    if (action->kind == ACTION_SHELL) {
        if (!is_safe_shell(action->target)) {
            snprintf(out, out_size, "shell command rejected by final allowlist");
            return 0;
        }

        file = popen(action->target, "r");
        if (file == NULL) {
            snprintf(out, out_size, "popen failed: %s", strerror(errno));
            return 0;
        }

        while (fgets(line, sizeof(line), file) != NULL) {
            if (!append(out, out_size, line)) {
                break;
            }
        }
        pclose(file);
        return 1;
    }

    snprintf(out, out_size, "unsupported action");
    return 0;
}
