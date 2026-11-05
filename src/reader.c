#define _POSIX_C_SOURCE 200809L

#include "reader.h"
#include <stdlib.h>
#include <string.h>

bool reader_init(Reader *r, const char *path, size_t max_line_bytes) {
    if (!r) return false;

    r->line = NULL;
    r->line_cap = 0;
    r->max_line_bytes = max_line_bytes > 0 ? max_line_bytes : 1048576; // Varsayılan 1MB
    r->truncated = false;

    if (!path || strcmp(path, "-") == 0) {
        r->file = stdin;
        r->is_stdin = true;
    } else {
        r->file = fopen(path, "r");
        r->is_stdin = false;
        if (!r->file) return false;
    }

    return true;
}

void reader_free(Reader *r) {
    if (!r) return;
    if (r->file && !r->is_stdin) {
        fclose(r->file);
    }
    free(r->line);
    r->line = NULL;
    r->line_cap = 0;
}

bool reader_read_line(Reader *r) {
    if (!r || !r->file) return false;

    // getline kullanarak satırı oku
    ssize_t read_bytes = getline(&r->line, &r->line_cap, r->file);
    if (read_bytes == -1) {
        return false; // EOF veya hata
    }

    r->truncated = false;

    // Eğer satır sınırı aşıldıysa kes
    if ((size_t)read_bytes > r->max_line_bytes) {
        r->line[r->max_line_bytes] = '\0';
        r->truncated = true;
    }

    return true;
}