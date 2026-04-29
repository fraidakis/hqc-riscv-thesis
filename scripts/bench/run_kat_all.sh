#!/usr/bin/env bash
# scripts/bench/run_kat_all.sh
#
# Run KAT for all 4 QEMU-compatible polymul variants × 3 security levels.
# Delegates to tests/kat/run_kat.sh (handles QEMU flags, timeout, kat_check.py).
# fafft_zvbc is skipped — QEMU does not support the Zvbc extension.
# Use scripts/bench/spike_kat.sh for fafft_zvbc.
#
# Usage:
#   scripts/bench/run_kat_all.sh
#
# Exit code: 0 if all 12 runs passed, 1 if any failed.
# All 12 runs are always attempted — failure does NOT abort early.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
RUN_KAT="${REPO_ROOT}/tests/kat/run_kat.sh"

# Reference .rsp files (NIST KAT vectors)
RSP_128="${REPO_ROOT}/vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp"
RSP_192="${REPO_ROOT}/vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp"
RSP_256="${REPO_ROOT}/vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp"

VARIANTS=(karatsuba_ref karatsuba_zbc fafft_zbc hybrid_zbc)
LEVELS=(128 192 256)

# ── Preflight checks ──────────────────────────────────────────────────────────
for rsp in "$RSP_128" "$RSP_192" "$RSP_256"; do
    if [[ ! -f "$rsp" ]]; then
        echo "ERROR: RSP file not found: $rsp" >&2
        echo "       Run: git submodule update --init vendor/hqc-ref" >&2
        exit 2
    fi
done

if [[ ! -x "$RUN_KAT" ]]; then
    echo "ERROR: tests/kat/run_kat.sh not found or not executable" >&2
    exit 2
fi

# ── Run all 12 combinations ───────────────────────────────────────────────────
# Results stored as associative array keyed by "level_variant"
declare -A RESULTS
OVERALL_RC=0

for LEVEL in "${LEVELS[@]}"; do
    case "$LEVEL" in
        128) RSP="$RSP_128" ;;
        192) RSP="$RSP_192" ;;
        256) RSP="$RSP_256" ;;
    esac

    for VARIANT in "${VARIANTS[@]}"; do
        ELF="${REPO_ROOT}/build/hqc${LEVEL}_${VARIANT}.elf"
        KEY="${LEVEL}_${VARIANT}"

        if [[ ! -f "$ELF" ]]; then
            RESULTS["$KEY"]="BUILD_MISSING"
            echo "[run_kat_all] SKIP  hqc${LEVEL}_${VARIANT}: ELF not found (build first)" >&2
            continue
        fi

        echo "[run_kat_all] Running hqc${LEVEL}_${VARIANT} ..." >&2
        if bash "$RUN_KAT" "$ELF" "$RSP" 2>/dev/null; then
            RESULTS["$KEY"]="PASS"
        else
            RESULTS["$KEY"]="FAIL"
            OVERALL_RC=1
        fi
    done
done

# ── Print summary table ───────────────────────────────────────────────────────
echo ""
echo "─────────────────────────────────────────────────────────────────"
printf "%-12s" "KAT results"
for VARIANT in "${VARIANTS[@]}"; do
    printf "  %-15s" "$VARIANT"
done
echo ""
echo "─────────────────────────────────────────────────────────────────"

for LEVEL in "${LEVELS[@]}"; do
    printf "%-12s" "HQC-${LEVEL}"
    for VARIANT in "${VARIANTS[@]}"; do
        KEY="${LEVEL}_${VARIANT}"
        RESULT="${RESULTS[$KEY]:-SKIP}"
        printf "  %-15s" "$RESULT"
    done
    echo ""
done

echo "─────────────────────────────────────────────────────────────────"
echo ""
echo "NOTE: fafft_zvbc skipped (Zvbc not supported by QEMU system emulation)."
echo "      To test fafft_zvbc under Spike:"
echo "      scripts/bench/spike_kat.sh build/hqc128_fafft_zvbc.elf \\"
echo "          vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp"
echo ""

if [[ $OVERALL_RC -ne 0 ]]; then
    echo "OVERALL: FAIL (one or more KAT checks failed)" >&2
else
    echo "OVERALL: PASS (all completed runs passed)"
fi

exit $OVERALL_RC
