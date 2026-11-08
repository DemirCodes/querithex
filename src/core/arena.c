#include "arena.h"                  // Arena, ArenaBlock yapıları ve fonksiyon prototipleri için başlık
#include <stdlib.h>                 // malloc, free için
#include <string.h>                 // (Bu dosyada doğrudan kullanılmıyor, header bağımlılığı olabilir)
#include <stdint.h>                 // SIZE_MAX sabiti için (taşma korumasında kullanılıyor)

#define ARENA_DEFAULT_BLOCK 4096    // Blok boyutu belirtilmediğinde kullanılacak varsayılan boyut (4 KB)
#define ARENA_ALIGNMENT 8           // Hizalama birimi: her tahsis 8'in katı adrese denk gelir

/* Boyutu hizala (8'in katı yap) */
static size_t align_size(size_t size) {   // Verilen boyutu yukarı yuvarlayarak 8'in katı yapar
    return (size + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1);   // (size+7) & ~7 → 8'in katına yuvarla
}

void arena_init(Arena *a, size_t block_size) {   // Arena'yı başlatır, blok boyutunu ayarlar
    if (!a) return;                              // NULL pointer koruması
    a->head = NULL;                              // Henüz hiç blok yok
    a->block_size = block_size > 0 ? block_size : ARENA_DEFAULT_BLOCK;   // 0 veya negatifse varsayılan 4096 kullan
}

void arena_free(Arena *a) {   // Arena'daki tüm blokları serbest bırakır
    if (!a) return;           // NULL pointer koruması
    ArenaBlock *curr = a->head;   // İlk bloktan başla
    while (curr) {                // Bloklar bitene kadar
        ArenaBlock *next = curr->next;   // Sonraki bloğu geçici olarak sakla (free sonrası erişim için)
        free(curr);                      // Mevcut bloğu serbest bırak
        curr = next;                     // Sonraki bloğa geç
    }
    a->head = NULL;           // Baş işaretçisini sıfırla (dangling pointer önlemi)
}

void *arena_alloc(Arena *a, size_t size) {   // Arena'dan 'size' byte'lık hizalanmış bellek tahsis eder
    if (!a || size == 0) return NULL;        // NULL arena veya sıfır boyut isteği reddedilir

    // İstenen boyutu hizala
    size = align_size(size);                 // Boyutu 8'in katına yuvarla

    // Mevcut blokta yeterli yer var mı?
    if (a->head && (a->head->cap - a->head->used) >= size) {   // En son blokta boş alan yeterliyse
        void *ptr = a->head->data + a->head->used;             // Bloğun kullanılmayan kısmının başlangıcı
        a->head->used += size;                                 // Kullanılan miktarı artır
        return ptr;                                            // Tahsis edilen adresi döndür
    }

    // Yeni blok oluştur
    size_t new_cap = a->block_size;          // Yeni blok kapasitesi varsayılan blok boyutu
    if (new_cap < size) {                    // İstenen boyut blok boyutundan büyükse
        new_cap = size; // İstenen boyut blok boyutundan büyükse   // Bloğu istenen boyuta göre ayarla
    }

    // Taşma koruması
    if (new_cap > (SIZE_MAX - sizeof(ArenaBlock))) {   // sizeof(ArenaBlock) + new_cap toplamı taşabilir mi?
        return NULL;                                   // Taşma olursa tahsis reddedilir
    }

    ArenaBlock *new_block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + new_cap);   // Blok başlığı + veri için tek parça bellek ayır
    if (!new_block) return NULL;                                                  // Ayırma başarısızsa NULL

    new_block->next = a->head;   // Yeni bloğu listenin başına bağla (LIFO düzen)
    new_block->used = size;      // İlk tahsis bu blokta yapıldığı için used = size
    new_block->cap = new_cap;    // Bloğun toplam veri kapasitesi
    a->head = new_block;         // Arena'nın başını yeni blok yap

    return new_block->data;      // Bloğun veri bölgesinin başlangıç adresini döndür
}

size_t arena_total_allocated(const Arena *a) {   // Arena'daki tüm bloklarda kullanılan toplam byte sayısını hesaplar
    if (!a) return 0;                            // NULL arena için 0
    size_t total = 0;                            // Toplam sayaç
    ArenaBlock *curr = a->head;                  // İlk bloktan başla
    while (curr) {                               // Bloklar bitene kadar
        total += curr->used;                     // Bu blokta kullanılan miktarı toplama ekle
        curr = curr->next;                       // Sonraki bloğa geç
    }
    return total;                                // Toplam kullanılan byte sayısı
}