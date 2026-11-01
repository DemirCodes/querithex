CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS  ?=

SRC_DIR   := src
BUILD_DIR := build
TARGET    := querithex

SRCS := \
	src/main.c \
	src/cli.c \
	src/config.c \
	src/reader.c \
	src/log_parser.c \
	src/sql_lexer.c \
	src/fingerprint.c \
	src/schema_introspect.c \
	src/explain_collect.c \
	src/security.c \
	src/performance.c \
	src/incidents.c \
	src/stats.c \
	src/ai_advisor.c \
	src/report.c \
	src/report_json.c \
	src/report_csv.c \
	src/report_html.c \
	src/signal.c \
	src/core/buffer.c \
	src/core/array.c \
	src/core/hashmap.c \
	src/core/arena.c

OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))

.PHONY: all clean debug test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

debug: CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O0 -fsanitize=address,undefined
debug: LDFLAGS := -fsanitize=address,undefined
debug: clean all

test: all
	./tests/run.sh

clean:
	rm -rf $(BUILD_DIR) $(TARGET)