#!/usr/bin/env bash
# Quick P-08 probe: compare scalar target cycle count with/without Polly.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

VARIANT="${1:-karatsuba_ref}"
LEVEL="${2:-128}"
ITER="${3:-1}"

if ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
    echo "[polly_delta] ERROR: level must be 128, 192, or 256" >&2
    exit 2
fi
if ! [[ "$ITER" =~ ^[1-9][0-9]*$ ]]; then
    echo "[polly_delta] ERROR: iter must be a positive integer" >&2
    exit 2
fi

RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"

if [[ ! -x "$SPIKE" ]]; then
    echo "[polly_delta] ERROR: Spike not found at $SPIKE" >&2
    exit 2
fi

run_mode() {
    local mode="$1"
    local polly="$2"
    local build_dir="$REPO_ROOT/build/polly_${mode}_hqc${LEVEL}_${VARIANT}"
    local target="bench_polymul_hqc${LEVEL}_${VARIANT}"
    local out="$build_dir/${target}.stdout"
    local err="$build_dir/${target}.stderr"

    rm -rf "$build_dir"
    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DRISCV="$RISCV" \
        -DHQC_SIM=spike \
        -DBENCH_N_ITER="$ITER" \
        -DHQC_ENABLE_POLLY="$polly" >/dev/null

    cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null
    "$SPIKE" --isa="$ISA" "$build_dir/${target}.elf" > "$out" 2> "$err"

    awk -F',' '$1=="vect_mul" {print $4; exit}' "$out"
}

base_cycles="$(run_mode base OFF)"
polly_cycles="$(run_mode polly ON)"

delta_pct="$(awk -v b="$base_cycles" -v p="$polly_cycles" 'BEGIN { if (b == 0) { print "0.00" } else { printf "%.2f", ((p-b)*100.0)/b } }')"

echo "[polly_delta] level=${LEVEL} variant=${VARIANT} iter=${ITER}"
echo "[polly_delta] baseline_cycles=${base_cycles}"
echo "[polly_delta] polly_cycles=${polly_cycles}"
echo "[polly_delta] delta_pct=${delta_pct}%"
