#!/usr/bin/env bash
# scripts/bench/run_full_analysis.sh
# Master orchestration script: runs ALL benchmarks, profiling, and analysis
# for the HQC-RISC-V project after source code changes.
#
# Scope:
#   - scalar_zb + vector_rvv: sim (Spike) AND hw (BPI-F3)
#   - vector_zvk: sim only (BPI-F3 doesn't support Zvbc/Zvbb)
#
# What it runs:
#   1. KEM benchmarks (sim + hw)
#   2. Component benchmarks: polymul, keccak, decode (sim + hw)
#   3. Ablation studies: scalar, vector RVV, decode RVV, Zvbc decode (sim + hw)
#   4. VLEN scaling sweep (sim only, vector variants)
#   5. Compiler comparison (hw: GCC vs Clang)
#   6. LTO/PGO comparison (hw: scalar variants)
#   7. Profiling: traces (sim), counters + hotspots + isa-mix + annotate (hw)
#
# Usage:
#   ./scripts/bench/run_full_analysis.sh [--quality dev|publish] [--remote HOST] [--local]
#   ./scripts/bench/run_full_analysis.sh --quality publish
#   ./scripts/bench/run_full_analysis.sh --skip-hw   # sim-only run
#   ./scripts/bench/run_full_analysis.sh --skip-sim  # hw-only run

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Defaults ─────────────────────────────────────────────────────────────────
QUALITY="dev"
REMOTE_ARGS=()
SKIP_SIM=0
SKIP_HW=0
SKIP_PROFILE=0
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --quality)    QUALITY="$2"; shift 2 ;;
        --remote)     REMOTE_ARGS=(--remote "$2"); shift 2 ;;
        --local)      REMOTE_ARGS=(--local); shift ;;
        --skip-hw)    SKIP_HW=1; shift ;;
        --skip-sim)   SKIP_SIM=1; shift ;;
        --skip-profile) SKIP_PROFILE=1; shift ;;
        --dry-run)    DRY_RUN=1; shift ;;
        -h|--help)
            echo "Usage: $0 [--quality dev|publish] [--remote HOST|--local] [--skip-hw] [--skip-sim] [--skip-profile] [--dry-run]"
            exit 0 ;;
        *) echo "ERROR: Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ── Helpers ──────────────────────────────────────────────────────────────────
PASS=0
FAIL=0
START_TIME=$(date +%s)

run() {
    echo ""
    echo "┌─────────────────────────────────────────────────────────────────"
    echo "│ $*"
    echo "└─────────────────────────────────────────────────────────────────"
    if [[ $DRY_RUN -eq 1 ]]; then
        echo "  [dry-run] skipped"
        return 0
    fi
    if "$@"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "  ⚠ FAILED: $*"
    fi
}

section() {
    echo ""
    echo "╔══════════════════════════════════════════════════════════════════"
    echo "║  $1"
    echo "╚══════════════════════════════════════════════════════════════════"
    echo ""
}

# ══════════════════════════════════════════════════════════════════════════════
# PART 1: SIMULATION BENCHMARKS (Spike ISS)
# ══════════════════════════════════════════════════════════════════════════════

if [[ $SKIP_SIM -eq 0 ]]; then

section "1. SIMULATION: KEM Benchmarks"

# KEM: scalar_zb, vector_rvv, vector_zvk — all levels
for variant in scalar_zb vector_rvv vector_zvk; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform sim --mode kem --variant "$variant" --level all
done

section "2. SIMULATION: Component Benchmarks (polymul)"

# Polymul: scalar_zb, vector_rvv, vector_zvk — all levels
for variant in scalar_zb vector_rvv vector_zvk; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform sim --mode mul --variant "$variant" --level all
done

section "3. SIMULATION: Component Benchmarks (keccak)"

# Keccak: ref and opt implementations
for impl in ref opt; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform sim --mode keccak --keccak "$impl" --level all
done

section "4. SIMULATION: Component Benchmarks (decode)"

# Decode: all implementations
for impl in ref scalar_zb rvv zvbc rm_rvv_E rm_rvv_EH rm_rvv_EHP; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform sim --mode decode --decode "$impl" --level all
done

section "5. SIMULATION: Ablation Studies (polymul scalar)"

run "${SCRIPT_DIR}/run_ablation.sh" --mode mul --scalar_zb --sim --level all

section "6. SIMULATION: Ablation Studies (polymul vector RVV)"

run "${SCRIPT_DIR}/run_ablation.sh" --mode mul --vector_rvv --sim --level all

section "7. SIMULATION: Ablation Studies (decode RVV)"

run "${SCRIPT_DIR}/run_ablation.sh" --mode decode --decode_rvv --sim --level all

section "8. SIMULATION: Ablation Studies (decode Zvbc)"

run "${SCRIPT_DIR}/run_ablation.sh" --mode decode --zvbc_decode --sim --level all

section "9. SIMULATION: VLEN Scaling Sweep"

# VLEN sweep for vector variants
for variant in vector_rvv vector_zvk; do
    run "${SCRIPT_DIR}/run_vlen_sweep.sh" --variant "$variant" --level all
done

section "10. SIMULATION: Profiling — Spike Traces"

