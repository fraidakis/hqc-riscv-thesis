#!/usr/bin/env bash
# scripts/bench/run_hw_lto_pgo_compare.sh
#
# Compare HQC polymul variant performance on BPI-F3 with and without LTO/PGO.
#
# Supported polymul variants:
#   scalar_ref  — vendor schoolbook (vendor/hqc-ref/src/ref/gf2x.c)
#   scalar_zb   — Karatsuba with clmul/clmulh (src/mult/karatsuba_zbc/gf2x_clmul.c)
#
# Configurations tested:
#   1. baseline    — plain -O3, no LTO, no PGO
#   2. lto         — -O3 -flto
#   3. pgo-use     — -O3 -flto -fprofile-use (trained profile from config 4)
#   4. pgo-gen     — (internal) -O3 -fprofile-generate (training run, not benchmarked)
#
# Workflow:
#   1. Build baseline & LTO binaries
#   2. Build PGO-generate binary, run it (training), collect profile data
#   3. Build PGO-use binary with the collected profile
#   4. Run all three (baseline, lto, pgo-use) benchmarks back-to-back
#
# Usage (on the BPI-F3 board):
#   cd ~/hqc-riscv
#   bash scripts/bench/run_hw_lto_pgo_compare.sh [--level 128] [--mode dev|publish]
#   bash scripts/bench/run_hw_lto_pgo_compare.sh --polymul scalar_zb --level 128
#
# Output:
#   results/hardware/benchmarks/lto_pgo/hw_lto_pgo_compare_<timestamp>.csv
#
# Requires: GCC with PGO support (gcc 14+ on the board).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

# ── Configuration ─────────────────────────────────────────────────────────────
LEVEL="${LEVEL:-128}"
MODE="${MODE:-dev}"
CC="${CC:-gcc}"
CORE="${CORE:-7}"
POLYMUL="${POLYMUL:-scalar_ref}"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --level)   LEVEL="$2"; shift 2 ;;
        --mode)    MODE="$2"; shift 2 ;;
        --cc)      CC="$2"; shift 2 ;;
        --core)    CORE="$2"; shift 2 ;;
        --polymul) POLYMUL="$(bench_resolve_variant "$2")"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--level 128|192|256] [--mode dev|publish] [--cc gcc] [--core 7] [--polymul scalar_ref|scalar_zb]"
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# Validate polymul choice
case "$POLYMUL" in
    scalar_ref|scalar_zb) ;;
    *) echo "ERROR: --polymul must be scalar_ref or scalar_zb; got: '$POLYMUL'" >&2; exit 1 ;;
esac

if [[ "$MODE" == "publish" ]]; then
    N_ITER="${N_ITER:-10000}"
    N_WARMUP="${N_WARMUP:-100}"
    PGO_TRAIN_ITER=50   # training iterations for PGO profile
else
    N_ITER="${N_ITER:-100}"
    N_WARMUP="${N_WARMUP:-10}"
    PGO_TRAIN_ITER=20
fi

MARCH="rv64gc_zbc_zbb_zbs"

# ── Polymul → source file mapping ────────────────────────────────────────────
# Paths
BENCH_DIR="${REPO_ROOT}/tests/bench"
BUILD_DIR="${REPO_ROOT}/build/hw_lto_pgo/${POLYMUL}"
OUT_DIR="${REPO_ROOT}/results/hardware/benchmarks/lto_pgo"
TIMESTAMP="$(date +'%Y-%m-%d_%H-%M')"

mkdir -p "$BUILD_DIR"/{baseline,lto,pgo_gen,pgo_use}
mkdir -p "$OUT_DIR"

# ── Level → variant dir mapping ──────────────────────────────────────────────
case "$LEVEL" in
    128) VARIANT_DIR="hqc-1" ;;
    192) VARIANT_DIR="hqc-3" ;;
    256) VARIANT_DIR="hqc-5" ;;
    *) echo "ERROR: LEVEL must be 128, 192, or 256" >&2; exit 1 ;;
esac

# ── Common sources & flags ────────────────────────────────────────────────────
VENDOR="${REPO_ROOT}/vendor/hqc-ref"
SRC="${REPO_ROOT}/src"

