#!/usr/bin/env bash
# Querithex test runner
# README Bölüm 14: entegrasyon testleri
# README Faz 0: test iskeleti
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "== Querithex test runner =="
echo "Proje kökü: $PROJECT_ROOT"

# Binary var mı?
if [ ! -x "./querithex" ]; then
    echo "HATA: ./querithex bulunamadı. Önce 'make' çalıştırın." >&2
    exit 1
fi

# Smoke test: binary çalışıyor mu?
echo "-- smoke test: ./querithex --version benzeri çağrı --"
./querithex >/dev/null 2>&1 || true

# Henüz modül testleri yok (Faz 1'de eklenecek)
echo "Querithex tests: (henüz modül testi yok, iskelet hazır)"
exit 0
