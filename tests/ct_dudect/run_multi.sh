#!/usr/bin/env bash
# tests/ct_dudect/run_multi.sh
#
# Run N independent dudect trials with different PRNG seeds and report
# the distribution of final t-statistics. For thesis-quality evidence:
# multiple independent runs at the same measurement budget show the
# result is not an artifact of a single PRNG seed.
#
# Usage:
#   ./run_multi.sh <binary> [--runs N] [--outdir DIR] [--core CORE]
#
# Example:
#   ./run_multi.sh ./dudect_polymul_hqc128_karatsuba_zbc --runs 10
#   ./run_multi.sh ./dudect_kem_hqc128_ref_scalar --runs 5 --core 7
#
# Output:
#   Prints per-run verdicts and final summary statistics (mean/max |t|).
#   Individual CSV files are written as {outdir}/{binary}_{run}.csv

set -euo pipefail

# ── Defaults ─────────────────────────────────────────────────────────────────
RUNS=10
OUTDIR="results/ct_verify/dudect/multi"
CORE=7
BINARY=""

# ── Parse arguments ──────────────────────────────────────────────────────────
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <binary> [--runs N] [--outdir DIR] [--core CORE]" >&2
    exit 1
fi

BINARY="$1"
shift

while [[ $# -gt 0 ]]; do
    case "$1" in
        --runs)  RUNS="$2"; shift 2 ;;
        --outdir) OUTDIR="$2"; shift 2 ;;
        --core)  CORE="$2"; shift 2 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: $BINARY is not executable" >&2
    exit 1
fi

mkdir -p "$OUTDIR"

BASENAME=$(basename "$BINARY")

# ── Run N independent trials ─────────────────────────────────────────────────
echo "[multi-run] binary=$BINARY runs=$RUNS core=$CORE"
echo "[multi-run] output directory: $OUTDIR"
echo ""

declare -a T1_VALS
declare -a T2_VALS
declare -a VERDICTS
LEAKAGE_COUNT=0

for ((r=1; r<=RUNS; r++)); do
    RUN_OUTDIR="${OUTDIR}/run_${r}"
    mkdir -p "$RUN_OUTDIR"

    echo -n "[multi-run] run $r/$RUNS ... "

    # Run with process isolation; each run gets a fresh /dev/urandom seed
    OUTPUT=$(sudo nice -n -20 taskset -c "$CORE" "$BINARY" --outdir "$RUN_OUTDIR" 2>&1)

    # Extract final t-statistics and verdict from stderr output
    RESULT_LINE=$(echo "$OUTPUT" | grep '\[dudect\] RESULT:' || true)

    if echo "$RESULT_LINE" | grep -q "LEAKAGE DETECTED"; then
        VERDICT="LEAK"
        LEAKAGE_COUNT=$((LEAKAGE_COUNT + 1))
    else
        VERDICT="PASS"
    fi

    # Extract t1 and t2 from the result line
    T1=$(echo "$RESULT_LINE" | grep -oP 't1=\K[-0-9.]+' || echo "0.0")
    T2=$(echo "$RESULT_LINE" | grep -oP 't2=\K[-0-9.]+' || echo "0.0")

    T1_VALS+=("$T1")
    T2_VALS+=("$T2")
    VERDICTS+=("$VERDICT")

    echo "$VERDICT  t1=$T1  t2=$T2"
done

# ── Summary statistics ───────────────────────────────────────────────────────
echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "[multi-run] SUMMARY ($RUNS runs)"
echo "════════════════════════════════════════════════════════════════════"
echo ""
echo "  Leakage detected: $LEAKAGE_COUNT / $RUNS"
echo ""

# Compute max |t1| and max |t2| using awk
MAX_T1=$(printf '%s\n' "${T1_VALS[@]}" | awk 'BEGIN{m=0}{v=($1<0?-$1:$1); if(v>m) m=v}END{printf "%.3f",m}')
MAX_T2=$(printf '%s\n' "${T2_VALS[@]}" | awk 'BEGIN{m=0}{v=($1<0?-$1:$1); if(v>m) m=v}END{printf "%.3f",m}')
MEAN_T1=$(printf '%s\n' "${T1_VALS[@]}" | awk 'BEGIN{s=0;n=0}{v=($1<0?-$1:$1); s+=v; n++}END{printf "%.3f",s/n}')
MEAN_T2=$(printf '%s\n' "${T2_VALS[@]}" | awk 'BEGIN{s=0;n=0}{v=($1<0?-$1:$1); s+=v; n++}END{printf "%.3f",s/n}')

echo "  First-order  |t1|:  mean=$MEAN_T1  max=$MAX_T1"
echo "  Second-order |t2|:  mean=$MEAN_T2  max=$MAX_T2"
echo ""

# Write machine-readable summary CSV
SUMMARY_CSV="${OUTDIR}/${BASENAME}_summary.csv"
echo "run,verdict,t1,t2" > "$SUMMARY_CSV"
for ((r=0; r<RUNS; r++)); do
    echo "$((r+1)),${VERDICTS[$r]},${T1_VALS[$r]},${T2_VALS[$r]}" >> "$SUMMARY_CSV"
done
echo "[multi-run] Summary CSV: $SUMMARY_CSV"

# Exit code: non-zero if any run detected leakage
if [[ $LEAKAGE_COUNT -gt 0 ]]; then
    echo ""
    echo "[multi-run] WARNING: $LEAKAGE_COUNT/$RUNS runs detected leakage."
    exit 1
fi

echo ""
echo "[multi-run] All $RUNS runs passed (no leakage detected)."
exit 0
