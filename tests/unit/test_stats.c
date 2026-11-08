#include <stdio.h>                  // printf için
#include <string.h>                 // (Bu dosyada doğrudan kullanılmıyor, header bağımlılığı olabilir)
#include <assert.h>                 // assert makrosu için (test doğrulamaları)
#include <stdbool.h>                // bool, true, false tipleri için
#include "stats.h"                  // Stats, QueryStat yapıları ve stats_init/free/record/get fonksiyonları için
#include "fingerprint.h"            // QueryFingerprint yapısı ve fingerprint_init/free/generate fonksiyonları için

void test_stats_basic(void) {                                    // Stats modülünün temel işlevlerini test eden fonksiyon
    Stats s;                                                     // Test edilecek Stats yapısı (stack'te)
    assert(stats_init(&s) == true);                              // Stats başlatma başarılı olmalı

    QueryFingerprint fp1;                                        // Birinci sorgu parmak izi
    fingerprint_init(&fp1);                                      // Parmak izini başlat
    fingerprint_generate(&fp1, "SELECT * FROM users WHERE id = 1");   // Sorguyu normalize et ve hash'le

    QueryFingerprint fp2;                                        // İkinci sorgu parmak izi
    fingerprint_init(&fp2);                                      // Parmak izini başlat
    fingerprint_generate(&fp2, "SELECT * FROM products WHERE id = 2");   // Sorguyu normalize et ve hash'le

    // İki farklı sorgu kaydet
    stats_record(&s, &fp1, 50.0, 100, 1000);                     // fp1 için 50ms süre kaydet (yavaş değil)
    stats_record(&s, &fp2, 150.0, 100, 1000); // Yavaş           // fp2 için 150ms süre kaydet (yavaş: 100ms üstü)
    stats_record(&s, &fp1, 60.0, 100, 1000);  // Aynı sorgu (fp1) // fp1 için 60ms daha kaydet (aynı parmak izi)

    assert(s.total_queries == 3);                                // Toplam 3 sorgu kaydedilmiş olmalı
    assert(s.slow_queries == 1); // Sadece fp2 yavaş           // Sadece fp2 yavaş eşiği geçmiş olmalı

    // fp1 istatistikleri
    const QueryStat *stat1 = stats_get(&s, fp1.hash);            // fp1'in hash'ine karşılık gelen istatistiği al
    assert(stat1 != NULL);                                       // İstatistik bulunmuş olmalı
    // Debug çıktısı
    printf("DEBUG: stat1->hash = %lu\n", stat1->hash);           // Debug: fp1 hash değerini yazdır
    printf("DEBUG: stat1->count = %zu\n", stat1->count);         // Debug: fp1 çalışma sayısını yazdır
    printf("DEBUG: stat1->total_duration_ms = %.2f\n", stat1->total_duration_ms);   // Debug: fp1 toplam süresini yazdır
    printf("DEBUG: stat1->max_duration_ms = %.2f\n", stat1->max_duration_ms);       // Debug: fp1 maks süresini yazdır
    assert(stat1->count == 2);                                   // fp1 iki kez çalıştırılmış olmalı
    assert(stat1->total_duration_ms == 110.0);                   // fp1 toplam süresi 50+60 = 110 ms olmalı
    assert(stat1->max_duration_ms == 60.0);                      // fp1 maks süresi 60 ms olmalı

    // fp2 istatistikleri
    const QueryStat *stat2 = stats_get(&s, fp2.hash);            // fp2'nin hash'ine karşılık gelen istatistiği al
    assert(stat2 != NULL);                                       // İstatistik bulunmuş olmalı
    assert(stat2->count == 1);                                   // fp2 bir kez çalıştırılmış olmalı

    fingerprint_free(&fp1);                                      // fp1'in tuttuğu belleği serbest bırak
    fingerprint_free(&fp2);                                      // fp2'nin tuttuğu belleği serbest bırak
    stats_free(&s);                                              // Stats'in tuttuğu tüm belleği serbest bırak
    printf("test_stats_basic passed\n");                         // Testin geçtiğini bildir
}

int main(void) {                                                 // Test programının giriş noktası
    test_stats_basic();                                          // Temel stats testini çalıştır
    printf("All stats tests passed successfully!\n");            // Tüm testlerin geçtiğini bildir
    return 0;                                                    // Başarılı çıkış
}