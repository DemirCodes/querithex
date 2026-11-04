#include "buffer.h"
#include <stdlib.h>      // malloc, realloc, free gibi bellek fonksiyonları için
#include <string.h>      // strlen, memcpy gibi string işlemleri için
#include <stdarg.h>      // va_list, va_start, va_end, va_copy (değişken argüman) için
#include <stdio.h>       // vsnprintf için

/* Başlangıç kapasitesi */
#define BUFFER_INIT_CAP 32   // Buffer ilk oluşturulduğunda ayrılacak bayt sayısı

void buffer_init(Buffer *b) {   // Buffer'ı başlatır, ilk bellek bloğunu ayırır
    if (!b) return;             // NULL pointer koruması
    b->data = (char *)malloc(BUFFER_INIT_CAP);   // Başlangıç kapasitesi kadar bellek ayır
    if (b->data) {                               // Bellek ayırma başarılıysa
        b->data[0] = '\0';                       // Boş string olarak başlat (null terminator)
        b->len = 0;                              // Uzunluk sıfır
        b->cap = BUFFER_INIT_CAP;                // Kapasiteyi kaydet
    } else {                                     // Bellek ayırma başarısızsa
        b->len = 0;                              // Uzunluğu sıfırla
        b->cap = 0;                              // Kapasiteyi sıfırla
    }
}

void buffer_free(Buffer *b) {   // Buffer'ın kullandığı belleği serbest bırakır
    if (!b) return;             // NULL pointer koruması
    free(b->data);              // Ayrılan belleği serbest bırak
    b->data = NULL;             // Dangling pointer'ı önlemek için NULL ata
    b->len = 0;                 // Uzunluğu sıfırla
    b->cap = 0;                 // Kapasiteyi sıfırla
}

void buffer_clear(Buffer *b) {  // Buffer'ı temizler ama belleği serbest bırakmaz
    if (!b || !b->data) return; // NULL veya boş buffer koruması
    b->len = 0;                 // Uzunluğu sıfırla
    b->data[0] = '\0';          // İlk karakteri null terminator yap (boş string)
}


/* buffer_reserve (Buffer *b, size_t required) :
* 
* Buffer'ın kapasitesini artırır (gerekirse) ve yeterli alan sağlar.
* Bu fonksiyon, buffer'ın mevcut kapasitesini kontrol eder ve gerekirse kapasiteyi artırır. 
* realloc kullanarak bellek taşmasını önler ve buffer'ın güvenli bir şekilde büyümesini sağlar.
*/
static bool buffer_reserve(Buffer *b, size_t required) {   // Gerekli kapasiteyi garanti eder, gerekirse büyütür
    if (!b) return false;                                   // NULL pointer koruması
    
    // required + 1 (null terminator için)
    if ( b->cap >= required + 1) return true;                // Mevcut kapasite yeterliyse hiçbir şey yapma

    size_t new_cap = b->cap;                                // Yeni kapasiteyi mevcut kapasiteden başlat
    if (new_cap == 0) new_cap = BUFFER_INIT_CAP;            // Kapasite sıfırsa başlangıç kapasitesini kullan
    
    while ( required + 1 > new_cap ) {                        // Yeni kapasite yeterli olana kadar
        if (new_cap > (SIZE_MAX / 2)) return false;         // Taşma koruması: iki katına çıkarma overflow yapabilir
        new_cap *= 2;                                       // Kapasiteyi iki katına çıkar (amortized O(1))
    }

    char *new_data = (char *)realloc(b->data, new_cap);     // Belleği yeni boyuta göre yeniden ayır
    if (!new_data) return false;                            // realloc başarısızsa eski bellek korunur

    b->data = new_data;                                     // Yeni pointer'ı kaydet
    b->cap = new_cap;                                       // Yeni kapasiteyi kaydet
    return true;                                            // Başarılı
}