INCLUDES="-I${VENDOR}/src/common/${VARIANT_DIR} \
          -I${VENDOR}/src/common \
          -I${VENDOR}/src/ref/${VARIANT_DIR} \
          -I${VENDOR}/src/ref \
          -I${VENDOR}/lib/fips202 \
          -I${SRC}/common \
          -I${REPO_ROOT}/include \
          -I${BENCH_DIR}"

# Select polymul source file based on variant
case "$POLYMUL" in
    scalar_ref) POLYMUL_SRC="${VENDOR}/src/ref/gf2x.c" ;;
    scalar_zb)  POLYMUL_SRC="${SRC}/mult/karatsuba_zbc/gf2x_clmul.c" ;;
esac

KEM_SOURCES="${BENCH_DIR}/bench_kem.c \
             ${VENDOR}/src/common/kem.c \
             ${VENDOR}/src/common/crypto_memset.c \
             ${VENDOR}/src/ref/parsing.c \
             ${SRC}/common/gf.c \
             ${SRC}/hqc.c \
             ${VENDOR}/src/common/code.c \
             ${VENDOR}/src/common/symmetric.c \
             ${VENDOR}/src/ref/reed_muller.c \
             ${VENDOR}/src/ref/reed_solomon.c \
             ${VENDOR}/src/common/fft.c \
             ${VENDOR}/lib/fips202/fips202.c \
             ${SRC}/vector.c \
             ${SRC}/common/randombytes.c \
             ${POLYMUL_SRC}"

BASE_CFLAGS="-O3 -static -march=${MARCH} -mabi=lp64d \
             -DSECURITY_LEVEL=${LEVEL} -DHQC_${LEVEL} \
             -DHQC_PLATFORM_HW=1 \
             -DHQC_BENCH_MODE=1 \
             -DPOLYMUL_NAME=\"${POLYMUL}\" \
             -DN_ITER=${N_ITER} -DN_WARMUP=${N_WARMUP} \
             -fno-tree-vectorize"

# ── Build functions ───────────────────────────────────────────────────────────
build_binary() {
    local config="$1"     # baseline, lto, pgo_gen, pgo_use
    local extra_flags="$2"
    local out_dir="${BUILD_DIR}/${config}"
    local bin_name="bench_hw_kem_hqc${LEVEL}_${POLYMUL}_${config}"
    local bin_path="${out_dir}/${bin_name}"

    echo "  Building: ${config} ..." >&2
    # shellcheck disable=SC2086
    $CC $BASE_CFLAGS $extra_flags $INCLUDES $KEM_SOURCES -lm -o "$bin_path" 2>&1 | head -20 >&2
    local rc=${PIPESTATUS[0]}

    if [[ $rc -ne 0 ]]; then
        echo "  ✗ BUILD FAILED for ${config} (rc=$rc)" >&2
        return 1
    fi

    local text_size
    text_size="$(size "$bin_path" 2>/dev/null | awk 'NR==2{print $1}')"
    echo "  ✓ Built: $(basename "$bin_path") (text=${text_size:-?} bytes)" >&2
    echo "$bin_path"
}

run_benchmark() {
    local bin_path="$1"
    local config_label="$2"
    local out_file="$3"

    echo "  Running: ${config_label} (${N_ITER} iterations) ..."
    sudo nice -n -20 taskset -c "$CORE" "$bin_path" > "$out_file" 2>&1
    local rc=$?

    if [[ $rc -ne 0 ]]; then
        echo "  ✗ RUN FAILED for ${config_label} (rc=$rc)" >&2
        cat "$out_file" >&2
        return 1
    fi
    echo "  ✓ Done: ${config_label}"
}

