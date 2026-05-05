#!/usr/bin/env bash
# scripts/bench/run_spike_bench_variant.sh
#
# Launch Spike benchmark jobs in the background (nohup) for all polymul
# variants of a given benchmark set. Prints PIDs and monitoring hints, then exits.
#
# Default: polymul micro-bench only (Spike-first hotspot iteration loop).
# Use --full at benchmark milestones to also launch full KEM benches.
#
# Canonical benchmark sets and their default variants:
#   baseline_ref     -> karatsuba_ref
#   polymul_zbc      -> karatsuba_zbc
#   fafft_zbc        -> fafft_zbc
#   hybrid           -> hybrid
#   hybrid_zbc       -> hybrid   (legacy alias)
#   hybrid_zbc_v2    -> hybrid   (legacy alias)
#   hybrid_2way_zbc  -> hybrid   (legacy alias)
#   polymul_zvbc     -> karatsuba_zvbc
#   polymul_zvbc_v2  -> fafft_zvbc
#
# Passing a known benchmark set automatically restricts benchmarks to its
# canonical variant. Use --variant to override for ad-hoc comparisons.
#
# Usage:
#   scripts/bench/run_spike_bench_variant.sh <bench_set> [options]
#
# Options:
#   --variant <grep-pattern>   Override the canonical variant filter
#                              e.g. "karatsuba_zbc" or "fafft_zbc|hybrid"
#   --full                     Also launch full KEM benchmarks (keypair/encaps/decaps)
#   --no-polymul               Skip polymul micro-benchmarks (requires --full)
#   --iter <N>                 Number of iterations baked into the ELF (default: 1)
#                              Used to name output CSVs as _N{N}.csv
#
# Examples:
#   scripts/bench/run_spike_bench_variant.sh baseline_ref
#   scripts/bench/run_spike_bench_variant.sh polymul_zbc
#   scripts/bench/run_spike_bench_variant.sh polymul_zbc --full
#   scripts/bench/run_spike_bench_variant.sh polymul_zvbc --full
#   scripts/bench/run_spike_bench_variant.sh polymul_zbc --variant "karatsuba_zbc|karatsuba_ref"
#
# Output files written to analysis/benchmarks/<track>/<bench_set>/<run_label>/,
# where run_label uses [variant]_[bench_type]_[YYYYMMDD_HHMM]:
#   hqc{level}_{variant}_polymul_N{N}.csv  polymul micro-bench CSV
#   hqc{level}_{variant}_N{N}.csv          full KEM bench CSV (--full only)

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_runner.sh"

# Implemented variants come from scripts/implemented_variants.txt -- the same
# file CMakeLists.txt reads, so CMake and scripts are always in sync.
bench_load_implemented_variants "$REPO_ROOT"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
BUILD_DIR="${REPO_ROOT}/build/spike"

LEVELS=(128 192 256)

# ── Benchmark set -> canonical variant mapping ───────────────────────────────
declare -A BENCH_SET_VARIANT_MAP
BENCH_SET_VARIANT_MAP=(
    [baseline_ref]="karatsuba_ref"
    [polymul_zbc]="karatsuba_zbc"
    [fafft_zbc]="fafft_zbc"
    [hybrid]="hybrid"
    [hybrid_zbc]="hybrid"
    [hybrid_zbc_v2]="hybrid"
    [hybrid_2way_zbc]="hybrid"
    [polymul_zvbc]="karatsuba_zvbc"
    [polymul_zvbc_v2]="fafft_zvbc"
)

declare -A BENCH_SET_TRACK_MAP
BENCH_SET_TRACK_MAP=(
    [baseline_ref]="scalar"
    [polymul_zbc]="scalar"
    [fafft_zbc]="scalar"
    [hybrid]="scalar"
    [hybrid_zbc]="scalar"
    [hybrid_zbc_v2]="scalar"
    [hybrid_2way_zbc]="scalar"
    [polymul_zvbc]="vector"
    [polymul_zvbc_v2]="vector"
    [fafft_zvbc]="vector"
)

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <bench_set> [--variant <pattern>] [--full] [--no-polymul]" >&2
    exit 2
fi

BENCH_SET="$1"
shift

VARIANT_FILTER=""
DO_FULL=0
DO_POLYMUL=1
ITER=1
BENCH_TIME=$(date +'%Y%m%d_%H%M')

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
        --iter)
            ITER="$2"
            if ! [[ "$ITER" =~ ^[1-9][0-9]*$ ]]; then
                echo "ERROR: --iter requires a positive integer, got: $ITER" >&2
                exit 2
            fi
            shift 2
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

# Resolve variant filter: explicit --variant overrides the canonical mapping.
if [[ -z "$VARIANT_FILTER" ]]; then
    if [[ -v BENCH_SET_VARIANT_MAP["$BENCH_SET"] ]]; then
        VARIANT_FILTER="${BENCH_SET_VARIANT_MAP[$BENCH_SET]}"
        echo "[bench] Benchmark set '${BENCH_SET}' -> canonical variant '${VARIANT_FILTER}'"
    else
        echo "WARNING: Unknown benchmark set '${BENCH_SET}' -- no canonical variant; running all implemented variants." >&2
        echo "         Known benchmark sets: ${!BENCH_SET_VARIANT_MAP[*]}" >&2
    fi
