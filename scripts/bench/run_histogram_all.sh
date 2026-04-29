#!/usr/bin/env bash
# scripts/bench/run_histogram_all.sh
#
# Run Spike --histogram for polymul micro-bench ELFs (launch mode), or print
# sorted top-20 instruction frequency tables from existing hist files (report mode).
#
# Both the instruction count CSV (stdout) and histogram (stderr) are captured in
# a single Spike run, so you get both metrics simultaneously.
#
# Usage:
#   scripts/bench/run_histogram_all.sh <phase> [options]
#
# Options:
#   --variant <grep-pattern>   Filter variants (e.g. "karatsuba_zbc")
#   --launch                   (default) Launch Spike --histogram jobs in background
#   --report                   Print top-20 instruction table from existing hist files
#
# Examples:
#   scripts/bench/run_histogram_all.sh phase0b
#   scripts/bench/run_histogram_all.sh phase1a --variant karatsuba_zbc
#   scripts/bench/run_histogram_all.sh phase1a --report
#   scripts/bench/run_histogram_all.sh phase1plus --variant fafft_zvbc --launch
#
# Output files (--launch mode):
#   analysis/benchmarks/<phase>/hqc{level}_{variant}_polymul_N1.csv
#   analysis/benchmarks/<phase>/hqc{level}_{variant}_hist.txt
#
# Note: --histogram adds negligible overhead vs. a plain bench run.
#       The CSV output is usable for extract_results.py just like regular bench output.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Implemented variants come from scripts/implemented_variants.txt — the same
# file CMakeLists.txt reads, so CMake and scripts are always in sync.
mapfile -t IMPLEMENTED_VARIANTS < "${REPO_ROOT}/scripts/implemented_variants.txt"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc"
BUILD_DIR="${REPO_ROOT}/build-spike-probe"

LEVELS=(128 192 256)

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <phase> [--variant <pattern>] [--launch|--report]" >&2
    exit 2
fi

PHASE="$1"
shift

VARIANT_FILTER=""
MODE="launch"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant)
            VARIANT_FILTER="$2"
            shift 2
            ;;
        --launch)
            MODE="launch"
            shift
            ;;
        --report)
            MODE="report"
            shift
            ;;
        *)
            echo "ERROR: Unknown option: $1" >&2
            exit 2
            ;;
    esac
done

OUT_DIR="${REPO_ROOT}/analysis/benchmarks/${PHASE}"

# ── Helper: apply variant filter ─────────────────────────────────────────────
variant_matches() {
    local v="$1"
    if [[ -z "$VARIANT_FILTER" ]]; then
        return 0
    fi
    echo "$v" | grep -qE "$VARIANT_FILTER"
}

# ─────────────────────────────────────────────────────────────────────────────
# REPORT MODE: print sorted top-20 from existing hist files
# ─────────────────────────────────────────────────────────────────────────────
if [[ "$MODE" == "report" ]]; then
    if [[ ! -d "$OUT_DIR" ]]; then
        echo "[histogram] No phase directory found: $OUT_DIR" >&2
        echo "            Run --launch first to generate histogram files." >&2
        exit 0
    fi

    FOUND=0
    for LEVEL in "${LEVELS[@]}"; do
        for VARIANT in "${IMPLEMENTED_VARIANTS[@]}"; do
            variant_matches "$VARIANT" || continue

            HIST="${OUT_DIR}/hqc${LEVEL}_${VARIANT}_hist.txt"
            if [[ ! -f "$HIST" ]]; then
                echo "[histogram] SKIP  hqc${LEVEL}_${VARIANT}: hist file not found" >&2
                continue
            fi

            FOUND=$((FOUND + 1))
            echo ""
            echo "=== hqc${LEVEL}_${VARIANT} (${PHASE}) ==="
            # Spike histogram format: instruction<TAB>count (tab-separated)
            # Sort by count descending, show top 20
            if sort -t$'\t' -k2 -rn "$HIST" | head -20 | \
               awk -F'\t' '{printf "  %-30s %s\n", $1, $2}'; then
                : # success
            else
                echo "  (empty or malformed histogram file)"
            fi
        done
    done

    if [[ $FOUND -eq 0 ]]; then
        echo ""
        echo "[histogram] No histogram files found in ${OUT_DIR}/"
        echo "            Run: scripts/bench/run_histogram_all.sh ${PHASE} --launch"
    fi
    exit 0
fi

# ─────────────────────────────────────────────────────────────────────────────
# LAUNCH MODE: start Spike --histogram jobs in background
# ─────────────────────────────────────────────────────────────────────────────
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found: $SPIKE" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

mkdir -p "$OUT_DIR"
PID_FILE="${OUT_DIR}/pids.txt"

LAUNCHED=0
declare -a JOB_LINES=()

for LEVEL in "${LEVELS[@]}"; do
    for VARIANT in "${IMPLEMENTED_VARIANTS[@]}"; do
        variant_matches "$VARIANT" || continue

        ELF="${BUILD_DIR}/bench_polymul_hqc${LEVEL}_${VARIANT}.elf"
        TARGET="bench_polymul_hqc${LEVEL}_${VARIANT}"
        PREFIX="hqc${LEVEL}_${VARIANT}"
        CSV="${OUT_DIR}/${PREFIX}_polymul_N1.csv"
        HIST="${OUT_DIR}/${PREFIX}_hist.txt"

        if [[ ! -f "$ELF" ]]; then
            if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
                echo "[histogram] BUILD ${TARGET}: ELF not found, building..." >&2
                cmake --build "$BUILD_DIR" --target "$TARGET" -j"$(nproc)" >&2 || {
                    echo "[histogram] BUILD FAIL ${TARGET}" >&2; continue
                }
            else
                echo "[histogram] SKIP  ${TARGET}: build dir not configured" >&2
                echo "              cmake -S . -B build-spike-probe -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=1" >&2
                continue
            fi
        fi

        # Spike --histogram: stdout → CSV, stderr → histogram text
        # Both captured in a single Spike run.
        nohup "$SPIKE" --isa="$ISA" --histogram "$ELF" \
            > "$CSV" \
            2> "$HIST" &

        PID=$!
        echo "$(date '+%Y-%m-%dT%H:%M:%S') PID=$PID bench_polymul_hqc${LEVEL}_${VARIANT} (histogram)" >> "$PID_FILE"
        JOB_LINES+=("  PID ${PID}  bench_polymul_hqc${LEVEL}_${VARIANT}  →  $(basename "$CSV") + $(basename "$HIST")")
        LAUNCHED=$((LAUNCHED + 1))
    done
done

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
if [[ $LAUNCHED -eq 0 ]]; then
    echo "[histogram] No jobs launched. Check that polymul ELFs are built in ${BUILD_DIR}/"
    echo "            cmake --build build-spike-probe -j\$(nproc)"
else
    echo "[histogram] Launched ${LAUNCHED} Spike --histogram job(s) for ${PHASE}:"
    for line in "${JOB_LINES[@]}"; do
        echo "$line"
    done
    echo ""
    echo "Monitor:"
    echo "  ps aux | grep '[s]pike.*bench_polymul'"
    echo "  watch -n 30 \"wc -c ${OUT_DIR}/*_hist.txt\""
    echo ""
    echo "When done, view top instructions:"
    echo "  scripts/bench/run_histogram_all.sh ${PHASE} --report"
fi
