#ifndef ARENA_H              // Header guard başlangıcı: bu dosyanın birden fazla include edilmesini engeller
#define ARENA_H              // Sembol tanımı: ARENA_H tanımlı değilse tanımla

#include <stddef.h>           // size_t tipi için (used, cap, block_size alanlarında kullanılıyor)
#include <stdbool.h>          // (Bu header'da doğrudan bool kullanılmıyor ama ileride eklenebilir bağımlılık)

/* Arena bellek yöneticisi (README Bölüm 11 ve 4.0) */
typedef struct ArenaBlock {          // Arena içindeki tek bir bellek bloğunu tanımlayan yapı
    struct ArenaBlock *next;         // Bir sonraki bloğa işaretçi (bağlı liste)
    size_t used;                     // Bu blokta şu ana kadar kullanılan byte sayısı
    size_t cap;                      // Bu bloğun toplam veri kapasitesi (byte)
    /* Veri bu bloğun hemen ardından gelir */
    char data[];                     // Esnek dizi (flexible array member): veri, struct'ın hemen ardından gelir
} ArenaBlock;                        // Yapıya ArenaBlock adını ver

typedef struct {                     // Arena'nın kendisini tanımlayan yapı
    ArenaBlock *head;                // Bloklar listesinin başı (en son eklenen blok)
    size_t block_size;               // Yeni bloklar için varsayılan boyut
} Arena;                             // Yapıya Arena adını ver

/* Başlatma ve Temizlik */
void arena_init(Arena *a, size_t block_size);   // Arena'yı başlatır, blok boyutunu ayarlar (0 ise varsayılan kullanılır)
void arena_free(Arena *a);                       // Arena'daki tüm blokları tek seferde serbest bırakır

/* Bellek ayırma (asla tek tek free edilmez, hepsi arena_free ile temizlenir) */
void *arena_alloc(Arena *a, size_t size);        // Arena'dan hizalanmış bellek tahsis eder; tek tek free edilmez

/* İstatistikler (test ve debug için) */
size_t arena_total_allocated(const Arena *a);    // Tüm bloklarda kullanılan toplam byte sayısını döndürür (debug/test amaçlı)

#endif /* ARENA_H */          // Header guard sonu: ARENA_H tanımının bittiğini belirtir