#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "log_parser.h"

void test_log_parser_timezones(void) {
    const char *test_file = "tests/fixtures/tmp_log_parser_tz.log";
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    
    // 1. Kısa timezone (UTC)
    fprintf(f, "2026-10-30 14:30:15.123 UTC [12345] postgres@testdb/psql 10.0.0.5 LOG:  statement: SELECT 1;\n");
    // 2. Avrupa/Istanbul (uzun timezone)
    fprintf(f, "2026-10-30 17:30:15.123 Europe/Istanbul [12346] postgres@testdb/psql 10.0.0.5 LOG:  statement: SELECT 2;\n");
    // 3. Numerik offset (+0300)
    fprintf(f, "2026-10-30 17:30:15.123 +0300 [12347] postgres@testdb/psql 10.0.0.5 LOG:  statement: SELECT 3;\n");
    fclose(f);

    FILE *in = fopen(test_file, "r");
    assert(in != NULL);

    LogParser p;
    assert(log_parser_init(&p, 1048576) == true);

    LogEvent event;

    // Test 1: UTC
    assert(log_parser_next(&p, in, &event) == true);
    printf("DEBUG: timestamp = '%s'\n", event.timestamp);
    assert(strcmp(event.timestamp, "2026-10-30 14:30:15.123 UTC") == 0);
    assert(event.pid == 12345);
    log_event_free(&event);

    // Test 2: Europe/Istanbul
    assert(log_parser_next(&p, in, &event) == true);
    printf("DEBUG: timestamp = '%s'\n", event.timestamp);
    assert(strcmp(event.timestamp, "2026-10-30 17:30:15.123 Europe/Istanbul") == 0);
    assert(event.pid == 12346);
    log_event_free(&event);

    // Test 3: +0300
    assert(log_parser_next(&p, in, &event) == true);
    printf("DEBUG: timestamp = '%s'\n", event.timestamp);
    assert(strcmp(event.timestamp, "2026-10-30 17:30:15.123 +0300") == 0);
    assert(event.pid == 12347);
    log_event_free(&event);

    log_parser_free(&p);
    fclose(in);
    remove(test_file);
    printf("test_log_parser_timezones passed\n");
}

int main(void) {
    test_log_parser_timezones();
    printf("All log_parser tests passed successfully!\n");
    return 0;
}