#include "stats.h"                    // Stats, QueryStat yapıları ve fonksiyon prototipleri için başlık
#include <stdlib.h>                   // malloc, free için
#include <string.h>                   // strlen, strncpy için

/* Hashmap için uint64_t hash fonksiyonu (anahtar zaten hash) */
static uint64_t hash_uint64(const void *key) {   // HashMap'in anahtar olarak uint64_t kullanmasını sağlayan hash fonksiyonu
    return *(const uint64_t *)key;               // Anahtar zaten hash değeri olduğu için doğrudan döndür (identity hash)
}

/* Hashmap için uint64_t eşitlik fonksiyonu */
static bool eq_uint64(const void *a, const void *b) {   // İki uint64_t anahtarın eşit olup olmadığını kontrol eder
    return *(const uint64_t *)a == *(const uint64_t *)b;   // Değerleri karşılaştır (pointer değil, işaret ettiği değer)
}

bool stats_init(Stats *s) {   // Stats yapısını başlatır, hashmap'i kurar, sayaçları sıfırlar
    if (!s) return false;                                    // NULL pointer koruması
    if (!hashmap_init(&s->map, hash_uint64, eq_uint64)) return false;   // Hashmap'i uint64_t anahtarlarla başlat
    s->total_queries = 0;                                    // Toplam sorgu sayacını sıfırla
    s->slow_queries = 0;                                     // Yavaş sorgu sayacını sıfırla
    s->very_slow_queries = 0;                                // Çok yavaş sorgu sayacını sıfırla
    s->truncated_samples = 0;                                // Kesilmiş örnek sayacını sıfırla
    return true;                                             // Başarılı
}

void stats_free(Stats *s) {   // Stats içindeki tüm QueryStat'ları ve hashmap'i serbest bırakır
    if (!s) return;           // NULL pointer koruması
    // Hashmap içindeki QueryStat'ları serbest bırak
    for (size_t i = 0; i < s->map.cap; i++) {   // Hashmap'in tüm slotlarını gez
        if (s->map.keys[i] != NULL && s->map.keys[i] != (void *)-1) {   // Dolu ve mezar taşı olmayan slotlar
            QueryStat *stat = (QueryStat *)s->map.values[i];            // Slottaki QueryStat'ı al
            if (stat) {                                                 // Stat geçerliyse
                free(stat->query_sample);                               // Örnek sorgu string'ini serbest bırak
                free(stat);                                             // QueryStat struct'ını serbest bırak
            }
        }
    }
    hashmap_free(&s->map);   // Hashmap'in kendi iç dizilerini serbest bırak
}

void stats_record(Stats *s, const QueryFingerprint *fp, double duration_ms, int slow_threshold, int very_slow_threshold) {   // Bir sorgu çalışmasını istatistiklere kaydeder
    if (!s || !fp) return;   // NULL pointer koruması

    s->total_queries++;                                          // Toplam sorgu sayacını artır
    if (duration_ms >= slow_threshold) s->slow_queries++;        // Süre yavaş eşiğini geçtiyse yavaş sayacını artır
    if (duration_ms >= very_slow_threshold) s->very_slow_queries++;   // Süre çok yavaş eşiğini geçtiyse çok yavaş sayacını artır

    // Hash değerini anahtar olarak kullan
    uint64_t hash = fp->hash;                                    // Fingerprint'in hash'ini anahtar olarak al
    QueryStat *stat = (QueryStat *)hashmap_get(&s->map, &hash);  // Bu hash için mevcut istatistiği ara

    if (!stat) {                                                 // Bu hash için henüz kayıt yoksa
        // Yeni kayıt
        stat = (QueryStat *)malloc(sizeof(QueryStat));           // Yeni QueryStat için bellek ayır
        if (!stat) return;                                       // Ayırma başarısızsa çık
        
        stat->hash = hash;                                       // Hash değerini kaydet
        stat->count = 0;                                         // Sayaç sıfırdan başlasın
        stat->total_duration_ms = 0;                             // Toplam süre sıfırdan başlasın
        stat->max_duration_ms = 0;                               // Maksimum süre sıfırdan başlasın
        
        // Örnek sorguyu kopyala (maksimum 256 byte - README Bölüm 8.2)
        if (fp->normalized) {                                    // Normalize edilmiş sorgu varsa
            size_t len = strlen(fp->normalized);                 // Uzunluğunu al
            if (len > 256) {                                     // 256 byte'tan uzunsa
                len = 256;                                       // 256'ya kırp
                s->truncated_samples++;                          // Kesilmiş örnek sayacını artır
            }
            stat->query_sample = (char *)malloc(len + 1);        // Kırpılmış uzunluk + null için bellek ayır
            if (stat->query_sample) {                            // Ayırma başarılıysa
                strncpy(stat->query_sample, fp->normalized, len);   // Sorguyu kopyala
                stat->query_sample[len] = '\0';                  // Null terminator ekle
            }
        } else {
            stat->query_sample = NULL;                           // Normalize sorgu yoksa NULL ata
        }
        
        // Hashmap'e ekle (anahtar olarak hash'in adresini ver)
        uint64_t *key_copy = (uint64_t *)malloc(sizeof(uint64_t));   // Hashmap'in anahtarı tutması için ayrı bellek ayır
        if (key_copy) {                                          // Ayırma başarılıysa
            *key_copy = hash;                                    // Hash değerini kopyala
            hashmap_put(&s->map, key_copy, stat);                // Hashmap'e ekle (anahtar: hash kopyası, değer: stat)
        }
    }

    // İstatistikleri güncelle
    stat->count++;                                               // Bu hash için çalışma sayısını artır
    stat->total_duration_ms += duration_ms;                      // Toplam süreye bu çalışmanın süresini ekle
    if (duration_ms > stat->max_duration_ms) {                   // Yeni süre mevcut maksimumdan büyükse
        stat->max_duration_ms = duration_ms;                     // Maksimum süreyi güncelle
    }
}

const QueryStat *stats_get(const Stats *s, uint64_t hash) {   // Verilen hash için istatistiği döndürür (salt okunur)
    if (!s) return NULL;                                      // NULL pointer koruması
    uint64_t h = hash;                                        // Hash'i yerel değişkene al (adres almak için)
    return (const QueryStat *)hashmap_get(&s->map, &h);       // Hashmap'ten istatistiği al ve döndür
}