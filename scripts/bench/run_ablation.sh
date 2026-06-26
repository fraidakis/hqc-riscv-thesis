#!/usr/bin/env bash
# scripts/bench/run_ablation.sh
#
# Run the full scalar Karatsuba ablation study: KAT validation and/or
# polymul micro-benchmarks across all ablation variants.
#
# This script wraps run_bench.sh and run_kat.sh to iterate over the
# ablation variant matrix in a single command.
#
# Usage:
#   run_ablation.sh                           # Default: --sim + --hw
#   run_ablation.sh --kat                     # KAT only (sim)
#   run_ablation.sh --sim                     # Polymul bench on Spike
#   run_ablation.sh --hw                      # Polymul bench on hardware
#   run_ablation.sh --kat --sim               # KAT then bench
#   run_ablation.sh --kat --sim --hw          # Everything
#   run_ablation.sh --sim --level 128         # Single level
#   run_ablation.sh --hw --quality publish    # Publication-quality HW run
#   run_ablation.sh --sim --variant scalar_zb_A  # Single variant only
#
# Flags:
#   --kat               Run KAT validation (via Spike)
#   --sim               Run polymul micro-bench on Spike (instret)
#   --hw                Run polymul micro-bench on hardware (cycles)
#   --level LEVEL       Security level: 128, 192, 256, or all (default: all)
#   --variant VARIANT   Run only this variant (default: all ablation variants)
#   --quality QUALITY   HW iteration quality: dev or publish (default: dev)
#   --remote HOST       SSH target for HW bench (default: auto from run_bench.sh)
#   --local             Run HW bench locally (on RISC-V hardware)
#   --stop-on-fail      Abort immediately if any KAT test fails
#   --dry-run           Print commands without executing
#   -h, --help          Show this help
#
# The ablation variants (in order of optimization layering):
#   scalar_ref    : baseline (bit-serial + memset + n>>1 split)
#   scalar_zb_A   : + word-level clmul
#   scalar_zb_AB  : + word-level + assign-on-first-write
#   scalar_zb_AC  : + word-level + specialized recursion tree
#   scalar_zb     : + all three (champion)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Ablation variant list (in thesis presentation order) ─────────────────────
ALL_ABLATION_VARIANTS=(
    scalar_ref
    scalar_zb_A
    scalar_zb_AB
    scalar_zb_AC
    scalar_zb
)

# ── Defaults ─────────────────────────────────────────────────────────────────
DO_KAT=0
DO_SIM=0
DO_HW=0
LEVEL="all"
VARIANT_FILTER=""
QUALITY="dev"
REMOTE_ARGS=()
STOP_ON_FAIL=0
DRY_RUN=0

# ── Argument Parsing ─────────────────────────────────────────────────────────
usage() {
    sed -n '3,/^$/p' "$0" | sed 's/^# \?//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --kat)        DO_KAT=1; shift ;;
        --sim)        DO_SIM=1; shift ;;
        --hw)         DO_HW=1;  shift ;;
        --level)
            [[ $# -lt 2 ]] && { echo "ERROR: --level requires a value" >&2; exit 1; }
            LEVEL="$2"; shift 2 ;;
        --variant)
            [[ $# -lt 2 ]] && { echo "ERROR: --variant requires a value" >&2; exit 1; }
            VARIANT_FILTER="$2"; shift 2 ;;
        --quality)
            [[ $# -lt 2 ]] && { echo "ERROR: --quality requires a value" >&2; exit 1; }
            QUALITY="$2"; shift 2 ;;
        --remote)
            [[ $# -lt 2 ]] && { echo "ERROR: --remote requires a host" >&2; exit 1; }
            REMOTE_ARGS=(--remote "$2"); shift 2 ;;
        --local)
            REMOTE_ARGS=(--local); shift ;;
        --stop-on-fail) STOP_ON_FAIL=1; shift ;;
        --dry-run)      DRY_RUN=1; shift ;;
        -h|--help)      usage; exit 0 ;;
        *)
            echo "ERROR: Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

# ── Validation ───────────────────────────────────────────────────────────────
# If no action flags given, default to running both --sim and --hw
if [[ $DO_KAT -eq 0 && $DO_SIM -eq 0 && $DO_HW -eq 0 ]]; then
    DO_SIM=1
    DO_HW=1
fi

