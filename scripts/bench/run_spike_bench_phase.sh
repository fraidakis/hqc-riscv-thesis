#!/usr/bin/env bash
# scripts/bench/run_spike_bench_phase.sh
#
# Launch Spike benchmark jobs in the background (nohup) for all polymul
# variants of a given phase.  Prints PIDs and monitoring hints, then exits.
#
# Default: polymul micro-bench only (~20–150 min each).
# Use --full at phase milestones to also launch full KEM benches.
#
# Usage:
#   scripts/bench/run_spike_bench_phase.sh <phase> [options]
#
# Options:
#   --variant <grep-pattern>   Filter variants by grep pattern
#                              e.g. "karatsuba_zbc" or "fafft_zbc|hybrid_zbc"
#   --full                     Also launch full KEM benchmarks (keypair/encaps/decaps)
#   --no-polymul               Skip polymul micro-benchmarks (requires --full)
#
# Examples:
#   scripts/bench/run_spike_bench_phase.sh phase0b
#   scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc
#   scripts/bench/run_spike_bench_phase.sh phase1a --full
#   scripts/bench/run_spike_bench_phase.sh phase1plus --variant fafft_zvbc --full
#
# Output files written to analysis/benchmarks/<phase>/:
#   hqc{level}_{variant}_polymul_N1.csv   polymul micro-bench CSV
#   hqc{level}_{variant}_polymul.log      Spike stderr (errors/warnings)
#   hqc{level}_{variant}_N1.csv           full KEM bench CSV (--full only)
#   hqc{level}_{variant}_spike.log        full KEM Spike stderr (--full only)
#   pids.txt                              PID log for all launched jobs

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc"
BUILD_DIR="${REPO_ROOT}/build-spike-probe"

VARIANTS=(karatsuba_ref karatsuba_zbc fafft_zbc hybrid_zbc fafft_zvbc)
LEVELS=(128 192 256)

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <phase> [--variant <pattern>] [--full] [--no-polymul]" >&2
    exit 2
fi

PHASE="$1"
shift

VARIANT_FILTER=""
DO_FULL=0
DO_POLYMUL=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant)
            VARIANT_FILTER="$2"
            shift 2
            ;;
        --full)
            DO_FULL=1
            shift
            ;;
        --no-polymul)
            DO_POLYMUL=0
            shift
            ;;
        *)
            echo "ERROR: Unknown option: $1" >&2
            exit 2
            ;;
    esac
done

if [[ $DO_POLYMUL -eq 0 && $DO_FULL -eq 0 ]]; then
    echo "ERROR: --no-polymul requires --full (nothing to do otherwise)" >&2
    exit 2
fi

# ── Preflight checks ──────────────────────────────────────────────────────────
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found: $SPIKE" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

OUT_DIR="${REPO_ROOT}/analysis/benchmarks/${PHASE}"
mkdir -p "$OUT_DIR"

PID_FILE="${OUT_DIR}/pids.txt"

# ── Helper: apply variant filter ─────────────────────────────────────────────
variant_matches() {
    local v="$1"
    if [[ -z "$VARIANT_FILTER" ]]; then
        return 0
    fi
    echo "$v" | grep -qE "$VARIANT_FILTER"
}

# ── Launch jobs ───────────────────────────────────────────────────────────────
LAUNCHED=0
declare -a JOB_LINES=()

for LEVEL in "${LEVELS[@]}"; do
    for VARIANT in "${VARIANTS[@]}"; do
        variant_matches "$VARIANT" || continue

        # ── Polymul micro-bench ───────────────────────────────────────────────
        if [[ $DO_POLYMUL -eq 1 ]]; then
            ELF="${BUILD_DIR}/bench_polymul_hqc${LEVEL}_${VARIANT}.elf"
            PREFIX="hqc${LEVEL}_${VARIANT}"
            CSV="${OUT_DIR}/${PREFIX}_polymul_N1.csv"
            LOG="${OUT_DIR}/${PREFIX}_polymul.log"

            if [[ ! -f "$ELF" ]]; then
                echo "[bench] SKIP  bench_polymul_hqc${LEVEL}_${VARIANT}: ELF not found" >&2
                echo "              Build with: cmake --build build-spike-probe --target bench_polymul_hqc${LEVEL}_${VARIANT} -j\$(nproc)" >&2
                continue
            fi

            nohup "$SPIKE" --isa="$ISA" "$ELF" > "$CSV" 2> "$LOG" &
            PID=$!
            echo "$(date '+%Y-%m-%dT%H:%M:%S') PID=$PID bench_polymul_hqc${LEVEL}_${VARIANT}" >> "$PID_FILE"
            JOB_LINES+=("  PID ${PID}  bench_polymul_hqc${LEVEL}_${VARIANT}  →  $(basename "$CSV")")
            LAUNCHED=$((LAUNCHED + 1))
        fi

        # ── Full KEM bench ────────────────────────────────────────────────────
        if [[ $DO_FULL -eq 1 ]]; then
            ELF="${BUILD_DIR}/bench_hqc${LEVEL}_${VARIANT}.elf"
            PREFIX="hqc${LEVEL}_${VARIANT}"
            CSV="${OUT_DIR}/${PREFIX}_N1.csv"
            LOG="${OUT_DIR}/${PREFIX}_spike.log"

            if [[ ! -f "$ELF" ]]; then
                echo "[bench] SKIP  bench_hqc${LEVEL}_${VARIANT}: ELF not found" >&2
                echo "              Build with: cmake --build build-spike-probe --target bench_hqc${LEVEL}_${VARIANT} -j\$(nproc)" >&2
                continue
            fi

            nohup "$SPIKE" --isa="$ISA" "$ELF" > "$CSV" 2> "$LOG" &
            PID=$!
            echo "$(date '+%Y-%m-%dT%H:%M:%S') PID=$PID bench_hqc${LEVEL}_${VARIANT}" >> "$PID_FILE"
            JOB_LINES+=("  PID ${PID}  bench_hqc${LEVEL}_${VARIANT}            →  $(basename "$CSV")")
            LAUNCHED=$((LAUNCHED + 1))
        fi
    done
done

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
if [[ $LAUNCHED -eq 0 ]]; then
    echo "[bench] No jobs launched. Check that ELFs are built in ${BUILD_DIR}/"
    echo "        cmake -S . -B build-spike-probe -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=1"
    echo "        cmake --build build-spike-probe -j\$(nproc)"
else
    echo "[bench] Launched ${LAUNCHED} Spike job(s) for ${PHASE}:"
    for line in "${JOB_LINES[@]}"; do
        echo "$line"
    done
    echo ""
    echo "Monitor:"
    echo "  watch -n 30 \"wc -c ${OUT_DIR}/*.csv\""
    echo "  ps aux | grep '[s]pike.*bench'"
    echo ""
    echo "When done, extract results:"
    echo "  python3 scripts/report/extract_results.py --phase ${PHASE}"
fi
