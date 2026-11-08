#ifndef CONFIG_H              // Header guard başlangıcı: bu dosyanın birden fazla include edilmesini engeller
#define CONFIG_H              // Sembol tanımı: CONFIG_H tanımlı değilse tanımla

#include <stddef.h>           // size_t tipi için (max_line_bytes, rules_count, rules_cap alanlarında kullanılıyor)
#include <stdbool.h>          // bool, true, false tipleri için (enabled, ai_enabled, kural_* alanlarında kullanılıyor)

/* Tek bir kuralın yapısı (README Bölüm 7.4 ve 12.1) */
typedef struct {              // Rule adında bir yapı tanımı başlat
    char *id;           /* Kural ID (S1, B1, vb.) */          // Kuralın benzersiz kimliği (dinamik string)
    char *pattern;      /* Regex deseni */                    // Kuralın eşleştirme deseni (regex, dinamik string)
    char *severity;     /* high, medium, low, critical */     // Kuralın önem derecesi (dinamik string)
    char *description;  /* Açıklama */                        // Kuralın insan-okunur açıklaması (dinamik string)
    bool enabled;       /* Açık/kapalı */                     // Kuralın aktif olup olmadığı
} Rule;                       // Yapıya Rule adını ver

/* Ana yapılandırma yapısı (README Bölüm 12.1) */
typedef struct {              // Config adında bir yapı tanımı başlat
    /* [baglanti] */
    char *main_host;          // Ana veritabanı sunucu adresi (dinamik string)
    int main_port;            // Ana veritabanı portu
    char *main_dbname;        // Ana veritabanı adı (dinamik string)
    char *main_user;          // Ana veritabanı kullanıcısı (dinamik string)
    char *test_host;          // Test veritabanı sunucu adresi (dinamik string)
    int test_port;            // Test veritabanı portu
    char *test_dbname;        // Test veritabanı adı (dinamik string)
    char *test_user;          // Test veritabanı kullanıcısı (dinamik string)

    /* [genel] */
    int slow_threshold_ms;            // Yavaş sorgu eşiği (ms)
    int very_slow_threshold_ms;       // Çok yavaş sorgu eşiği (ms)
    int top_n;                        // Raporlarda gösterilecek ilk N kayıt
    size_t max_line_bytes;            // Log satırı için maksimum byte sayısı
    int follow_summary_interval_s;    // Follow modunda özet aralığı (saniye)

    /* [ai] */
    bool ai_enabled;          // AI özelliğinin açık/kapalı olması
    char *ai_provider;        // AI sağlayıcı adı (dinamik string)
    char *ai_model;           // Kullanılacak AI modeli (dinamik string)
    char *ai_endpoint;        // AI servis endpoint URL'i (dinamik string)

    /* [kurallar] */
    char *kural_dosyasi;      // Harici kural dosyasının yolu (dinamik string)
    bool kural_s1, kural_s2, kural_s3, kural_s4, kural_s5, kural_s6, kural_s7, kural_s8, kural_s9;   // S serisi kuralların aktiflik durumu
    bool kural_b1, kural_b2, kural_b3;                                                               // B serisi kuralların aktiflik durumu

    /* Dinamik kurallar [kural:<id>] */
    Rule *rules;              // Dinamik kuralların dizisi (Rule pointer'ı)
    size_t rules_count;       // Dizideki mevcut kural sayısı
    size_t rules_cap;         // Dizinin toplam kapasitesi

} Config;                     // Yapıya Config adını ver

/* Başlatma ve Temizlik */
void config_init_defaults(Config *c);      // Config'i varsayılan değerlerle doldurur (string'ler için bellek ayırır)
void config_free(Config *c);               // Config'teki tüm dinamik bellekleri serbest bırakır

/* INI Dosyası Okuma */
bool config_load(Config *c, const char *path);   // Belirtilen INI dosyasını okuyup Config'i doldurur, başarı durumunu döndürür

/* Yardımcı Fonksiyonlar */
const Rule *config_get_rule(const Config *c, const char *id);   // Verilen ID'ye sahip kuralı bulur (salt okunur pointer döndürür)

#endif /* CONFIG_H */          // Header guard sonu: CONFIG_H tanımının bittiğini belirtir