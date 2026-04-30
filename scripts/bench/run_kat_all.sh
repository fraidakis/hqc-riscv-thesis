#!/usr/bin/env bash
# scripts/bench/run_kat_all.sh
#
# Run KAT for all implemented QEMU-compatible polymul variants x 3 security
# levels. Uses sim/qemu/run_kat.sh to execute each ELF and tests/kat/kat_check.py
# to compare captured shared-secret lines against the reference RSP vectors.
#
# Implemented variants and build gate come from scripts/implemented_variants.txt.
# Missing ELFs for implemented variants are auto-built before running.
# fafft_zvbc is always skipped here — QEMU does not support Zvbc.
# Use scripts/bench/spike_kat.sh for fafft_zvbc.
#
# Usage:
#   scripts/bench/run_kat_all.sh
#
# Exit code: 0 if all runs passed, 1 if any failed or failed to build.
# All implemented variants are always attempted — failure does NOT abort early.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Implemented variants come from scripts/implemented_variants.txt — the same
# file CMakeLists.txt reads, so CMake and scripts are always in sync.
mapfile -t IMPLEMENTED_VARIANTS < "${REPO_ROOT}/scripts/implemented_variants.txt"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
RUN_QEMU_KAT="${REPO_ROOT}/sim/qemu/run_kat.sh"
KAT_CHECK="${REPO_ROOT}/tests/kat/kat_check.py"
BUILD_DIR="${REPO_ROOT}/build"

# Reference .rsp files (NIST KAT vectors)
RSP_128="${REPO_ROOT}/vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp"
RSP_192="${REPO_ROOT}/vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp"
RSP_256="${REPO_ROOT}/vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp"

LEVELS=(128 192 256)

# ── Preflight checks ──────────────────────────────────────────────────────────
for rsp in "$RSP_128" "$RSP_192" "$RSP_256"; do
    if [[ ! -f "$rsp" ]]; then
        echo "ERROR: RSP file not found: $rsp" >&2
        echo "       Run: git submodule update --init vendor/hqc-ref" >&2
        exit 2
    fi
done

if [[ ! -x "$RUN_QEMU_KAT" ]]; then
    echo "ERROR: sim/qemu/run_kat.sh not found or not executable" >&2
    exit 2
fi

if [[ ! -f "$KAT_CHECK" ]]; then
    echo "ERROR: tests/kat/kat_check.py not found" >&2
    exit 2
fi

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo "ERROR: build dir not configured: ${BUILD_DIR}" >&2
    echo "       Run: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake" >&2
    exit 2
fi

# ── Run all implemented variants (QEMU only — no fafft_zvbc) ─────────────────
declare -A RESULTS
OVERALL_RC=0

for LEVEL in "${LEVELS[@]}"; do
    case "$LEVEL" in
        128) RSP="$RSP_128" ;;
        192) RSP="$RSP_192" ;;
        256) RSP="$RSP_256" ;;
    esac

    for VARIANT in "${IMPLEMENTED_VARIANTS[@]}"; do
        # fafft_zvbc requires Spike — skip silently in this QEMU script
        if [[ "$VARIANT" == "fafft_zvbc" ]]; then
            continue
        fi

        ELF="${BUILD_DIR}/hqc${LEVEL}_${VARIANT}.elf"
        KAT_TARGET="hqc${LEVEL}_${VARIANT}"
        KEY="${LEVEL}_${VARIANT}"

        # Auto-build if ELF is missing
        if [[ ! -f "$ELF" ]]; then
            echo "[run_kat_all] BUILD ${KAT_TARGET}: ELF not found, building..." >&2
            if ! cmake --build "$BUILD_DIR" --target "$KAT_TARGET" -j"$(nproc)" >&2; then
                RESULTS["$KEY"]="BUILD_FAIL"
                OVERALL_RC=1
                continue
            fi
        fi

        echo "[run_kat_all] Running ${KAT_TARGET} ..." >&2

        LOG="$(mktemp /tmp/hqc_kat_all_XXXXXX.log)"
        "$RUN_QEMU_KAT" "$ELF" >"$LOG" 2>&1 || true
        if python3 "$KAT_CHECK" --actual "$LOG" --rsp "$RSP" --level "$LEVEL" >/dev/null 2>&1; then
            RESULTS["$KEY"]="PASS"
        else
            RESULTS["$KEY"]="FAIL"
            OVERALL_RC=1
        fi
        rm -f "$LOG"
    done
done

# ── Print summary table ───────────────────────────────────────────────────────
# Filter out fafft_zvbc from display columns (not run here)
DISPLAY_VARIANTS=()
for V in "${IMPLEMENTED_VARIANTS[@]}"; do
    [[ "$V" != "fafft_zvbc" ]] && DISPLAY_VARIANTS+=("$V")
done

echo ""
echo "─────────────────────────────────────────────────────────────────"
printf "%-12s" "KAT results"
for VARIANT in "${DISPLAY_VARIANTS[@]}"; do
    printf "  %-15s" "$VARIANT"
done
echo ""
echo "─────────────────────────────────────────────────────────────────"

for LEVEL in "${LEVELS[@]}"; do
    printf "%-12s" "HQC-${LEVEL}"
    for VARIANT in "${DISPLAY_VARIANTS[@]}"; do
        KEY="${LEVEL}_${VARIANT}"
        RESULT="${RESULTS[$KEY]:-SKIP}"
        printf "  %-15s" "$RESULT"
    done
    echo ""
done

echo "─────────────────────────────────────────────────────────────────"
echo ""
echo "NOTE: fafft_zvbc requires Spike (Zvbc not supported by QEMU)."
echo "      scripts/bench/spike_kat.sh build/hqc128_fafft_zvbc.elf \\"
echo "          vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp"
echo ""

if [[ $OVERALL_RC -ne 0 ]]; then
    echo "OVERALL: FAIL (one or more KAT checks failed or did not build)" >&2
else
    echo "OVERALL: PASS (all implemented variants passed)"
fi

exit $OVERALL_RC
