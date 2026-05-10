#!/usr/bin/env bash
# scripts/bench/run_spike_suite_sync.sh
#
# Spike-first benchmark suite runner (synchronous).
#
# Runs BOTH benchmark kinds for every selected LEVEL x VARIANT:
#   1) polymul micro-bench: bench_polymul_hqc<L>_<V>.elf
#   2) full KEM bench:      bench_hqc<L>_<V>.elf
#
# Outputs are organized per run-id for reproducibility:
#   analysis/benchmarks/runs/<run_id>/
#     raw/      # raw Spike stdout/stderr per run (ignored by git)
#     curated/  # clean CSVs used by reports
#     summary.csv  # wall-time + metric summary table
#
# Usage:
#   scripts/bench/run_spike_suite_sync.sh [options]
#
# Options:
#   --run-id <id>           Custom run id (default: [variant]_suite_[YYYYMMDD_HHMM])
#   --iter <N>              BENCH_N_ITER for both benches (default: 1)
#   --levels "128 192 256"  Levels to run (default: all)
#   --variant <regex>       Filter implemented variants with grep -E
#   --promote <name>        Copy curated CSVs to analysis/benchmarks/canonical/<name>/
#
# Notes:
# - This script intentionally uses Spike only.
# - QEMU remains useful as optional fallback correctness/KAT check.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_runner.sh"

bench_load_implemented_variants "$REPO_ROOT"

RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
BUILD_DIR="${REPO_ROOT}/build/spike"

RUN_ID=""
ITER=1
LEVELS="128 192 256"
VARIANT_FILTER=""
PROMOTE_NAME=""
BENCH_TIME=$(date +'%Y%m%d_%H%M')

RUN_ID_OVERRIDE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --run-id)
            RUN_ID="$2"; RUN_ID_OVERRIDE=1; shift 2 ;;
        --iter)
            ITER="$2"; shift 2 ;;
        --levels)
            LEVELS="$2"; shift 2 ;;
        --variant)
            VARIANT_FILTER="$2"; shift 2 ;;
        --promote)
            PROMOTE_NAME="$2"; shift 2 ;;
        *)
            echo "Unknown option: $1" >&2
            exit 2 ;;
    esac
done

if [[ "$RUN_ID_OVERRIDE" -eq 0 ]]; then
    VARIANT_LABEL="$(bench_sanitize_label "${VARIANT_FILTER:-all}")"
    RUN_ID="${VARIANT_LABEL}_suite_${BENCH_TIME}"
fi

if ! [[ "$RUN_ID" =~ ^[A-Za-z0-9_]+$ ]]; then
    echo "ERROR: --run-id must contain only [A-Za-z0-9_]" >&2
    exit 2
fi

if ! [[ "$ITER" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: --iter must be positive integer" >&2
    exit 2
fi

if ! bench_require_executable "$SPIKE" "Spike not found"; then
    exit 2
fi

RUN_ROOT="${REPO_ROOT}/analysis/benchmarks/runs/${RUN_ID}"
RAW_DIR="${RUN_ROOT}/raw"
CURATED_DIR="${RUN_ROOT}/curated"
mkdir -p "$RAW_DIR" "$CURATED_DIR"

SUMMARY_CSV="${RUN_ROOT}/summary.csv"
echo "level,variant,kind,op,wall_seconds,median_cycles,median_instret,csv_path" > "$SUMMARY_CSV"

extract_median_row() {
    local csv="$1" op="$2"
    awk -F',' -v op="$op" '$1==op {print $4 "," $7; exit}' "$csv"
}

run_one() {
    local level="$1" variant="$2" kind="$3" target="$4" op_for_row="$5"

    local elf="${BUILD_DIR}/${target}.elf"
    local base="hqc${level}_${variant}_${kind}_N${ITER}"
    local raw_out="${RAW_DIR}/${base}.stdout"
    local raw_err="${RAW_DIR}/${base}.stderr"
    local curated_csv="${CURATED_DIR}/${base}.csv"

    cmake --build "$BUILD_DIR" --target "$target" -j"$(nproc)" >/dev/null

    local elapsed_s
    elapsed_s=$(bench_run_spike_timed "$SPIKE" "$ISA" "$elf" "$raw_out" "$raw_err")

    # Keep only CSV/metadata lines in curated output.
    awk 'BEGIN{keep=0}
         /^# level=/ {print; keep=1; next}
         /^op,iterations,/ {print; keep=1; next}
         keep && /^[a-z_]+,[0-9]+,/ {print}
    ' "$raw_out" > "$curated_csv"

    if [[ "$kind" == "polymul" ]]; then
        local row
        row=$(extract_median_row "$curated_csv" "vect_mul")
        local med_cyc med_ins
        med_cyc="${row%%,*}"
        med_ins="${row##*,}"
        echo "${level},${variant},${kind},vect_mul,${elapsed_s},${med_cyc},${med_ins},${curated_csv}" >> "$SUMMARY_CSV"
    else
        for op in keypair encaps decaps; do
            local row
            row=$(extract_median_row "$curated_csv" "$op")
            local med_cyc med_ins
            med_cyc="${row%%,*}"
            med_ins="${row##*,}"
            echo "${level},${variant},${kind},${op},${elapsed_s},${med_cyc},${med_ins},${curated_csv}" >> "$SUMMARY_CSV"
        done
    fi

    echo "[suite] ${kind} L${level} ${variant}: ${elapsed_s}s"
}

echo "[suite] run-id: ${RUN_ID}"
echo "[suite] build dir: ${BUILD_DIR}"
echo "[suite] output: ${RUN_ROOT}"

# Ensure spike build tree is configured for this iteration count.
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=spike \
    -DBENCH_N_ITER="$ITER" >/dev/null

for level in $LEVELS; do
    for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
        bench_variant_matches "$variant" "$VARIANT_FILTER" || continue

        run_one "$level" "$variant" "polymul" "bench_polymul_hqc${level}_${variant}" "vect_mul"
        run_one "$level" "$variant" "kem" "bench_hqc${level}_${variant}" "keypair"
    done
done

if [[ -n "$PROMOTE_NAME" ]]; then
    CANON_DIR="${REPO_ROOT}/analysis/benchmarks/canonical/${PROMOTE_NAME}"
    mkdir -p "$CANON_DIR"
    cp -f "${CURATED_DIR}"/*.csv "$CANON_DIR"/
    cp -f "$SUMMARY_CSV" "${CANON_DIR}/summary.csv"
    echo "[suite] promoted curated outputs to ${CANON_DIR}"
fi

echo "[suite] done: ${SUMMARY_CSV}"