# Trace profiling: scalar_zb, vector_rvv, vector_zvk (KEM + polymul)
for variant in scalar_zb vector_rvv vector_zvk; do
    run "${SCRIPT_DIR}/run_profile.sh" --trace --variant "$variant" --level all
    run "${SCRIPT_DIR}/run_profile.sh" --trace --variant "$variant" --level all --polymul
done

# Trace profiling with line-level granularity for the champions
for variant in scalar_zb vector_rvv vector_zvk; do
    run "${SCRIPT_DIR}/run_profile.sh" --trace --variant "$variant" --level all --polymul --lines
done

fi  # end SKIP_SIM


# ══════════════════════════════════════════════════════════════════════════════
# PART 2: HARDWARE BENCHMARKS (BPI-F3)
# ══════════════════════════════════════════════════════════════════════════════

if [[ $SKIP_HW -eq 0 ]]; then

section "11. HARDWARE: KEM Benchmarks"

# KEM: scalar_zb, vector_rvv (hw-supported) — all levels
for variant in scalar_zb vector_rvv; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform hw --mode kem --variant "$variant" --level all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "12. HARDWARE: Component Benchmarks (polymul)"

for variant in scalar_zb vector_rvv; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform hw --mode mul --variant "$variant" --level all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "13. HARDWARE: Component Benchmarks (keccak)"

for impl in ref opt; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform hw --mode keccak --keccak "$impl" --level all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "14. HARDWARE: Component Benchmarks (decode)"

for impl in ref scalar_zb rvv rm_rvv_E rm_rvv_EH rm_rvv_EHP; do
    run "${SCRIPT_DIR}/run_bench.sh" --platform hw --mode decode --decode "$impl" --level all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "15. HARDWARE: Ablation Studies (polymul scalar)"

run "${SCRIPT_DIR}/run_ablation.sh" --mode mul --scalar_zb --hw --level all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"

section "16. HARDWARE: Ablation Studies (polymul vector RVV)"

run "${SCRIPT_DIR}/run_ablation.sh" --mode mul --vector_rvv --hw --level all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"

section "17. HARDWARE: Ablation Studies (decode RVV)"

run "${SCRIPT_DIR}/run_ablation.sh" --mode decode --decode_rvv --hw --level all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"

section "18. HARDWARE: Ablation Studies (decode Zvbc — sim only, hw n/a)"
# Zvbc decode ablation: sim only (BPI-F3 doesn't have Zvbc)
echo "  (Zvbc decode ablation is sim-only — already done in Part 1)"

section "19. HARDWARE: Compiler Comparison (GCC vs Clang)"

for variant in scalar_zb vector_rvv; do
    run "${SCRIPT_DIR}/run_compiler_compare.sh" --variant "$variant" --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "20. HARDWARE: LTO/PGO Comparison"

for polymul in scalar_ref scalar_zb; do
    for level in 128 192 256; do
        run "${SCRIPT_DIR}/run_hw_lto_pgo_compare.sh" --polymul "$polymul" --level "$level" --mode "$QUALITY"
    done
done

fi  # end SKIP_HW


# ══════════════════════════════════════════════════════════════════════════════
# PART 3: HARDWARE PROFILING (BPI-F3)
# ══════════════════════════════════════════════════════════════════════════════

if [[ $SKIP_HW -eq 0 && $SKIP_PROFILE -eq 0 ]]; then

section "21. HARDWARE PROFILING: Counters (EXTENDED perf_event_open)"

for variant in scalar_zb vector_rvv; do
    run "${SCRIPT_DIR}/run_profile.sh" --platform hw --counters --variant "$variant" --level all --component all --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "22. HARDWARE PROFILING: Hotspots (perf record task-clock)"

for variant in scalar_zb vector_rvv; do
    run "${SCRIPT_DIR}/run_profile.sh" --platform hw --hotspots --variant "$variant" --level all --component all "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "23. HARDWARE PROFILING: ISA Instruction Mix (objdump)"

for variant in scalar_zb vector_rvv; do
    run "${SCRIPT_DIR}/run_profile.sh" --platform hw --isa-mix --variant "$variant" --level all --component all "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

section "24. HARDWARE PROFILING: Annotate (perf annotate top functions)"

for variant in scalar_zb vector_rvv; do
    run "${SCRIPT_DIR}/run_profile.sh" --platform hw --annotate --variant "$variant" --level all --component all "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"
done

fi  # end SKIP_HW + SKIP_PROFILE


# ══════════════════════════════════════════════════════════════════════════════
# SUMMARY
# ══════════════════════════════════════════════════════════════════════════════

END_TIME=$(date +%s)
ELAPSED=$(( END_TIME - START_TIME ))
MINUTES=$(( ELAPSED / 60 ))
SECONDS_REMAIN=$(( ELAPSED % 60 ))

echo ""
echo "╔══════════════════════════════════════════════════════════════════"
echo "║  FULL ANALYSIS COMPLETE"
echo "║"
echo "║  Passed: ${PASS}    Failed: ${FAIL}"
echo "║  Elapsed: ${MINUTES}m ${SECONDS_REMAIN}s"
echo "║  Quality: ${QUALITY}"
echo "║"
if [[ $FAIL -gt 0 ]]; then
echo "║  ⚠ Some tasks failed — check output above for details"
fi
echo "╚══════════════════════════════════════════════════════════════════"
echo ""
