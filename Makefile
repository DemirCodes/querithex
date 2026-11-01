# Querithex — Makefile
# README Bölüm 13 (Faz 0): -std=c11 -Wall -Wextra -Wpedantic -Werror
#                          debug hedefi -fsanitize=address,undefined

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS  ?=

SRC_DIR   := src
CORE_DIR  := $(SRC_DIR)/core
BUILD_DIR := build
TARGET    := querithex

SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/cli.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/reader.c \
	$(SRC_DIR)/log_parser.c \
	$(SRC_DIR)/sql_lexer.c \
	$(SRC_DIR)/fingerprint.c \
	$(SRC_DIR)/schema_introspect.c \
	$(SRC_DIR)/explain_collect.c \
	$(SRC_DIR)/security.c \
	$(SRC_DIR)/performance.c \
	$(SRC_DIR)/incidents.c \
	$(SRC_DIR)/stats.c \
	$(SRC_DIR)/ai_advisor.c \
	$(SRC_DIR)/report.c \
	$(SRC_DIR)/report_json.c \
	$(SRC_DIR)/report_csv.c \
	$(SRC_DIR)/report_html.c \
	$(SRC_DIR)/signal.c \
	$(CORE_DIR)/buffer.c \
	$(CORE_DIR)/array.c \
	$(CORE_DIR)/hashmap.c \
	$(CORE_DIR)/arena.c

OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean debug test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Debug hedefi: ASan + UBSan (README Faz 0)
debug: CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O0 -fsanitize=address,undefined
debug: LDFLAGS := -fsanitize=address,undefined
debug: clean all

test: all
	./tests/run.sh

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
