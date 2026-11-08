#ifndef STATS_H                  // Header guard başlangıcı: bu dosyanın birden fazla include edilmesini engeller
#define STATS_H                  // Sembol tanımı: STATS_H tanımlı değilse tanımla

#include <stddef.h>              // size_t tipi için (count, total_queries vb. alanlarda kullanılıyor)
#include <stdint.h>              // uint64_t tipi için (hash alanında kullanılıyor)
#include <stdbool.h>             // bool, true, false tipleri için (stats_init dönüş tipinde kullanılıyor)
#include "core/hashmap.h"        // HashMap yapısı ve hashmap_init/free/get/put fonksiyonları için
#include "fingerprint.h"         // QueryFingerprint yapısı için (stats_record parametresinde kullanılıyor)

/* Tek bir sorgu parmak izinin istatistikleri (README Bölüm 8.2) */
typedef struct {                 // QueryStat adında bir yapı tanımı başlat (tek bir parmak izinin istatistikleri)
    uint64_t hash;          /* Parmak izi hash'i */                    // Bu istatistiğin ait olduğu parmak izi hash'i
    char *query_sample;     /* Örnek sorgu (maskelenmiş, 256 byte sınırlı) */   // Temsili örnek sorgu (dinamik, maks 256 byte)
    size_t count;           /* Kaç kez çalıştı */                      // Bu parmak izinin kaç kez çalıştırıldığı
    double total_duration_ms;/* Toplam süre */                         // Tüm çalışmaların toplam süresi (ms)
    double max_duration_ms; /* En yüksek süre */                       // En uzun tek çalışmanın süresi (ms)
} QueryStat;                     // Yapıya QueryStat adını ver

/* İstatistik toplayıcı (README Bölüm 8.2) */
typedef struct {                 // Stats adında bir yapı tanımı başlat (tüm istatistikleri toplar)
    HashMap map;            /* Hash -> QueryStat eşlemesi */          // Hash değerinden QueryStat'a eşleme yapan hashmap
    size_t total_queries;   /* Toplam sorgu sayısı */                  // Genel toplam sorgu sayısı
    size_t slow_queries;    /* Yavaş sorgu sayısı (eşik üstü) */       // Yavaş eşiği aşan sorgu sayısı
    size_t very_slow_queries; /* Çok yavaş sorgu sayısı */             // Çok yavaş eşiği aşan sorgu sayısı
    size_t truncated_samples; /* 256 byte'ı aşan örnek sayısı */       // 256 byte sınırına takılıp kırpılan örnek sayısı
} Stats;                         // Yapıya Stats adını ver

/* Başlatma ve Temizlik */
bool stats_init(Stats *s);       // Stats yapısını başlatır, hashmap'i kurar, sayaçları sıfırlar
void stats_free(Stats *s);       // Stats içindeki tüm QueryStat'ları ve hashmap'i serbest bırakır

/* Sorgu kaydet */
void stats_record(Stats *s, const QueryFingerprint *fp, double duration_ms, int slow_threshold, int very_slow_threshold);   // Bir sorgu çalışmasını istatistiklere kaydeder

/* Yardımcı: Belirli bir hash'in istatistiklerini al */
const QueryStat *stats_get(const Stats *s, uint64_t hash);   // Verilen hash için istatistiği döndürür (salt okunur)

#endif /* STATS_H */             // Header guard sonu: STATS_H tanımının bittiğini belirtir