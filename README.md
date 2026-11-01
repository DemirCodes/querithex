# Querithex — Proje Dökümanı

**Versiyon:** 2.4 · **Tarih:** 2026-10-30 · **Durum:** Planlama tamamlandı, kodlamaya hazır **Dil:** C (C11) · **Platform:** Linux (Ubuntu/Debian/Fedora) · **Hedef PostgreSQL:** 13+ (jsonlog için 15+)

---

## 1. Proje Özeti

**Querithex**, PostgreSQL log dosyalarını okuyup sorguları, güvenlik olaylarını, performans sorunlarını ve kararlılık belirtilerini çıkaran, C ile yazılmış, tek geçişli (streaming) bir log analiz motorudur. v2.2 ile birlikte, opsiyonel olarak veritabanına bağlanıp şema keşfi ve plan toplama da yapabilir.

**İsim:** `Querith` (sorgu) + `-ex` (ötesi) → *"Sorgunun ötesinde"* **Slogan:** *"Verinin ötesinde, güvenliğin merkezinde"*

### Kapsam

- **Kapsam içi (v1):** log dosyası analizi (batch ve follow modu), sorgu parmak izi (fingerprint) çıkarma, kural tabanlı güvenlik tespiti, yavaş sorgu ve olay analizi, JSON/CSV/HTML/metin rapor. Opsiyonel: libpq ile şema keşfi (K1), yalnızca test container'ında `EXPLAIN (ANALYZE, FORMAT JSON)` ile plan toplama (K2, K6), kural tabanlı deterministik tavsiye (K3), opsiyonel AI advisory katmanı (K4).
- **Kapsam dışı (v1):** canlı trafik yakalama (proxy/sniffing), otomatik engelleme, tam SQL parser (PostgreSQL grammar'ı), PostgreSQL dışı DBMS logları, `csvlog`/`jsonlog` (Faz 4), HTTP sunucusu/dashboard, plugin sistemi, rapor şifreleme (Faz 5), threading (Faz 4), farklı config formatları (JSON/YAML/TOML/env). Sorgu analizi "yeterince iyi" bir lexer ile yapılır; lexer'ın kapsamı Bölüm 6.2'de tanımlıdır.

### Benzer araçlardan farkı

`pgBadger` (log raporlama), `pg_stat_statements` (sunucu içi istatistik) ve `auto_explain` zaten var. Querithex'in değeri: **tek binary, bağımlılıksız, güvenlik odaklı kurallar + performans + olay analizini aynı hatta birleştirmesi**, ayrıca **"neden yavaş, ne yapmalı" deterministik tavsiyesi** (K3) ve **ana DB'ye dokunmadan test container'ında plan toplama** (K6). C ile sıfırdan öğrenme projesi olması da ayrı bir değerdir.

---

## 2. Amaçlar ve Gerçekçi Sınırlar

Tasarım gerçeği: **log dosyası her şeyi söylemez.** Her amaç için neyin logdan çıkarılabildiği, neyin ek kaynak gerektirdiği aşağıdadır. Modül sahipliği de bu tabloda belirtilmiştir; böylece aynı tespitin iki modülde birden ele alınması önlenir.

| # | Amaç | Logdan çıkarılabilen | Log tek başına yetmez (ek kaynak) | Modül |
| --- | --- | --- | --- | --- |
| 1 | Listeleme | Sorgu metni, kullanıcı, veritabanı, uygulama, istemci IP | Tablo adları için SQL lexer gerekir | log_parser, stats |
| 2 | Analiz | Sorgu tipi, sıklık, tekrar (parmak izi ile) | — | fingerprint, stats |
| 3 | Güvenlik | Enjeksiyon desenleri, kaba kuvvet, yetkisiz erişim denemeleri | Gerçek yetki bilgisi için `pg_roles`/`pg_hba.conf` | security |
| 4 | Performans | Süre (ms), yavaş sorgular, N+1 deseni | Plan: libpq ile `EXPLAIN (ANALYZE, FORMAT JSON)` (yalnızca SELECT; DML için ANALYZE'sız `EXPLAIN`) — bkz. K2. Kural tabanlı tavsiye ("neden yavaş, ne yapmalı") — bkz. K3 | performance |
| 5 | Kullanılabilirlik | FATAL/ERROR sayısı, bağlantı reddi, timeout | Hata **oranı** için toplam sorgu sayısı gerekir (bkz. 9.2) | incidents |
| 6 | Stabilizasyon | Deadlock, kilit bekleme, checkpoint sıklığı, geçici dosya, çökme/OOM | Kaynak metrikleri için `pg_stat_*` | incidents |

**Sahiplik kuralı:** Her tespit türü yalnızca bir modülün sorumluluğundadır. Yetkisiz erişim denemeleri (amaç 3) `security` modülüne aittir; `incidents` modülü yalnızca Bölüm 9.1'deki olayları üretir.

### 2.1 Alınan Kararlar (v2.2–v2.3; düzeltmeler için Ek B.3)

Bu bölüm, v2.1'den sonra alınan ve dökümanın diğer bölümlerini etkileyen kararları listeler. Sonraki bölümler bu kararlara referans verir (ör. "bkz. K2").

**K1 — Şema keşfi libpq ile doğrudan yapılır.**
Querithex, veritabanı şemasını (tablolar, sütunlar, foreign key'ler, index'ler) PostgreSQL'in resmi C kütüphanesi olan libpq ile `information_schema`/`pg_catalog` sorguları çalıştırarak çeker. Node.js/Prisma kullanılmaz.
- *Neden:* Daha az bağımlılık (Node.js gerekmez), daha doğru tip bilgisi (Prisma'nın eşleme katmanı yok), zaten EXPLAIN için libpq kullanılacak.
- *Etki:* Sistemde Node.js kurulu olması gerekmez. Şema yalnızca `--introspect` bayrağı verildiğinde çekilir; verilmezse Querithex şema bilgisi olmadan da çalışır (bazı performans kuralları devre dışı kalır).

**K2 — Performans analizi için `EXPLAIN (ANALYZE, FORMAT JSON)` libpq ile çalıştırılır.**
Querithex, yavaş `SELECT` sorguları için planı JSON formatında alır ve ayrıştırır. `INSERT/UPDATE/DELETE` için `EXPLAIN ANALYZE` **asla** çalıştırılmaz (sorguyu gerçekten yürütür, veriyi değiştirir); DML için yalnızca ANALYZE'sız `EXPLAIN` kullanılır.
- *Neden:* "Neden yavaş" ancak planla anlaşılır; plan olmadan yalnızca süre bilinir.
- *Etki:* Querithex veritabanına bağlanıp sorgu çalıştırır. Bunun için salt okunur bir kullanıcı ve `statement_timeout` ayarı gerekir. Üretimde DML'e `EXPLAIN ANALYZE` çalıştırılmaz.

**K3 — Tavsiyeler deterministik kural motoruyla üretilir.**
"Şu index'i ekle", "sorguyu şöyle yaz", "`ANALYZE` çalıştır" gibi tavsiyeler AI olmadan, plan + şema + sorgu metnini birleştiren kural tabanlı bir motorla üretilir.
- *Neden:* Deterministik (aynı girdi → aynı çıktı), test edilebilir (golden testler), gizlilik riski yok, AI'ya bağımlı değil.
- *Etki:* AI kapalıyken de Querithex tavsiye verir. AI'ya özel olan "tahmini etki" ve "serbest yorum"dur; "ne yapmalı" tavsiyesi AI'ya özel değildir.

**K4 — AI opsiyonel bir "advisory" katmanıdır; varsayılan kapalı.**
AI, deterministik bulguların üstüne **ek** olarak "tahmini etki" (ör. "~%40 hızlanma (tahmini)") ve "serbest yorum" (ör. "bu sorgu N+1'in parçası olabilir") ekler. Karar vermez; yorum yapar. Kullanıcı isterse açar (`--ai openai`, `--ai ollama`, `--ai custom`), isterse hiç kullanmaz.
- *Neden:* Herkesin AI'ya erişimi/isteği yok; gizlilik riski; AI yanlış yorum yaparsa deterministik kısım bozulmasın.
- *Etki:* AI'sız Querithex tam çalışır. AI açılırsa rapora ek `advisory` alanı gelir. AI başarısız olursa (ağ yok, anahtar yok) rapor deterministik kısımla devam eder, çökmez.

**K5 — Raporda her bulgu `source: "rule" | "ai"` etiketi taşır.**
Deterministik kural bulguları `source: "rule"`, AI yorumları `source: "ai"` olarak işaretlenir. AI kapalıysa `advisory` alanı hiç eklenmez.
- *Neden:* AI kapalıyken raporun tam kalması; AI yorumunun deterministik bulgudan ayırt edilebilmesi.
- *Etki:* Raporda küçük bir etiket; kullanıcının ek bir şey yapması gerekmez.

**K6 — Ana veritabanı salt okunur, test veritabanı ayrı Docker container.**
Querithex, ana veritabanı container'ına (`main_database`) **yalnızca okuma** için bağlanır: şema, tablo, index, sorgu metni, log. **Hiçbir yazma, DDL, DML, `EXPLAIN ANALYZE` yok.** Ana container'dan çekilen şema/veri, ayrı bir test container'ına (`test_database`) kopyalanır (`--sync-test`). Tüm `EXPLAIN ANALYZE`, kural testleri, plan toplama **yalnızca `test_database` üzerinde** yapılır. `main_database` üzerinde asla değişiklik yapılmaz.
- *Neden:* Üretim verisine dokunmadan analiz; `EXPLAIN ANALYZE`'in DML'de veriyi değiştirme riski sıfırlanır; test container'ında istediğiniz kadar deneme yapılabilir.
- *Etki:* İki ayrı bağlantı: `--main-conninfo` (salt okunur) ve `--test-conninfo` (test container). Querithex hangi işlemi hangi bağlantıda yapacağını bilir; `EXPLAIN ANALYZE` yalnızca test bağlantısında çalışır. Ana bağlantıda `SET default_transaction_read_only = on;` çalıştırılır — yanlışlıkla yazma imkânsız hale gelir.

**K7 — Tek config dosyası: `querithex.conf` (INI).**
Tüm yapılandırma (container/veritabanı bağlantıları, analiz eşikleri, AI, kural seti) **tek bir** `querithex.conf` dosyasından okunur. Başka conf dosyası yoktur; AI dahil her şey aynı dosyanın bölümlerindedir (`[baglanti]`, `[genel]`, `[ai]`, `[kurallar]`). Dosya adı/uzantısı Querithex'e özgüdür; sistemde başka yazılımla çakışmaz. `.c` uzantısı bilinçli olarak seçilmedi (derleyici/IDE/git karışıklığı). Parola/API anahtarı dosyada **tutulmaz**; `PGPASSWORD` / `PGTESTPASSWORD` / `QUERITHEX_AI_KEY` ortam değişkenlerinden okunur. Öncelik: CLI > env > `querithex.conf` > varsayılan.
- *Neden:* Kullanıcı her çalıştırmada CLI'ye özel ayar yazmak zorunda kalmasın; kalıcı yapılandırma tek yerde dursun.
- *Etki:* `config.c/h` yalnızca INI ayrıştırır; JSON/YAML/TOML/env için ekstra okuyucu yok. `-c/--config` ile aynı formatta başka bir dosya gösterilebilir; verilmezse `./querithex.conf` aranır.

**Karar–bölüm eşlemesi:**

| Karar | Etkilediği bölümler |
| --- | --- |
| K1 | 2 (Amaçlar), 4 (Mimari), 11 (Proje Yapısı), 12 (CLI) |
| K2 | 2 (Amaçlar), 8.1 (Performans), 12 (CLI), 17 (Riskler) |
| K3 | 2 (Amaçlar), 8.1 (Performans), 10 (Rapor) |
| K4 | 4 (Mimari), 10 (Rapor), 12 (CLI), 17 (Riskler) |
| K5 | 10 (Rapor), 15 (Başarı Kriterleri) |
| K6 | 4 (Mimari), 8.1 (Performans), 12 (CLI), 15 (Başarı Kriterleri), 17 (Riskler) |
| K7 | 11 (Proje Yapısı), 12 (CLI), 12.1 (Config şeması) |
| D1–D9 | Düzeltmeler; ayrıntı Ek B.3 |

---

## 3. PostgreSQL Tarafı Yapılandırma

Querithex'in ne görebileceği `postgresql.conf` ayarlarına bağlıdır. Önerilen başlangıç:

```
logging_collector = on
log_line_prefix = '%m [%p] %q%u@%d/%a %h '
log_min_duration_statement = 100   # ms; 0 = hepsi (yoğun sistemde pahalı)
log_statement = 'none'             # veya 'ddl' / 'mod'
log_connections = on
log_disconnections = on
log_lock_waits = on
deadlock_timeout = 1s
log_temp_files = 0
log_checkpoints = on
log_min_error_statement = error
```

**Dikkat edilecekler:**

- `log_statement='all'` ile `log_min_duration_statement` birlikte açılırsa PostgreSQL aynı sorguyu iki kez yazmaz: `statement:` satırı ayrı, `duration:` satırı sorgu metni olmadan gelir. Parser bu iki satırı **PID ile eşleştirmelidir**.
- `%q` oturum dışı (arka plan) süreçlerde `user@db` kısmını atlar; parser bu satırları da kabul etmelidir.
- `log_line_prefix` kullanıcıya göre değişir → prefix formatı `querithex.conf` içinde ayarlanabilir olmalı (veya `jsonlog`/`csvlog` desteklenmeli).
- Parametreli sorgularda (extended protocol) değerler ayrı bir `DETAIL: parameters: $1 = '...'` satırında gelir. Enjeksiyon payload'ı burada olabilir.
- **Loglar hassas veri içerir** (parola, e-posta, kart no). Bkz. 7.3 ve 10.
- **Eşik senkronu:** Querithex'in yavaş sorgu eşiği (varsayılan 100 ms, Bölüm 8.2) `log_min_duration_statement` ile aynı olmalıdır. Farklıysa Querithex bunu bilemez; `querithex.conf` içindeki `slow_threshold_ms` değeri kullanıcı tarafından bu ayarla eşitlenmelidir. Bu bağımlılık `KULLANIM.md`'de açıkça yazılır.

---

## 4. Mimari

**Tek geçiş, sabit bellek, ortak olay yapısı.** Şema keşfi ve AI advisory opsiyonel ön/son adımlardır; kapalıyken akış değişmez.

```
  postgresql.log  (dosya / stdin / follow modu)
          │
          ▼
  ┌──────────────────────────────┐
  │ 1. Okuyucu                   │  getline, uzun satır limiti, rotasyon takibi
  └──────────────┬───────────────┘
                 ▼
  ┌──────────────────────────────┐
  │ 2. Log Parser                │  çok satırlı kayıt birleştirme, prefix ayrıştırma
  │    → LogEvent struct         │  zaman, pid, user, db, app, ip, seviye, süre, mesaj
  └──────────────┬───────────────┘
                 ▼
  ┌──────────────────────────────┐
  │ 2.5 schema_introspect        │  (opsiyonel, --introspect) İKİ AŞAMALI (K6):
  │    (yalnızca --introspect)   │  (a) main_database'den SALT OKUMA ile şema çek;
  │                              │  (b) --sync-test ile test_database'e kopyala → Şema modeli
  └──────────────┬───────────────┘
                 ▼
  ┌──────────────────────────────┐
  │ 3. Maskeleme + Parmak izi    │  literal → ?, fingerprint hash
  └──────────────┬───────────────┘
                 ▼
  ┌──────────────────────────────┐
  │ 4. Analizörler (paralel akış)│
  │  Güvenlik │ Performans │ Olay│  her biri LogEvent okur, Bulgu (Finding) üretir
  │           ↑                  │  performance, Şema modelini kullanır
  └──────────────┬───────────────┘
                 ▼
  ┌──────────────────────────────┐
  │ 5. İstatistik (Aggregator)   │  hash tablosu, histogram, top-N, zaman kovaları
  └──────────────┬───────────────┘
                 ▼
  ┌──────────────────────────────┐
  │ 5.5 ai_advisor               │  (opsiyonel, --ai) deterministik bulgulara
  │    (yalnızca --ai)           │  ek: tahmini etki + serbest yorum → advisory
  └──────────────┬───────────────┘
                 ▼
  ┌──────────────────────────────┐
  │ 6. Rapor                     │  JSON / CSV / HTML / metin özet
  │                              │  her bulgu source: "rule" | "ai"
  └──────────────────────────────┘
```

**Opsiyonel adımların kuralı:** `schema_introspect` ve `ai_advisor` kapalıyken akış 1→2→3→4→5→6 olarak çalışır; açıkken araya girerler. Hiçbiri zorunlu değildir; biri başarısız olursa (bağlantı yok, AI yok) akış deterministik kısımla devam eder.

### 4.0 Terimler (bağlayıcı sözlük)

- **LogEvent:** Parser'ın ürettiği, tek bir log **kaydını** temsil eden yapı. Alanları: `timestamp, pid, user, db, app, client_ip, level, duration_ms, message, raw_record, truncated, continued`. Her log kaydı bir `LogEvent`'tir.
- **Bulgu (Finding):** Bir analizörün bir `LogEvent` üzerinden ürettiği sonuç. Alanları: `rule_id, severity, module, evidence_masked, event_ref, source`. Raporun atomu budur. `event_ref` bir **kopya değil**, ilgili `LogEvent`'in kısa özetidir (timestamp, pid, rule_id). Follow modunda `LogEvent` serbest bırakıldıktan sonra `Finding` yaşamaya devam edebilir; bu nedenle `event_ref` arena'da tutulur, `LogEvent`'e işaretçi taşımaz.
- **Olay (Incident):** Bölüm 9.1'deki özel durumlar (deadlock, auth hatası, OOM vb.). Bunlar `incidents` modülünün ürettiği **Bulgu türleridir**; `LogEvent` ile karıştırılmaz.
- **Parmak izi (Fingerprint):** Normalize edilmiş sorgunun 64-bit hash'i.
- **Kayıt (Record):** Bir veya daha fazla fiziksel satırdan oluşan mantıksal log birimi; parser çıktısı tek bir `LogEvent`'tir.
- **Şema modeli (Schema model):** `schema_introspect` ile libpq üzerinden çekilen, bellekte tutulan tablo/sütun/foreign key/index yapısı. `--introspect` verilmezse boştur; performance modülü şema gerektiren kuralları atlar.
- **Advisory:** AI'nın deterministik bir bulguya eklediği "tahmini etki" ve "serbest yorum" alanı. `source: "ai"` etiketi taşır; AI kapalıyken hiç üretilmez. Karar değil, yorumdur.

Metinde "olay" kelimesi tek başına geçtiğinde **Incident** kastedilir; "kayıt" ise `LogEvent`.

### 4.1 Çalışma modları

- `batch` — dosyayı baştan sona analiz eder, rapor üretir.
- `follow` — `tail -f` benzeri; `inotify` + log rotasyonu (inode değişimi) takibi; periyodik özet. "Gerçek zamanlı" hedefi yalnızca bu modla karşılanır.

**Opsiyonel adımların modlarla ilişkisi:** `--introspect` her iki modda da çalışır (batch'te bir kez, follow'da başlangıçta bir kez). `--ai` her iki modda da rapor aşamasında çalışır. İkisi de kapalıyken mod davranışı değişmez.

**K6 notu:** `--introspect` ana container'dan yalnızca okur; `--sync-test` ile test container'ına kopyalar. `EXPLAIN ANALYZE` (performans modülü) **yalnızca** `--test-conninfo` üzerinde çalışır; ana bağlantıda hiç `EXPLAIN` çalıştırılmaz.

**Tasarım ilkeleri:** her satır en fazla bir kez işlenir; bellek log boyutundan bağımsızdır (parmak izi tablosuna üst sınır konur, aşan kayıtlar `diğer` kovasına düşer); modüller `LogEvent` dışında birbirinin iç yapısını bilmez.

---

## 5. Log Formatı ve Ayrıştırma Kuralları

### 5.0 Terimler (bu bölüm için)

- **Prefix (`log_line_prefix`):** Her log satırının başına PostgreSQL'in eklediği sabit alanlar (zaman, PID, kullanıcı@db/app, IP). Parser bu alanları ayrıştırır; format `postgresql.conf`'ta kullanıcıya göre değişir.
- **Kayıt (Record):** Bir veya daha fazla fiziksel satırdan oluşan mantıksal log birimi. Uzun sorgular TAB ile devam eden satırlara bölünür; parser bunları tek kayıt olarak birleştirir (bkz. 4.0).
- **Devam satırı (Continuation):** Çok satırlı kayıtta, ilk satırdan sonra TAB ile başlayan satırlar. `DETAIL:`, `HINT:`, `CONTEXT:`, `STATEMENT:` satırları da aynı kayda aittir.
- **Extended protocol:** PostgreSQL'in parametreli sorgu yolu. Sorgu metni ile parametre değerleri ayrı gelir; değerler `DETAIL: parameters: $1 = '...'` satırındadır.
- **`%q`:** `log_line_prefix` içinde kullanılan özel alan; oturum dışı (arka plan) süreçlerde `user@db` kısmını **atlar**. Parser bu satırları da kabul etmelidir.
- **`duration:` / `statement:`:** PostgreSQL'in süre ve sorgu metnini yazdığı iki ayrı mesaj türü. `log_statement` ile `log_min_duration_statement` birlikte açıksa aynı sorgu için iki satır gelir; parser bunları **PID ile eşleştirir** (bkz. 3).
- **Kesildi işareti (Truncated):** Satır başına üst sınır (`max_line_bytes`) aşıldığında satırın kesildiğini belirten bayrak; `LogEvent.truncated` alanında taşınır.

### 5.1 Örnek satırlar

```
2026-10-30 14:30:15.123 UTC [12345] postgres@testdb/psql 10.0.0.5 LOG:  statement: SELECT * FROM kullanicilar;
2026-10-30 14:30:16.456 UTC [12345] postgres@testdb/psql 10.0.0.5 LOG:  duration: 0.123 ms  statement: SELECT * FROM kullanicilar WHERE id = 1;
2026-10-30 14:30:17.789 UTC [12345] postgres@testdb/psql 10.0.0.5 ERROR:  relation "olmayan_tablo" does not exist
2026-10-30 14:30:17.789 UTC [12345] postgres@testdb/psql 10.0.0.5 STATEMENT:  SELECT * FROM olmayan_tablo;
2026-10-30 14:30:18.001 UTC [999] LOG:  checkpoint starting: time
```

Son satır `%q` davranışını gösterir: arka plan sürecinde `user@db/app` ve istemci IP alanları yoktur; parser bu satırları da kabul etmelidir.

### 5.2 Alanlar

| Alan | Örnek | Not |
| --- | --- | --- |
| Zaman | `2026-10-30 14:30:15.123 UTC` | `%m` |
| PID | `[12345]` | `%p`; duration/statement eşleştirmesi için anahtar |
| Kullanıcı@DB/Uygulama | `postgres@testdb/psql` | `%u@%d/%a`, oturum dışında yok |
| İstemci | `10.0.0.5` | `%h`; güvenlik analizi için kritik; oturum dışında yok |
| Seviye | `LOG`, `ERROR`, `FATAL`, `WARNING`, `PANIC`, `DETAIL`, `STATEMENT` |  |
| Mesaj | `statement: ...`, `duration: ...` |  |

### 5.3 Parser'ın dikkat etmesi gereken zor durumlar

1. **Çok satırlı kayıtlar:** uzun sorgularda devam satırları TAB ile başlar; `DETAIL:`, `HINT:`, `CONTEXT:`, `STATEMENT:` satırları aynı olaya aittir. Satır satır işlemek yanlış sonuç verir → *kayıt* (record) bazlı okuma gerekir.
2. `duration:` ile `statement:` ayrı satırlarda gelebilir (bkz. 3).
3. Extended protocol: `execute <unnamed>: ...` + `DETAIL: parameters: ...`.
4. Aşırı uzun satırlar (MB'larca `INSERT`): satır başına üst sınır (varsayılan 1 MB, `querithex.conf` ile değiştirilebilir) ve kesildi işareti.
5. Bozuk/yarım satırlar ve geçersiz UTF-8: parser **asla çökmemeli**, hatalı satırı sayaçla geçmeli.
6. Zaman dilimi ve gece yarısı geçişi; log rotasyonu sırasında yarım satır.
7. Alternatif biçimler: `csvlog` ve `jsonlog` (PG 15+) çok daha güvenilir ayrıştırılır; **Faz 4'te (v1.x sonrası)** desteklenmesi önerilir (bkz. Bölüm 1 kapsam dışı listesi).

---

## 6. Sorgu Parmak İzi (Fingerprint) ve Lexer Kapsamı

### 6.0 Terimler (bu bölüm için)

- **Normalizasyon:** Sorgu metnini literal değerlerden arındırıp `?` ile değiştirme işlemi (yorum silme, boşluk sadeleştirme, `IN (...)` sadeleştirme). Aynı sorgunun farklı literal'lerle yazılmış hâllerini aynı parmak izine indirger.
- **Parmak izi (Fingerprint):** Normalize edilmiş sorgunun 64-bit hash'i (FNV-1a). "Aynı sorgu kaç kez çalıştı?" sorusunun doğru cevabı bu hash ile verilir (bkz. 4.0).
- **Maskeleme (Masking):** Rapora literal değerler yerine normalize sorgu yazma; parola/kişisel verinin rapora sızmamasını sağlar. Varsayılan açıktır; `--no-mask` ile kapatılır (bkz. 10).
- **Tokenizer:** Girdi metnini anlamlı parçalara (token) ayıran bileşen. `sql_lexer` bir tokenizer'dır; tam parser değildir (grammar kurmaz).
- **Token:** Tokenizer'ın ürettiği en küçük anlamlı birim: dize literali, tanımlayıcı, sayı, operatör, noktalama.
- **Stacked query (yığılmış sorgu):** Tek bir `execute` içinde dize/yorum dışında `;` ile ayrılmış ikinci (ve sonraki) SQL ifadesi. Enjeksiyon göstergesi olabilir (kural S3).
- **İfade tipi (Statement type):** Sorgunun ilk anahtar kelimesine göre türü: `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `COPY`, `CREATE`, `ALTER`, `DROP`, `TRUNCATE`.
- **Şema nitelemesi (Schema qualification):** Tablo adının şema ile birlikte yazılması (`public.tablo`). Lexer bunu tek tablo adı olarak çıkarır.
- **Literal:** Sorgu içindeki sabit değer: dize (`'...'`), sayı (`42`), boolean (`true`). Normalizasyonda `?`'e çevrilir.

### 6.1 Parmak izi

"Aynı sorgu 10 kereden fazla" gibi analizler, **normalize edilmeden** anlamsızdır. Normalizasyon adımları:

1. Yorumları sil, boşlukları tek boşluğa indir, anahtar kelimeleri küçük harfe çevir.
2. Dize literallerini (`'...'`) ve sayıları `?` ile değiştir.
3. `IN (?, ?, ?)` listelerini `IN (?)` yap.
4. Sonucu 64-bit hash'le (FNV-1a yeterli); hash + örnek orijinal sorgu saklanır.

Bu modül aynı zamanda **maskeleme** işlevi görür: rapora literal değerler yerine normalize sorgu yazılır, böylece parola ve kişisel veri raporlara sızmaz.

### 6.2 Lexer kapsamı (net sınır)

`sql_lexer` bir **tokenizer**'dır, tam parser değildir. Destekledikleri:

- Dize literalleri (`'...'`, `E'...'`, `$$...$$`), yorumlar (`--`, `/* */`), tanımlayıcılar, sayılar, operatörler.
- İfade sınırı tespiti (dize/yorum dışındaki `;`) → stacked query kuralı (S3).
- `SELECT/INSERT/UPDATE/DELETE/COPY/CREATE/ALTER/DROP/TRUNCATE` anahtar kelimeleriyle ifade tipi.
- `FROM/JOIN/INTO/UPDATE` sonrası ilk tanımlayıcıyı tablo adı olarak çıkarma (şema nitelemesi `public.tablo` desteklenir).

Desteklemedikleri (bilinçli sınır): CTE iç gövdesi, alt sorgu içindeki tablo adları, `RETURNING`, karmaşık `JOIN ON` ifadeleri, PostgreSQL'e özgü sözdizimi (ör. `LATERAL`, `WINDOW`). Bu sınırlar `KURALLAR.md`'de listelenir; "yeterince iyi" ölçütü budur.

---

## 7. Güvenlik Modülü

### 7.1 Eski kuralların sorunları (düzeltildi)

| Eski kural | Sorun | Yeni durum |
| --- | --- | --- |
| `;` tek başına | Her SQL ifadesi `;` ile biter → sürekli yanlış alarm | S3: lexer ile dize dışı `;` sonrası ikinci ifade |
| `--` tek başına | Meşru yorumlar çok yaygın | S7: yalnızca `'...' --` kombinasyonu |
| `[;|`$()]`` | Parantez neredeyse her sorguda var | Kaldırıldı |
| `<script`, `../` | SQL logunda enjeksiyon belirtisi değil | Kapsam dışı (uygulama katmanı) |
| `\s` | POSIX ERE'de standart değil | `[[:space:]]` + `REG_ICASE|REG_EXTENDED` |
| Hepsi aynı ağırlıkta | Rapor gürültü dolu | Önem dereceleri (Kritik/Yüksek/Orta/Düşük) |

### 7.2 Gözden geçirilmiş kural seti (PostgreSQL'e özgü, önem dereceli)

| ID | Kural | Yöntem | Önem |
| --- | --- | --- | --- |
| S1 | Totoloji (`OR 1=1`) | Önce regex: `or[[:space:]]+'?1'?[[:space:]]*=[[:space:]]*'?1'?`; lexer varsa "iki taraf eşit mi" genel hâli | Yüksek |
| S2 | UNION tabanlı | `union([[:space:]]+all)?[[:space:]]+select` ; `select[[:space:]]+null` ile birlikteyse | Orta → Yüksek |
| S3 | Yığılmış sorgu (stacked) | Lexer ile: dize içi olmayan `;` sonrası ikinci ifade | Yüksek |
| S4 | Zaman tabanlı | `pg_sleep[[:space:]]*\(` | Yüksek |
| S5 | Dosya/komut erişimi | `copy.*(to|from)[[:space:]]+program`, `pg_read_file`, `pg_ls_dir`, `lo_import`, `lo_export` | Kritik |
| S6 | Katalog keşfi | `information_schema`, `pg_shadow`, `pg_authid`, `pg_user` (yetkisiz kullanıcıdan) | Orta |
| S7 | Tırnak + yorum kesmesi | `'[[:space:]]*--` | Yüksek |
| S8 | Yıkıcı DDL | `drop[[:space:]]+(table|database|schema)`, `truncate` — **izin listesi** dışında | Politikaya bağlı |
| S9 | Parola içeren komut | `(create|alter)[[:space:]]+(user|role).*password` | Gizlilik uyarısı |
| B1 | Kaba kuvvet | Aynı IP/kullanıcıdan **kayan 60 sn pencerede** ≥ 5 `FATAL: password authentication failed` | Yüksek |
| B2 | Hata taraması | Aynı kaynaktan kısa sürede çok sayıda `syntax error` / `does not exist` (kayan 60 sn pencerede ≥ 10) | Orta |
| B3 | Beklenmedik kaynak | Öğrenme penceresi (varsayılan: ilk 10 dakika veya ilk 1000 kayıt, hangisi önce dolarsa) sonrası yeni IP/kullanıcı/uygulama kombinasyonu | Düşük |

**S1 uygulaması netleştirildi:** Regex her zaman çalışır (hızlı ön filtre); lexer varsa genel totoloji kontrolü ek olarak yapılır ve bulgu `S1-genel` olarak raporlanır. İkisi birbirini dışlamaz; aynı kayıtta ikisi de tetiklenirse tek bulgu olarak birleştirilir (en yüksek önem korunur).

**B1/B2 penceresi netleştirildi:** Kayan pencere; her yeni olayda pencereden düşen kayıtlar temizlenir. Sabit dakika değildir.

### 7.3 İlkeler

- Kurallar `querithex.conf` içindeki `[kurallar]` bölümünden veya ayrı kural dosyasından yüklenir; her kural: `id, pattern, severity, description, enabled`.
- Regex'ler **ön-derlenir** (`regcomp` başlangıçta bir kez), satır başına derleme yapılmaz.
- Her kural için en az bir **pozitif** ve bir **negatif** (meşru sorgu) test örneği zorunludur.
- Tespit "saldırı kanıtı" değil "şüpheli desen" olarak raporlanır; skor + gerekçe verilir.
- Aranan metin, maskelenmemiş orijinal sorgudur; rapora yalnızca maskelenmiş hâli yazılır.
- Aracın kendisi de saldırı yüzeyidir: log içeriği **güvenilmez girdi**dir. Parser sınır kontrollü yazılır (`strcpy`/`sprintf` yok) ve HTML raporda tüm içerik **escape** edilir.

### 7.4 Kural dosyası örneği

```
[kurallar]
S1 = on
S2 = on
S3 = on
S4 = on
S5 = on
S6 = on
S7 = on
S8 = on
S9 = on
B1 = on
B2 = on
B3 = on

[kural:S1]
pattern = or[[:space:]]+'?1'?[[:space:]]*=[[:space:]]*'?1'?
severity = high
description = Totoloji tabanlı enjeksiyon denemesi
enabled = true
```

---

## 8. Performans Modülü

### 8.1 Yaklaşım düzeltmeleri

- **"`SELECT *` = tam tablo taraması" yanlıştır.** Tarama türü sorgu metninden değil plandan anlaşılır. `SELECT *` yalnızca bir *stil uyarısı*dır (gereksiz sütun transferi).
- **İndeks eksikliği tek başına logdan çıkarılamaz; Querithex üç kaynağı birleştirerek çıkarır** (bkz. K1, K2, K3):
  - (a) libpq ile alınan `EXPLAIN (ANALYZE, FORMAT JSON)` planı — yalnızca `SELECT`; `INSERT/UPDATE/DELETE` için ANALYZE'sız `EXPLAIN` (sorguyu çalıştırmaz).
  - (b) libpq ile çekilen **Şema modeli** — ilgili sütunda index var mı, tipi ne (bkz. 4.0).
  - (c) sorgu metni — ifade uyuşmazlığı, tür uyuşmazlığı, fonksiyon sarmalama var mı.
  - Üçü birlikte "index var ama kullanılmıyor", "istatistik sapması", "ifade uyuşmazlığı" gibi **deterministik** bulgular üretir. `auto_explain` logları varsa ek sinyal olarak okunur; yoksa da (a)+(b)+(c) yeterlidir.
- **`EXPLAIN ANALYZE` güvenlik kuralı (K2 + K6):** Yalnızca `--test-conninfo` (test container) üzerinde çalışır. `SELECT` için serbesttir (sorguyu bir kez daha çalıştırır; yavaşsa yük bindirir, `statement_timeout` ile sınırlanır). `INSERT/UPDATE/DELETE` için test container'ında bile **asla** çalıştırılmaz — sorguyu gerçekten yürütür, veriyi değiştirir; DML için yalnızca ANALYZE'sız `EXPLAIN` kullanılır. **`--main-conninfo` (ana container) üzerinde hiçbir `EXPLAIN` çalıştırılmaz**; ana DB'den yalnızca şema/log okunur.
- **Düz `EXPLAIN` (ANALYZE'sız) güvenli:** Sorguyu **çalıştırmaz**, yalnızca planı gösterir; bu nedenle test container'ında DML için de güvenlidir. "Ana DB'de hiç EXPLAIN yok" kuralı yalnızca ana bağlantı içindir; test bağlantısında DML'e düz `EXPLAIN` serbesttir.
- **Tavsiye deterministiktir (K3):** "Şu index'i ekle", "sorguyu şu aralık olarak yaz", "`ANALYZE` çalıştır" gibi öneriler kural motorundan gelir; AI'ya bağımlı değildir. AI (K4) yalnızca *ek* "tahmini etki" ve "serbest yorum" verir; tavsiyenin kendisini değiştirmez.
- Sabit 100 ms eşiği yerine **yapılandırılabilir eşik + yüzdelikler** (p50/p95/p99).

### 8.2 Metrikler

| Metrik | Varsayılan | Not |
| --- | --- | --- |
| Yavaş sorgu | ≥ 100 ms (`slow_threshold_ms`) | Bölüm 3'teki `log_min_duration_statement` ile eşitlenmeli |
| Çok yavaş sorgu | ≥ 1000 ms (`very_slow_threshold_ms`) |  |
| En maliyetli sorgular | Parmak izine göre toplam süre, çağrı sayısı, ortalama, p95 | Top-N, varsayılan N=20 (`top_n` ile değiştirilebilir) |
| N+1 deseni | Aynı oturumda aynı parmak izi kısa sürede çok kez (kayan 5 sn pencerede ≥ 10) | Tekrarlayan sorgu tespitinin doğru hâli |
| Geçici dosya | `log_temp_files` satırları | Bellek/`work_mem` sorunu |
| Plan sinyali | `auto_explain` içinde `Seq Scan` + büyük satır sayısı | Sadece mevcutsa |

Süre dağılımı için **log ölçekli sabit boyutlu histogram** kullanılır. Kova sınırları (ms): `0-1, 1-10, 10-100, 100-1000, 1000-10000, 10000+`. Kova sayısı 6'dır; her kova `uint64_t` sayaç tutar (6 × 8 = 48 bayt **toplam**, global histogram). Histogram **kayıt başına değil, global** bir yapıdır; D6'daki ~200 bayt/kayıt bütçesine **dahil değildir**. 100.000 kayıt için histogram maliyeti sabit 48 bayttır.

**Parmak izi tablosu bellek bütçesi (Bölüm 15 kriter 4 ile uyumlu):** Kayıt başına bütçe ~200 bayt: hash (8) + sayaçlar (~24) + örnek sorgu (üst sınır **256 bayt**, aşanlar kesilir) + hash tablosu overhead (~%50). 100.000 kayıt × 200 bayt ≈ 20 MB; %50 overhead ile ~30 MB — 64 MB sınırının altında. Örnek sorgu üst sınırı aşılırsa sorgu **kesilir** ve `truncated` işareti taşır; bu, belleğin log boyutundan bağımsız kalmasını sağlar.

---

## 9. Kullanılabilirlik ve Stabilizasyon (Olaylar Modülü)

Bu modülün çıktıları **Incident** türü Bulgu'lardır (bkz. 4.0).

### 9.1 Tespit edilecek olaylar

| Olay | Log belirtisi |
| --- | --- |
| Kimlik doğrulama hatası | `FATAL: password authentication failed` |
| Bağlantı limiti | `FATAL: too many connections` / `remaining connection slots` |
| Deadlock | `ERROR: deadlock detected` |
| Kilit beklemesi | `still waiting for ... Lock` (`log_lock_waits`) |
| Sorgu iptali | `canceling statement due to statement timeout` |
| Checkpoint sıklığı | `checkpoints are occurring too frequently` |
| Geçici dosya | `temporary file: path ..., size ...` |
| Çökme / OOM | `terminated by signal 9`, `PANIC`, `crash recovery`, `database system is shut down` |
| Yanlış obje | `relation ... does not exist`, `syntax error at or near` |

### 9.2 Hata oranı tanımı

"Toplam sorgu" çoğu yapılandırmada logda yoktur (`log_min_duration_statement=100` yalnızca yavaş sorguları yazar). Bu nedenle:

- `log_statement` açıksa: `ERROR+FATAL / toplam statement` oranı raporlanır.
- Değilse: **dakikada hata sayısı** ve **önceki döneme göre artış** raporlanır; oran hesabı yalnızca `pg_stat_database` ile Faz 4'te yapılır.

Hangi yöntemin kullanıldığı raporda açıkça belirtilir (ör. `"error_rate_method": "count_only"`).

**Faz ayrımı:** libpq bağlantısı Faz 2'de **şema (K1) ve plan (K2)** için kullanılır. `pg_stat_database` ile hata oranı hesabı ise bilinçli olarak Faz 4'e bırakılmıştır; çünkü bu, performans değil **kullanılabilirlik** metriğidir ve `log_statement` açık değilse logdan payda çıkmaz. İki kullanım farklı fazlardadır; karıştırılmamalıdır.

---

## 10. Rapor Modülü

- **Biçimler:** metin özet (varsayılan), JSON (makine), CSV (tablolar), HTML (tek dosya, statik).
- **İçerik:** özet sayaçlar, en yavaş/en sık sorgular, güvenlik bulguları (önem sırasıyla), olay zaman çizelgesi, kullanıcı/uygulama/IP dağılımı.
- **Bulgu kaynağı (K5):** Her bulgu `source: "rule"` veya `source: "ai"` etiketi taşır. Deterministik kural bulguları `rule`; AI yorumları `ai` olarak işaretlenir. AI kapalıyken `advisory` alanı hiç eklenmez; rapor tamamen deterministik kalır.
- **Advisory alanı (K4, K5):** AI açıksa, deterministik bir bulguya **ek** olarak `advisory` alanı eklenir: `provider`, `model`, `estimated_impact` ("~%40 hızlanma (tahmini)"), `comment` (serbest yorum). `advisory` alanı **karar değil, yorumdur**; bulgunun kendisini (rule_id, severity, evidence) değiştirmez. AI başarısız olursa (ağ yok, anahtar yok) alan eklenmez, rapor deterministik kısımla devam eder.
- **Maskeleme varsayılan açık:** literaller `?`'e çevrilir; `--no-mask` ile kapatılır. AI'ya gönderilen özet **her zaman maskelenmiştir** (literal, kullanıcı, IP, tablo/sütun adları yok).
- **Dosya izinleri:** Tüm rapor çıktıları (metin, JSON, CSV, HTML) varsayılan olarak `0600` izniyle yazılır; `--umask` ile değiştirilebilir.
- **Şifreleme (AES-256-GCM, Faz 5):** Logun kendisini şifrelemek yerine, *isteğe bağlı olarak rapor dosyasını* şifrelemek mantıklıdır. Anahtar yönetimi (parola → `PBKDF2/scrypt` ile anahtar, her dosyada rastgele nonce, asla nonce tekrarı yok) Faz 5'e alınmıştır. Faz 5'te yapılacak iş: rapor şifreleme.
- **Web dashboard:** C'de HTTP sunucusu yazmak yerine statik HTML rapor (içine gömülü JS/CSS) önerilir; sunucu yüzeyi ve güvenlik riski azalır.

**Örnek JSON bulgu (AI kapalı):**

```json
{
  "rule_id": "P-042",
  "severity": "high",
  "source": "rule",
  "evidence": { "node_type": "Seq Scan", "actual_time_ms": 2400, "index": "orders_created_at_idx" },
  "advice": "İfade uyuşmazlığı: created_at::date vs created_at. Sorguyu aralık olarak yaz veya expression index ekle."
}
```

**Örnek JSON bulgu (AI açık):**

```json
{
  "rule_id": "P-042",
  "severity": "high",
  "source": "rule",
  "evidence": { "node_type": "Seq Scan", "actual_time_ms": 2400, "index": "orders_created_at_idx" },
  "advice": "İfade uyuşmazlığı: created_at::date vs created_at. Sorguyu aralık olarak yaz veya expression index ekle.",
  "advisory": {
    "source": "ai",
    "provider": "openai",
    "model": "gpt-4o",
    "estimated_impact": "~%40 hızlanma (tahmini)",
    "comment": "Bu sorgu N+1 deseninin parçası görünüyor; JOIN'e çevirmek daha etkili olabilir."
  }
}
```

---

## 11. Proje Yapısı (karar verilmiş)

**Karar:** Tüm kaynak dosya adları **İngilizce** olacaktır. Dokümanlar Türkçe kalır. Gerekçe: GitHub'da yayın, CI ve bazı dosya sistemlerinde tutarlılık.

```
querithex/
├── README.md  LICENSE  Makefile  .clang-format  .gitignore
├── querithex.conf            → varsayılan config (Bölüm 12.1)
├── src/
│   ├── main.c                → giriş, mod seçimi (batch/follow)
│   ├── cli.c/h               → argüman ayrıştırma
│   ├── config.c/h            → querithex.conf (INI) tek okuyucu;
│   │                           CLI > env > config > varsayılan önceliği
│   ├── reader.c/h            → dosya/stdin/follow, rotasyon takibi
│   ├── log_parser.c/h        → kayıt bazlı ayrıştırma → LogEvent
│   ├── sql_lexer.c/h         → tokenizer (dize/yorum/ifade sınırları)
│   ├── fingerprint.c/h       → normalize + hash + maskeleme
│   ├── schema_introspect.c/h → K1/K6: libpq ile main DB'den SALT OKUMA
│   │                           ile şema çek; test DB'ye kopyala (--sync-test)
│   ├── explain_collect.c/h   → K2/K6: yalnızca test DB'de EXPLAIN
│   │                           (ANALYZE, FORMAT JSON); ana DB'de EXPLAIN yok
│   ├── security.c/h          → kural motoru + davranış kuralları
│   ├── performance.c/h       → eşikler, histogram, N+1; şema modelini kullanır
│   ├── incidents.c/h         → deadlock, auth hatası, çökme, checkpoint
│   ├── stats.c/h             → toplayıcı (hash tablosu, top-N)
│   ├── ai_advisor.c/h        → K4: opsiyonel advisory (tahmini etki + yorum);
│   │                           provider soyutlaması (none/openai/ollama/custom)
│   ├── report.c/h            → ortak arayüz
│   ├── report_json.c  report_csv.c  report_html.c
│   ├── signal.c/h            → SIGINT/SIGTERM/SIGHUP, temiz kapanış
│   └── core/
│       ├── buffer.c/h        → dinamik buffer
│       ├── array.c/h         → dinamik dizi
│       ├── hashmap.c/h       → hash tablosu
│       └── arena.c/h         → arena ayırıcı (isteğe bağlı)
├── tests/   (test_*.c, fixtures/, golden/, run.sh,
│            test_pipeline.sh, test_no_write.sh, test_config.sh)
├── fuzz/    (harness + corpus)
├── ornek/   (üretilmiş örnek loglar)
└── docs/    KULLANIM.md  MIMARI.md  KURALLAR.md
```

**Not:** Dosya adlarında Türkçe karakter kullanılmaz (`MİMARİ.md` → `MIMARI.md`). Config dosyası (`querithex.conf`) proje kökünde durur; örnek dosya `docs/` altında da kopyalanabilir.

---

## 12. CLI Arayüzü

| Bayrak | Açıklama |
| --- | --- |
| `-i, --input <yol>` | Girdi log dosyası (veya `-` ile stdin) |
| `-o, --output <yol>` | Rapor çıktı dosyası (varsayılan: stdout) |
| `-f, --format <tür>` | `text` (varsayılan), `json`, `csv`, `html` |
| `-F, --follow` | Follow modu (tail benzeri) |
| `-c, --config <yol>` | Config dosyası (INI). Varsayılan: `./querithex.conf`. Aynı formatta başka bir dosya gösterilebilir; farklı format (JSON/YAML/env) desteklenmez. Öncelik: CLI > env > config > varsayılan. |
| `-t, --threshold <ms>` | Yavaş sorgu eşiği (varsayılan: 100) |
| `-n, --top <N>` | Top-N listesi boyutu (varsayılan: 20) |
| `--no-mask` | Maskelemeyi kapat |
| `--umask <maske>` | Rapor çıktı dosyalarının iznini ayarlar (varsayılan `0600`; ör. `--umask 0640`) |
| `--main-conninfo <dize>` | Ana veritabanı (`main_database`) bağlantısı — **salt okunur** (K6). Şema/tablo/index/log okumak için; yazma yok |
| `--test-conninfo <dize>` | Test veritabanı (`test_database`) bağlantısı — tüm `EXPLAIN ANALYZE` ve kural testleri burada (K6) |
| `--sync-test` | Ana container'dan şema/veriyi test container'ına kopyala (K6). Yalnızca `pg_dump`/`pg_restore` ile; ana DB'de yazma yok |
| `--introspect` | Şemayı `--main-conninfo` üzerinden çek (K1, K6) |
| `--ai <provider>` | AI'yı açar ve provider'ı set eder: `openai`, `ollama`, `custom`. Verilmezse config'teki `[ai]` bölümü geçerlidir (`enabled`, `provider`). AI'yı kapatmak için `--ai none` veya config'te `enabled = false`. (K4) |
| `--ai-model <ad>` | Provider'a özgü model adı (ör. `gpt-4o`, `llama3`) |
| `--ai-endpoint <url>` | `custom` provider için endpoint URL |
| `-v, --verbose` | Ayrıntılı çıktı (birden çok kez artırılabilir) |
| `-q, --quiet` | Yalnızca hata mesajları |
| `-h, --help` | Yardım |
| `--version` | Sürüm bilgisi |

**Örnek kullanımlar:**

```
# En temel: yalnızca log analizi, DB'ye bağlanmadan
querithex -i postgresql.log -f json

# Şema ana DB'den okunur, test DB'ye kopyalanır, EXPLAIN ANALYZE test'te
querithex -i postgresql.log \
  --main-conninfo "host=main_database dbname=app user=ro_user" \
  --test-conninfo "host=test_database dbname=app_test user=test_user" \
  --sync-test --introspect -f json

# AI'lı (advisory), aynı güvenlik modeli
querithex -i postgresql.log \
  --main-conninfo "..." --test-conninfo "..." --sync-test --introspect \
  --ai openai --ai-model gpt-4o -f html
```

**Güvenlik kuralları (K6):**
- `--main-conninfo` ile bağlanan kullanıcı **salt okunur** olmalı (`GRANT SELECT` yeterli; `INSERT/UPDATE/DELETE/CREATE` yok). Querithex bağlantıda `SET default_transaction_read_only = on;` çalıştırır — yanlışlıkla yazma imkânsız hale gelir.
- `EXPLAIN ANALYZE` **yalnızca `--test-conninfo`** üzerinde çalışır. `--main-conninfo` üzerinde hiçbir `EXPLAIN` (ANALYZE'lı veya değil) çalıştırılmaz; ana DB'den yalnızca şema/log okunur.
- `--sync-test` ana DB'den **okur** (`pg_dump`), test DB'ye **yazar** (`pg_restore`). Ana DB'de yazma yoktur.
- `--sync-test` için `ro_user`'ın tüm tablolarda `SELECT`, sequence'lerde `USAGE, SELECT` yetkisi olmalı; ayrıntı `KULLANIM.md`'de.
- `--main-conninfo` / `--test-conninfo` verilirse, config'teki ilgili `main_*` / `test_*` alanları **tamamen yok sayılır** (conninfo tek kaynak). Conninfo ile config alanları **alan alan birleştirilmez**; biri diğerini geçersiz kılar. Ayrıntı `KULLANIM.md`'de.
- Parolalar `PGPASSWORD` / `PGTESTPASSWORD` env'inden okunur; komut satırına yazılmaz.
- `--ai` varsayılan `none`; AI'ya gönderilen özet her zaman maskelenmiştir (Bölüm 10).
- AI başarısız olursa rapor deterministik kısımla devam eder; çıkış kodu 0'dır (AI opsiyonel olduğu için hata sayılmaz).

---

## 12.1 Config Dosyası Şeması (`querithex.conf`)

Tek dosya, dört sabit bölüm + dinamik kural bölümleri. Parola/API anahtarı **yok** (env'den okunur). CLI'ye ait alanlar (`-i`, `-o`, `-f`, `-F`, `-v`, `-q`) burada **değil**.

### `[baglanti]` — Docker container + veritabanı erişimi (K6)

| Alan | Açıklama | Zorunlu? |
| --- | --- | --- |
| `main_host` | Main Docker container adı (network'te çözümlenir) | `--introspect`/plan için evet |
| `main_port` | Main container PostgreSQL portu | Hayır (varsayılan 5432) |
| `main_dbname` | Main container içindeki veritabanı adı | Evet (main kullanılacaksa) |
| `main_user` | Main veritabanı kullanıcısı (**salt okunur**, K6) | Evet (main kullanılacaksa) |
| `test_host` | Test Docker container adı | `--sync-test`/`EXPLAIN ANALYZE` için evet |
| `test_port` | Test container PostgreSQL portu | Hayır (varsayılan 5432) |
| `test_dbname` | Test container içindeki veritabanı adı | Evet (test kullanılacaksa) |
| `test_user` | Test veritabanı kullanıcısı | Evet (test kullanılacaksa) |

Parola alanı yok: `main_password` → `PGPASSWORD` env; `test_password` → `PGTESTPASSWORD` env.

### `[genel]` — Analiz ayarları

| Alan | Açıklama | Varsayılan |
| --- | --- | --- |
| `slow_threshold_ms` | Yavaş sorgu eşiği (Bölüm 8.2) | 100 |
| `very_slow_threshold_ms` | Çok yavaş sorgu eşiği | 1000 |
| `top_n` | En maliyetli sorgu listesi boyutu | 20 |
| `max_line_bytes` | Satır başına üst sınır (Bölüm 5) | 1048576 |
| `follow_summary_interval_s` | Follow modunda özet aralığı (sn) | 60 |

### `[ai]` — AI advisory katmanı (K4)

| Alan | Açıklama | Varsayılan |
| --- | --- | --- |
| `enabled` | AI açık/kapalı | `false` |
| `provider` | `none` / `openai` / `ollama` / `custom` | `none` |
| `model` | Provider'a özgü model adı | boş |
| `endpoint` | `custom` provider için URL | boş |

`api_key` alanı yok: `QUERITHEX_AI_KEY` env'den okunur. `enabled = false` ise AI hiç çağrılmaz; rapor deterministik kısımla tam kalır (K5).

### `[kurallar]` — Güvenlik kural seti (Bölüm 7.4)

| Alan | Açıklama | Varsayılan |
| --- | --- | --- |
| `kural_dosyasi` | Harici kural dosyası yolu (boşsa gömülü set) | boş |
| `S1` … `S9` | Enjeksiyon kuralları açık/kapalı (9 alan) | `on` |
| `B1` … `B3` | Davranış kuralları açık/kapalı (3 alan) | `on` |

**Dinamik bölümler:** Sabit dört bölümün (`[baglanti]`, `[genel]`, `[ai]`, `[kurallar]`) yanında `[kural:<id>]` biçiminde **dinamik bölümler** de okunur (ör. `[kural:S1]`, `[kural:B1]`). Her dinamik bölüm o kuralın `pattern`, `severity`, `description`, `enabled` alanlarını taşır (bkz. 7.4). INI okuyucu, bilinmeyen `[kural:*]` bölümlerini de kabul eder; bilinmeyen **sabit** bölümler (`[bilinmeyen]`) ise yok sayılır ve `--verbose` ile uyarı verilir.

### Örnek `querithex.conf`

```ini
# querithex.conf — Querithex yapılandırması (INI)
# Parolalar burada DEĞİL: PGPASSWORD / PGTESTPASSWORD / QUERITHEX_AI_KEY env'den okunur.

[baglanti]
main_host   = main_database
main_port   = 5432
main_dbname = app
main_user   = ro_user

test_host   = test_database
test_port   = 5432
test_dbname = app_test
test_user   = test_user

[genel]
slow_threshold_ms         = 100
very_slow_threshold_ms    = 1000
top_n                     = 20
max_line_bytes            = 1048576
follow_summary_interval_s = 60

[ai]
enabled  = false
provider = none
model    =
endpoint =

[kurallar]
kural_dosyasi =
S1 = on
S2 = on
S3 = on
S4 = on
S5 = on
S6 = on
S7 = on
S8 = on
S9 = on
B1 = on
B2 = on
B3 = on

[kural:S1]
pattern = or[[:space:]]+'?1'?[[:space:]]*=[[:space:]]*'?1'?
severity = high
description = Totoloji tabanlı enjeksiyon denemesi
enabled = true
```

### Öncelik kuralı

CLI bayrağı > ortam değişkeni > `querithex.conf` > varsayılan. Aynı ayar birden çok yerde varsa soldaki kazanır.

---

## 13. Geliştirme Planı

Test ve doğrulama her modülle birlikte yazılır. Süreler yarı zamanlı çalışma için tahmindir. Her fazın sonunda Bölüm 15'teki ilgili kriterler doğrulanır; faz geçişi bu doğrulamaya bağlıdır.

### Faz 0 — Hazırlık (1–2 gün)

- [ ] Git deposu, `.gitignore`, `.clang-format`
- [ ] `Makefile`: `-std=c11 -Wall -Wextra -Wpedantic -Werror`, debug hedefi `-fsanitize=address,undefined`
- [ ] PostgreSQL kurulumu (Docker olabilir) + log ayarları (bölüm 3)
- [ ] **Docker ortamı (K6):** `main_database` ve `test_database` container'ları; `--sync-test` için `pg_dump`/`pg_restore` testi; `ro_user`/`test_user` yetkileri (KULLANIM.md)
- [ ] Örnek log üretimi: `pgbench`, elle yazılmış hata/saldırı/deadlock senaryoları
- [ ] Test iskeleti + CI (GitHub Actions: build + test + cppcheck)
- [ ] **Doğrulama:** CI ilk commit'te yeşil; `pg_dump` → `pg_restore` ana DB'de yazma olmadan çalışıyor.

### Faz 1 — Çekirdek / MVP (1,5–2 hafta)

- [ ] `core/buffer`, `core/array`, `core/hashmap` (+ birim testleri)
- [ ] `reader` (`getline`, satır üst sınırı) ve `log_parser` (çok satırlı kayıt, prefix, duration/statement eşleştirme)
- [ ] `fingerprint` (normalize + hash + maskeleme)
- [ ] `config` (INI; `querithex.conf` şeması, Bölüm 12.1). **Faz 1'de:** dinamik `[kural:<id>]` bölümlerini **ayrıştır ve bellekte sakla**. **Faz 2'de:** `security` modülü bu saklanan kuralları **kullanır**. Faz 1'de kural **çalıştırılmaz**.
- [ ] `stats` + basit metin özet raporu
- [ ] `cli`: `-i -o -f -F -c -t -n -v -q -h --version`
- [ ] **Doğrulama:** Bölüm 15 kriter 1 (ayrıştırma doğruluğu), kriter 4 (bellek), kriter 9 (config önceliği).

* **Çıktı:** `querithex -i ornek/postgresql.log` → en sık/en yavaş sorgular listesi.

### Faz 2 — Analiz (2 hafta)

- [ ] `sql_lexer` (tablo adı, ifade tipi, stacked query)
- [ ] `security`: kural motoru, önem dereceleri, davranış kuralları (B1–B3)
- [ ] `performance`: histogram, yüzdelikler, top-N, N+1
- [ ] `incidents`: bölüm 9 tablosu
- [ ] `report_json`, `report_csv`, `report_html` (escape'li)
- [ ] Etiketli test külliyatı (zararlı + meşru örnekler)
- [ ] `schema_introspect` (K1/K6): main DB'den salt okuma; `--sync-test`
- [ ] `explain_collect` (K2/K6): yalnızca test DB'de `EXPLAIN (ANALYZE, FORMAT JSON)`; ana DB'de EXPLAIN yok; DML için düz `EXPLAIN`
- [ ] Kural tabanlı deterministik tavsiye (K3): plan + şema + sorgu metni → bulgu
- [ ] **Doğrulama:** Bölüm 15 kriter 2 (güvenlik tespiti), kriter 7 (ana DB'de sıfır yazma), kriter 8 (AI kapalıyken rapor tam).

### Faz 3 — Dayanıklılık ve Kalite (1,5 hafta)

- [ ] `signal`: temiz kapanış, `SIGHUP` ile yapılandırma yeniden yükleme
- [ ] `follow` modu + log rotasyonu
- [ ] Fuzzing (parser + lexer) — AFL++ veya libFuzzer
- [ ] `valgrind`/ASan temiz, `gcov` kapsama, benchmark
- [ ] Dokümantasyon (README, KULLANIM, MIMARI, KURALLAR), paketleme (`make install`, systemd birimi)
- [ ] **Doğrulama:** Bölüm 15 kriter 3 (hız), 5 (bellek güvenliği), 6 (kapsam).

### Faz 4 — Genişleme (isteğe bağlı)

- [ ] SQLite'a sonuç kaydı (geçmiş karşılaştırma, trend)
- [ ] Threading: önce ölçüm yap; darboğaz parse ise dosyayı bloklara bölüp paralel işle
- [ ] libpq modu: `pg_stat_database` ile hata oranı (plan toplama zaten Faz 2'de)
- [ ] `csvlog` / `jsonlog` desteği (JSON parser burada gerekir)

### Faz 5 — Bonus

- [ ] `ai_advisor` (K4): provider soyutlaması (none/openai/ollama/custom); `advisory` alanı
- [ ] Plugin sistemi (`dlopen`) — kural arayüzü sabitlendikten sonra
- [ ] Rapor şifreleme (AES-256-GCM, OpenSSL EVP) — Bölüm 10 ile birebir
- [ ] Statik dashboard geliştirmeleri

**Not:** K4 (AI) Faz 5'e, K1/K2/K6 (şema + plan) Faz 2'ye yerleştirildi; çünkü şema/plan olmadan performans kuralları çalışmaz, AI ise opsiyonel bir katmandır. Faz 2 içinde sıralama: `schema_introspect` → `explain_collect` → `performance` → deterministik tavsiye (K3); çünkü tavsiye plan + şema + sorgu metnine bağlıdır.

---

## 14. Test Stratejisi

| Tür | Ne test eder | Araç |
| --- | --- | --- |
| Birim | Her modül ayrı | `test_*.c`, basit assert çatısı |
| Golden dosya | Bilinen log → beklenen JSON | `tests/golden/` + `diff` |
| Entegrasyon | Modüller birlikte | `run.sh` |
| Uçtan uca | Gerçek PostgreSQL logu (Docker) → rapor | `test_pipeline.sh` |
| Güvenlik kuralları | Pozitif + negatif örnekler | Etiketli külliyat |
| Bellek | Sızıntı, taşma, tanımsız davranış | `valgrind`, ASan/UBSan |
| Fuzzing | Bozuk/uzun/rastgele satırlar | AFL++ / libFuzzer |
| Performans | Satır/sn, tepe bellek | `bench/` betiği + `/usr/bin/time -v` |
| **K6 (ana DB'de sıfır yazma)** | Ana bağlantıda `INSERT/UPDATE/DELETE/DDL` sayısı 0; `default_transaction_read_only=on` | `test_no_write.sh` + PostgreSQL log denetimi |
| **K5 (AI kapalıyken rapor tam)** | `advisory` alanı çıkarılmış rapor = deterministik rapor | `diff` |
| **K7 (config önceliği)** | CLI > env > config > varsayılan | `test_config.sh` |

---

## 15. Başarı Kriterleri (Ölçülebilir Hâle Getirildi)

| # | Kriter | Ölçüm | Doğrulandığı faz |
| --- | --- | --- | --- |
| 1 | Ayrıştırma doğruluğu | Golden külliyatta %100; bozuk satırlarda çökme yok | Faz 1 |
| 2 | Güvenlik tespiti | Etiketli külliyatta (≥ 200 zararlı, ≥ 1000 meşru) recall ≥ %90, yanlış pozitif ≤ %2 | Faz 2 |
| 3 | Hız | Tek iş parçacığında parse + parmak izi ≥ 50.000 satır/sn | Faz 3 |
| 4 | Bellek | Log boyutundan bağımsız; varsayılan ≤ 64 MB (parmak izi tablosu üst sınırı 100.000 kayıt; kayıt başına ~200 bayt, örnek sorgu 256 bayt; aşanlar `diğer` kovasına düşer) | Faz 1 |
| 5 | Bellek güvenliği | `valgrind` ve ASan/UBSan temiz; fuzz çalışmasında çökme yok | Faz 3 |
| 6 | Test kapsamı | Çekirdek modüllerde `gcov` ≥ %80 | Faz 3 |
| 7 | **Ana DB'de sıfır yazma (K6)** | Test ortamında: `--main-conninfo` ile bağlanıp tüm akış çalıştırıldığında ana DB'de `INSERT/UPDATE/DELETE/DDL` sayısı **0**; ana bağlantıda `default_transaction_read_only=on` doğrulanır | Faz 2 |
| 8 | **AI kapalıyken rapor tam (K5)** | `enabled=false` ile üretilen rapor, `enabled=true` ile üretilen raporun `advisory` alanı çıkarılmış haliyle **birebir aynı** deterministik alanları içerir. Karşılaştırma: `rule_id`, `severity`, `evidence` (sıralı anahtarlar), `advice`; float alanlar (`p95`, `actual_time_ms`) **hariç**; sıralama `rule_id`'ye göre. Böylece float yuvarlaması diff'i bozmaz. | Faz 2 |
| 9 | **Config tek kaynak (K7)** | CLI > env > config > varsayılan önceliği test edilir: aynı ayar üç yerde farklıysa CLI kazanır; config yoksa varsayılan; zorunlu bilgi eksikse anlamlı hata (çökme yok) | Faz 1 |

**Referans makine (kriter 3 için):** 4 çekirdek / 8 GB RAM / SSD, Ubuntu 24.04, GCC 13, `-O2`. Ölçüm `bench/` betiğiyle ve `/usr/bin/time -v` ile yapılır; sonuç README'ye yazılır.

**K6 doğrulama notu:** Kriter 7, test container'ında (`test_database`) çalıştırılır; ana container'a (`main_database`) yalnızca salt okunur rolle bağlanılır. Ana DB'de yazma denemesi yapılmaz; `default_transaction_read_only=on` ile yazmanın **imkânsız** olduğu doğrulanır.

---

## 16. Güvenli C Kodlama Kuralları

- Dizeler için sınırlı fonksiyonlar (`snprintf`, `memcpy` + uzunluk); `strcpy`, `sprintf`, `gets` yasak.
- Her `malloc/realloc` dönüşü kontrol edilir; `realloc` sonucu geçici değişkende tutulur.
- Sahiplik kuralı: ayıran serbest bırakır; `*_init` / `*_free` çiftleri.
- Girdi uzunlukları ve tamsayı taşmaları için açık kontrol (`size_t` aritmetiği).
- Kaynak sızıntısına karşı dosya/regex nesneleri (`fclose`, `regfree`) tek çıkış noktasında temizlenir.
- **libpq bağlantıları:** `PQfinish` tek çıkış noktasında; `PQclear` her `PGresult` için; hata durumunda `PQerrorMessage` loglanır, çökme yok.
- **SQL enjeksiyonu (aracın kendisi):** Şema sorguları parametreli (`PQexecParams`) veya sabit metin; kullanıcı girdisi SQL'e string birleştirmeyle **girmez**.
- CI'da: `cppcheck`, `clang-tidy`, `-Werror`, sanitizer'lı test koşusu.

---

## 17. Riskler

| Risk | Etki | Önlem |
| --- | --- | --- |
| Log formatı kurulumlara göre değişir | Parser kırılır | Yapılandırılabilir prefix, `jsonlog`/`csvlog`, kapsamlı örnek külliyat |
| Regex tabanlı tespit atlatılabilir / gürültülü | Yanlış alarm veya kaçırma | Önem dereceleri, lexer tabanlı kurallar, davranış kuralları; "engelleyici" değil "uyarıcı" konumlandırma |
| Loglarda hassas veri | Gizlilik ihlali | Varsayılan maskeleme, çıktı dosya izinleri `0600` |
| Kapsamın büyümesi (HTTP sunucusu, plugin, threading) | Proje bitmez | Faz 1 MVP'ye kadar yeni özellik yok |
| Canlı veritabanında `EXPLAIN`/bağlantı | Üretimi etkileme | Ana bağlantıda hiç `EXPLAIN` yok (yalnızca şema/log okuma, salt okunur rol); `statement_timeout` **yalnızca test bağlantısında** `EXPLAIN ANALYZE` için |
| **Ana veritabanına yanlışlıkla yazma** (K6) | Üretim verisi bozulur | Ana bağlantı salt okunur rol + `default_transaction_read_only=on`; `EXPLAIN ANALYZE` yalnızca test container'ında; ana DB'de hiç `EXPLAIN` yok |
| **AI'ya veri gönderme** (K4) | Gizlilik ihlali (log/şema sızıntısı) | AI varsayılan kapalı; gönderilen özet her zaman maskelenmiş (literal, kullanıcı, IP, tablo/sütun adı yok); `--ai` açıkça istenmeden çağrı yok |

---

## 18. Öğrenilecek Konular

| Konu | Nerede | PostgreSQL tarafındaki kazanım |
| --- | --- | --- |
| C11, bellek yönetimi | Tüm proje | — |
| Log yapılandırması ve formatı | `log_parser`, bölüm 3 | `log_*` parametreleri, `csvlog/jsonlog` |
| SQL lexing, parmak izi | `sql_lexer`, `fingerprint` | `pg_stat_statements` mantığı (queryid) |
| POSIX regex | `security` | SQL injection teknikleri |
| Plan okuma | `explain_collect`, `performance` | `EXPLAIN`, `auto_explain`, indeks stratejisi |
| Kilitler, checkpoint, WAL | `incidents` | Deadlock, `log_lock_waits`, checkpoint ayarı |
| Hash tablosu, histogram | `core/`, `stats` | — |
| libpq, katalog sorguları | `schema_introspect` | `information_schema`, `pg_catalog` |
| libpq, SQLite, pthread, OpenSSL | Faz 4–5 | — |
| Git, CI, fuzzing | Faz 0, 3 | — |

---

## 19. Notlar

- **Sıfırdan yazılacak** — eski pipeline kodu kullanılmayacak.
- **Modüler tasarım** — her modül bağımsız test edilebilir.
- **Açık kaynak** — GitHub'da yayınlanacak (lisans seçilmeli: MIT/Apache-2.0/GPL).
- **Dokümantasyon** — her modül için Doxygen yorumları.

---

## 20. Referanslar

- PostgreSQL log yapılandırması: https://www.postgresql.org/docs/current/runtime-config-logging.html
- `auto_explain`: https://www.postgresql.org/docs/current/auto-explain.html
- `pg_stat_statements`: https://www.postgresql.org/docs/current/pgstatstatements.html
- pgBadger (benzer araç): https://github.com/darold/pgbadger
- POSIX regex: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/regex.h.html
- OpenSSL EVP: https://www.openssl.org/docs/manmaster/man3/EVP_EncryptInit.html
- libpq: https://www.postgresql.org/docs/current/libpq.html
- C11: https://en.cppreference.com/w/c

---

## Ek A — Eski Dökümana Göre Değişiklik Günlüğü

| # | Eski durum | Sorun | Yeni durum | İlgili bölüm |
| --- | --- | --- | --- | --- |
| 1 | "26 adım" başlığı, tabloda 24 madde | Tutarsızlık | Faz bazlı plan | 13 |
| 2 | Tek satırlık `duration: ... statement:` varsayımı | `log_statement` ile satırlar ayrı gelir; çok satırlı kayıtlar var | PID eşleştirme + kayıt bazlı parser | 3, 5 |
| 3 | `user@db` sabit prefix | `log_line_prefix` varsayılanı farklı, `%q` kısıtı | Yapılandırılabilir prefix / jsonlog | 3, 5 |
| 4 | `;`, `--`, `[;|`$()]`` kuralları | Her sorguda tetiklenir | Önem dereceli, PostgreSQL'e özgü kural seti | 7 |
| 5 | `\s` regex | POSIX ERE'de standart değil | `[[:space:]]` | 7 |
| 6 | `SELECT *` = tam tablo taraması | Yanlış; plan gerekir | `auto_explain` / `EXPLAIN` ile plan sinyali | 8 |
| 7 | "Aynı sorgu > 10 kez" | Literal farkı yüzünden eşleşmez | Parmak izi modülü | 6 |
| 8 | "Hata oranı > %5" | Payda logda yok | Dakika başı hata + koşullu oran | 9.2 |
| 9 | "Gerçek zamanlı" amacı, sadece batch mimari | Çelişki | `follow` modu + rotasyon takibi | 4.1 |
| 10 | `array` ve `buffer` var, hash tablosu yok | Sayaçlar yazılamaz | `core/hashmap` eklendi | 11 |
| 11 | Git/test/CI sonda | Geç kalan doğrulama | Faz 0'a taşındı, testler modülle birlikte | 13 |
| 12 | AES log şifreleme (adım 7), threading (13) erken | Önce çekirdek gerekir; anahtar yönetimi belirsiz | Faz 4–5, rapor şifreleme olarak yeniden tanımlandı | 10, 13 |
| 13 | JSON parser (adım 18) raporlardan sonra | Bağımlılık sırası ters | Config INI; JSON yalnızca `jsonlog` için Faz 4'te | 13 |
| 14 | "Network soketleri = libpq" | Farklı kavramlar | "libpq modu" | 8, 13 |
| 15 | HTTP sunucusu / dashboard | Büyük saldırı yüzeyi, gereksiz iş | Statik HTML rapor | 10 |
| 16 | Maskeleme yok | Loglardaki parola/kişisel veri raporlara sızar | Varsayılan maskeleme | 6, 10 |
| 17 | Başarı kriterleri "%100", ">%95" | Ölçülemez | Etiketli külliyat + sayısal hedefler + referans makine | 15 |
| 18 | Süre tahminleri saat cinsinden | Gerçekçi değil | Hafta bazlı faz planı | 13 |
| 19 | `MİMARİ.md` | Türkçe karakterli dosya adı | `MIMARI.md` | 11 |
| 20 | Aracın kendi güvenliği yok | Log güvenilmez girdi; HTML'de XSS riski | Güvenli C kuralları + escape + fuzzing | 7, 16 |

---

## Ek B — Netleştirme Kaydı

### B.1 v2.0 → v2.1

| # | Belirsizlik | Çözüm | Bölüm |
| --- | --- | --- | --- |
| 1 | "Event" iki anlamda kullanılıyordu | Terim sözlüğü: LogEvent / Bulgu / Incident / Fingerprint / Record | 4.0 |
| 2 | Dosya adı dili "açık karar" bırakılmıştı | İngilizce kaynak dosya adları kararı verildi | 11 |
| 3 | "Yeterince iyi lexer" ölçütü yoktu | Lexer kapsamı: desteklenen/desteklenmeyen listesi | 6.2 |
| 4 | "Öğrenme penceresi" tanımsızdı | İlk 10 dakika veya ilk 1000 kayıt (hangisi önce) | 7.2 (B3) |
| 5 | "Top-N" N'i belirsizdi | Varsayılan 20, `top_n` ile değiştirilebilir | 8.2 |
| 6 | B1/B2 penceresi sabit/kayan belirsizdi | Kayan 60 sn pencere | 7.2 |
| 7 | Histogram kova sınırları yoktu | 6 kova: 0-1, 1-10, 10-100, 100-1000, 1000-10000, 10000+ ms | 8.2 |
| 8 | 50k satır/sn referans makine yoktu | 4 çekirdek / 8 GB / SSD, GCC 13, -O2 | 15 |
| 9 | 64 MB bellek üst sınırı tablo boyutu belirsizdi | Parmak izi tablosu 100.000 kayıt üst sınırı | 15 |
| 10 | JSON parser çelişkisi (config vs. jsonlog) | Config INI; JSON parser yalnızca `jsonlog` için Faz 4 | 13 |
| 11 | CLI bayrakları açıklanmamıştı | Tam bayrak tablosu | 12 |
| 12 | `LogEvent` alanları iki bölümde farklıydı | Tek alan listesi tanımlandı | 4.0 |
| 13 | Yavaş sorgu eşiği ile PostgreSQL ayarı ilişkisi | `slow_threshold_ms` senkronu belgelendi | 3 |
| 14 | Çıktı izinleri yalnızca risklerde geçiyordu | Rapor bölümüne taşındı (`0600`, `--umask`) | 10 |
| 15 | S1 regex mi lexer mi belirsizdi | Regex ön filtre + lexer genel kontrol; birleştirme kuralı | 7.2 |
| 16 | Başarı kriterleri hangi fazda doğrulanacak | Her kritere faz atandı | 15 |
| 17 | Modül sahipliği (güvenlik vs. olaylar) örtüşüyordu | Sahiplik kuralı tabloya eklendi | 2 |
| 18 | "İleri aşamaya alındı" (AES) hangi faz belirsizdi | Faz 5, rapor şifreleme olarak netleşti | 10, 13 |

### B.2 v2.1 → v2.2 (yeni kararlar)

| # | Karar | Ne değişti | Etkilediği bölümler |
| --- | --- | --- | --- |
| K1 | Şema keşfi libpq ile doğrudan; Node.js/Prisma yok | Yeni `schema_introspect` bileşeni; `--introspect`, `--main-conninfo` bayrakları | 2, 4, 11, 12 |
| K2 | `EXPLAIN (ANALYZE, FORMAT JSON)` libpq ile; SELECT serbest, DML yasak | 8.1 yeniden yazıldı; "indeks logdan çıkarılamaz" → "üç kaynak birleşimi" | 2, 8.1, 12, 17 |
| K3 | Tavsiyeler deterministik kural motoruyla (AI'sız da) | 2 tablosuna "kural tabanlı tavsiye" eklendi; 8.1'de vurgu | 2, 8.1, 10 |
| K4 | AI opsiyonel `advisory` katmanı; varsayılan kapalı | Yeni `ai_advisor` bileşeni; `--ai*` bayrakları; rapora `advisory` alanı | 2.1, 4, 10, 12, 17 |
| K5 | Raporda `source: "rule" \| "ai"` etiketi | Rapor bölümüne kaynak etiketi ve örnek JSON'lar eklendi | 10, 15 |
| K6 | Ana DB salt okunur; test DB ayrı container | İki bağlantı (`--main-conninfo`, `--test-conninfo`); `--sync-test`; `EXPLAIN ANALYZE` yalnızca test'te | 2.1, 4, 8.1, 12, 15, 17 |
| K7 | Tek config dosyası `querithex.conf` (INI) | Yeni Bölüm 12.1; `-c` netleştirildi; öncelik kuralı | 2.1, 11, 12, 12.1 |

### B.3 v2.2 → v2.3 (7 düzeltme + D8, D9)

| # | Düzeltme | Nerede | Ne değişti | Durum |
| --- | --- | --- | --- | --- |
| D1 | `ro_user` / `pg_dump` yetkileri | KULLANIM.md + 12 | `GRANT SELECT` + `GRANT USAGE, SELECT ON SEQUENCES`; `--sync-test` tam `pg_dump` | ✅ KULLANIM.md + 12 |
| D2 | Düz `EXPLAIN` güvenli | 8.1 | "Sorguyu çalıştırmaz; test'te DML için güvenli" cümlesi | ✅ 8.1 |
| D3 | libpq faz ayrımı | 9.2 | libpq Faz 2 (şema+plan) / `pg_stat_database` Faz 4 (hata oranı) | ✅ 9.2 |
| D4 | `--ai-provider` sadeleşti | 12 | `--ai-provider` kaldırıldı; `--ai` hem enabled hem provider | ✅ 12 |
| D5 | `tests/` script'leri | 11 | `test_pipeline.sh`, `test_no_write.sh`, `test_config.sh` eklendi | ✅ 11 |
| D6 | Parmak izi bütçesi | 8.2 + 15 | Kayıt başına ~200 bayt; örnek sorgu 256 bayt üst sınır | ✅ 8.2 + 15 |
| D7 | Dinamik `[kural:<id>]` | 12.1 | Dinamik bölüm desteği açıkça yazıldı | ✅ 12.1 |
| D8 | Terimler doğru yere | 5.0, 6.0 | Bölüm başlarına taşındı | ✅ 5.0 + 6.0 |
| D9 | Faz 0 Docker ortamı | 13 | `main_database`/`test_database` container'ları Faz 0'a eklendi | ✅ 13 |

### B.4 v2.3 → v2.4 (D10–D22)

| # | Düzeltme | Nerede | Ne değişti | Durum |
| --- | --- | --- | --- | --- |
| D10 | 2.1 başlığı | 2.1 | "v2.2" → "v2.2–v2.3; düzeltmeler için Ek B.3" | ✅ 2.1 |
| D11 | Karar–bölüm tablosu | 2.1 | "D1–D9 → Ek B.3" satırı eklendi | ✅ 2.1 |
| D12 | `--umask` bayrağı | 12 | CLI tablosuna eklendi | ✅ 12 |
| D13 | Şifreleme başlığı | 10 | "(Faz 5)" eklendi | ✅ 10 |
| D14 | Histogram netleştirme | 8.2 | Global histogram; 48 bayt toplam; bütçeye dahil değil | ✅ 8.2 |
| D15 | Conninfo/config önceliği | 12 | Conninfo verilirse config alanları yok sayılır | ✅ 12 |
| D16 | `pg_restore --clean` | KULLANIM.md | `--clean --if-exists` notu | ✅ KULLANIM.md |
| D17 | `event_ref` arena notu | 4.0 | Kopya değil, kısa özet; arena'da | ✅ 4.0 |
| D18 | 5.3 "Faz 4'te" | 5.3 | v1.x → Faz 4 | ✅ 5.3 |
| D19 | Risk satırı `statement_timeout` | 17 | Yalnızca test bağlantısında | ✅ 17 |
| D20 | Ek B.3 "Durum" sütunu | Ek B.3 | Eklendi | ✅ Ek B.3 |
| D21 | Kriter 8 karşılaştırma detayı | 15 | Float alanlar hariç; sıralama rule_id | ✅ 15 |
| D22 | 12.1 `[kural:S1]` örneği | 12.1 | Örneğe eklendi | ✅ 12.1 |
| D23 | Faz 1 config ayrıştırma/kullanım | 13 | Faz 1 ayrıştır+sakla; Faz 2 kullan | ✅ 13 |

### B.5 K1–K7 + D1–D23 sonrası çelişki taraması

| # | Kontrol | Sonuç |
| --- | --- | --- |
| 1 | "Logdan çıkarılamaz" diyen her yer, libpq/plan/şema ile güncellendi mi? | ✅ Bölüm 2 (Performans), 8.1 güncellendi; Bölüm 2'deki diğer "log tek başına yetmez" satırları (güvenlik için `pg_roles`, kullanılabilirlik için payda) **bilinçli olarak** değişmedi |
| 2 | "AI" geçen her yer opsiyonel + varsayılan kapalı diyor mu? | ✅ K4, Bölüm 4.0 (Advisory), 10, 12, 17 |
| 3 | "`EXPLAIN ANALYZE` yasak" diyen her yer SELECT/DML + main/test ayrımını yapıyor mu? | ✅ 8.1 (K2+K6), 17 (K6 risk satırı); D2 ile düz EXPLAIN'in güvenli olduğu eklendi |
| 4 | Ana DB'ye yazma riski her yerde önlenmiş mi? | ✅ 12 (salt okunur rol + `default_transaction_read_only`), 8.1 (ana DB'de hiç EXPLAIN yok), 17 (risk satırı); D1 ile `pg_dump` yetkileri belgelendi |
| 5 | Rapor AI kapalıyken tam mı? | ✅ 10 (advisory yoksa alan eklenmez), 15 (kriter 8); D21 ile karşılaştırma detayı |
| 6 | Config tek dosya mı, ekstra conf yok mu? | ✅ K7, 12.1 (tek dosya, 4 sabit + dinamik bölüm), 11 (`querithex.conf` kökte); D7 ile dinamik bölümler netleşti |
| 7 | Node.js/Prisma dökümanda geçiyor mu? | ✅ K1'de "kullanılmaz" olarak; başka yerde bağımlılık olarak yok |
| 8 | `--ai-provider` çelişkisi kaldı mı? | ✅ D4 ile kaldırıldı; tek `--ai` |
| 9 | Terimler doğru yerde mi? | ✅ D8 ile 5.0 ve 6.0 bölüm başlarına taşındı |
| 10 | `tests/` script'leri tutarlı mı? | ✅ D5 ile 11 ve 14 uyumlu |
| 11 | Bellek bütçesi ölçülebilir mi? | ✅ D6 ile kayıt başına bütçe + örnek sorgu üst sınırı; D14 ile histogram global |
| 12 | `--umask` CLI'de var mı? | ✅ D12 ile eklendi |
| 13 | Şifreleme v1'de mi sanılıyor? | ✅ D13 ile "(Faz 5)" başlıkta |
| 14 | Conninfo/config etkileşimi net mi? | ✅ D15 ile netleşti |
| 15 | `event_ref` bellek modeli net mi? | ✅ D17 ile arena notu |
| 16 | 5.3 ile Bölüm 1 çelişiyor mu? | ✅ D18 ile uyumlu |
| 17 | `statement_timeout` yeri net mi? | ✅ D19 ile test bağlantısı |
| 18 | 2.1 başlığı güncel mi? | ✅ D10 ile v2.2–v2.3 |
| 19 | Karar–bölüm tablosu D'leri içeriyor mu? | ✅ D11 ile D1–D9 satırı |
| 20 | Kriter 8 diff'i float yuvarlamasından etkilenir mi? | ✅ D21 ile float alanlar hariç |
| 21 | 12.1 örneğinde `[kural:S1]` var mı? | ✅ D22 ile eklendi |
| 22 | Faz 1'de kural çalışıyor mu? | ✅ D23 ile "ayrıştır+sakla, çalıştırma" |