else
    echo "[bench] Benchmark set '${BENCH_SET}' -> variant override '${VARIANT_FILTER}'"
fi

VARIANT_LABEL="$(bench_sanitize_label "${VARIANT_FILTER:-all}")"
RUN_LABEL="${VARIANT_LABEL}_${BENCH_SET}_${BENCH_TIME}"

# ── Preflight checks ──────────────────────────────────────────────────────────
if ! bench_require_executable "$SPIKE" "Spike not found"; then
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

BENCH_TRACK="${BENCH_SET_TRACK_MAP[$BENCH_SET]:-}"
if [[ -n "$BENCH_TRACK" ]]; then
    OUT_DIR="${REPO_ROOT}/analysis/benchmarks/${BENCH_TRACK}/${BENCH_SET}/${RUN_LABEL}"
else
    OUT_DIR="${REPO_ROOT}/analysis/benchmarks/${BENCH_SET}/${RUN_LABEL}"
fi
mkdir -p "$OUT_DIR"

# ── Helper: apply variant filter ─────────────────────────────────────────────
# ── Launch jobs ───────────────────────────────────────────────────────────────
LAUNCHED=0
declare -a JOB_LINES=()

for LEVEL in "${LEVELS[@]}"; do
    for VARIANT in "${IMPLEMENTED_VARIANTS[@]}"; do
        bench_variant_matches "$VARIANT" "$VARIANT_FILTER" || continue

        # ── Polymul micro-bench ───────────────────────────────────────────────
        if [[ $DO_POLYMUL -eq 1 ]]; then
            ELF="${BUILD_DIR}/bench_polymul_hqc${LEVEL}_${VARIANT}.elf"
            TARGET="bench_polymul_hqc${LEVEL}_${VARIANT}"
            PREFIX="hqc${LEVEL}_${VARIANT}"
            CSV="${OUT_DIR}/${PREFIX}_polymul_N${ITER}.csv"

            # Always do an incremental cmake build before running — no-op if nothing
            # changed, but catches stale ELFs when sources (e.g. bench.c) are updated.
            bench_build_target_if_configured "$BUILD_DIR" "$TARGET" "bench" || continue
            if ! bench_require_elf_or_hint "$ELF" "$TARGET" \
                "cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}" \
                "bench"; then
                continue
            fi

            LABEL="polymul_hqc${LEVEL}_${VARIANT}"
            PID=$(bench_launch_spike_job "$SPIKE" "$ISA" "$ELF" "$CSV")
            JOB_LINES+=("  PID ${PID}  ${LABEL}  ->  $(basename "$CSV")")
            LAUNCHED=$((LAUNCHED + 1))
        fi

        # ── Full KEM bench ────────────────────────────────────────────────────
        if [[ $DO_FULL -eq 1 ]]; then
            ELF="${BUILD_DIR}/bench_hqc${LEVEL}_${VARIANT}.elf"
            TARGET="bench_hqc${LEVEL}_${VARIANT}"
            PREFIX="hqc${LEVEL}_${VARIANT}"
            CSV="${OUT_DIR}/${PREFIX}_N${ITER}.csv"

            # Always do an incremental cmake build before running — no-op if nothing
            # changed, but catches stale ELFs when sources (e.g. bench.c) are updated.
            bench_build_target_if_configured "$BUILD_DIR" "$TARGET" "bench" || continue
            if ! bench_require_elf_or_hint "$ELF" "$TARGET" \
                "cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}" \
                "bench"; then
                continue
            fi

            LABEL="kem_hqc${LEVEL}_${VARIANT}"
            PID=$(bench_launch_spike_job "$SPIKE" "$ISA" "$ELF" "$CSV")
            JOB_LINES+=("  PID ${PID}  ${LABEL}  ->  $(basename "$CSV")")
            LAUNCHED=$((LAUNCHED + 1))
        fi
    done
done

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
if [[ $LAUNCHED -eq 0 ]]; then
    echo "[bench] No jobs launched. Check that ELFs are built in ${BUILD_DIR}/"
    echo "        cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}"
    echo "        cmake --build build/spike -j\$(nproc)"
else
    echo "[bench] Launched ${LAUNCHED} Spike job(s) for ${BENCH_SET}:"
    for line in "${JOB_LINES[@]}"; do
        echo "$line"
    done
    echo ""
    echo "Monitor:"
    echo "  watch -n 30 \"wc -c ${OUT_DIR}/*.csv\""
    echo "  ps aux | grep '[s]pike.*bench'"
    echo ""
    echo "When done, extract results:"
    echo "  python3 scripts/report/extract_results.py --bench-set ${BENCH_SET}"
fi
