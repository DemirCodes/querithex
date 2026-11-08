#define _POSIX_C_SOURCE 200809L

#include "log_parser.h"              // LogParser, LogEvent yapıları ve fonksiyon prototipleri için başlık
#include <stdlib.h>                  // malloc, free, atoi için
#include <string.h>                  // strlen, strncpy, strchr, memcpy, memset için

bool log_parser_init(LogParser *p, size_t max_line_bytes) {   // LogParser'ı başlatır, maksimum satır boyutunu ayarlar
    if (!p) return false;                                     // NULL pointer koruması
    p->line = NULL;                                           // Satır buffer'ı henüz ayrılmadı (ilk getline ayıracak)
    p->line_cap = 0;                                          // Kapasite sıfır (getline otomatik büyütecek)
    p->max_line_bytes = max_line_bytes > 0 ? max_line_bytes : 1048576;   // 0 veya negatifse varsayılan 1MB kullan
    return true;                                              // Başarılı
}

void log_parser_free(LogParser *p) {   // Parser'ın tuttuğu satır buffer'ını serbest bırakır
    if (!p) return;                    // NULL pointer koruması
    free(p->line);                     // getline'ın ayırdığı satır buffer'ını serbest bırak
    p->line = NULL;                    // Dangling pointer'ı önlemek için NULL ata
    p->line_cap = 0;                   // Kapasiteyi sıfırla
}

void log_event_free(LogEvent *event) {   // LogEvent içindeki tüm dinamik string alanlarını serbest bırakır
    if (!event) return;                  // NULL pointer koruması
    free(event->timestamp);              // Zaman damgası string'ini serbest bırak
    free(event->user);                   // Kullanıcı adı string'ini serbest bırak
    free(event->db);                     // Veritabanı adı string'ini serbest bırak
    free(event->app);                    // Uygulama adı string'ini serbest bırak
    free(event->client_ip);              // İstemci IP string'ini serbest bırak
    free(event->level);                  // Log seviyesi string'ini serbest bırak
    free(event->message);                // Mesaj string'ini serbest bırak
    free(event->raw_record);             // Ham kayıt string'ini serbest bırak
    memset(event, 0, sizeof(LogEvent));  // Tüm struct'ı sıfırla (dangling pointer'ları temizle)
}

/* Basit string kopyalayıcı (bellek güvenli) */
static char *my_strdup(const char *s) {   // strdup'ın taşınabilir/elle yazılmış versiyonu
    if (!s) return NULL;                  // NULL girişte NULL döndür
    size_t len = strlen(s) + 1;           // Uzunluk + null terminator için yer hesapla
    char *new_s = (char *)malloc(len);    // Yeni string için bellek ayır
    if (new_s) {                          // Ayırma başarılıysa
        memcpy(new_s, s, len);            // Kaynak string'i null terminator dahil kopyala
    }
    return new_s;                         // Yeni string'i (veya NULL) döndür
}

