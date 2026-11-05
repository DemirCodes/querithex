#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Hash fonksiyonu tipi: key -> 64-bit hash */
typedef uint64_t (*HashFunc)(const void *key);

/* Eşitlik fonksiyonu tipi: iki key eşit mi? */
typedef bool (*EqFunc)(const void *a, const void *b);

/* Hash tablosu yapısı (README Bölüm 11) */
typedef struct {
    void **keys;        /* Anahtarlar */
    void **values;      /* Değerler */
    size_t *hashes;     /* Önceden hesaplanmış hash'ler */
    size_t len;         /* Mevcut eleman sayısı */
    size_t cap;         /* Toplam kapasite */
    HashFunc hash_fn;   /* Hash fonksiyonu */
    EqFunc eq_fn;       /* Eşitlik fonksiyonu */
} HashMap;

/* Başlatma ve Temizlik */
bool hashmap_init(HashMap *m, HashFunc hash_fn, EqFunc eq_fn);
void hashmap_free(HashMap *m);

/* Veri Ekleme / Erişim */
bool hashmap_put(HashMap *m, void *key, void *value);
void *hashmap_get(const HashMap *m, const void *key);
bool hashmap_remove(HashMap *m, const void *key);

/* Yineleme (callback her eleman için çağrılır) */
typedef void (*IterFunc)(void *key, void *value, void *user_data);
void hashmap_foreach(const HashMap *m, IterFunc fn, void *user_data);

#endif /* HASHMAP_H */