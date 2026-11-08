#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "fingerprint.h"


void test_fingerprint_basic(void) {
    QueryFingerprint fp;
    fingerprint_init(&fp);

    // Basit bir sorgu
    const char *query = "SELECT * FROM users WHERE id = 42";
    assert(fingerprint_generate(&fp, query) == true);
    
    // Normalize edilmiş sorgu literalleri '?' yapmalı
    assert(strcmp(fp.normalized, "select * from users where id = ?") == 0);
    
    // Hash sıfırdan farklı olmalı
    assert(fp.hash != 0);

    fingerprint_free(&fp);
    printf("test_fingerprint_basic passed\n");
}

void test_fingerprint_different_literals(void) {
    QueryFingerprint fp1, fp2;
    fingerprint_init(&fp1);
    fingerprint_init(&fp2);

    // Aynı sorgu, farklı literaller
    assert(fingerprint_generate(&fp1, "SELECT * FROM users WHERE id = 42") == true);
    assert(fingerprint_generate(&fp2, "SELECT * FROM users WHERE id = 99") == true);

    // Normalize edilmiş sorgular aynı olmalı
    assert(strcmp(fp1.normalized, fp2.normalized) == 0);
    
    // Hash'ler de aynı olmalı
    assert(fp1.hash == fp2.hash);

    fingerprint_free(&fp1);
    fingerprint_free(&fp2);
    printf("test_fingerprint_different_literals passed\n");
}

void test_fingerprint_string_literals(void) {
    QueryFingerprint fp;
    fingerprint_init(&fp);

    // String literaller
    const char *query = "SELECT * FROM users WHERE name = 'admin'";
    assert(fingerprint_generate(&fp, query) == true);
    assert(strcmp(fp.normalized, "select * from users where name = ?") == 0);

    fingerprint_free(&fp);
    printf("test_fingerprint_string_literals passed\n");
}

void test_fingerprint_comments(void) {
    QueryFingerprint fp;
    fingerprint_init(&fp);

    // Yorumlar silinmeli
    const char *query = "SELECT * FROM users -- bu bir yorum\nWHERE id = 1";
    assert(fingerprint_generate(&fp, query) == true);
    assert(strcmp(fp.normalized, "select * from users where id = ?") == 0);

    fingerprint_free(&fp);
    printf("test_fingerprint_comments passed\n");
}

int main(void) {
    test_fingerprint_basic();
    test_fingerprint_different_literals();
    test_fingerprint_string_literals();
    test_fingerprint_comments();
    printf("All fingerprint tests passed successfully!\n");
    return 0;
}