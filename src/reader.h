#ifndef READER_H
#define READER_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include "core/buffer.h"

/* Okuyucu yapısı (README Bölüm 11) */
typedef struct {
    FILE *file;             /* Dosya işaretçisi (stdin için NULL) */
    bool is_stdin;          /* Girdi stdin mi? */
    char *line;             /* Okunan satır (dinamik) */
    size_t line_cap;        /* Satır kapasitesi */
    size_t max_line_bytes;  /* Satır başına üst sınır (README Bölüm 5.3) */
    bool truncated;         /* Satır kesildi mi? */
} Reader;

/* Başlatma ve Temizlik */
bool reader_init(Reader *r, const char *path, size_t max_line_bytes);
void reader_free(Reader *r);

/* Satır okuma (başarılıysa true, EOF'ta false döner) */
bool reader_read_line(Reader *r);

#endif /* READER_H */