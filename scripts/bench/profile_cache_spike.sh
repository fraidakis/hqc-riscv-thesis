#!/usr/bin/env bash
# Profile polymul memory behavior with Spike's built-in D$ simulator.
#
# Uses Spike cache model (default --dc=512:8:64) and writes variant-specific
# CSV output under analysis/benchmarks/memory_profile/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

VARIANT="${1:-karatsuba_ref}"
LEVEL="${2:-128}"
ITER="${3:-1}"
DC_CONFIG="${DC_CONFIG:-512:8:64}"

if ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
    echo "[profile_cache_spike] ERROR: level must be 128, 192, or 256" >&2
    exit 2
fi
if ! [[ "$ITER" =~ ^[1-9][0-9]*$ ]]; then
    echo "[profile_cache_spike] ERROR: iter must be a positive integer" >&2
    exit 2
fi

RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="${ISA:-rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm}"

if [[ ! -x "$SPIKE" ]]; then
    echo "[profile_cache_spike] ERROR: spike not found at $SPIKE" >&2
    exit 2
fi

BUILD_DIR="$REPO_ROOT/build/spike_cacheprof_hqc${LEVEL}_${VARIANT}"
TARGET="bench_polymul_hqc${LEVEL}_${VARIANT}"
ELF="$BUILD_DIR/${TARGET}.elf"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DRISCV="$RISCV" \
    -DHQC_SIM=spike \
    -DBENCH_N_ITER="$ITER" >/dev/null

cmake --build "$BUILD_DIR" --target "$TARGET" -j"$(nproc)" >/dev/null

OUT_DIR="$REPO_ROOT/analysis/benchmarks/memory_profile"
mkdir -p "$OUT_DIR"

STAMP="$(date +%Y%m%d_%H%M%S)"
RUN_MODE="direct"
RAW_STDOUT="$OUT_DIR/cache_spike_stdout_hqc${LEVEL}_${VARIANT}_${STAMP}.log"
RAW_STDERR="$OUT_DIR/cache_spike_stderr_hqc${LEVEL}_${VARIANT}_${STAMP}.log"
CSV_OUT="$OUT_DIR/cache_profile_hqc${LEVEL}_${VARIANT}.csv"

# Bare-metal direct execution (no Proxy Kernel) for pristine microarchitectural profiling
"$SPIKE" --dc="$DC_CONFIG" --isa="$ISA" "$ELF" >"$RAW_STDOUT" 2>"$RAW_STDERR"

extract_metric() {
    local pattern="$1"
    local file="$2"
    awk -F':' -v p="$pattern" '$1 ~ p {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$file"
}

READ_MISSES="$(extract_metric 'D\\$ Read Misses' "$RAW_STDOUT")"
WRITE_MISSES="$(extract_metric 'D\\$ Write Misses' "$RAW_STDOUT")"
WRITEBACKS="$(extract_metric 'D\\$ Writebacks' "$RAW_STDOUT")"

if [[ -z "$READ_MISSES" || -z "$WRITE_MISSES" || -z "$WRITEBACKS" ]]; then
    echo "[profile_cache_spike] ERROR: failed to parse D$ cache metrics from $RAW_STDOUT" >&2
    echo "[profile_cache_spike] tail of output:" >&2
    tail -n 40 "$RAW_STDOUT" >&2 || true
    exit 1
fi

DC_MISSES=$((READ_MISSES + WRITE_MISSES))

if [[ ! -f "$CSV_OUT" ]]; then
    echo "timestamp,level,variant,iter,dc_config,dc_misses,dc_read_misses,dc_write_misses,dc_writebacks,run_mode,elf,stdout_log,stderr_log" > "$CSV_OUT"
fi

echo "${STAMP},${LEVEL},${VARIANT},${ITER},${DC_CONFIG},${DC_MISSES},${READ_MISSES},${WRITE_MISSES},${WRITEBACKS},${RUN_MODE},${ELF},${RAW_STDOUT},${RAW_STDERR}" >> "$CSV_OUT"

echo "[profile_cache_spike] variant=${VARIANT} level=${LEVEL} iter=${ITER} dc=${DC_CONFIG} run_mode=${RUN_MODE}"
echo "[profile_cache_spike] D$ Misses=${DC_MISSES} (Read=${READ_MISSES}, Write=${WRITE_MISSES})"
echo "[profile_cache_spike] D$ Writebacks=${WRITEBACKS}"
echo "[profile_cache_spike] csv=${CSV_OUT}"
echo "[profile_cache_spike] stdout_log=${RAW_STDOUT}"