# ── Resolve variant list ─────────────────────────────────────────────────────
if [[ -n "$VARIANT_FILTER" ]]; then
    VARIANTS=("$VARIANT_FILTER")
else
    VARIANTS=("${ALL_ABLATION_VARIANTS[@]}")
fi

# ── Helpers ──────────────────────────────────────────────────────────────────
PASS=0
FAIL=0
SKIP=0

run_cmd() {
    if [[ $DRY_RUN -eq 1 ]]; then
        echo "  [dry-run] $*"
        return 0
    fi
    "$@"
}

incr_pass() { PASS=$((PASS + 1)); }
incr_fail() { FAIL=$((FAIL + 1)); }

header() {
    echo ""
    echo "════════════════════════════════════════════════════════════════════"
    echo "  $1"
    echo "════════════════════════════════════════════════════════════════════"
}

section() {
    echo ""
    echo "── $1 ──────────────────────────────────────────────────────────────"
}

# ── Phase 1: KAT Validation ─────────────────────────────────────────────────
if [[ $DO_KAT -eq 1 ]]; then
    header "KAT VALIDATION"
    for v in "${VARIANTS[@]}"; do
        section "KAT: ${v} (level=${LEVEL})"
        if run_cmd "${REPO_ROOT}/scripts/test/run_kat.sh" \
                --variant "$v" --level "$LEVEL"; then
            echo "  ✓ ${v}: PASS"
            incr_pass
        else
            echo "  ✗ ${v}: FAIL"
            incr_fail
            if [[ $STOP_ON_FAIL -eq 1 ]]; then
                echo "ERROR: KAT failed for ${v}, aborting (--stop-on-fail)" >&2
                exit 1
            fi
        fi
    done
    echo ""
    echo "KAT summary: ${PASS} passed, ${FAIL} failed"

    if [[ $FAIL -gt 0 && ($DO_SIM -eq 1 || $DO_HW -eq 1) ]]; then
        echo "WARNING: Some KAT tests failed. Benchmark results may be invalid." >&2
    fi
fi

# ── Phase 2: Simulation Benchmark (Spike) ────────────────────────────────────
if [[ $DO_SIM -eq 1 ]]; then
    header "SIMULATION BENCHMARK (Spike — instret)"
    for v in "${VARIANTS[@]}"; do
        section "Bench sim: ${v} (level=${LEVEL})"
        if run_cmd "${REPO_ROOT}/scripts/bench/run_bench.sh" \
                --platform sim --mode mul --variant "$v" --level "$LEVEL"; then
            echo "  ✓ ${v}: done"
        else
            echo "  ✗ ${v}: FAILED"
            incr_fail
            if [[ $STOP_ON_FAIL -eq 1 ]]; then
                echo "ERROR: Sim bench failed for ${v}, aborting" >&2
                exit 1
            fi
        fi
    done
fi

# ── Phase 3: Hardware Benchmark ──────────────────────────────────────────────
if [[ $DO_HW -eq 1 ]]; then
    header "HARDWARE BENCHMARK (cycles)"
    for v in "${VARIANTS[@]}"; do
        section "Bench hw: ${v} (level=${LEVEL}, quality=${QUALITY})"
        if run_cmd "${REPO_ROOT}/scripts/bench/run_bench.sh" \
                --platform hw --mode mul --variant "$v" --level "$LEVEL" \
                --quality "$QUALITY" "${REMOTE_ARGS[@]+"${REMOTE_ARGS[@]}"}"; then
            echo "  ✓ ${v}: done"
        else
            echo "  ✗ ${v}: FAILED"
            incr_fail
            if [[ $STOP_ON_FAIL -eq 1 ]]; then
                echo "ERROR: HW bench failed for ${v}, aborting" >&2
                exit 1
            fi
        fi
    done
fi

# ── Phase 4: Aggregate Results & Generate Unified Report ─────────────────────
# Always generate the report (after --sim, --hw, or both)
header "GENERATING ABLATION REPORT"

REPORT_DIR="${REPO_ROOT}/results/reports/summaries"
mkdir -p "$REPORT_DIR"

# Resolve levels
if [[ "$LEVEL" == "all" ]]; then
    REPORT_LEVELS=(128 192 256)
else
    IFS=',' read -ra REPORT_LEVELS <<< "$LEVEL"
fi

