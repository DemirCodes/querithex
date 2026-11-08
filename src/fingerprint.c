#define _POSIX_C_SOURCE 200809L          // POSIX 2008 API'lerini (ör. strdup) etkinleştirmek için (include'lardan önce tanımlanmalı)
#include "fingerprint.h"                 // QueryFingerprint yapısı ve fonksiyon prototipleri için başlık
#include "core/buffer.h"                 // Buffer yapısı ve buffer_init/append/free/cstr fonksiyonları için
#include <stdlib.h>                      // free, strdup için
#include <string.h>                      // (Bu dosyada doğrudan kullanılmıyor, header bağımlılığı olabilir)
#include <ctype.h>                       // isdigit, tolower için

void fingerprint_init(QueryFingerprint *fp) {   // QueryFingerprint yapısını başlangıç durumuna getirir
    if (!fp) return;                            // NULL pointer koruması
    fp->hash = 0;                               // Hash değerini sıfırla
    fp->normalized = NULL;                      // Normalize edilmiş string henüz yok
}

void fingerprint_free(QueryFingerprint *fp) {   // QueryFingerprint'in tuttuğu belleği serbest bırakır
    if (!fp) return;                            // NULL pointer koruması
    free(fp->normalized);                       // Normalize edilmiş string'i serbest bırak
    fp->normalized = NULL;                      // Dangling pointer'ı önlemek için NULL ata
    fp->hash = 0;                               // Hash değerini sıfırla
}

/* FNV-1a 64-bit hash fonksiyonu */
static uint64_t fnv1a_hash(const char *str) {   // FNV-1a algoritmasıyla 64-bit hash hesaplar
    uint64_t hash = 14695981039346656037ULL;    // FNV offset basis sabiti (64-bit)
    while (*str) {                              // String sonuna kadar
        hash ^= (uint64_t)(unsigned char)*str++;   // Karakteri hash'e XOR'la (unsigned cast ile işaret sorunu önlenir)
        hash *= 1099511628211ULL;               // FNV prime sabitiyle çarp
    }
    return hash;                                // Hesaplanan hash'i döndür
}

bool fingerprint_generate(QueryFingerprint *fp, const char *query) {   // SQL sorgusunu normalize edip hash'ini üretir
    if (!fp || !query) return false;                                   // NULL pointer koruması

    Buffer buf;                              // İlk geçiş: normalize edilmiş ham buffer
    buffer_init(&buf);                       // Buffer'ı başlat
    if (!buf.data) return false;             // Bellek ayırma başarısızsa çık

    const char *p = query;                   // Sorgu üzerinde ilerleyen pointer
    bool in_single_quote = false;            // Tek tırnak içinde miyiz (string literal)?
    bool in_comment = false;                 // Yorum içinde miyiz?

    while (*p) {                             // Sorgu sonuna kadar
        // Yorumları sil (-- ile başlayan)
        if (!in_single_quote && *p == '-' && *(p + 1) == '-') {   // String içinde değilken "--" gördüysek
            in_comment = true;               // Yorum moduna gir
            p += 2;                          // "--" karakterlerini atla
            continue;                        // Döngünün başına dön
        }

        if (in_comment) {                    // Yorum içindeysek
            if (*p == '\n') {                // Satır sonuna geldiysek
                in_comment = false;          // Yorum modundan çık
                buffer_append_char(&buf, ' '); // Yorumu boşlukla değiştir (token ayrımı için)
            }
            p++;                             // Yorum karakterlerini atla
            continue;                        // Döngünün başına dön
        }

        // String literalleri
        if (*p == '\'') {                    // Tek tırnak gördüysek
            if (!in_single_quote) {          // String literal başlangıcıysa
                in_single_quote = true;      // String moduna gir
                buffer_append_char(&buf, '?');   // String içeriğini '?' ile değiştir (parametre placeholder)
            } else {
                // Eğer iki tırnak üst üste gelirse (escape)
                if (*(p + 1) == '\'') {      // '' (escape edilmiş tek tırnak) ise
                    p += 2;                  // İki karakteri de atla
                    continue;                // Döngünün başına dön
                }
                in_single_quote = false;     // String literal bitti
            }
            p++;                             // Tırnağı atla
            continue;                        // Döngünün başına dön
        }

        if (in_single_quote) {               // String literal içindeysek
            p++;                             // İçeriği atla (zaten '?' yazdık)
            continue;                        // Döngünün başına dön
        }

        // Sayı literalleri (basit yaklaşım: rakamla başlayan kelimeler)
        if (isdigit((unsigned char)*p)) {    // Rakam gördüysek
            buffer_append_char(&buf, '?');   // Sayıyı '?' ile değiştir (parametre placeholder)
            while (isdigit((unsigned char)*p) || *p == '.') {   // Rakam ve nokta karakterlerini atla
                p++;
            }
            continue;                        // Döngünün başına dön
        }

        // Diğer karakterler (küçük harfe çevir)
        buffer_append_char(&buf, (char)tolower((unsigned char)*p));   // Karakteri küçük harfe çevirip buffer'a ekle
        p++;                                                          // Sonraki karaktere geç
    }

    // Boşlukları sadeleştir (çoklu boşlukları tek boşluk yap, baş/son boşlukları temizle)
    const char *raw = buffer_cstr(&buf);     // İlk geçişin sonucunu al
    Buffer clean_buf;                        // İkinci geçiş: boşlukları sadeleştirilmiş buffer
    buffer_init(&clean_buf);                 // Buffer'ı başlat
    bool last_was_space = true; // Başlangıçtaki boşlukları silmek için   // Başta boşluk varmış gibi davran (baş boşlukları atılsın)

    for (const char *c = raw; *c; c++) {     // Ham buffer üzerinde gez
        if (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r') {   // Boşluk karakteri ise
            if (!last_was_space) {           // Önceki karakter boşluk değilse
                buffer_append_char(&clean_buf, ' ');   // Tek bir boşluk ekle
                last_was_space = true;       // Boşluk durumunu işaretle
            }
        } else {                             // Boşluk olmayan karakter ise
            buffer_append_char(&clean_buf, *c);   // Karakteri ekle
            last_was_space = false;               // Boşluk durumunu temizle
        }
    }

    // Sondaki boşluğu temizle
    if (clean_buf.len > 0 && clean_buf.data[clean_buf.len - 1] == ' ') {   // Son karakter boşluksa
        clean_buf.data[clean_buf.len - 1] = '\0';                          // Null terminator ile değiştir
        clean_buf.len--;                                                    // Uzunluğu azalt
    }

    fp->normalized = strdup(buffer_cstr(&clean_buf));   // Normalize edilmiş string'in kopyasını sakla
    fp->hash = fnv1a_hash(fp->normalized);              // Normalize edilmiş string'in hash'ini hesapla

    buffer_free(&buf);        // İlk geçiş buffer'ını serbest bırak
    buffer_free(&clean_buf);  // İkinci geçiş buffer'ını serbest bırak

    return true;              // Başarılı
}

const char *fingerprint_get_normalized(const QueryFingerprint *fp) {   // Normalize edilmiş string'e salt okunur erişim
    if (!fp) return NULL;                                              // NULL pointer koruması
    return fp->normalized;                                             // Normalize edilmiş string'i döndür
}