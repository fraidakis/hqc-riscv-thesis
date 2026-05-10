#!/usr/bin/env bash
# Run dudect workflow for all implemented variants (scalar + vector).
#
# Vector and Zbc-only variants are expected to be skipped by check_ct.sh on
# x86-64 and should be analyzed by BINSEC; this script still records them.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

LEVELS="${LEVELS:-128}"
REPORT_DIR="$REPO_ROOT/analysis/ct_verify/dudect"
STAMP="$(date +%Y%m%d_%H%M%S)"
SUMMARY="$REPORT_DIR/dudect_summary_${STAMP}.csv"

mkdir -p "$REPORT_DIR"

echo "level,variant,status,report_or_reason" > "$SUMMARY"

while IFS= read -r variant; do
    [[ -z "$variant" ]] && continue
    for level in $LEVELS; do
        echo "[dudect_all] level=${level} variant=${variant}"
        out_file="$REPORT_DIR/dudect_hqc${level}_${variant}.txt"

        set +e
        cmd_out="$($REPO_ROOT/scripts/ci/check_ct.sh --dudect "$level" "$variant" 2>&1)"
        rc=$?
        set -e

        printf "%s\n" "$cmd_out" > "$out_file"

        if grep -q "SKIP:" "$out_file"; then
            echo "${level},${variant},SKIP,binsec_required" >> "$SUMMARY"
            continue
        fi

        if [[ $rc -eq 0 ]]; then
            echo "${level},${variant},PASS,${out_file}" >> "$SUMMARY"
        else
            echo "${level},${variant},FAIL,${out_file}" >> "$SUMMARY"
        fi
    done
done < "$REPO_ROOT/scripts/implemented_variants.txt"

echo "[dudect_all] summary: $SUMMARY"
