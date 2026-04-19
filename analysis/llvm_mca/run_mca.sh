#!/usr/bin/env bash
# analysis/llvm_mca/run_mca.sh
# Run LLVM-MCA on HQC source functions to get throughput/latency estimates.
# Usage: run_mca.sh <source_file> [extra_mca_flags...]

set -euo pipefail

SOURCE="${1:?Usage: $0 <source.c> [mca_flags...]}"
shift
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

echo "[llvm-mca] Compiling to assembly: $SOURCE"
"$CLANG" \
    --target="$TARGET" \
    -march="$MARCH" \
    -mabi="$MABI" \
    -menable-experimental-extensions \
    -O3 -flto \
    -ffreestanding -fno-builtin \
    -Iinclude \
    -S -o "$ASM_FILE" \
    "$SOURCE"

echo "[llvm-mca] Running MCA on: $ASM_FILE"
"$MCA" \
    -march=riscv64 \
    -mcpu=generic-rv64 \
    --timeline \
    --iterations=10 \
    "$@" \
    "$ASM_FILE" > "$REPORT"

echo "[llvm-mca] Report written to: $REPORT"
head -40 "$REPORT"