# ── Header ────────────────────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
printf "║  %-60s║\n" "HW LTO/PGO Comparison — ${POLYMUL} on BPI-F3"
printf "║  %-60s║\n" "Level: HQC-${LEVEL}  Mode: ${MODE}  CC: ${CC}"
printf "║  %-60s║\n" "N_ITER: ${N_ITER}  N_WARMUP: ${N_WARMUP}  Core: ${CORE}"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ── Step 1: Check governor ────────────────────────────────────────────────────
GOV="$(cat /sys/devices/system/cpu/cpu${CORE}/cpufreq/scaling_governor 2>/dev/null || echo 'unknown')"
if [[ "$GOV" != "performance" ]]; then
    echo "⚠ WARNING: CPU governor is '${GOV}', not 'performance'."
    echo "  Run: echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor"
    echo ""
fi

# ── Step 2: Build baseline (no LTO, no PGO) ──────────────────────────────────
echo "━━━ [1/4] Building baseline (plain -O3) ━━━"
BASELINE_BIN="$(build_binary baseline "")"
echo ""

# ── Step 3: Build LTO (just -flto) ───────────────────────────────────────────
echo "━━━ [2/4] Building LTO (-O3 -flto) ━━━"
LTO_BIN="$(build_binary lto "-flto")"
echo ""

# ── Step 4: PGO training run ─────────────────────────────────────────────────
echo "━━━ [3/4] PGO training ━━━"

# For GCC with LTO, profile filenames embed the output binary name.
# We must use the SAME output binary name for both -fprofile-generate and
# -fprofile-use, so the .gcda files match.
PGO_PROFILE_DIR="${BUILD_DIR}/pgo_use/profile_data"
mkdir -p "$PGO_PROFILE_DIR"

# The final PGO-use binary path (both gen and use must share this name)
PGO_FINAL_BIN="${BUILD_DIR}/pgo_use/bench_hw_kem_hqc${LEVEL}_${POLYMUL}_pgo_use"

PGO_GEN_CFLAGS_OVERRIDE="${BASE_CFLAGS/-DN_ITER=${N_ITER}/-DN_ITER=${PGO_TRAIN_ITER}}"
PGO_GEN_CFLAGS_OVERRIDE="${PGO_GEN_CFLAGS_OVERRIDE/-DN_WARMUP=${N_WARMUP}/-DN_WARMUP=5}"

echo "  Building PGO-generate binary (${PGO_TRAIN_ITER} training iters) ..."
# shellcheck disable=SC2086
$CC $PGO_GEN_CFLAGS_OVERRIDE -flto -fprofile-generate="${PGO_PROFILE_DIR}" \
    $INCLUDES $KEM_SOURCES -lm -lgcov -o "$PGO_FINAL_BIN" 2>&1 | head -10
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    echo "  ✗ PGO-generate build failed" >&2
    exit 1
fi
echo "  ✓ PGO-generate binary built"

echo "  Running PGO training (this generates profile data) ..."
# Run the training binary — profile data gets written to PGO_PROFILE_DIR
sudo nice -n -20 taskset -c "$CORE" "$PGO_FINAL_BIN" > /dev/null 2>&1
echo "  ✓ Profile data collected in: ${PGO_PROFILE_DIR}"

# Count profile files
PROFILE_COUNT="$(find "$PGO_PROFILE_DIR" -name '*.gcda' 2>/dev/null | wc -l)"
echo "  Profile files: ${PROFILE_COUNT} .gcda files"

# Rebuild the same binary with -fprofile-use (same name → matching .gcda)
echo "  Building PGO-use binary (-flto -fprofile-use) ..."
# shellcheck disable=SC2086
$CC $BASE_CFLAGS -flto -fprofile-use="${PGO_PROFILE_DIR}" -fprofile-correction \
    $INCLUDES $KEM_SOURCES -lm -o "$PGO_FINAL_BIN" 2>&1 | head -10
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    echo "  ✗ PGO-use build failed" >&2
    exit 1
fi
PGO_USE_BIN="$PGO_FINAL_BIN"

local_text_size="$(size "$PGO_USE_BIN" 2>/dev/null | awk 'NR==2{print $1}')"
echo "  ✓ Built: $(basename "$PGO_USE_BIN") (text=${local_text_size:-?} bytes)"
echo ""

# ── Step 5: Run all benchmarks ────────────────────────────────────────────────
echo "━━━ [4/4] Running benchmarks ━━━"
echo ""

