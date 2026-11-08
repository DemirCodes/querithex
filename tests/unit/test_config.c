#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "config.h"

void test_config_defaults(void) {
    Config c;
    config_init_defaults(&c);

    assert(c.slow_threshold_ms == 100);
    assert(c.very_slow_threshold_ms == 1000);
    assert(c.top_n == 20);
    assert(c.max_line_bytes == 1048576);
    assert(c.follow_summary_interval_s == 60);
    assert(c.ai_enabled == false);
    assert(strcmp(c.ai_provider, "none") == 0);

    config_free(&c);
    printf("test_config_defaults passed\n");
}

void test_config_load_basic(void) {
    const char *test_file = "tests/fixtures/tmp_querithex.conf";
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fprintf(f, "[genel]\n");
    fprintf(f, "slow_threshold_ms = 250\n");
    fprintf(f, "top_n = 50\n");
    fprintf(f, "\n");
    fprintf(f, "[ai]\n");
    fprintf(f, "enabled = true\n");
    fprintf(f, "provider = openai\n");
    fclose(f);

    Config c;
    config_init_defaults(&c);
    assert(config_load(&c, test_file) == true);

    assert(c.slow_threshold_ms == 250);
    assert(c.top_n == 50);
    assert(c.ai_enabled == true);
    assert(strcmp(c.ai_provider, "openai") == 0);
    // Diğer varsayılanlar bozulmamalı
    assert(c.very_slow_threshold_ms == 1000);

    config_free(&c);
    remove(test_file);
    printf("test_config_load_basic passed\n");
}

void test_config_load_rules(void) {
    const char *test_file = "tests/fixtures/tmp_querithex_rules.conf";
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fprintf(f, "[kurallar]\n");
    fprintf(f, "S1 = off\n");
    fprintf(f, "B1 = on\n");
    fprintf(f, "\n");
    fprintf(f, "[kural:S2]\n");
    fprintf(f, "pattern = union.*select\n");
    fprintf(f, "severity = high\n");
    fprintf(f, "description = UNION tabanli\n");
    fprintf(f, "enabled = true\n");
    fclose(f);

    Config c;
    config_init_defaults(&c);
    assert(config_load(&c, test_file) == true);

    assert(c.kural_s1 == false);
    assert(c.kural_b1 == true);
    assert(c.rules_count == 1);
    
    const Rule *r = config_get_rule(&c, "S2");
    assert(r != NULL);
    assert(strcmp(r->pattern, "union.*select") == 0);
    assert(strcmp(r->severity, "high") == 0);
    assert(r->enabled == true);

    config_free(&c);
    remove(test_file);
    printf("test_config_load_rules passed\n");
}

void test_config_load_missing_file(void) {
    Config c;
    config_init_defaults(&c);
    assert(config_load(&c, "bu_dosya_yok_6789.conf") == false);
    assert(c.slow_threshold_ms == 100);
    config_free(&c);
    printf("test_config_load_missing_file passed\n");
}

int main(void) {
    test_config_defaults();
    test_config_load_basic();
    test_config_load_rules();
    test_config_load_missing_file();
    printf("All config tests passed successfully!\n");
    return 0;
}