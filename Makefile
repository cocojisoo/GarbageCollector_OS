CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude
CORE_LIBS ?=
EXEEXT ?=

BUILD_DIR := build
TARGET := $(BUILD_DIR)/gcos-tui$(EXEEXT)
CORE_SRCS := src/agent.c src/policy.c src/codex_broker.c
SRCS := src/main.c src/tui.c $(CORE_SRCS)
OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

.PHONY: all run clean check context codex-check api-check portable-check

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c include/gcos.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(CORE_LIBS) -o $@

run: $(TARGET)
	$(TARGET)

check: $(TARGET)
	$(TARGET) --self-test
	$(TARGET) --os-demo-smoke
	$(TARGET) --tui-smoke
	$(TARGET) --input-smoke
	$(TARGET) --wrap-smoke
	$(TARGET) --api-config-smoke

context:
	sh scripts/gcos-context

portable-check:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $(SRCS)

codex-check: $(TARGET)
	GCOS_LLM_BACKEND=codex $(TARGET) --codex-smoke

api-check: $(TARGET)
	GCOS_LLM_BACKEND=api $(TARGET) --api-smoke

clean:
	rm -rf $(BUILD_DIR)
