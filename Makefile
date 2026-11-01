CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS  ?=

TARGET    := querithex
BUILD_DIR := build

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

# Her dosya için ayrı ayrı kural (pattern kullanmadan, %100 garantili)
$(BUILD_DIR)/main.o: src/main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/cli.o: src/cli.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/config.o: src/config.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/reader.o: src/reader.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/log_parser.o: src/log_parser.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/sql_lexer.o: src/sql_lexer.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/fingerprint.o: src/fingerprint.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/schema_introspect.o: src/schema_introspect.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/explain_collect.o: src/explain_collect.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/security.o: src/security.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/performance.o: src/performance.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/incidents.o: src/incidents.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/stats.o: src/stats.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/ai_advisor.o: src/ai_advisor.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/report.o: src/report.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/report_json.o: src/report_json.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/report_csv.o: src/report_csv.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/report_html.o: src/report_html.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/signal.o: src/signal.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/core/buffer.o: src/core/buffer.c
	@mkdir -p $(BUILD_DIR)/core
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/core/array.o: src/core/array.c
	@mkdir -p $(BUILD_DIR)/core
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/core/hashmap.o: src/core/hashmap.c
	@mkdir -p $(BUILD_DIR)/core
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/core/arena.o: src/core/arena.c
	@mkdir -p $(BUILD_DIR)/core
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

debug: CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O0 -fsanitize=address,undefined
debug: LDFLAGS := -fsanitize=address,undefined
debug: clean all

test: all
	./tests/run.sh

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
