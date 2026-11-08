#define _POSIX_C_SOURCE 200809L   // POSIX 2008 API'lerini (ör. getline vb.) etkinleştirmek için (include'lardan önce tanımlanmalı)

#include "config.h"               // Config, Rule yapıları ve fonksiyon prototipleri için başlık
#include <stdio.h>                // FILE, fopen, fgets, fclose için
#include <stdlib.h>               // malloc, realloc, free, atoi, atol için
#include <string.h>               // strlen, strcmp, strncmp, strchr, strcpy, strncpy, memcpy, memmove, memset için
#include <ctype.h>                // tolower, isspace için


/* Basit string karşılaştırma (büyük/küçük harf duyarsız) */
static int my_strcasecmp(const char *s1, const char *s2) {   // İki stringi büyük/küçük harf duyarsız karşılaştırır
    while (*s1 && *s2) {                                     // Her iki stringde de karakter varken
        int c1 = tolower((unsigned char)*s1);                // s1 karakterini küçük harfe çevir (unsigned cast ile UB önlenir)
        int c2 = tolower((unsigned char)*s2);                // s2 karakterini küçük harfe çevir
        if (c1 != c2) return c1 - c2;                        // Farklıysa farkı döndür (strcmp semantiği)
        s1++; s2++;                                          // Sonraki karakterlere geç
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);   // Biri bittiyse kalan karakterlerin farkını döndür
}

/* Basit string kopyalayıcı */
static char *my_strdup(const char *s) {   // strdup'ın elle yazılmış, taşınabilir versiyonu
    if (!s) return NULL;                  // NULL girişte NULL döndür
    size_t len = strlen(s) + 1;           // Uzunluk + null terminator için yer hesapla
    char *new_s = (char *)malloc(len);    // Yeni string için bellek ayır
    if (new_s) memcpy(new_s, s, len);     // Kaynak string'i null terminator dahil kopyala
    return new_s;                         // Yeni string'i (veya NULL) döndür
}

/* Satır sonundaki boşlukları temizle */
static void trim(char *str) {             // Baştaki ve sondaki boşlukları temizler (in-place)
    if (!str) return;                     // NULL kontrolü
    size_t len = strlen(str);             // String uzunluğunu al
    while (len > 0 && isspace((unsigned char)str[len - 1])) {   // Sondan başlayarak boşluk karakterlerini
        str[--len] = '\0';                                      // null terminator ile değiştir
    }
    char *start = str;                    // Başlangıç işaretçisi
    while (*start && isspace((unsigned char)*start)) start++;   // Baştaki boşlukları atla
    if (start != str) memmove(str, start, strlen(start) + 1);   // Başta boşluk varsa stringi sola kaydır (üst üste binme güvenli)
}

/* String'i küçük harfe çevir */
static void to_lower(char *str) {         // String'i yerinde küçük harfe çevirir
    if (!str) return;                     // NULL kontrolü
    for (; *str; str++) *str = (char)tolower((unsigned char)*str);   // Her karakteri küçük harfe çevir
}

void config_init_defaults(Config *c) {    // Config yapısını varsayılan değerlerle doldurur
    if (!c) return;                       // NULL pointer koruması
    memset(c, 0, sizeof(Config));         // Tüm alanları sıfırla (string'ler NULL, sayılar 0, bool'lar false)

    // [genel] varsayılanları
    c->slow_threshold_ms = 100;                        // Yavaş sorgu eşiği: 100 ms
    c->very_slow_threshold_ms = 1000;                  // Çok yavaş sorgu eşiği: 1000 ms
    c->top_n = 20;                                     // Raporlarda gösterilecek ilk N kayıt: 20
    c->max_line_bytes = 1048576;                       // Maksimum satır boyutu: 1 MB
    c->follow_summary_interval_s = 60;                 // Follow modunda özet aralığı: 60 saniye

    // [ai] varsayılanları
    c->ai_enabled = false;                             // AI varsayılan olarak kapalı
    c->ai_provider = my_strdup("none");                // Sağlayıcı: "none" (kullanılmıyor)

    // [kurallar] varsayılanları (hepsi açık)
    c->kural_s1 = c->kural_s2 = c->kural_s3 = c->kural_s4 = c->kural_s5 = true;   // S serisi kuralları açık
    c->kural_s6 = c->kural_s7 = c->kural_s8 = c->kural_s9 = true;                 // S6-S9 da açık
    c->kural_b1 = c->kural_b2 = c->kural_b3 = true;                                // B serisi kuralları açık

    c->rules = NULL;                      // Dinamik kurallar listesi henüz yok
    c->rules_count = 0;                   // Kural sayısı sıfır
    c->rules_cap = 0;                     // Kural kapasitesi sıfır (ilk eklemede ayrılacak)
}

