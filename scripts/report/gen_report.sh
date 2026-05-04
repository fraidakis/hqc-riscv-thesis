#!/usr/bin/env bash
# scripts/report/gen_report.sh
#
# Master report generator.  Calls extract_results.py → plot.py → amdahl.py,
# then assembles a dated Markdown report in analysis/report/.
#
# Soft-fail design: each sub-command is optional.  The report is always written
# even if individual steps fail (they appear as SKIPPED with the error message).
#
# Usage:
#   scripts/report/gen_report.sh [options]
#
# Options:
#   --bench-set NAME Restrict extract_results.py to one benchmark set
#   --fraction N     vect_mul fraction of runtime for Amdahl sweep (default: 0.90)
#
# Output:
#   analysis/report/report_YYYY-MM-DD.md

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

BENCH_DIR="${REPO_ROOT}/analysis/benchmarks"
FIGURE_DIR="${REPO_ROOT}/docs/figures"
REPORT_DIR="${REPO_ROOT}/analysis/report"
EXTRACT_PY="${SCRIPT_DIR}/extract_results.py"
PLOT_PY="${REPO_ROOT}/analysis/benchmarks/plot.py"
AMDAHL_PY="${REPO_ROOT}/analysis/benchmarks/amdahl.py"

# ── Argument parsing ──────────────────────────────────────────────────────────
BENCH_SET_ARG=""
FRACTION="0.90"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --bench-set)
            BENCH_SET_ARG="--bench-set $2"
            shift 2
            ;;
        --fraction)
            FRACTION="$2"
            shift 2
            ;;
        --help|-h)
            sed -n '2,20p' "${BASH_SOURCE[0]}" | grep '^#' | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "ERROR: Unknown option: $1" >&2
            exit 2
            ;;
    esac
done

# ── Setup ─────────────────────────────────────────────────────────────────────
mkdir -p "$REPORT_DIR"
DATE="$(date '+%Y-%m-%d')"
REPORT="${REPORT_DIR}/report_${DATE}.md"

GIT_SHA="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo 'no-git')"

BENCH_SETS_FOUND="$(find "${BENCH_DIR}" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | sort | tr '\n' ' ' || echo 'none')"

echo "[gen_report] Generating report: $REPORT" >&2

# ── Step 1: extract_results.py ────────────────────────────────────────────────
TMP_MD="$(mktemp /tmp/hqc_results_XXXXXX.md)"
TMP_LATEX="$(mktemp /tmp/hqc_results_XXXXXX.tex)"
trap 'rm -f "$TMP_MD" "$TMP_LATEX"' EXIT

EXTRACT_STATUS=""
if python3 "$EXTRACT_PY" $BENCH_SET_ARG --write-summary-csv > "$TMP_MD" 2>&1; then
    EXTRACT_STATUS="OK"
    echo "[gen_report] extract_results.py: OK" >&2
else
    EXTRACT_STATUS="FAILED — see report for details"
    echo "[gen_report] extract_results.py: FAILED (see $REPORT)" >&2
fi

# Also generate LaTeX table
LATEX_STATUS=""
if python3 "$EXTRACT_PY" $BENCH_SET_ARG --latex > "$TMP_LATEX" 2>/dev/null; then
    LATEX_OUT="${REPORT_DIR}/table_results_${DATE}.tex"
    cp "$TMP_LATEX" "$LATEX_OUT"
    LATEX_STATUS="OK — written to $(basename "$LATEX_OUT")"
    echo "[gen_report] LaTeX table: $LATEX_OUT" >&2
else
    LATEX_STATUS="FAILED"
fi

# ── Step 2: plot.py ───────────────────────────────────────────────────────────
PLOT_STATUS=""
if python3 "$PLOT_PY" 2>/dev/null; then
    FIGURES_WRITTEN="$(ls "${FIGURE_DIR}"/*.pdf 2>/dev/null | xargs -r -n1 basename | tr '\n' ' ' || echo 'none')"
    PLOT_STATUS="OK — PDFs: ${FIGURES_WRITTEN}"
    echo "[gen_report] plot.py: OK" >&2
else
    PLOT_STATUS="SKIPPED (matplotlib missing or CSV empty — run extract_results.py --write-summary-csv first)"
    echo "[gen_report] plot.py: SKIPPED" >&2
fi

# ── Step 3: amdahl.py ─────────────────────────────────────────────────────────
TMP_AMDAHL="$(mktemp /tmp/hqc_amdahl_XXXXXX.txt)"
trap 'rm -f "$TMP_MD" "$TMP_LATEX" "$TMP_AMDAHL"' EXIT

AMDAHL_STATUS=""
if python3 "$AMDAHL_PY" --fraction "$FRACTION" --sweep > "$TMP_AMDAHL" 2>/dev/null; then
    AMDAHL_STATUS="OK"
    echo "[gen_report] amdahl.py: OK" >&2
else
    AMDAHL_STATUS="FAILED"
    echo "fraction=$FRACTION, sweep=failed" > "$TMP_AMDAHL"
fi

# ── Assemble report ───────────────────────────────────────────────────────────
cat > "$REPORT" << REPORT_EOF
# HQC-RISC-V Benchmark Report

**Date:** ${DATE}
**Git SHA:** ${GIT_SHA}
**Benchmark sets found:** ${BENCH_SETS_FOUND}

---

## Pipeline Status

| Step | Status |
|------|--------|
| extract_results.py (Markdown tables) | ${EXTRACT_STATUS} |
| extract_results.py (LaTeX tables)    | ${LATEX_STATUS} |
| plot.py (PDF figures)                | ${PLOT_STATUS} |
| amdahl.py (speedup projections)      | ${AMDAHL_STATUS} |

---

## Instruction Count Tables

REPORT_EOF

# Append the Markdown tables from extract_results.py
if [[ -s "$TMP_MD" ]]; then
    cat "$TMP_MD" >> "$REPORT"
else
    echo "(No data — run Spike benchmarks and then \`scripts/report/gen_report.sh\`)" >> "$REPORT"
fi

cat >> "$REPORT" << REPORT_EOF

---

## Figures

REPORT_EOF

if ls "${FIGURE_DIR}"/*.pdf 2>/dev/null | head -1 &>/dev/null; then
    for pdf in "${FIGURE_DIR}"/*.pdf; do
        echo "- \`$(realpath --relative-to="$REPO_ROOT" "$pdf")\`" >> "$REPORT"
    done
else
    echo "(No figures generated yet)" >> "$REPORT"
fi

cat >> "$REPORT" << REPORT_EOF

---

## Amdahl's Law Projections

vect_mul fraction of total runtime: **${FRACTION}**

\`\`\`
REPORT_EOF

cat "$TMP_AMDAHL" >> "$REPORT"

cat >> "$REPORT" << REPORT_EOF
\`\`\`

Use \`--fraction\` to adjust: e.g. \`scripts/report/gen_report.sh --fraction 0.85\`

---

## Next Steps

- [ ] Verify all KAT pass: \`scripts/bench/run_kat_all.sh\`
- [ ] Run polymul micro-benches for next benchmark set: \`scripts/bench/run_spike_bench_variant.sh <bench_set>\`
- [ ] Run histogram analysis: \`scripts/bench/run_histogram_all.sh <bench_set>\`
- [ ] Commit results: \`git add analysis/benchmarks/<bench_set>/ && git commit\`
- [ ] Generate updated report: \`scripts/report/gen_report.sh --bench-set <bench_set>\`

---

*Generated by \`scripts/report/gen_report.sh\`*
REPORT_EOF

echo "[gen_report] Report written: $REPORT" >&2
echo "$REPORT"