/*
* buffer_append (Buffer *b, const char *str) :
* Dinamik olarak büyüyen bir buffer'a bir string ekler.
* Bu fonksiyon, buffer'ın mevcut kapasitesini kontrol eder ve gerekirse kapasiteyi artırır. 
* Eklenecek string'in uzunluğunu hesaplar ve buffer'ın sonuna kopyalar. 
* Null terminator ekler ve buffer'ın uzunluğunu günceller.
*/
bool buffer_append(Buffer *b, const char *str) {   // Buffer'a bir string ekler
    if (!b || !str) return false;                  // NULL pointer koruması
    size_t str_len = strlen(str);                  // Eklenecek string'in uzunluğunu hesapla
    if (!buffer_reserve(b, b->len + str_len)) return false;   // Yeterli kapasite olduğundan emin ol
    
    memcpy(b->data + b->len, str, str_len);        // String'i buffer'ın sonuna kopyala (null hariç)
    b->len += str_len;                             // Uzunluğu güncelle
    b->data[b->len] = '\0';                        // Null terminator ekle
    return true;                                   // Başarılı
}



/*
* buffer_append_char (Buffer *b, char c) :
* Dinamik olarak büyüyen bir buffer'a tek bir karakter ekler . 
* Çünkü sayı dönüşümü , parsing , escape çözme gibi işlemlşerde veri doğal olarak tek tek karakterler halinde gelir. 
* Bu yüzden buffer_append_char fonksiyonu buffer_append fonksiyonundan daha hızlıdır.
*/ 
bool buffer_append_char(Buffer *b, char c) {   // Buffer'a tek bir karakter ekler
    if (!b) return false;                      // NULL pointer koruması
    if (!buffer_reserve(b, b->len + 1)) return false;   // 1 byte + null için yer olduğundan emin ol
    
    b->data[b->len] = c;                       // Karakteri buffer'ın sonuna yaz
    b->len++;                                  // Uzunluğu bir artır
    b->data[b->len] = '\0';                    // Null terminator ekle
    return true;                               // Başarılı
}



/*
* buffer_append_fmt (Buffer *b, const char *fmt, ...) :
* Dinamik olarak log/mesaj/JSON/SQL/HTML biriktirirken,
* Formatlı metni bilinmeyen boyutta bir buffer'a yazarken,
* sprintf'in taşma riskinden ve snprintf'in kesme davranışından kurtulmak istediğinde.
*/
bool buffer_append_fmt(Buffer *b, const char *fmt, ...) {   // printf benzeri formatlı string ekler
    if (!b || !fmt) return false;                           // NULL pointer koruması
    
    va_list args;                                           // Değişken argüman listesi
    va_start(args, fmt);                                    // Argümanları başlat (fmt'den sonrası)
    va_list args_copy;                                      // vsnprintf iki kez kullanılacağı için kopya
    va_copy(args_copy, args);                               // Argüman listesini kopyala (ikinci geçiş için)
    

    // vsnprintf : printf ailesinin buffer a yazan versiyonudur .
    // __s : bellek adresi taşır , __n : buffer boyutu , __fmt : format string , __args : değişken argüman listesi
    // Gerekli boyutu hesapla
    int needed = vsnprintf(NULL, 0, fmt, args);             // İlk geçiş: NULL ile çağırıp gereken boyutu öğren
    va_end(args);                                           // İlk argüman listesini kapat
    
    if (needed < 0) {                                       // Format hatası kontrolü
        va_end(args_copy);                                  // Kopyayı temizle
        return false;                                       // Başarısız
    }
    
    size_t size_needed = (size_t)needed;                    // int'i size_t'ye çevir (negatif değil, kontrol edildi)
    if (!buffer_reserve(b, b->len + size_needed)) {         // Yeterli kapasite var mı?
        va_end(args_copy);                                  // Kopyayı temizle
        return false;                                       // Başarısız
    }
    
    // Gerçek yazma işlemi
    vsnprintf(b->data + b->len, size_needed + 1, fmt, args_copy);   // Formatlı string'i buffer sonuna yaz (+1 null için)
    va_end(args_copy);                                      // Kopyayı temizle
    
    b->len += size_needed;                                  // Uzunluğu güncelle
    return true;                                            // Başarılı
}



/*
* buffer_cstr (Buffer *b) :
* Buffer'ın C string (null-terminated) halini döndürür
* null-terminated pointer içerisine eklenen bir string in sonuna \0 karakteri ekler , c dilinde string okuması için gereklidir.
*/
const char *buffer_cstr(Buffer *b) {   // Buffer'ın C string (null-terminated) halini döndürür
    if (!b || !b->data) return NULL;   // NULL veya boş buffer koruması
    return b->data;                    // İç veriyi döndür (salt okunur amaçlı)
}