RESULT_BASELINE="${BUILD_DIR}/result_baseline.csv"
RESULT_LTO="${BUILD_DIR}/result_lto.csv"
RESULT_PGO="${BUILD_DIR}/result_pgo_use.csv"

run_benchmark "$BASELINE_BIN" "baseline" "$RESULT_BASELINE"
echo ""
sleep 2   # brief cooldown between runs

run_benchmark "$LTO_BIN" "lto" "$RESULT_LTO"
echo ""
sleep 2

run_benchmark "$PGO_USE_BIN" "pgo_use" "$RESULT_PGO"
echo ""

# ── Step 6: Combine results into comparison CSV ──────────────────────────────
echo "━━━ Generating comparison report ━━━"

CSV_OUT="${OUT_DIR}/hw_lto_pgo_compare_hqc${LEVEL}_${POLYMUL}_${TIMESTAMP}.csv"
CSV_LATEST="${OUT_DIR}/hw_lto_pgo_compare_hqc${LEVEL}_${POLYMUL}_latest.csv"

{
    echo "# HW LTO/PGO Comparison — ${POLYMUL} on BPI-F3"
    echo "# date: $(date +'%Y-%m-%d %H:%M')"
    echo "# commit: $(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
    echo "# level: ${LEVEL}"
    echo "# polymul: ${POLYMUL}"
    echo "# n_iter: ${N_ITER}"
    echo "# n_warmup: ${N_WARMUP}"
    echo "# mode: ${MODE}"
    echo "# cc: ${CC} $(${CC} -dumpversion 2>/dev/null || echo '?')"
    echo "# march: ${MARCH}"
    echo "#"
    echo "config,op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"

    for config in baseline lto pgo_use; do
        local_file="${BUILD_DIR}/result_${config}.csv"
        if [[ -f "$local_file" ]]; then
            # Extract data rows (skip comment lines)
            grep -v '^#' "$local_file" | grep -v '^op,' | while IFS= read -r line; do
                echo "${config},${line}"
            done
        fi
    done
} > "$CSV_OUT"

bench_refresh_tracked_csv "$CSV_OUT" "$CSV_LATEST" || true

echo ""
echo "━━━ Results ━━━"
echo ""

# Print a quick summary table
printf "%-10s  %-8s  %12s  %12s  %12s\n" "Config" "Op" "Med Cycles" "Med Instret" "Δ vs base"
printf "%-10s  %-8s  %12s  %12s  %12s\n" "------" "------" "----------" "-----------" "---------"

# Parse baseline median cycles for comparison
declare -A BASELINE_CYCLES

for config in baseline lto pgo_use; do
    local_file="${BUILD_DIR}/result_${config}.csv"
    if [[ ! -f "$local_file" ]]; then continue; fi

    while IFS=',' read -r op iterations min_c med_c max_c mean_c std_c min_i med_i max_i mean_i std_i _rest; do
        [[ "$op" =~ ^#|^op ]] && continue
        [[ -z "$med_c" ]] && continue

        if [[ "$config" == "baseline" ]]; then
            BASELINE_CYCLES["$op"]="$med_c"
        fi

        delta="—"
        base_val="${BASELINE_CYCLES[$op]:-}"
        if [[ -n "$base_val" && "$base_val" != "0" && "$config" != "baseline" ]]; then
            delta="$(awk -v cur="$med_c" -v base="$base_val" 'BEGIN { printf "%+.1f%%", (cur - base) / base * 100 }')"
        fi

        printf "%-10s  %-8s  %12s  %12s  %12s\n" "$config" "$op" "$med_c" "$med_i" "$delta"
    done < "$local_file"
done

echo ""
echo "  Archive: ${CSV_OUT}"
echo ""
echo "Text sizes:"
for config in baseline lto pgo_use; do
    bin="${BUILD_DIR}/${config}/bench_hw_kem_hqc${LEVEL}_${POLYMUL}_${config}"
    if [[ -f "$bin" ]]; then
        sz="$(size "$bin" 2>/dev/null | awk 'NR==2{print $1}')"
        printf "  %-10s: %s bytes (.text)\n" "$config" "${sz:-?}"
    fi
done
echo ""
