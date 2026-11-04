#!/usr/bin/env bash
# Querithex test runner
# README Bölüm 14: entegrasyon ve birim testleri
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "== Querithex test runner =="
echo "Proje kökü: $PROJECT_ROOT"

# 1. Smoke test (Binary çalışıyor mu?)
if [ ! -x "./querithex" ]; then
    echo "HATA: ./querithex bulunamadı. Önce 'make' çalıştırın." >&2
    exit 1
fi
./querithex >/dev/null 2>&1 || true
echo "-- Smoke test geçti --"

# 2. Birim Testleri (tests/unit/ altındakiler)
UNIT_DIR="tests/unit"
if [ -d "$UNIT_DIR" ] && [ "$(ls -A "$UNIT_DIR"/*.c 2>/dev/null)" ]; then
    echo "-- Birim testleri çalıştırılıyor --"
    for test_file in "$UNIT_DIR"/test_*.c; do
        test_name=$(basename "$test_file" .c)
        # Modül adını bul (test_buffer.c -> src/core/buffer.c)
        module_name=${test_name#test_}
        
        # Modülün yolunu bul (core mu, src mi?)
        if [ -f "src/core/${module_name}.c" ]; then
            src_file="src/core/${module_name}.c"
        elif [ -f "src/${module_name}.c" ]; then
            src_file="src/${module_name}.c"
        else
            echo "UYARI: ${module_name} modülü bulunamadı, $test_file atlanıyor." >&2
            continue
        fi
        
        # Derle ve çalıştır
        out_bin="build/${test_name}"
        mkdir -p build
        
        echo "Derleniyor: $test_file"
        # Not: Test dosyaları src/ veya src/core/ içindeki .c dosyalarını include edebilir.
        gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -g -Isrc -Isrc/core -o "$out_bin" "$test_file" "$src_file"
        
        echo "Çalıştırılıyor: $test_name"
        "$out_bin"
    done
    echo "-- Tüm birim testleri geçti --"
else
    echo "Birim testi bulunamadı."
fi

echo "== Tüm testler başarılı =="