#!/usr/bin/env bash
# Run BINSEC CT verification for configured variants.
# Uses config files in analysis/ct_verify/binsec/configs/*.binsec.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
CONFIG_DIR="$SCRIPT_DIR/configs"
BUILD_DIR="${1:-$REPO_ROOT/build/spike}"
LEVELS="${LEVELS:-128}"
DRY_RUN=0

if [[ "${2:-}" == "--dry-run" || "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=1
fi

OUT_DIR="$SCRIPT_DIR/reports"
mkdir -p "$OUT_DIR"
STAMP="$(date +%Y%m%d_%H%M%S)"
SUMMARY="$OUT_DIR/binsec_summary_${STAMP}.csv"

echo "level,variant,binary,status,report" > "$SUMMARY"

run_one() {
    local cfg="$1"
    local level="$2"

    # shellcheck disable=SC1090
    source "$cfg"

    local target_rel="${TARGET_PATTERN//\{LEVEL\}/$level}"
    local target_bin="$BUILD_DIR/$target_rel"
    local report="$OUT_DIR/binsec_${VARIANT}_hqc${level}_${STAMP}.txt"

    if [[ ! -f "$target_bin" ]]; then
        echo "${level},${VARIANT},${target_bin},MISSING,${report}" >> "$SUMMARY"
        return 0
    fi

    if [[ $DRY_RUN -eq 1 ]]; then
        {
            echo "[binsec_variants] DRY RUN"
            echo "variant=${VARIANT}"
            echo "entry=${ENTRY_SYMBOL}"
            echo "secret_inputs=${SECRET_INPUTS}"
            echo "observables=${OBSERVABLES}"
            echo "binary=${target_bin}"
        } > "$report"
        echo "${level},${VARIANT},${target_bin},DRY_RUN,${report}" >> "$SUMMARY"
        return 0
    fi

    if ! command -v binsec >/dev/null 2>&1; then
        {
            echo "[binsec_variants] BINSEC not installed"
            echo "binary=${target_bin}"
            echo "config=${cfg}"
        } > "$report"
        echo "${level},${VARIANT},${target_bin},BINSEC_MISSING,${report}" >> "$SUMMARY"
        return 0
    fi

    {
        echo "; Auto-generated BINSEC script for ${VARIANT}"
        echo "; target binary: ${target_bin}"
        echo "; entry symbol: ${ENTRY_SYMBOL}"
        echo "; secret inputs: ${SECRET_INPUTS}"
        echo "; observables: ${OBSERVABLES}"
    } > "$OUT_DIR/current_${VARIANT}.script.binsec"

    if binsec --script "$OUT_DIR/current_${VARIANT}.script.binsec" "$target_bin" > "$report" 2>&1; then
        echo "${level},${VARIANT},${target_bin},PASS,${report}" >> "$SUMMARY"
    else
        echo "${level},${VARIANT},${target_bin},FAIL,${report}" >> "$SUMMARY"
    fi
}

for level in $LEVELS; do
    for cfg in "$CONFIG_DIR"/*.binsec; do
        [[ -f "$cfg" ]] || continue
        run_one "$cfg" "$level"
    done
done

echo "[binsec_variants] summary: $SUMMARY"
