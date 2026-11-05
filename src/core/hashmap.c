#include "hashmap.h"                 // HashMap yapısı ve fonksiyon prototipleri için başlık
#include <stdlib.h>                  // calloc, free için

#define HASHMAP_INIT_CAP 16          // HashMap ilk oluşturulduğunda ayrılacak slot sayısı
#define TOMBSTONE ((void *)-1)       // Silinmiş slotları işaretlemek için özel işaretçi değeri (geçerli bir adres değil)

/* Kapasiteyi artır (güvenli realloc) */
static bool hashmap_resize(HashMap *m, size_t new_cap) {   // Tabloyu yeni kapasiteye büyütür ve elemanları yeniden yerleştirir
    if (!m) return false;                                  // NULL pointer koruması
    
    // -- Guard 1: zero capacity check --
    if (new_cap == 0) return false;                        // Kapasite sıfır olamaz (0 kapasite geçersizdir)

    // -- Guard 2: new capacity must be larger than current --
    if (m->cap >= new_cap) return false;                   // Yeni kapasite mevcut kapasiteden büyük olmalı

    void **new_keys = (void **)calloc(new_cap, sizeof(void *));      // Yeni anahtar dizisi (sıfırlanmış)
    void **new_values = (void **)calloc(new_cap, sizeof(void *));    // Yeni değer dizisi (sıfırlanmış)
    size_t *new_hashes = (size_t *)calloc(new_cap, sizeof(size_t));  // Yeni hash önbellek dizisi (sıfırlanmış)
    
    if (!new_keys || !new_values || !new_hashes) {         // Herhangi bir ayırma başarısızsa
        free(new_keys);                                    // Kısmen ayrılan belleği temizle
        free(new_values);
        free(new_hashes);
        return false;                                      // Başarısız
    }

    // Eski elemanları yeni tabloya yerleştir
    for (size_t i = 0; i < m->cap; i++) {                  // Eski tablodaki tüm slotları gez
        if (m->keys[i] != NULL && m->keys[i] != TOMBSTONE) {   // Dolu ve mezar taşı olmayan slotlar
            size_t idx = m->hashes[i] % new_cap;           // Yeni tablodaki başlangıç indeksini hesapla
            while (new_keys[idx] != NULL) {                // Çakışma varsa (linear probing)
                idx = (idx + 1) % new_cap;                 // Bir sonraki slota geç
            }
            new_keys[idx] = m->keys[i];                    // Anahtarı yeni tabloya koy
            new_values[idx] = m->values[i];                // Değeri yeni tabloya kopyala
            new_hashes[idx] = m->hashes[i];                // Hash değerini kopyala
        }
    }

    free(m->keys);                                         // Eski anahtar dizisini serbest bırak
    free(m->values);                                       // Eski değer dizisini serbest bırak
    free(m->hashes);                                       // Eski hash dizisini serbest bırak

    m->keys = new_keys;                                    // Yeni dizileri kaydet
    m->values = new_values;
    m->hashes = new_hashes;
    m->cap = new_cap;                                      // Yeni kapasiteyi kaydet
    return true;                                           // Başarılı
}

bool hashmap_init(HashMap *m, HashFunc hash_fn, EqFunc eq_fn) {   // HashMap'i başlatır, hash ve eşitlik fonksiyonlarını atar
    if (!m || !hash_fn || !eq_fn) return false;                   // NULL pointer koruması
    
    m->keys = (void **)calloc(HASHMAP_INIT_CAP, sizeof(void *));      // Anahtar dizisi için bellek ayır (sıfırlanmış)
    m->values = (void **)calloc(HASHMAP_INIT_CAP, sizeof(void *));    // Değer dizisi için bellek ayır (sıfırlanmış)
    m->hashes = (size_t *)calloc(HASHMAP_INIT_CAP, sizeof(size_t));  // Hash önbellek dizisi için bellek ayır (sıfırlanmış)
    
    if (!m->keys || !m->values || !m->hashes) {                   // Herhangi bir ayırma başarısızsa
        free(m->keys);                                            // Kısmen ayrılan belleği temizle
        free(m->values);
        free(m->hashes);
        return false;                                             // Başarısız
    }

    m->len = 0;                                                   // Başlangıçta eleman yok
    m->cap = HASHMAP_INIT_CAP;                                    // Kapasiteyi kaydet
    m->hash_fn = hash_fn;                                         // Hash fonksiyonunu kaydet
    m->eq_fn = eq_fn;                                             // Eşitlik fonksiyonunu kaydet
    return true;                                                  // Başarılı
}

void hashmap_free(HashMap *m) {   // HashMap'in iç dizilerini serbest bırakır (anahtar/değerlerin gösterdiği bellekler serbest bırakılmaz)
    if (!m) return;               // NULL pointer koruması
    free(m->keys);                // Anahtar dizisini serbest bırak
    free(m->values);              // Değer dizisini serbest bırak
    free(m->hashes);              // Hash dizisini serbest bırak
    m->keys = NULL;               // Dangling pointer'ları önlemek için NULL ata
    m->values = NULL;
    m->hashes = NULL;
    m->len = 0;                   // Uzunluğu sıfırla
    m->cap = 0;                   // Kapasiteyi sıfırla
}

