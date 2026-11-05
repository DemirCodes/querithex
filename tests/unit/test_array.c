#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "array.h"

void test_array_basic(void) {
    Array a;
    array_init(&a);
    
    assert(a.len == 0);
    assert(a.data != NULL);

    // Push test
    int val1 = 10;
    int val2 = 20;
    assert(array_push(&a, &val1) == true);
    assert(array_push(&a, &val2) == true);
    assert(a.len == 2);

    // Get test
    assert(array_get(&a, 0) == &val1);
    assert(array_get(&a, 1) == &val2);
    assert(array_get(&a, 2) == NULL); // Sınır dışı

    // Clear test
    array_clear(&a);
    assert(a.len == 0);

    array_free(&a);
    printf("test_array_basic passed\n");
}

int main(void) {
    test_array_basic();
    printf("All array tests passed successfully!\n");
    return 0;
}