void config_free(Config *c) {             // Config yapısındaki tüm dinamik bellekleri serbest bırakır
    if (!c) return;                       // NULL pointer koruması
    free(c->main_host);                   // Ana bağlantı host string'ini serbest bırak
    free(c->main_dbname);                 // Ana veritabanı adı string'ini serbest bırak
    free(c->main_user);                   // Ana kullanıcı adı string'ini serbest bırak
    free(c->test_host);                   // Test host string'ini serbest bırak
    free(c->test_dbname);                 // Test veritabanı adı string'ini serbest bırak
    free(c->test_user);                   // Test kullanıcı adı string'ini serbest bırak
    free(c->ai_provider);                 // AI sağlayıcı string'ini serbest bırak
    free(c->ai_model);                    // AI model string'ini serbest bırak
    free(c->ai_endpoint);                 // AI endpoint string'ini serbest bırak
    free(c->kural_dosyasi);               // Kural dosyası yolu string'ini serbest bırak

    for (size_t i = 0; i < c->rules_count; i++) {   // Her dinamik kural için
        free(c->rules[i].id);                       // Kural ID string'ini serbest bırak
        free(c->rules[i].pattern);                  // Kural pattern string'ini serbest bırak
        free(c->rules[i].severity);                 // Kural severity string'ini serbest bırak
        free(c->rules[i].description);              // Kural açıklama string'ini serbest bırak
    }
    free(c->rules);                       // Kural dizisinin kendisini serbest bırak
    memset(c, 0, sizeof(Config));         // Tüm struct'ı sıfırla (dangling pointer'ları temizle)
}

const Rule *config_get_rule(const Config *c, const char *id) {   // Verilen ID'ye sahip kuralı bulur (salt okunur)
    if (!c || !id) return NULL;                                   // NULL pointer koruması
    for (size_t i = 0; i < c->rules_count; i++) {                 // Tüm dinamik kuralları gez
        if (strcmp(c->rules[i].id, id) == 0) return &c->rules[i]; // ID eşleşirse kuralın adresini döndür
    }
    return NULL;                                                  // Bulunamadı
}

/* Dinamik kural ekle */
static Rule *add_rule(Config *c, const char *id) {   // Config'e yeni bir kural ekler (gerekirse kapasiteyi büyütür)
    if (!c || !id) return NULL;                      // NULL pointer koruması
    
    if (c->rules_count >= c->rules_cap) {            // Kapasite doluysa
        size_t new_cap = c->rules_cap == 0 ? 4 : c->rules_cap * 2;   // İlk seferde 4, sonra iki katına çıkar
        Rule *new_rules = (Rule *)realloc(c->rules, new_cap * sizeof(Rule));   // Kural dizisini yeni boyuta göre yeniden ayır
        if (!new_rules) return NULL;                 // realloc başarısızsa çık
        c->rules = new_rules;                        // Yeni diziyi kaydet
        c->rules_cap = new_cap;                      // Yeni kapasiteyi kaydet
    }

    Rule *r = &c->rules[c->rules_count];             // Yeni kural için dizinin sonundaki boş slot
    memset(r, 0, sizeof(Rule));                      // Slotu sıfırla
    r->id = my_strdup(id);                           // ID'yi kopyala
    r->enabled = true;                               // Yeni kurallar varsayılan olarak aktif
    c->rules_count++;                                // Kural sayısını artır
    return r;                                        // Yeni kuralın adresini döndür
}

/* Boolean değer parse et */
static bool parse_bool(const char *val) {   // "true/on/yes/1" gibi değerleri true olarak yorumlar
    if (!val) return false;                 // NULL girişte false
    return (my_strcasecmp(val, "true") == 0 || my_strcasecmp(val, "on") == 0 ||   // true/on/yes/1 ise
            my_strcasecmp(val, "yes") == 0 || strcmp(val, "1") == 0);             // true döndür
}

