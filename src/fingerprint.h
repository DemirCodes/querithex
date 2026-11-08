#ifndef FINGERPRINT_H            // Header guard başlangıcı: bu dosyanın birden fazla include edilmesini engeller
#define FINGERPRINT_H            // Sembol tanımı: FINGERPRINT_H tanımlı değilse tanımla

#include <stddef.h>              // size_t tipi için (bu header'da doğrudan kullanılmıyor ama yaygın bağımlılık)
#include <stdint.h>              // uint64_t tipi için (hash alanında kullanılıyor)
#include <stdbool.h>             // bool, true, false tipleri için (fonksiyon dönüş tiplerinde kullanılıyor)

/* Sorgu parmak izi yapısı (README Bölüm 6) */
typedef struct {                 // QueryFingerprint adında bir yapı tanımı başlat
    uint64_t hash;      /* 64-bit FNV-1a hash */                  // Normalize edilmiş sorgunun 64-bit hash değeri
    char *normalized;   /* Normalize edilmiş sorgu (literaller '?' olmuş) */   // Literallerin '?' ile maskelendiği normalize string (dinamik)
} QueryFingerprint;              // Yapıya QueryFingerprint adını ver

/* Başlatma ve Temizlik */
void fingerprint_init(QueryFingerprint *fp);      // Yapıyı başlangıç durumuna getirir (hash=0, normalized=NULL)
void fingerprint_free(QueryFingerprint *fp);      // Yapının tuttuğu dinamik belleği serbest bırakır

/* Sorguyu normalize et ve hash'le */
bool fingerprint_generate(QueryFingerprint *fp, const char *query);   // SQL sorgusunu normalize eder, hash üretir, başarı durumunu döndürür

/* Yardımcı: Normalize edilmiş sorguyu döndür (maskeleme için) */
const char *fingerprint_get_normalized(const QueryFingerprint *fp);   // Normalize edilmiş string'e salt okunur erişim sağlar

#endif /* FINGERPRINT_H */       // Header guard sonu: FINGERPRINT_H tanımının bittiğini belirtir