bool log_parser_next(LogParser *p, FILE *file, LogEvent *event) {   // Dosyadan bir sonraki satırı okuyup LogEvent'e ayrıştırır
    if (!p || !file || !event) return false;                        // NULL pointer koruması

    memset(event, 0, sizeof(LogEvent));   // Event'i sıfırla (önceki veriler kalmasın)

    // Satırı oku
    ssize_t read = getline(&p->line, &p->line_cap, file);   // Bir satır oku (buffer gerekirse getline büyütür)
    if (read == -1) {                                       // EOF veya hata
        return false; // EOF
    }

    // Satır sonundaki \n ve \r'yi temizle
    while (read > 0 && (p->line[read - 1] == '\n' || p->line[read - 1] == '\r')) {   // Satır sonundaki CR/LF karakterlerini
        p->line[--read] = '\0';                                                      // null terminator ile değiştir
    }

    event->raw_record = my_strdup(p->line);   // Ham satırı ayrı bir kopya olarak sakla
    
    // Prefix Ayrıştırma (Basit örnek: 2026-10-30 14:30:15.123 UTC [12345] postgres@testdb/psql 10.0.0.5 LOG: ...)
    // NOT: Şimdilik sadece bu spesifik formata odaklanıyoruz.
    char *p_line = p->line;   // Ayrıştırma için satır üzerinde ilerleyen geçici pointer
    
    // 1. Timestamp (Dinamik ayrıştırma: '[' karakterine kadar oku)
    char *bracket_start = strchr(p_line, '[');              // İlk '[' karakterini bul (PID başlangıcı)
    if (bracket_start) {                                    // '[' bulunduysa
        size_t ts_len = bracket_start - p_line;             // Timestamp uzunluğunu hesapla
        while (ts_len > 0 && p_line[ts_len - 1] == ' ') {   // Sondaki boşlukları temizle
            ts_len--;
        }
        event->timestamp = (char *)malloc(ts_len + 1);      // Uzunluk + null için yer ayır
        if (event->timestamp) {                             // Ayırma başarılıysa
            strncpy(event->timestamp, p_line, ts_len);      // Timestamp'i kopyala
            event->timestamp[ts_len] = '\0';                // Null terminator ekle
        }
        p_line = bracket_start;                             // Pointer'ı '[' karakterine getir
    }
    
    // 2. PID ([12345])
    if (*p_line == '[') {                                   // PID parantez içinde başlıyorsa
        char *end_bracket = strchr(p_line, ']');            // Kapanış parantezini bul
        if (end_bracket) {                                  // Bulunduysa
            event->pid = atoi(p_line + 1);                  // Parantez içindeki sayıyı PID olarak ayrıştır
            p_line = end_bracket + 1;                       // Pointer'ı parantezden sonrasına taşı
            while (*p_line == ' ') p_line++;                // Boşlukları atla
        }
    }
    
    // 3. User@DB/App (postgres@testdb/psql)
    char *user_end = strchr(p_line, '@');                   // '@' karakterini bul (user/db ayırıcı)
    if (user_end) {                                         // Bulunduysa
        *user_end = '\0';                                   // '@' yerine null yaz (string'i böl)
        event->user = my_strdup(p_line);                    // '@' öncesini user olarak kopyala
        p_line = user_end + 1;                              // Pointer'ı '@' sonrasına taşı
        
        char *db_end = strchr(p_line, '/');                 // '/' karakterini bul (db/app ayırıcı)
        if (db_end) {                                       // Bulunduysa
            *db_end = '\0';                                 // '/' yerine null yaz (string'i böl)
            event->db = my_strdup(p_line);                  // '/' öncesini db olarak kopyala
            p_line = db_end + 1;                            // Pointer'ı '/' sonrasına taşı
            
            char *app_end = strchr(p_line, ' ');            // Bir sonraki boşluğu bul (app sonu)
            if (app_end) {                                  // Bulunduysa
                *app_end = '\0';                            // Boşluk yerine null yaz (string'i böl)
                event->app = my_strdup(p_line);             // Boşluk öncesini app olarak kopyala
                p_line = app_end + 1;                       // Pointer'ı boşluk sonrasına taşı
                while (*p_line == ' ') p_line++;            // Boşlukları atla
            }
        }
    }
    
    // 4. Client IP (10.0.0.5)
    char *ip_end = strchr(p_line, ' ');                     // Bir sonraki boşluğu bul (IP sonu)
    if (ip_end) {                                           // Bulunduysa
        *ip_end = '\0';                                     // Boşluk yerine null yaz (string'i böl)
        event->client_ip = my_strdup(p_line);               // IP'yi kopyala
        p_line = ip_end + 1;                                // Pointer'ı boşluk sonrasına taşı
        while (*p_line == ' ') p_line++;                    // Boşlukları atla
    }
    
    // 5. Level (LOG, ERROR vb.)
    char *level_end = strchr(p_line, ':');                  // ':' karakterini bul (level sonu)
    if (level_end) {                                        // Bulunduysa
        *level_end = '\0';                                  // ':' yerine null yaz (string'i böl)
        event->level = my_strdup(p_line);                   // Level'ı kopyala
        p_line = level_end + 1;                             // Pointer'ı ':' sonrasına taşı
        while (*p_line == ' ') p_line++;                    // Boşlukları atla
    }
    
    // 6. Message (Geri kalan her şey)
    event->message = my_strdup(p_line);                     // Satırın geri kalanını message olarak kopyala

    return true;                                            // Ayrıştırma başarılı
}