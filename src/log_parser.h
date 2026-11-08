#ifndef LOG_PARSER_H              // Header guard başlangıcı: bu dosyanın birden fazla include edilmesini engeller
#define LOG_PARSER_H              // Sembol tanımı: LOG_PARSER_H tanımlı değilse tanımla

#include <stddef.h>               // size_t tipi için (line_cap, max_line_bytes alanlarında kullanılıyor)
#include <stdbool.h>              // bool, true, false tipleri için (truncated, continued ve fonksiyon dönüş tiplerinde)
#include <stdio.h>                // FILE tipi için (log_parser_next fonksiyon parametresinde kullanılıyor)

/* LogEvent yapısı (README Bölüm 4.0) */
typedef struct {                  // LogEvent adında bir yapı tanımı başlat (tek bir log kaydını temsil eder)
    char *timestamp;              // Zaman damgası string'i (dinamik)
    int pid;                      // Süreç ID'si
    char *user;                   // Veritabanı kullanıcı adı (dinamik)
    char *db;                     // Veritabanı adı (dinamik)
    char *app;                    // Uygulama adı (dinamik)
    char *client_ip;              // İstemci IP adresi (dinamik)
    char *level;                  // Log seviyesi (LOG, ERROR vb., dinamik)
    double duration_ms;           // Sorgu süresi (ms) — varsa
    char *message;                // Log mesajı (dinamik)
    char *raw_record;             // Ham log satırı (dinamik)
    bool truncated;               // Satır max_line_bytes sınırından kesildi mi?
    bool continued;               // Çok satırlı kayıt devamı mı? (birleştirme sonucu)
} LogEvent;                       // Yapıya LogEvent adını ver

/* Parser yapısı (çok satırlı kayıt birleştirme için) */
typedef struct {                  // LogParser adında bir yapı tanımı başlat (parser durumunu tutar)
    char *line;                   // getline tarafından yönetilen geçici satır buffer'ı
    size_t line_cap;              // Satır buffer'ının mevcut kapasitesi (byte)
    size_t max_line_bytes;        // İzin verilen maksimum satır boyutu (byte)
} LogParser;                      // Yapıya LogParser adını ver

/* Başlatma ve Temizlik */
bool log_parser_init(LogParser *p, size_t max_line_bytes);   // Parser'ı başlatır, max_line_bytes ayarlar (0 ise varsayılan)
void log_parser_free(LogParser *p);                          // Parser'ın tuttuğu satır buffer'ını serbest bırakır

/* Kayıt okuma (bir veya daha fazla fiziksel satırdan bir LogEvent üretir) */
bool log_parser_next(LogParser *p, FILE *file, LogEvent *event);   // Dosyadan bir sonraki kaydı okuyup ayrıştırır
void log_event_free(LogEvent *event);                             // LogEvent içindeki tüm dinamik string'leri serbest bırakır

#endif /* LOG_PARSER_H */          // Header guard sonu: LOG_PARSER_H tanımının bittiğini belirtir