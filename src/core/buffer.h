#ifndef BUFFER_H              // Header guard başlangıcı: bu dosyanın birden fazla kez include edilmesini engeller
#define BUFFER_H              // Sembol tanımı: BUFFER_H tanımlı değilse tanımla

#include <stddef.h>           // size_t tipi için (len, cap alanlarında kullanılıyor)
#include <stdbool.h>          // bool, true, false tipleri için (fonksiyon dönüş tiplerinde kullanılıyor)
#include <stdint.h>           // SIZE_MAX, uintX_t gibi sabit genişlikli tipler için (buffer.c içinde SIZE_MAX kullanılıyor)

/* Dinamik buffer yapısı (README Bölüm 11) */
typedef struct {              // Buffer adında bir yapı (struct) tanımı başlat
    char *data;     /* Veri tutucu */          // Buffer'ın içindeki karakter dizisinin başlangıç adresi
    size_t len;     /* Mevcut uzunluk */       // Şu an buffer'da bulunan karakter sayısı (null terminator hariç)
    size_t cap;     /* Toplam kapasite */      // Ayrılmış toplam bellek miktarı (bayt cinsinden)
} Buffer;                     // Yapıya Buffer adını ver (typedef sayesinde "struct Buffer" yerine "Buffer" yazılabilir)

/* Başlatma ve Temizlik */
void buffer_init(Buffer *b);          // Buffer'ı başlatır: bellek ayırır, len=0, cap başlangıç değerini atar
void buffer_free(Buffer *b);          // Buffer'ın tuttuğu belleği heap yöneticisine iade eder (serbest bırakır)

/* Veri Ekleme (string, char, formatted string) */
bool buffer_append(Buffer *b, const char *str);              // Bir C string'ini buffer sonuna ekler, başarı durumunu döndürür
bool buffer_append_char(Buffer *b, char c);                  // Tek bir karakteri buffer sonuna ekler
bool buffer_append_fmt(Buffer *b, const char *fmt, ...);     // printf formatında string ekler (değişken argümanlı)

/* Yardımcı Fonksiyonlar */
void buffer_clear(Buffer *b);          // Buffer'ı temizler (len=0, ilk karakter '\0') ama belleği serbest bırakmaz
const char *buffer_cstr(Buffer *b);    // Buffer'ın null-terminated C string halini döndürür (salt okunur erişim)

#endif /* BUFFER_H */          // Header guard sonu: BUFFER_H tanımının bittiğini belirtir