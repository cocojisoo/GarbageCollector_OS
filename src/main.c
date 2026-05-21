#include "gcos.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    AgentRuntime runtime;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--self-test") == 0) {
            return self_test();
        }
        if (strcmp(argv[i], "--os-demo-smoke") == 0) {
            return os_demo_smoke_test();
        }
        if (strcmp(argv[i], "--tui-smoke") == 0) {
            return tui_smoke_test();
        }
        if (strcmp(argv[i], "--rr-smoke") == 0) {
            return rr_smoke_test();
        }
        if (strcmp(argv[i], "--input-smoke") == 0) {
            return input_smoke_test();
        }
        if (strcmp(argv[i], "--wrap-smoke") == 0) {
            return wrap_smoke_test();
        }
        if (strcmp(argv[i], "--api-config-smoke") == 0) {
            return api_config_smoke_test();
        }
        if (strcmp(argv[i], "--codex-smoke") == 0) {
            return codex_smoke_test();
        }
        if (strcmp(argv[i], "--api-smoke") == 0) {
            return api_smoke_test();
        }
    }

    runtime_init(&runtime);
    return tui_run(&runtime);
}
