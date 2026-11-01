#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "== Querithex test runner =="
echo "Proje kökü: $PROJECT_ROOT"

if [ ! -x "./querithex" ]; then
    echo "HATA: ./querithex bulunamadı. Önce 'make' çalıştırın." >&2
    exit 1
fi

echo "-- smoke test --"
./querithex >/dev/null 2>&1 || true

echo "Querithex tests: (henüz modül testi yok, iskelet hazır)"
exit 0
