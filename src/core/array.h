#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>
#include <stdbool.h>

/* Dinamik dizi yapısı (README Bölüm 11) */
typedef struct {
    void **data;    /* Elemanları tutan dizi */
    size_t len;     /* Mevcut eleman sayısı */
    size_t cap;     /* Toplam kapasite */
} Array;

/* Başlatma ve Temizlik */
void array_init(Array *a);
void array_free(Array *a);

/* Veri Ekleme */
bool array_push(Array *a, void *item);
void *array_get(const Array *a, size_t index);
void array_clear(Array *a);

#endif /* ARRAY_H */