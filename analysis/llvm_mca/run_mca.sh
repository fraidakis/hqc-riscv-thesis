#!/usr/bin/env bash
# analysis/llvm_mca/run_mca.sh
# Run LLVM-MCA on HQC source functions to get throughput/latency estimates.
# Usage: run_mca.sh <source_file> [hqc_level=128|192|256] [extra_mca_flags...]
# Example: ./analysis/llvm_mca/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c 128
#          ./analysis/llvm_mca/run_mca.sh src/poly_mul/fafft_zbc/fafft.c 128 --mattr=+zbc

set -euo pipefail

SOURCE="${1:?Usage: $0 <source.c> [hqc_level] [mca_flags...]}"
shift
# Optional second arg: HQC security level (128/192/256); defaults to 128
LEVEL="128"
if [[ "${1:-}" =~ ^(128|192|256)$ ]]; then
    LEVEL="$1"
    shift
fi
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
CLANG="$RISCV/bin/clang"
MCA="$RISCV/bin/llvm-mca"
REPORT_DIR="analysis/llvm_mca/reports"
mkdir -p "$REPORT_DIR"

BASENAME=$(basename "${SOURCE%.*}")
ASM_FILE="$REPORT_DIR/${BASENAME}.s"
REPORT="$REPORT_DIR/${BASENAME}_mca.txt"

MARCH="rv64gcv_zbc_zbb_zbkb_zvbc"
MABI="lp64d"
TARGET="riscv64-unknown-elf"

# Map level to hqc-N directory names (hqc-1=128, hqc-3=192, hqc-5=256)
case "$LEVEL" in
  128) HQC_DIR="hqc-1" ;;
  192) HQC_DIR="hqc-3" ;;
  256) HQC_DIR="hqc-5" ;;
esac

echo "[llvm-mca] Compiling to assembly: $SOURCE (HQC-$LEVEL)"
"$CLANG" \
    --target="$TARGET" \
    -march="$MARCH" \
    -mabi="$MABI" \
    -menable-experimental-extensions \
    -O3 \
    -ffreestanding -fno-builtin \
    -Iinclude \
    -Ivendor/hqc-ref/src/ref \
    -Ivendor/hqc-ref/src/ref/$HQC_DIR \
    -Ivendor/hqc-ref/src/common/$HQC_DIR \
    -DHQC_$LEVEL \
    -S -o "$ASM_FILE" \
    "$SOURCE"

echo "[llvm-mca] Running MCA on: $ASM_FILE"
"$MCA" \
    -march=riscv64 \
    -mcpu=sifive-u74 \
    --timeline \
    --iterations=10 \
    "$@" \
    "$ASM_FILE" > "$REPORT"

echo "[llvm-mca] Report written to: $REPORT"
head -40 "$REPORT"