bool hashmap_put(HashMap *m, void *key, void *value) {   // Anahtar-değer çifti ekler veya varsa günceller
    if (!m || !key) return false;                        // NULL pointer koruması

    // Load factor % 70'i geçerse kapasiteyi iki katına çıkar
    if ((m->len + 1) * 10 >= m->cap * 7) {               // (len+1)/cap >= 0.7 kontrolü (tam sayı aritmetiğiyle)
        if (!hashmap_resize(m, m->cap * 2)) return false;   // Yeniden boyutlandırma başarısızsa çık
    }

    size_t hash = (size_t)m->hash_fn(key);               // Anahtarın hash değerini hesapla
    size_t idx = hash % m->cap;                          // Başlangıç indeksini hesapla
    size_t first_tombstone = (size_t)-1;                 // İlk bulunan mezar taşı indeksini tut (yoksa SIZE_MAX)

    while (m->keys[idx] != NULL) {                       // Boş slot bulana kadar dolaş
        if (m->keys[idx] == TOMBSTONE) {                 // Mezar taşına rastladıysak
            if (first_tombstone == (size_t)-1) first_tombstone = idx;   // İlk mezar taşını kaydet (yeniden kullanmak için)
        } else if (m->hashes[idx] == hash && m->eq_fn(m->keys[idx], key)) {   // Hash ve anahtar eşleşiyorsa
            // Anahtar zaten var, değeri güncelle
            m->values[idx] = value;                      // Değeri güncelle
            return true;                                 // Başarılı
        }
        idx = (idx + 1) % m->cap;                        // Bir sonraki slota geç (linear probing)
    }

    // Yeni eleman için boş yer (veya ilk çöp) bulundu
    if (first_tombstone != (size_t)-1) {                 // Dolaşma sırasında mezar taşı bulunduysa
        idx = first_tombstone;                           // Onu yeniden kullan (yeni slot açmak yerine)
    }

    m->keys[idx] = key;                                  // Anahtarı yerleştir
    m->values[idx] = value;                              // Değeri yerleştir
    m->hashes[idx] = hash;                               // Hash'i önbelleğe al (sonraki aramalarda tekrar hesaplamamak için)
    m->len++;                                            // Uzunluğu artır
    return true;                                         // Başarılı
}

void *hashmap_get(const HashMap *m, const void *key) {   // Anahtara karşılık gelen değeri döndürür, yoksa NULL
    if (!m || !key) return NULL;                         // NULL pointer koruması

    size_t hash = (size_t)m->hash_fn(key);               // Anahtarın hash değerini hesapla
    size_t idx = hash % m->cap;                          // Başlangıç indeksini hesapla

    while (m->keys[idx] != NULL) {                       // Boş slot bulana kadar dolaş
        if (m->keys[idx] != TOMBSTONE &&                 // Mezar taşı değilse
            m->hashes[idx] == hash &&                    // Hash değeri eşleşiyorsa
            m->eq_fn(m->keys[idx], key)) {               // Anahtar eşleşiyorsa
            return m->values[idx];                       // Değeri döndür
        }
        idx = (idx + 1) % m->cap;                        // Bir sonraki slota geç
    }
    return NULL;                                         // Bulunamadı
}

bool hashmap_remove(HashMap *m, const void *key) {   // Anahtarı ve değerini tablodan siler
    if (!m || !key) return false;                    // NULL pointer koruması

    size_t hash = (size_t)m->hash_fn(key);           // Anahtarın hash değerini hesapla
    size_t idx = hash % m->cap;                      // Başlangıç indeksini hesapla

    while (m->keys[idx] != NULL) {                   // Boş slot bulana kadar dolaş
        if (m->keys[idx] != TOMBSTONE &&             // Mezar taşı değilse
            m->hashes[idx] == hash &&                // Hash eşleşiyorsa
            m->eq_fn(m->keys[idx], key)) {           // Anahtar eşleşiyorsa
            // Silme işlemi için mezar taşı bırak
            m->keys[idx] = TOMBSTONE;                // Slotu mezar taşıyla işaretle (probe zincirini koparmamak için)
            m->values[idx] = NULL;                   // Değeri temizle
            m->len--;                                // Uzunluğu azalt
            return true;                             // Başarılı
        }
        idx = (idx + 1) % m->cap;                    // Bir sonraki slota geç
    }
    return false;                                    // Anahtar bulunamadı
}

void hashmap_foreach(const HashMap *m, IterFunc fn, void *user_data) {   // Her anahtar-değer çifti için callback çağırır
    if (!m || !fn) return;                                               // NULL pointer koruması
    for (size_t i = 0; i < m->cap; i++) {                                // Tüm slotları gez
        if (m->keys[i] != NULL && m->keys[i] != TOMBSTONE) {             // Dolu ve mezar taşı olmayan slotlar
            fn(m->keys[i], m->values[i], user_data);                     // Callback'i çağır
        }
    }
}