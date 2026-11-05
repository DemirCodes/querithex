#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "reader.h"

void test_reader_basic(void) {
    const char *test_file = "tests/fixtures/tmp_reader_test.log";
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fprintf(f, "line1\nline2\nline3\n");
    fclose(f);

    Reader r;
    assert(reader_init(&r, test_file, 1024) == true);

    assert(reader_read_line(&r) == true);
    assert(strcmp(r.line, "line1\n") == 0);
    assert(r.truncated == false);

    assert(reader_read_line(&r) == true);
    assert(strcmp(r.line, "line2\n") == 0);

    assert(reader_read_line(&r) == true);
    assert(strcmp(r.line, "line3\n") == 0);

    assert(reader_read_line(&r) == false); // EOF

    reader_free(&r);
    remove(test_file);
    printf("test_reader_basic passed\n");
}

void test_reader_truncation(void) {
    const char *test_file = "tests/fixtures/tmp_reader_long.log";
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fprintf(f, "kisa\n");
    for (int i = 0; i < 100; i++) fprintf(f, "A");
    fprintf(f, "\n");
    fclose(f);

    Reader r;
    assert(reader_init(&r, test_file, 50) == true);

    assert(reader_read_line(&r) == true);
    assert(strcmp(r.line, "kisa\n") == 0);
    assert(r.truncated == false);

    assert(reader_read_line(&r) == true);
    assert(strlen(r.line) == 50); // Tam 50 byte olmalı
    assert(r.truncated == true);  // Kesildi işareti

    reader_free(&r);
    remove(test_file);
    printf("test_reader_truncation passed\n");
}

void test_reader_boundary(void) {
    const char *test_file = "tests/fixtures/tmp_reader_bound.log";
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    // Tam sınırda: 50 karakter (49 A + 1 \n = 50 byte)
    for (int i = 0; i < 49; i++) fprintf(f, "A");
    fprintf(f, "\n");
    // Bir fazlası: 51 karakter
    for (int i = 0; i < 50; i++) fprintf(f, "B");
    fprintf(f, "\n");
    fclose(f);

    Reader r;
    assert(reader_init(&r, test_file, 50) == true);

    // 1. Satır: 50 byte (sınırda) -> Kesilmemeli
    assert(reader_read_line(&r) == true);
    assert(r.truncated == false);
    assert(strlen(r.line) == 50);

    // 2. Satır: 51 byte (sınırı aşıyor) -> Kesilmeli
    assert(reader_read_line(&r) == true);
    assert(r.truncated == true);
    assert(strlen(r.line) == 50);

    reader_free(&r);
    remove(test_file);
    printf("test_reader_boundary passed\n");
}

void test_reader_empty_file(void) {
    const char *test_file = "tests/fixtures/tmp_reader_empty.log";
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fclose(f); // Boş dosya

    Reader r;
    assert(reader_init(&r, test_file, 1024) == true);

    // Boş dosyada EOF hemen dönmeli
    assert(reader_read_line(&r) == false);
    // Art arda EOF çağrısı da sorun çıkarmamalı
    assert(reader_read_line(&r) == false);

    reader_free(&r);
    remove(test_file);
    printf("test_reader_empty_file passed\n");
}

void test_reader_null_args(void) {
    Reader r;
    // NULL reader ile başlatma başarısız olmalı
    assert(reader_init(NULL, "test.log", 1024) == false);
    
    // NULL reader ile okuma başarısız olmalı (çökmemeli)
    assert(reader_read_line(NULL) == false);
    
    // NULL reader ile free çökmemeli
    reader_free(NULL);
    
    // Olmayan dosya ile başlatma başarısız olmalı
    assert(reader_init(&r, "bu_dosya_yok_12345.log", 1024) == false);

    printf("test_reader_null_args passed\n");
}

void test_reader_stdin(void) {
    Reader r;
    // "-" stdin anlamına gelir, başlatma başarılı olmalı
    assert(reader_init(&r, "-", 1024) == true);
    assert(r.is_stdin == true);
    assert(r.file == stdin);
    reader_free(&r);
    
    // NULL path de stdin anlamına gelir
    assert(reader_init(&r, NULL, 1024) == true);
    assert(r.is_stdin == true);
    reader_free(&r);
    
    printf("test_reader_stdin passed\n");
}

int main(void) {
    test_reader_basic();
    test_reader_truncation();
    test_reader_boundary();
    test_reader_empty_file();
    test_reader_null_args();
    test_reader_stdin();
    printf("All reader tests passed successfully!\n");
    return 0;
}