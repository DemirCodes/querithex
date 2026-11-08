#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "arena.h"

void test_arena_basic(void) {
    Arena a;
    arena_init(&a, 1024);
    assert(a.head == NULL); // Tembel ayırma: başlangıçta blok yok
    assert(a.block_size == 1024);

    // Bellek ayır
    int *p1 = (int *)arena_alloc(&a, sizeof(int));
    assert(p1 != NULL);
    *p1 = 42;
    assert(a.head != NULL); // Şimdi blok var

    char *p2 = (char *)arena_alloc(&a, 100);
    assert(p2 != NULL);
    strcpy(p2, "hello arena");

    assert(*p1 == 42);
    assert(strcmp(p2, "hello arena") == 0);

    arena_free(&a);
    assert(a.head == NULL);
    printf("test_arena_basic passed\n");
}

void test_arena_large_alloc(void) {
    Arena a;
    arena_init(&a, 64);

    char *p = (char *)arena_alloc(&a, 1000);
    assert(p != NULL);
    memset(p, 'A', 1000);
    assert(p[0] == 'A');
    assert(p[999] == 'A');

    arena_free(&a);
    printf("test_arena_large_alloc passed\n");
}

void test_arena_many_allocs(void) {
    Arena a;
    arena_init(&a, 128);

    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = arena_alloc(&a, 16);
        assert(ptrs[i] != NULL);
    }

    for (int i = 0; i < 100; i++) {
        for (int j = i + 1; j < 100; j++) {
            assert(ptrs[i] != ptrs[j]);
        }
    }

    arena_free(&a);
    printf("test_arena_many_allocs passed\n");
}

void test_arena_null_args(void) {
    arena_free(NULL);
    
    Arena a;
    arena_init(&a, 0);
    assert(a.block_size > 0);
    arena_free(&a);

    printf("test_arena_null_args passed\n");
}

int main(void) {
    test_arena_basic();
    test_arena_large_alloc();
    test_arena_many_allocs();
    test_arena_null_args();
    printf("All arena tests passed successfully!\n");
    return 0;
}