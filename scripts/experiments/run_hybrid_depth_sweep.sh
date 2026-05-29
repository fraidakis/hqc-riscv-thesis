#!/usr/bin/env bash
# scripts/experiments/run_hybrid_depth_sweep.sh
#
# Hybrid Karatsuba-FAFFT depth sweep.
#
# Builds bench_polymul_hqc{128,192,256}_hybrid for each Karatsuba depth
# d in {0,1,2,3,4} using the existing build/spike directory, then runs Spike
# and saves CSVs to results/simulation/benchmarks/experiments/hybrid/.
#
# Usage:
#   scripts/experiments/run_hybrid_depth_sweep.sh [--depths "0 1 2 3 4"] [--levels "128 192 256"]
#
# Output:
#   results/simulation/benchmarks/experiments/hybrid/hqc{LEVEL}_hybrid_d{D}_polymul_N1.csv
#
# The script reconfigures the build/spike CMake cache for each depth (only
# the HYBRID_KARATSUBA_DEPTH cache variable changes; all other flags are
# preserved).  Only hybrid objects are recompiled between depths.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${REPO_ROOT}/scripts/bench/lib_bench.sh"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
BUILD_DIR="${REPO_ROOT}/build/spike"
OUT_DIR="${REPO_ROOT}/results/simulation/benchmarks/experiments/hybrid"

DEPTHS="0 1 2 3 4"
LEVELS="128 192 256"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --depths) DEPTHS="$2"; shift 2 ;;
        --levels) LEVELS="$2"; shift 2 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

mkdir -p "${OUT_DIR}"
LOG="${OUT_DIR}/sweep_events.log"
echo "=== Depth sweep started: $(date) ===" | tee -a "${LOG}"

if ! bench_require_executable "$SPIKE" "Spike not found"; then
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

for DEPTH in $DEPTHS; do
    echo ""
    echo "── Depth d=${DEPTH} ────────────────────────────────────────────────"

    # Reconfigure build/spike with the new depth
    echo "[sweep] cmake -DHYBRID_KARATSUBA_DEPTH=${DEPTH} ${BUILD_DIR}" | tee -a "${LOG}"
    cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
        -DHYBRID_KARATSUBA_DEPTH="${DEPTH}" \
        --no-warn-unused-cli -Wno-dev >/dev/null 2>&1

    for LEVEL in $LEVELS; do
        TARGET="bench_polymul_hqc${LEVEL}_hybrid"
        ELF="${BUILD_DIR}/${TARGET}.elf"
        CSV="${OUT_DIR}/hqc${LEVEL}_hybrid_d${DEPTH}_polymul_N1.csv"

        echo "[sweep] Building ${TARGET}..." | tee -a "${LOG}"
        cmake --build "${BUILD_DIR}" --target "${TARGET}" -j"$(nproc)" 2>&1 | tail -3

        echo "[sweep] Running Spike for HQC-${LEVEL} d=${DEPTH}..." | tee -a "${LOG}"
        START_T=$(date +%s)
        "${SPIKE}" \
            --isa="${ISA}" \
            +signature="${BUILD_DIR}/${TARGET}.sig" \
            "${ELF}" >"${CSV}" 2>/dev/null
        END_T=$(date +%s)
        ELAPSED=$(( END_T - START_T ))

        # Print the CSV result
        INSTRET=$(grep "^vect_mul," "${CSV}" | cut -d',' -f7 || echo "N/A")
        echo "[sweep] HQC-${LEVEL} d=${DEPTH}: median_instret=${INSTRET}  (${ELAPSED}s)" \
            | tee -a "${LOG}"
    done
done

echo ""
echo "=== Depth sweep completed: $(date) ===" | tee -a "${LOG}"
echo ""
echo "Results in ${OUT_DIR}:"
echo ""
echo "| Level   | d=0 | d=1 | d=2 | d=3 | d=4 |"
echo "|---------|-----|-----|-----|-----|-----|"
for LEVEL in $LEVELS; do
    ROW="| HQC-${LEVEL} |"
    for DEPTH in 0 1 2 3 4; do
        CSV="${OUT_DIR}/hqc${LEVEL}_hybrid_d${DEPTH}_polymul_N1.csv"
        if [[ -f "${CSV}" ]]; then
            VAL=$(grep "^vect_mul," "${CSV}" | cut -d',' -f7 2>/dev/null || echo "?")
        else
            VAL="?"
        fi
        ROW="${ROW} ${VAL} |"
    done
    echo "${ROW}"
done
