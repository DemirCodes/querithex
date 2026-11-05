#include "array.h"                  // Array yapısı ve fonksiyon prototipleri (array_init, array_free, vs.) için başlık
#include <stdlib.h>                 // malloc, realloc, free için
#include <stdint.h>                 // SIZE_MAX sabiti için (taşma korumasında kullanılıyor)

#define ARRAY_INIT_CAP 16           // Array ilk oluşturulduğunda ayrılacak pointer sayısı


/* array_init (Array *a) :
*   Array yapısını başlatır ve başlangıç kapasitesi kadar bellek ayırır.
*/
void array_init(Array *a) {                              // Array'i başlatır, başlangıç kapasitesi kadar bellek ayırır
    if (!a) return;                                      // NULL pointer koruması
    a->data = (void **)malloc(ARRAY_INIT_CAP * sizeof(void *));   // 16 adet void* tutacak bellek ayır
    if (a->data) {                                       // Bellek ayırma başarılıysa
        a->len = 0;                                      // Başlangıçta eleman yok
        a->cap = ARRAY_INIT_CAP;                         // Kapasiteyi kaydet
    } else {                                             // Bellek ayırma başarısızsa
        a->len = 0;                                      // Uzunluğu sıfırla
        a->cap = 0;                                      // Kapasiteyi sıfırla (data NULL olarak kalır)
    }
}

// Not: ARRAY_INIT_CAP sabiti , tanımlanmış olan array ın başlangıctaki kapasitesini tutar ve belirler




/* array_free (Array *a) :
*   Array'in data dizisini serbest bırakır (elemanların gösterdiği bellekler serbest bırakılmaz).
*   a->data NULL yapılır, a->len ve a->cap sıfırlanır.
*   
*/
void array_free(Array *a) {               // Array'in data dizisini serbest bırakır (elemanların gösterdiği bellekler serbest bırakılmaz)
    if (!a) return;                       // NULL pointer koruması
    free(a->data);                        // data dizisinin kendisini serbest bırak (void* elemanların gösterdiği yerler değil)
    a->data = NULL;                       // Dangling pointer'ı önlemek için NULL ata
    a->len = 0;                           // Uzunluğu sıfırla
    a->cap = 0;                           // Kapasiteyi sıfırla
}


/* array_clear (Array *a) :
*   Array'i temizler: uzunluğu sıfırlar, belleği serbest bırakmaz.
*/
void array_clear(Array *a) {              // Array'i temizler: uzunluğu sıfırlar, belleği serbest bırakmaz
    if (!a) return;                       // NULL pointer koruması
    a->len = 0;                           // Mantıksal olarak boş kabul et (data'daki pointer'lar yerinde duruyor)
}


/*  array_reserve (Array *a, size_t required) :
*   Array'in kapasitesini artırır (gerekirse) ve yeterli alan sağlar.
*   Bu fonksiyon, array'in mevcut kapasitesini kontrol eder ve gerekirse kapasiteyi artırır. 
*   realloc kullanarak bellek taşmasını önler ve array'in güvenli bir şekilde büyümesini sağlar.
*/
static bool array_reserve(Array *a, size_t required) {   // En az 'required' eleman kapasitesi olduğundan emin olur, gerekirse büyütür
    if (!a) return false;                                // NULL pointer koruması
    if ( a->cap >= required ) return true;                 // Mevcut kapasite yeterliyse hiçbir şey yapma

    size_t new_cap = a->cap;                             // Yeni kapasiteyi mevcut kapasiteden başlat
    if (new_cap == 0) new_cap = ARRAY_INIT_CAP;          // Kapasite sıfırsa başlangıç kapasitesini kullan

    while (new_cap < required) {                         // Yeni kapasite yeterli olana kadar
        if (new_cap > (SIZE_MAX / 2)) return false;      // Taşma koruması: iki katına çıkarma overflow yapabilir
        new_cap *= 2;                                    // Kapasiteyi iki katına çıkar (amortized O(1))
    }

    void **new_data = (void **)realloc(a->data, new_cap * sizeof(void *));   // Belleği yeni kapasiteye göre yeniden ayır
    if (!new_data) return false;                         // realloc başarısızsa eski bellek korunur, data değişmez

    a->data = new_data;                                  // Yeni pointer'ı kaydet
    a->cap = new_cap;                                    // Yeni kapasiteyi kaydet
    return true;                                         // Başarılı
}


/* array_push (Array *a, void *item) :
*   Array'in sonuna bir void* eleman ekler.  (null-terminated string veya diğer pointer tipleri olabilir)
*/
bool array_push(Array *a, void *item) {   // Array'in sonuna bir void* eleman ekler
    if (!a) return false;                 // NULL pointer koruması
    if (!array_reserve(a, a->len + 1)) return false;   // Bir eleman daha eklemek için yer olduğundan emin ol

    a->data[a->len] = item;               // Elemanı dizinin sonuna yaz
    a->len++;                             // Uzunluğu bir artır
    return true;                          // Başarılı
}


/* *array_get (const Array *a, size_t index) :
*   Belirtilen indeksteki elemanı döndürür. 
*   Eğer indeks sınır dışı ise NULL döndürür.
*   
*/
void *array_get(const Array *a, size_t index) {   // Belirtilen indeksteki elemanı döndürür
    if (!a || index >= a->len) return NULL;       // NULL array veya sınır dışı indeks için NULL döndür
    return a->data[index];                        // İlgili indeksteki void* pointer'ı döndür
}