bool config_load(Config *c, const char *path) {   // INI formatındaki config dosyasını okur ve Config yapısını doldurur
    if (!c || !path) return false;                // NULL pointer koruması

    FILE *f = fopen(path, "r");                   // Config dosyasını okuma modunda aç
    if (!f) return false;                         // Açılamazsa başarısız

    char line[2048];                              // Her satırı tutacak geçici buffer (maks 2048 byte)
    char current_section[64] = "";                // Şu anki bölüm adı (örn. "baglanti", "genel", "kural")
    char current_dynamic_rule[64] = "";           // Şu anki dinamik kural ID'si (örn. "kural:s1")
    Rule *dynamic_rule = NULL;                    // Şu anki dinamik kuralın pointer'ı (bölüm kural: ise dolu)

    while (fgets(line, sizeof(line), f)) {        // Dosyadan satır satır oku
        trim(line);                               // Baştaki/sondaki boşlukları temizle
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue; // Boş satır ve yorum

        if (line[0] == '[' && line[strlen(line) - 1] == ']') {   // Bölüm başlangıcı: [section] formatı
            // Bölüm başlangıcı
            line[strlen(line) - 1] = '\0';                       // Kapanış ']' karakterini null ile değiştir
            char *section = line + 1;                            // '[' sonrası kısmı section olarak al
            
            if (strncmp(section, "kural:", 6) == 0) {            // "kural:xxx" formatında dinamik kural bölümü mü?
                // Dinamik kural
                strncpy(current_dynamic_rule, section + 6, sizeof(current_dynamic_rule) - 1);   // Kural ID'sini kopyala
                current_dynamic_rule[sizeof(current_dynamic_rule) - 1] = '\0';                  // Null terminator garantile
                strcpy(current_section, "kural");                // Bölüm adını "kural" yap
                dynamic_rule = add_rule(c, current_dynamic_rule);   // Yeni kural ekle, pointer'ını sakla
            } else {
                strncpy(current_section, section, sizeof(current_section) - 1);   // Normal bölüm adını kopyala
                current_section[sizeof(current_section) - 1] = '\0';              // Null terminator garantile
                to_lower(current_section);                       // Bölüm adını küçük harfe çevir
                dynamic_rule = NULL;                             // Dinamik kural pointer'ını sıfırla
            }
            continue;                                            // Bölüm satırından sonra key=value bekle
        }

        // Key = Value
        char *eq = strchr(line, '=');            // '=' karakterini bul
        if (!eq) continue;                       // '=' yoksa satırı atla

        *eq = '\0';                              // '=' yerine null yaz (key/value ayır)
        char *key = line;                        // '=' öncesi key
        char *value = eq + 1;                    // '=' sonrası value
        trim(key);                               // Key'in başındaki/sonundaki boşlukları temizle
        trim(value);                             // Value'nun başındaki/sonundaki boşlukları temizle
        to_lower(key);                           // Key'i küçük harfe çevir (case-insensitive)

        // Değer tırnak içindeyse tırnakları temizle
        if (value[0] == '"' && value[strlen(value) - 1] == '"') {   // Value çift tırnak içindeyse
            value[strlen(value) - 1] = '\0';                        // Kapanış tırnağını sil
            value++;                                                // Açılış tırnağını atla (pointer'ı kaydır)
        }

        // Bölüme göre atama yap
        if (strcmp(current_section, "baglanti") == 0) {          // [baglanti] bölümündeysek
            if (strcmp(key, "main_host") == 0) { free(c->main_host); c->main_host = my_strdup(value); }   // main_host ata
            else if (strcmp(key, "main_port") == 0) c->main_port = atoi(value);                          // main_port ata
            else if (strcmp(key, "main_dbname") == 0) { free(c->main_dbname); c->main_dbname = my_strdup(value); }   // main_dbname ata
            else if (strcmp(key, "main_user") == 0) { free(c->main_user); c->main_user = my_strdup(value); }         // main_user ata
            else if (strcmp(key, "test_host") == 0) { free(c->test_host); c->test_host = my_strdup(value); }         // test_host ata
            else if (strcmp(key, "test_port") == 0) c->test_port = atoi(value);                                      // test_port ata
            else if (strcmp(key, "test_dbname") == 0) { free(c->test_dbname); c->test_dbname = my_strdup(value); }   // test_dbname ata
            else if (strcmp(key, "test_user") == 0) { free(c->test_user); c->test_user = my_strdup(value); }         // test_user ata
        } 
        else if (strcmp(current_section, "genel") == 0) {        // [genel] bölümündeysek
            if (strcmp(key, "slow_threshold_ms") == 0) c->slow_threshold_ms = atoi(value);              // Yavaş eşiği ata
            else if (strcmp(key, "very_slow_threshold_ms") == 0) c->very_slow_threshold_ms = atoi(value);   // Çok yavaş eşiği ata
            else if (strcmp(key, "top_n") == 0) c->top_n = atoi(value);                                 // top_n ata
            else if (strcmp(key, "max_line_bytes") == 0) c->max_line_bytes = (size_t)atol(value);       // max_line_bytes ata
            else if (strcmp(key, "follow_summary_interval_s") == 0) c->follow_summary_interval_s = atoi(value);   // Follow özet aralığı ata
        }
        else if (strcmp(current_section, "ai") == 0) {           // [ai] bölümündeysek
            if (strcmp(key, "enabled") == 0) c->ai_enabled = parse_bool(value);                         // AI aktif/pasif ata
            else if (strcmp(key, "provider") == 0) { free(c->ai_provider); c->ai_provider = my_strdup(value); }   // Provider ata
            else if (strcmp(key, "model") == 0) { free(c->ai_model); c->ai_model = my_strdup(value); }            // Model ata
            else if (strcmp(key, "endpoint") == 0) { free(c->ai_endpoint); c->ai_endpoint = my_strdup(value); }   // Endpoint ata
        }
        else if (strcmp(current_section, "kurallar") == 0) {     // [kurallar] bölümündeysek
            if (strcmp(key, "kural_dosyasi") == 0) { free(c->kural_dosyasi); c->kural_dosyasi = my_strdup(value); }   // Kural dosyası yolu ata
            else if (strcmp(key, "s1") == 0) c->kural_s1 = parse_bool(value);   // S1 kuralı aktif/pasif
            else if (strcmp(key, "s2") == 0) c->kural_s2 = parse_bool(value);   // S2 kuralı aktif/pasif
            else if (strcmp(key, "s3") == 0) c->kural_s3 = parse_bool(value);   // S3 kuralı aktif/pasif
            else if (strcmp(key, "s4") == 0) c->kural_s4 = parse_bool(value);   // S4 kuralı aktif/pasif
            else if (strcmp(key, "s5") == 0) c->kural_s5 = parse_bool(value);   // S5 kuralı aktif/pasif
            else if (strcmp(key, "s6") == 0) c->kural_s6 = parse_bool(value);   // S6 kuralı aktif/pasif
            else if (strcmp(key, "s7") == 0) c->kural_s7 = parse_bool(value);   // S7 kuralı aktif/pasif
            else if (strcmp(key, "s8") == 0) c->kural_s8 = parse_bool(value);   // S8 kuralı aktif/pasif
            else if (strcmp(key, "s9") == 0) c->kural_s9 = parse_bool(value);   // S9 kuralı aktif/pasif
            else if (strcmp(key, "b1") == 0) c->kural_b1 = parse_bool(value);   // B1 kuralı aktif/pasif
            else if (strcmp(key, "b2") == 0) c->kural_b2 = parse_bool(value);   // B2 kuralı aktif/pasif
            else if (strcmp(key, "b3") == 0) c->kural_b3 = parse_bool(value);   // B3 kuralı aktif/pasif
        }
        else if (strcmp(current_section, "kural") == 0 && dynamic_rule) {   // Dinamik kural bölümündeysek
            if (strcmp(key, "pattern") == 0) { free(dynamic_rule->pattern); dynamic_rule->pattern = my_strdup(value); }           // Pattern ata
            else if (strcmp(key, "severity") == 0) { free(dynamic_rule->severity); dynamic_rule->severity = my_strdup(value); }   // Severity ata
            else if (strcmp(key, "description") == 0) { free(dynamic_rule->description); dynamic_rule->description = my_strdup(value); }   // Açıklama ata
            else if (strcmp(key, "enabled") == 0) dynamic_rule->enabled = parse_bool(value);                                      // Aktif/pasif ata
        }
    }

    fclose(f);         // Dosyayı kapat
    return true;       // Ayrıştırma tamamlandı
}