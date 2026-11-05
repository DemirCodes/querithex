#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "hashmap.h"

/* Basit string hash fonksiyonu (FNV-1a) */
static uint64_t str_hash(const void *key) {
    const char *s = (const char *)key;
    uint64_t h = 14695981039346656037ULL; // 8 kentre FNV offset basis
    while (*s) {
        h ^= (uint64_t)(unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

/* Basit string eşitlik fonksiyonu */
static bool str_eq(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b) == 0;
}

void test_hashmap_basic(void) {
    HashMap m;
    assert(hashmap_init(&m, str_hash, str_eq) == true);

    // Put test
    int val1 = 100;
    int val2 = 200;
    assert(hashmap_put(&m, "apple", &val1) == true);
    assert(hashmap_put(&m, "banana", &val2) == true);
    assert(m.len == 2);

    // Get test
    assert(hashmap_get(&m, "apple") == &val1);
    assert(hashmap_get(&m, "banana") == &val2);
    assert(hashmap_get(&m, "cherry") == NULL); // Olmayan key

    // Güncelleme testi
    int val3 = 300;
    assert(hashmap_put(&m, "apple", &val3) == true);
    assert(m.len == 2); // Hâlâ 2 eleman (güncellendi)
    assert(hashmap_get(&m, "apple") == &val3);

    // Remove test
    assert(hashmap_remove(&m, "apple") == true);
    assert(hashmap_get(&m, "apple") == NULL);
    assert(m.len == 1);

    // Olmayan key'i silmeye çalış
    assert(hashmap_remove(&m, "cherry") == false);

    hashmap_free(&m);
    printf("test_hashmap_basic passed\n");
}

int main(void) {
    test_hashmap_basic();
    printf("All hashmap tests passed successfully!\n");
    return 0;
}