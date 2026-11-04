#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../../src/core/buffer.h"

void test_buffer_basic(void) {
    Buffer b;
    buffer_init(&b);
    
    assert(b.len == 0);
    assert(b.data != NULL);

    // Append test
    assert(buffer_append(&b, "Hello") == true);
    assert(b.len == 5);
    assert(strcmp(buffer_cstr(&b), "Hello") == 0);

    // Append char test
    assert(buffer_append_char(&b, ' ') == true);
    assert(buffer_append(&b, "World") == true);
    assert(strcmp(buffer_cstr(&b), "Hello World") == 0);

    // Clear test
    buffer_clear(&b);
    assert(b.len == 0);
    assert(strcmp(buffer_cstr(&b), "") == 0);

    buffer_free(&b);
    printf("test_buffer_basic passed\n");
}

void test_buffer_fmt(void) {
    Buffer b;
    buffer_init(&b);

    // Format test
    assert(buffer_append_fmt(&b, "User: %s, ID: %d", "admin", 42) == true);
    assert(strcmp(buffer_cstr(&b), "User: admin, ID: 42") == 0);

    buffer_free(&b);
    printf("test_buffer_fmt passed\n");
}

int main(void) {
    test_buffer_basic();
    test_buffer_fmt();
    printf("All buffer tests passed successfully!\n");
    return 0;
}