# Slug helper (underscore to hyphen, lowercase)
slug() { echo "${1//_/-}" | tr '[:upper:]' '[:lower:]'; }

# Data source directories
HW_CSV_DIR="${REPO_ROOT}/results/hardware/benchmarks/components/poly_mul"
SIM_CSV_DIR="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul"
REPORT="${REPORT_DIR}/ablation.md"

# Determine which data sources are available
HAS_HW=0
HAS_SIM=0
[[ $DO_HW -eq 1 ]] && HAS_HW=1
[[ $DO_SIM -eq 1 ]] && HAS_SIM=1

# If only sim was run but HW data exists from a previous run, include it
if [[ $HAS_HW -eq 0 ]]; then
    hw_probe="${HW_CSV_DIR}/hw_mul_scalar-ref_latest.csv"
    [[ -f "$hw_probe" ]] && HAS_HW=1
fi
# If only hw was run but sim data exists from a previous run, include it
if [[ $HAS_SIM -eq 0 ]]; then
    sim_probe="${SIM_CSV_DIR}/sim_mul_scalar-ref_latest.csv"
    [[ -f "$sim_probe" ]] && HAS_SIM=1
fi

if [[ $DRY_RUN -eq 0 ]]; then
    {
        echo "# Scalar Karatsuba Ablation Study"
        echo ""
        echo "Generated: $(date '+%Y-%m-%d %H:%M')"
        if [[ $HAS_HW -eq 1 ]]; then
            echo "Platform: BPI-F3 (SpacemiT K1 X60)"
            echo "Quality: ${QUALITY}"
        fi
        echo ""
        echo "Baseline: \`scalar_ref\` (bit-serial schoolbook, memset, n>>1 split)"
        echo ""
        echo "Optimizations:"
        echo "- **A** — Word-level carry-less multiply (Zbc \`clmul\`)"
        echo "- **B** — Assign-on-first-write (memset elimination)"
        echo "- **C** — Compile-time specialized recursion tree"
        echo ""

        # ── Per-level tables ─────────────────────────────────────────────
        for lvl in "${REPORT_LEVELS[@]}"; do
            echo "## HQC-${lvl}"
            echo ""

            # Table header depends on available data
            if [[ $HAS_HW -eq 1 && $HAS_SIM -eq 1 ]]; then
                echo "| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |"
                echo "|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|"
            elif [[ $HAS_HW -eq 1 ]]; then
                echo "| Variant | Optimizations | median cycles | vs ref | vs prev | instret |"
                echo "|---------|---------------|-------------:|-------:|--------:|--------:|"
            else
                echo "| Variant | Optimizations | instret | vs ref | vs prev |"
                echo "|---------|---------------|--------:|-------:|--------:|"
            fi

            # Collect data for all variants at this level
            declare -A LVL_CYCLES=()
            declare -A LVL_INSTRET=()

            for v in "${VARIANTS[@]}"; do
                s="$(slug "$v")"

                # HW data (cycles + instret from perf counters)
                if [[ $HAS_HW -eq 1 ]]; then
                    csv="${HW_CSV_DIR}/hw_mul_${s}_latest.csv"
                    if [[ -f "$csv" ]]; then
                        read -r cyc inst <<< $(awk -F',' -v l="$lvl" \
                            '!/^#/&&!/^level/&&$1==l{print $6, $10}' "$csv")
                        LVL_CYCLES["$v"]="$cyc"
                        LVL_INSTRET["$v"]="$inst"
                    else
                        LVL_CYCLES["$v"]=""
                        LVL_INSTRET["$v"]=""
                    fi
                fi

                # Sim data (instret only — use if no HW instret available)
                if [[ $HAS_SIM -eq 1 && -z "${LVL_INSTRET[$v]:-}" ]]; then
                    csv="${SIM_CSV_DIR}/sim_mul_${s}_latest.csv"
                    if [[ -f "$csv" ]]; then
                        inst=$(awk -F',' -v l="$lvl" \
                            '!/^#/&&!/^level/&&$1==l{print $3}' "$csv")
                        LVL_INSTRET["$v"]="$inst"
                    else
                        LVL_INSTRET["$v"]=""
                    fi
                fi
            done

            # Reference values for speedup computation
            ref_cycles="${LVL_CYCLES[scalar_ref]:-}"
            ref_instret="${LVL_INSTRET[scalar_ref]:-}"
            a_cycles="${LVL_CYCLES[scalar_zb_A]:-}"
            a_instret="${LVL_INSTRET[scalar_zb_A]:-}"
            ab_cycles="${LVL_CYCLES[scalar_zb_AB]:-}"
            ac_cycles="${LVL_CYCLES[scalar_zb_AC]:-}"
            ab_instret="${LVL_INSTRET[scalar_zb_AB]:-}"
            ac_instret="${LVL_INSTRET[scalar_zb_AC]:-}"

            # Best branch for ABC "vs prev" (lower is better)
            best_branch_cycles=""
            if [[ -n "$ab_cycles" && -n "$ac_cycles" ]]; then
                [[ "$ac_cycles" -le "$ab_cycles" ]] && best_branch_cycles="$ac_cycles" || best_branch_cycles="$ab_cycles"
            elif [[ -n "$ab_cycles" ]]; then best_branch_cycles="$ab_cycles"
            elif [[ -n "$ac_cycles" ]]; then best_branch_cycles="$ac_cycles"
            fi

            best_branch_instret=""
            if [[ -n "$ab_instret" && -n "$ac_instret" ]]; then
                [[ "$ac_instret" -le "$ab_instret" ]] && best_branch_instret="$ac_instret" || best_branch_instret="$ab_instret"
            elif [[ -n "$ab_instret" ]]; then best_branch_instret="$ab_instret"
            elif [[ -n "$ac_instret" ]]; then best_branch_instret="$ac_instret"
            fi

            # Emit rows
            for v in "${VARIANTS[@]}"; do
                cycles="${LVL_CYCLES[$v]:-}"
                instret="${LVL_INSTRET[$v]:-}"

                # Optimization label
                case "$v" in
                    scalar_ref)   opt_label="(none)" ;;
                    scalar_zb_A)  opt_label="A" ;;
                    scalar_zb_AB) opt_label="A+B" ;;
                    scalar_zb_AC) opt_label="A+C" ;;
                    scalar_zb)    opt_label="A+B+C" ;;
                    *)            opt_label="?" ;;
                esac

                # ── Cycles columns ───────────────────────────────────────
                if [[ $HAS_HW -eq 1 ]]; then
                    if [[ -z "$cycles" ]]; then
                        col_cycles="—"; col_cyc_ref="—"; col_cyc_prev="—"
                    else
                        col_cycles=$(printf "%'d" "$cycles")
                        if [[ -n "$ref_cycles" && "$ref_cycles" -gt 0 ]]; then
                            col_cyc_ref=$(awk "BEGIN{printf \"×%.1f\",${ref_cycles}/${cycles}}")
                        else col_cyc_ref="—"; fi

                        case "$v" in
                            scalar_ref) col_cyc_prev="—" ;;
                            scalar_zb_A)
                                [[ -n "$ref_cycles" && "$ref_cycles" -gt 0 && "$cycles" -gt 0 ]] \
                                    && col_cyc_prev=$(awk "BEGIN{printf \"×%.1f\",${ref_cycles}/${cycles}}") \
                                    || col_cyc_prev="—" ;;
                            scalar_zb_AB|scalar_zb_AC)
                                [[ -n "$a_cycles" && "$a_cycles" -gt 0 && "$cycles" -gt 0 ]] \
                                    && col_cyc_prev=$(awk "BEGIN{printf \"×%.1f\",${a_cycles}/${cycles}}") \
                                    || col_cyc_prev="—" ;;
                            scalar_zb)
                                [[ -n "$best_branch_cycles" && "$best_branch_cycles" -gt 0 && "$cycles" -gt 0 ]] \
                                    && col_cyc_prev=$(awk "BEGIN{printf \"×%.1f\",${best_branch_cycles}/${cycles}}") \
                                    || col_cyc_prev="—" ;;
                            *) col_cyc_prev="—" ;;
                        esac
                    fi
                fi

                # ── Instret columns ──────────────────────────────────────
                if [[ -z "$instret" ]]; then
                    col_instret="—"; col_inst_ref="—"; col_inst_prev="—"
                else
                    col_instret=$(printf "%'d" "$instret")
                    if [[ -n "$ref_instret" && "$ref_instret" -gt 0 ]]; then
                        col_inst_ref=$(awk "BEGIN{printf \"×%.1f\",${ref_instret}/${instret}}")
                    else col_inst_ref="—"; fi

                    case "$v" in
                        scalar_ref) col_inst_prev="—" ;;
                        scalar_zb_A)
                            [[ -n "$ref_instret" && "$ref_instret" -gt 0 && "$instret" -gt 0 ]] \
                                && col_inst_prev=$(awk "BEGIN{printf \"×%.1f\",${ref_instret}/${instret}}") \
                                || col_inst_prev="—" ;;
                        scalar_zb_AB|scalar_zb_AC)
                            [[ -n "$a_instret" && "$a_instret" -gt 0 && "$instret" -gt 0 ]] \
                                && col_inst_prev=$(awk "BEGIN{printf \"×%.1f\",${a_instret}/${instret}}") \
                                || col_inst_prev="—" ;;
                        scalar_zb)
                            [[ -n "$best_branch_instret" && "$best_branch_instret" -gt 0 && "$instret" -gt 0 ]] \
                                && col_inst_prev=$(awk "BEGIN{printf \"×%.1f\",${best_branch_instret}/${instret}}") \
                                || col_inst_prev="—" ;;
                        *) col_inst_prev="—" ;;
                    esac
                fi

                # ── Emit row ─────────────────────────────────────────────
                if [[ $HAS_HW -eq 1 && $HAS_SIM -eq 1 ]]; then
                    echo "| \`${v}\` | ${opt_label} | ${col_cycles} | ${col_cyc_ref} | ${col_cyc_prev} | ${col_instret} | ${col_inst_ref} | ${col_inst_prev} |"
                elif [[ $HAS_HW -eq 1 ]]; then
                    echo "| \`${v}\` | ${opt_label} | ${col_cycles} | ${col_cyc_ref} | ${col_cyc_prev} | ${col_instret} |"
                else
                    echo "| \`${v}\` | ${opt_label} | ${col_instret} | ${col_inst_ref} | ${col_inst_prev} |"
                fi
            done
            echo ""
        done

        # ── Incremental Optimization Contribution (cycles) ───────────────
        if [[ $HAS_HW -eq 1 ]]; then
            echo "## Incremental Optimization Contribution (cycles)"
            echo ""
            echo "| Transition | What changes | HQC-128 Δ | HQC-192 Δ | HQC-256 Δ |"
            echo "|------------|--------------|----------:|----------:|----------:|"

            hw_compute_delta() {
                local from_var="$1" to_var="$2" lvl="$3"
                local from_csv="${HW_CSV_DIR}/hw_mul_$(slug "$from_var")_latest.csv"
                local to_csv="${HW_CSV_DIR}/hw_mul_$(slug "$to_var")_latest.csv"
                if [[ ! -f "$from_csv" || ! -f "$to_csv" ]]; then echo "—"; return; fi
                local from_val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $6}' "$from_csv")
                local to_val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $6}' "$to_csv")
                if [[ -z "$from_val" || -z "$to_val" || "$from_val" -eq 0 ]]; then echo "—"; return; fi
                awk "BEGIN{printf \"%+.1f%%\",(${to_val}-${from_val})/${from_val}*100}"
            }

            hw_get_cycles() {
                local var="$1" lvl="$2"
                local csv="${HW_CSV_DIR}/hw_mul_$(slug "$var")_latest.csv"
                if [[ ! -f "$csv" ]]; then echo ""; return; fi
                awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $6}' "$csv"
            }

            # Determine finalist (lower cycles wins at HQC-128)
            hw_ab_128=$(hw_get_cycles "scalar_zb_AB" 128)
            hw_ac_128=$(hw_get_cycles "scalar_zb_AC" 128)
            if [[ -n "$hw_ac_128" && -n "$hw_ab_128" && "$hw_ac_128" -le "$hw_ab_128" ]]; then
                hw_finalist="scalar_zb_AC"; hw_finalist_label="AC"; hw_finalist_opt="A+C"
                hw_final_step_desc="+noinit (on top of ${hw_finalist_opt})"
            else
                hw_finalist="scalar_zb_AB"; hw_finalist_label="AB"; hw_finalist_opt="A+B"
                hw_final_step_desc="+specialized tree (on top of ${hw_finalist_opt})"
            fi

            hw_transitions=(
                "ref→A|+word-level clmul|scalar_ref|scalar_zb_A"
                "A→AB|+assign-on-first-write|scalar_zb_A|scalar_zb_AB"
                "A→AC|+specialized recursion tree|scalar_zb_A|scalar_zb_AC"
                "${hw_finalist_label}→ABC|${hw_final_step_desc}|${hw_finalist}|scalar_zb"
            )

            for t in "${hw_transitions[@]}"; do
                IFS='|' read -r label desc from_v to_v <<< "$t"
                d128=$(hw_compute_delta "$from_v" "$to_v" 128)
                d192=$(hw_compute_delta "$from_v" "$to_v" 192)
                d256=$(hw_compute_delta "$from_v" "$to_v" 256)
                echo "| ${label} | ${desc} | ${d128} | ${d192} | ${d256} |"
            done
            echo ""
        fi

        # ── Incremental Optimization Contribution (instret) ──────────────
        if [[ $HAS_SIM -eq 1 || $HAS_HW -eq 1 ]]; then
            echo "## Incremental Optimization Contribution (instret)"
            echo ""
            echo "| Transition | What changes | HQC-128 Δ | HQC-192 Δ | HQC-256 Δ |"
            echo "|------------|--------------|----------:|----------:|----------:|"

            # Use HW instret if available, otherwise sim instret
            instret_get() {
                local var="$1" lvl="$2"
                if [[ $HAS_HW -eq 1 ]]; then
                    local csv="${HW_CSV_DIR}/hw_mul_$(slug "$var")_latest.csv"
                    if [[ -f "$csv" ]]; then
                        local val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $10}' "$csv")
                        if [[ -n "$val" ]]; then echo "$val"; return; fi
                    fi
                fi
                if [[ $HAS_SIM -eq 1 ]]; then
                    local csv="${SIM_CSV_DIR}/sim_mul_$(slug "$var")_latest.csv"
                    if [[ -f "$csv" ]]; then
                        awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $3}' "$csv"
                        return
                    fi
                fi
                echo ""
            }

            instret_compute_delta() {
                local from_var="$1" to_var="$2" lvl="$3"
                local from_val=$(instret_get "$from_var" "$lvl")
                local to_val=$(instret_get "$to_var" "$lvl")
                if [[ -z "$from_val" || -z "$to_val" || "$from_val" -eq 0 ]]; then echo "—"; return; fi
                awk "BEGIN{printf \"%+.1f%%\",(${to_val}-${from_val})/${from_val}*100}"
            }

            # Determine finalist by instret at HQC-128
            inst_ab_128=$(instret_get "scalar_zb_AB" 128)
            inst_ac_128=$(instret_get "scalar_zb_AC" 128)
            if [[ -n "$inst_ac_128" && -n "$inst_ab_128" && "$inst_ac_128" -le "$inst_ab_128" ]]; then
                inst_finalist="scalar_zb_AC"; inst_finalist_label="AC"; inst_finalist_opt="A+C"
                inst_final_step_desc="+noinit (on top of ${inst_finalist_opt})"
            else
                inst_finalist="scalar_zb_AB"; inst_finalist_label="AB"; inst_finalist_opt="A+B"
                inst_final_step_desc="+specialized tree (on top of ${inst_finalist_opt})"
            fi

            inst_transitions=(
                "ref→A|+word-level clmul|scalar_ref|scalar_zb_A"
                "A→AB|+assign-on-first-write|scalar_zb_A|scalar_zb_AB"
                "A→AC|+specialized recursion tree|scalar_zb_A|scalar_zb_AC"
                "${inst_finalist_label}→ABC|${inst_final_step_desc}|${inst_finalist}|scalar_zb"
            )

            for t in "${inst_transitions[@]}"; do
                IFS='|' read -r label desc from_v to_v <<< "$t"
                d128=$(instret_compute_delta "$from_v" "$to_v" 128)
                d192=$(instret_compute_delta "$from_v" "$to_v" 192)
                d256=$(instret_compute_delta "$from_v" "$to_v" 256)
                echo "| ${label} | ${desc} | ${d128} | ${d192} | ${d256} |"
            done
            echo ""
        fi

    } > "$REPORT"

    echo "  → Ablation report: ${REPORT}"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
header "ABLATION STUDY COMPLETE"
echo "  Variants tested: ${#VARIANTS[@]}"
echo "  Failures:        ${FAIL}"

if [[ $FAIL -gt 0 ]]; then
    exit 1
fi
