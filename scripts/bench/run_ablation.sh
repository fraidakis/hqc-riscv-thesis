#!/usr/bin/env bash
# scripts/bench/run_ablation.sh
#
# Run ablation studies for HQC component optimizations: KAT validation
# and/or micro-benchmarks across all ablation variants.
#
# This script wraps run_bench.sh and run_kat.sh to iterate over the
# ablation variant matrix in a single command.
#
# Usage:
#   run_ablation.sh --mode mul --scalar_zb          # Scalar polymul ablation (default: --sim + --hw)
#   run_ablation.sh --mode mul --vector_rvv         # Vector RVV polymul ablation
#   run_ablation.sh --mode mul --scalar_zb --vector_rvv  # Both polymul studies
#   run_ablation.sh --mode mul --scalar_zb --kat    # Scalar KAT only (sim)
#   run_ablation.sh --mode mul --vector_rvv --sim   # Vector bench on Spike
#   run_ablation.sh --mode mul --vector_rvv --hw    # Vector bench on hardware
#   run_ablation.sh --mode mul --scalar_zb --sim --level 128   # Single level
#   run_ablation.sh --mode mul --vector_rvv --hw --quality publish  # Publication-quality
#   run_ablation.sh --mode mul --scalar_zb --sim --variant scalar_zb_A  # Single variant
#   run_ablation.sh --mode mul --vector_rvv --sim --variant vector_rvv_V  # Single variant
#
# Mode selection (required):
#   --mode MODE         Component to ablate: mul (polymul), keccak, decode
#
# Study selection (at least one required per mode):
#   For --mode mul:
#     --scalar_zb       Run scalar Karatsuba ablation (Tier 1)
#     --vector_rvv      Run vector RVV ablation (Tier 2)
#
# Flags:
#   --kat               Run KAT validation (via Spike)
#   --sim               Run micro-bench on Spike (instret)
#   --hw                Run micro-bench on hardware (cycles)
#   --level LEVEL       Security level: 128, 192, 256, or all (default: all)
#   --variant VARIANT   Run only this variant (default: all ablation variants)
#   --quality QUALITY   HW iteration quality: dev or publish (default: dev)
#   --remote HOST       SSH target for HW bench (default: auto from run_bench.sh)
#   --local             Run HW bench locally (on RISC-V hardware)
#   --stop-on-fail      Abort immediately if any KAT test fails
#   --dry-run           Print commands without executing
#   -h, --help          Show this help
#
# Polymul scalar ablation variants (in order of optimization layering):
#   scalar_ref    : baseline (bit-serial + memset + n>>1 split)
#   scalar_zb_A   : + word-level clmul
#   scalar_zb_AC  : + word-level + specialized recursion tree (generic leaves)
#   scalar_zb     : + word-level + specialized tree + assign-on-first-write (champion)
#
# Polymul vector RVV ablation variants (in order of optimization layering):
#   scalar_zb       : baseline (Tier 1 champion — includes direct-write + overlap-save)
#   vector_rvv_V    : + RVV-vectorized recomposition (inline LMUL-tuned)
#   vector_rvv      : + RVV recomposition + RVV reduce (champion)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Ablation variant lists (in thesis presentation order) ────────────────────
SCALAR_ABLATION_VARIANTS=(
    scalar_ref
    scalar_zb_A
    scalar_zb_AC
    scalar_zb
)

VECTOR_RVV_ABLATION_VARIANTS=(
    scalar_zb
    vector_rvv_V
)

# ── Defaults ─────────────────────────────────────────────────────────────────
MODE=""
DO_SCALAR=0
DO_VECTOR_RVV=0
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
        --mode)
            [[ $# -lt 2 ]] && { echo "ERROR: --mode requires a value" >&2; exit 1; }
            MODE="$2"; shift 2 ;;
        --scalar_zb|--scalar-zb)  DO_SCALAR=1; shift ;;
        --vector_rvv|--vector-rvv) DO_VECTOR_RVV=1; shift ;;
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

# ── Auto-infer mode from study flags if not given ─────────────────────────────
if [[ -z "$MODE" ]]; then
    if [[ $DO_SCALAR -eq 1 || $DO_VECTOR_RVV -eq 1 ]]; then
        MODE="mul"
    fi
fi

# ── Auto-infer study from --variant if no study flag given ────────────────────
if [[ $DO_SCALAR -eq 0 && $DO_VECTOR_RVV -eq 0 && -n "$VARIANT_FILTER" ]]; then
    case "$VARIANT_FILTER" in
        scalar_ref|scalar_zb|scalar_zb_*)
            DO_SCALAR=1; MODE="${MODE:-mul}" ;;
        vector_rvv_*|vector_rvv)
            DO_VECTOR_RVV=1; MODE="${MODE:-mul}" ;;
        *)
            echo "ERROR: Cannot infer study from variant '${VARIANT_FILTER}'." >&2
            echo "       Specify --scalar_zb or --vector_rvv explicitly." >&2
            exit 1 ;;
    esac
fi

# ── Validation ───────────────────────────────────────────────────────────────
# Mode must be specified
if [[ -z "$MODE" ]]; then
    echo "ERROR: --mode is required (mul, keccak, decode)" >&2
    usage >&2
    exit 1
fi

case "$MODE" in
    mul) ;;
    keccak|decode)
        echo "ERROR: --mode ${MODE} is not yet implemented." >&2
        exit 1 ;;
    *)
        echo "ERROR: Unknown mode '${MODE}'. Valid: mul, keccak, decode" >&2
        exit 1 ;;
esac

# At least one study must be selected (mode-specific)
if [[ "$MODE" == "mul" && $DO_SCALAR -eq 0 && $DO_VECTOR_RVV -eq 0 ]]; then
    echo "ERROR: At least one study must be selected: --scalar_zb or --vector_rvv" >&2
    usage >&2
    exit 1
fi

# If no action flags given, default to running both --sim and --hw
if [[ $DO_KAT -eq 0 && $DO_SIM -eq 0 && $DO_HW -eq 0 ]]; then
    DO_SIM=1
    DO_HW=1
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

# Slug helper (underscore to hyphen, lowercase)
slug() { echo "${1//_/-}" | tr '[:upper:]' '[:lower:]'; }

# ── Generic ablation runner ──────────────────────────────────────────────────
# Arguments:
#   $1 - study_name (for display)
#   $2 - variants array name (nameref)
run_ablation_phases() {
    local study_name="$1"
    local -n _variants="$2"

    # ── Resolve variant list ─────────────────────────────────────────────
    local variants_to_run=()
    if [[ -n "$VARIANT_FILTER" ]]; then
        # Check that the filter is valid for this study
        local found=0
        for v in "${_variants[@]}"; do
            if [[ "$v" == "$VARIANT_FILTER" ]]; then
                found=1
                break
            fi
        done
        if [[ $found -eq 0 ]]; then
            echo "  ⏭  Skipping ${study_name}: --variant '${VARIANT_FILTER}' not in this study"
            return 0
        fi
        variants_to_run=("$VARIANT_FILTER")
    else
        variants_to_run=("${_variants[@]}")
    fi

    # ── Phase 1: KAT Validation ──────────────────────────────────────────
    if [[ $DO_KAT -eq 1 ]]; then
        header "KAT VALIDATION — ${study_name}"
        for v in "${variants_to_run[@]}"; do
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
        echo "KAT summary (${study_name}): ${PASS} passed, ${FAIL} failed"

        if [[ $FAIL -gt 0 && ($DO_SIM -eq 1 || $DO_HW -eq 1) ]]; then
            echo "WARNING: Some KAT tests failed. Benchmark results may be invalid." >&2
        fi
    fi

    # ── Phase 2: Simulation Benchmark (Spike) ────────────────────────────
    if [[ $DO_SIM -eq 1 ]]; then
        header "SIMULATION BENCHMARK — ${study_name} (Spike — instret)"
        for v in "${variants_to_run[@]}"; do
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

    # ── Phase 3: Hardware Benchmark ──────────────────────────────────────
    if [[ $DO_HW -eq 1 ]]; then
        header "HARDWARE BENCHMARK — ${study_name} (cycles)"
        for v in "${variants_to_run[@]}"; do
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
}

# ══════════════════════════════════════════════════════════════════════════════
# Run selected studies
# ══════════════════════════════════════════════════════════════════════════════

if [[ $DO_SCALAR -eq 1 ]]; then
    run_ablation_phases "Scalar Karatsuba (Tier 1)" SCALAR_ABLATION_VARIANTS
fi

if [[ $DO_VECTOR_RVV -eq 1 ]]; then
    run_ablation_phases "Vector RVV (Tier 2)" VECTOR_RVV_ABLATION_VARIANTS
fi

# ══════════════════════════════════════════════════════════════════════════════
# Phase 4: Report Generation
# ══════════════════════════════════════════════════════════════════════════════

REPORT_DIR="${REPO_ROOT}/results/reports/ablation"
mkdir -p "$REPORT_DIR"

# Resolve levels
if [[ "$LEVEL" == "all" ]]; then
    REPORT_LEVELS=(128 192 256)
else
    IFS=',' read -ra REPORT_LEVELS <<< "$LEVEL"
fi

# Data source directories (ablation CSVs live under ablation/ subdirectory)
HW_CSV_DIR="${REPO_ROOT}/results/hardware/benchmarks/components/poly_mul/ablation"
SIM_CSV_DIR="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul/ablation"

# Champion data may still be in the parent directory
HW_CSV_PARENT="${REPO_ROOT}/results/hardware/benchmarks/components/poly_mul"
SIM_CSV_PARENT="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul"

# Helper: resolve CSV path (check ablation/ first, then parent for champions)
resolve_hw_csv() {
    local slug="$1"
    local csv="${HW_CSV_DIR}/hw_mul_${slug}_latest.csv"
    [[ -f "$csv" ]] && { echo "$csv"; return; }
    csv="${HW_CSV_PARENT}/hw_mul_${slug}_latest.csv"
    [[ -f "$csv" ]] && { echo "$csv"; return; }
    echo ""
}
resolve_sim_csv() {
    local slug="$1"
    local csv="${SIM_CSV_DIR}/sim_mul_${slug}_latest.csv"
    [[ -f "$csv" ]] && { echo "$csv"; return; }
    csv="${SIM_CSV_PARENT}/sim_mul_${slug}_latest.csv"
    [[ -f "$csv" ]] && { echo "$csv"; return; }
    echo ""
}

# ── Scalar Ablation Report ───────────────────────────────────────────────────
if [[ $DO_SCALAR -eq 1 && $DRY_RUN -eq 0 ]]; then
    header "GENERATING SCALAR ABLATION REPORT"

    REPORT="${REPORT_DIR}/polymul_scalar_zbc.md"
    VARIANTS=("${SCALAR_ABLATION_VARIANTS[@]}")
    if [[ -n "$VARIANT_FILTER" ]]; then
        # Check if filter belongs to scalar study
        _in_scalar=0
        for v in "${SCALAR_ABLATION_VARIANTS[@]}"; do
            [[ "$v" == "$VARIANT_FILTER" ]] && _in_scalar=1
        done
        [[ $_in_scalar -eq 1 ]] && VARIANTS=("$VARIANT_FILTER")
    fi

    # Determine which data sources are available
    HAS_HW=0
    HAS_SIM=0
    [[ $DO_HW -eq 1 ]] && HAS_HW=1
    [[ $DO_SIM -eq 1 ]] && HAS_SIM=1

    if [[ $HAS_HW -eq 0 ]]; then
        hw_probe="$(resolve_hw_csv "scalar-ref")"
        [[ -n "$hw_probe" ]] && HAS_HW=1
    fi
    if [[ $HAS_SIM -eq 0 ]]; then
        sim_probe="$(resolve_sim_csv "scalar-ref")"
        [[ -n "$sim_probe" ]] && HAS_SIM=1
    fi

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
        echo "- **C** — Compile-time specialized recursion tree"
        echo "- **B** — Assign-on-first-write policy (tree recomposition + leaf specializations)"
        echo ""

        for lvl in "${REPORT_LEVELS[@]}"; do
            echo "## HQC-${lvl}"
            echo ""

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

            declare -A LVL_CYCLES=()
            declare -A LVL_INSTRET=()

            for v in "${VARIANTS[@]}"; do
                s="$(slug "$v")"

                if [[ $HAS_HW -eq 1 ]]; then
                    csv="$(resolve_hw_csv "$s")"
                    if [[ -n "$csv" ]]; then
                        read -r cyc inst <<< $(awk -F',' -v l="$lvl" \
                            '!/^#/&&!/^level/&&$1==l{print $6, $10}' "$csv")
                        LVL_CYCLES["$v"]="$cyc"
                        LVL_INSTRET["$v"]="$inst"
                    else
                        LVL_CYCLES["$v"]=""
                        LVL_INSTRET["$v"]=""
                    fi
                fi

                if [[ $HAS_SIM -eq 1 && -z "${LVL_INSTRET[$v]:-}" ]]; then
                    csv="$(resolve_sim_csv "$s")"
                    if [[ -n "$csv" ]]; then
                        inst=$(awk -F',' -v l="$lvl" \
                            '!/^#/&&!/^level/&&$1==l{print $3}' "$csv")
                        LVL_INSTRET["$v"]="$inst"
                    else
                        LVL_INSTRET["$v"]=""
                    fi
                fi
            done

            ref_cycles="${LVL_CYCLES[scalar_ref]:-}"
            ref_instret="${LVL_INSTRET[scalar_ref]:-}"
            a_cycles="${LVL_CYCLES[scalar_zb_A]:-}"
            a_instret="${LVL_INSTRET[scalar_zb_A]:-}"
            ac_cycles="${LVL_CYCLES[scalar_zb_AC]:-}"
            ac_instret="${LVL_INSTRET[scalar_zb_AC]:-}"

            for v in "${VARIANTS[@]}"; do
                cycles="${LVL_CYCLES[$v]:-}"
                instret="${LVL_INSTRET[$v]:-}"

                case "$v" in
                    scalar_ref)    opt_label="(none)" ;;
                    scalar_zb_A)   opt_label="A" ;;
                    scalar_zb_AC)  opt_label="A+C" ;;
                    scalar_zb)     opt_label="A+C+B" ;;
                    *)             opt_label="?" ;;
                esac

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
                            scalar_zb_AC)
                                [[ -n "$a_cycles" && "$a_cycles" -gt 0 && "$cycles" -gt 0 ]] \
                                    && col_cyc_prev=$(awk "BEGIN{printf \"×%.1f\",${a_cycles}/${cycles}}") \
                                    || col_cyc_prev="—" ;;
                            scalar_zb)
                                [[ -n "$ac_cycles" && "$ac_cycles" -gt 0 && "$cycles" -gt 0 ]] \
                                    && col_cyc_prev=$(awk "BEGIN{printf \"×%.1f\",${ac_cycles}/${cycles}}") \
                                    || col_cyc_prev="—" ;;
                            *) col_cyc_prev="—" ;;
                        esac
                    fi
                fi

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
                        scalar_zb_AC)
                            [[ -n "$a_instret" && "$a_instret" -gt 0 && "$instret" -gt 0 ]] \
                                && col_inst_prev=$(awk "BEGIN{printf \"×%.1f\",${a_instret}/${instret}}") \
                                || col_inst_prev="—" ;;
                        scalar_zb)
                            [[ -n "$ac_instret" && "$ac_instret" -gt 0 && "$instret" -gt 0 ]] \
                                && col_inst_prev=$(awk "BEGIN{printf \"×%.1f\",${ac_instret}/${instret}}") \
                                || col_inst_prev="—" ;;
                        *) col_inst_prev="—" ;;
                    esac
                fi

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
            echo "| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |"
            echo "|------------|--------------|--------:|--------:|--------:|"

            hw_compute_delta() {
                local from_var="$1" to_var="$2" lvl="$3"
                local from_csv="$(resolve_hw_csv "$(slug "$from_var")")"
                local to_csv="$(resolve_hw_csv "$(slug "$to_var")")"
                if [[ -z "$from_csv" || -z "$to_csv" ]]; then echo "—"; return; fi
                local from_val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $6}' "$from_csv")
                local to_val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $6}' "$to_csv")
                if [[ -z "$from_val" || -z "$to_val" || "$to_val" -eq 0 ]]; then echo "—"; return; fi
                awk "BEGIN{printf \"×%.2f\",${from_val}/${to_val}}"
            }

            hw_transitions=(
                "ref→A|+word-level clmul|scalar_ref|scalar_zb_A"
                "A→AC|+specialized recursion tree|scalar_zb_A|scalar_zb_AC"
                "AC→ACB|+assign-on-first-write (tree + leaf)|scalar_zb_AC|scalar_zb"
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
            echo "| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |"
            echo "|------------|--------------|--------:|--------:|--------:|"

            instret_get() {
                local var="$1" lvl="$2"
                if [[ $HAS_HW -eq 1 ]]; then
                    local csv="$(resolve_hw_csv "$(slug "$var")")"
                    if [[ -n "$csv" ]]; then
                        local val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $10}' "$csv")
                        if [[ -n "$val" ]]; then echo "$val"; return; fi
                    fi
                fi
                if [[ $HAS_SIM -eq 1 ]]; then
                    local csv="$(resolve_sim_csv "$(slug "$var")")"
                    if [[ -n "$csv" ]]; then
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
                if [[ -z "$from_val" || -z "$to_val" || "$to_val" -eq 0 ]]; then echo "—"; return; fi
                awk "BEGIN{printf \"×%.2f\",${from_val}/${to_val}}"
            }

            inst_transitions=(
                "ref→A|+word-level clmul|scalar_ref|scalar_zb_A"
                "A→AC|+specialized recursion tree|scalar_zb_A|scalar_zb_AC"
                "AC→ACB|+assign-on-first-write (tree + leaf)|scalar_zb_AC|scalar_zb"
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

    echo "  → Scalar ablation report: ${REPORT}"
fi

# ── Vector RVV Ablation Report ───────────────────────────────────────────────
if [[ $DO_VECTOR_RVV -eq 1 && $DRY_RUN -eq 0 ]]; then
    header "GENERATING VECTOR RVV ABLATION REPORT"

    REPORT="${REPORT_DIR}/polymul_vector_rvv.md"
    VARIANTS=("${VECTOR_RVV_ABLATION_VARIANTS[@]}")
    if [[ -n "$VARIANT_FILTER" ]]; then
        _in_vector=0
        for v in "${VECTOR_RVV_ABLATION_VARIANTS[@]}"; do
            [[ "$v" == "$VARIANT_FILTER" ]] && _in_vector=1
        done
        [[ $_in_vector -eq 1 ]] && VARIANTS=("$VARIANT_FILTER")
    fi

    # Determine which data sources are available
    HAS_HW=0
    HAS_SIM=0
    [[ $DO_HW -eq 1 ]] && HAS_HW=1
    [[ $DO_SIM -eq 1 ]] && HAS_SIM=1

    if [[ $HAS_HW -eq 0 ]]; then
        hw_probe="$(resolve_hw_csv "scalar-zb")"
        [[ -n "$hw_probe" ]] && HAS_HW=1
    fi
    if [[ $HAS_SIM -eq 0 ]]; then
        sim_probe="$(resolve_sim_csv "scalar-zb")"
        [[ -n "$sim_probe" ]] && HAS_SIM=1
    fi

    {
        echo "# Vector RVV Ablation Study (Tier 2)"
        echo ""
        echo "Generated: $(date '+%Y-%m-%d %H:%M')"
        if [[ $HAS_HW -eq 1 ]]; then
            echo "Platform: BPI-F3 (SpacemiT K1 X60)"
            echo "Quality: ${QUALITY}"
        fi
        echo ""
        echo "Baseline: \`scalar_zb\` (Tier 1 champion — includes direct-write discipline + overlap-save)"
        echo ""
        echo "Optimizations (layered incrementally on top of scalar_zb):"
        echo "- **V** — RVV-vectorized recomposition (inline LMUL-tuned XOR/copy at upper tree levels)"
        echo ""

        # ── Per-level tables ─────────────────────────────────────────────
        for lvl in "${REPORT_LEVELS[@]}"; do
            echo "## HQC-${lvl}"
            echo ""

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
            declare -A VEC_CYCLES=()
            declare -A VEC_INSTRET=()

            for v in "${VARIANTS[@]}"; do
                s="$(slug "$v")"

                if [[ $HAS_HW -eq 1 ]]; then
                    csv="$(resolve_hw_csv "$s")"
                    if [[ -n "$csv" ]]; then
                        read -r cyc inst <<< $(awk -F',' -v l="$lvl" \
                            '!/^#/&&!/^level/&&$1==l{print $6, $10}' "$csv")
                        VEC_CYCLES["$v"]="$cyc"
                        VEC_INSTRET["$v"]="$inst"
                    else
                        VEC_CYCLES["$v"]=""
                        VEC_INSTRET["$v"]=""
                    fi
                fi

                if [[ $HAS_SIM -eq 1 && -z "${VEC_INSTRET[$v]:-}" ]]; then
                    csv="$(resolve_sim_csv "$s")"
                    if [[ -n "$csv" ]]; then
                        inst=$(awk -F',' -v l="$lvl" \
                            '!/^#/&&!/^level/&&$1==l{print $3}' "$csv")
                        VEC_INSTRET["$v"]="$inst"
                    else
                        VEC_INSTRET["$v"]=""
                    fi
                fi
            done

            # Reference values for speedup computation
            ref_cycles="${VEC_CYCLES[scalar_zb]:-}"
            ref_instret="${VEC_INSTRET[scalar_zb]:-}"

            # Emit rows
            for v in "${VARIANTS[@]}"; do
                cycles="${VEC_CYCLES[$v]:-}"
                instret="${VEC_INSTRET[$v]:-}"

                # Optimization label
                case "$v" in
                    scalar_zb)       opt_label="(none — Tier 1 champion)" ;;
                    vector_rvv_V)    opt_label="V" ;;
                    *)               opt_label="?" ;;
                esac

                # Determine "prev" variant for vs-prev speedup
                prev_cycles="" ; prev_instret=""
                case "$v" in
                    scalar_zb)       prev_cycles=""; prev_instret="" ;;
                    vector_rvv_V)    prev_cycles="$ref_cycles"; prev_instret="$ref_instret" ;;
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

                        if [[ -z "$prev_cycles" || "$prev_cycles" -eq 0 ]]; then
                            col_cyc_prev="—"
                        else
                            col_cyc_prev=$(awk "BEGIN{printf \"×%.1f\",${prev_cycles}/${cycles}}")
                        fi
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

                    if [[ -z "$prev_instret" || "$prev_instret" -eq 0 ]]; then
                        col_inst_prev="—"
                    else
                        col_inst_prev=$(awk "BEGIN{printf \"×%.1f\",${prev_instret}/${instret}}")
                    fi
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
            echo "| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |"
            echo "|------------|--------------|--------:|--------:|--------:|"

            vec_hw_compute_delta() {
                local from_var="$1" to_var="$2" lvl="$3"
                local from_csv="$(resolve_hw_csv "$(slug "$from_var")")"
                local to_csv="$(resolve_hw_csv "$(slug "$to_var")")"
                if [[ -z "$from_csv" || -z "$to_csv" ]]; then echo "—"; return; fi
                local from_val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $6}' "$from_csv")
                local to_val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $6}' "$to_csv")
                if [[ -z "$from_val" || -z "$to_val" || "$to_val" -eq 0 ]]; then echo "—"; return; fi
                awk "BEGIN{printf \"×%.2f\",${from_val}/${to_val}}"
            }

            vec_hw_transitions=(
                "ref→V|+RVV-vectorized recomposition (inline LMUL-tuned)|scalar_zb|vector_rvv_V"
            )

            for t in "${vec_hw_transitions[@]}"; do
                IFS='|' read -r label desc from_v to_v <<< "$t"
                d128=$(vec_hw_compute_delta "$from_v" "$to_v" 128)
                d192=$(vec_hw_compute_delta "$from_v" "$to_v" 192)
                d256=$(vec_hw_compute_delta "$from_v" "$to_v" 256)
                echo "| ${label} | ${desc} | ${d128} | ${d192} | ${d256} |"
            done
            echo ""
        fi

        # ── Incremental Optimization Contribution (instret) ──────────────
        if [[ $HAS_SIM -eq 1 || $HAS_HW -eq 1 ]]; then
            echo "## Incremental Optimization Contribution (instret)"
            echo ""
            echo "| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |"
            echo "|------------|--------------|--------:|--------:|--------:|"

            vec_instret_get() {
                local var="$1" lvl="$2"
                if [[ $HAS_HW -eq 1 ]]; then
                    local csv="$(resolve_hw_csv "$(slug "$var")")"
                    if [[ -n "$csv" ]]; then
                        local val=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $10}' "$csv")
                        if [[ -n "$val" ]]; then echo "$val"; return; fi
                    fi
                fi
                if [[ $HAS_SIM -eq 1 ]]; then
                    local csv="$(resolve_sim_csv "$(slug "$var")")"
                    if [[ -n "$csv" ]]; then
                        awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $3}' "$csv"
                        return
                    fi
                fi
                echo ""
            }

            vec_instret_compute_delta() {
                local from_var="$1" to_var="$2" lvl="$3"
                local from_val=$(vec_instret_get "$from_var" "$lvl")
                local to_val=$(vec_instret_get "$to_var" "$lvl")
                if [[ -z "$from_val" || -z "$to_val" || "$to_val" -eq 0 ]]; then echo "—"; return; fi
                awk "BEGIN{printf \"×%.2f\",${from_val}/${to_val}}"
            }

            vec_inst_transitions=(
                "ref→V|+RVV-vectorized recomposition (inline LMUL-tuned)|scalar_zb|vector_rvv_V"
            )

            for t in "${vec_inst_transitions[@]}"; do
                IFS='|' read -r label desc from_v to_v <<< "$t"
                d128=$(vec_instret_compute_delta "$from_v" "$to_v" 128)
                d192=$(vec_instret_compute_delta "$from_v" "$to_v" 192)
                d256=$(vec_instret_compute_delta "$from_v" "$to_v" 256)
                echo "| ${label} | ${desc} | ${d128} | ${d192} | ${d256} |"
            done
            echo ""
        fi

        # ── Comparison vs Tier 1 champion ────────────────────────────────
        echo "## Comparison: Tier 2 champion vs Tier 1 champion"
        echo ""
        echo "| Level | \`scalar_zb\` (Tier 1) | \`vector_rvv_V\` (Tier 2) | RVV speedup |"
        echo "|-------|---------------------:|----------------------:|------------:|"

        for lvl in "${REPORT_LEVELS[@]}"; do
            # Get scalar_zb data
            scalar_inst=""
            if [[ $HAS_SIM -eq 1 ]]; then
                csv="$(resolve_sim_csv "scalar-zb")"
                [[ -n "$csv" ]] && scalar_inst=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $3}' "$csv")
            fi
            if [[ -z "$scalar_inst" && $HAS_HW -eq 1 ]]; then
                csv="$(resolve_hw_csv "scalar-zb")"
                [[ -n "$csv" ]] && scalar_inst=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $10}' "$csv")
            fi

            # Get vector_rvv_V data
            rvv_inst=""
            if [[ $HAS_SIM -eq 1 ]]; then
                csv="$(resolve_sim_csv "vector-rvv-v")"
                [[ -n "$csv" ]] && rvv_inst=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $3}' "$csv")
            fi
            if [[ -z "$rvv_inst" && $HAS_HW -eq 1 ]]; then
                csv="$(resolve_hw_csv "vector-rvv-v")"
                [[ -n "$csv" ]] && rvv_inst=$(awk -F',' -v l="$lvl" '!/^#/&&!/^level/&&$1==l{print $10}' "$csv")
            fi

            if [[ -n "$scalar_inst" && -n "$rvv_inst" && "$rvv_inst" -gt 0 ]]; then
                scalar_fmt=$(printf "%'d" "$scalar_inst")
                rvv_fmt=$(printf "%'d" "$rvv_inst")
                speedup=$(awk "BEGIN{printf \"×%.2f\",${scalar_inst}/${rvv_inst}}")
                echo "| HQC-${lvl} | ${scalar_fmt} | ${rvv_fmt} | ${speedup} |"
            else
                echo "| HQC-${lvl} | — | — | — |"
            fi
        done
        echo ""

    } > "$REPORT"

    echo "  → Vector RVV ablation report: ${REPORT}"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
header "ABLATION STUDY COMPLETE"
echo "  Mode:             ${MODE}"
echo "  Studies run:      $(( DO_SCALAR + DO_VECTOR_RVV ))"
echo "  Failures:         ${FAIL}"

if [[ $FAIL -gt 0 ]]; then
    exit 1
fi
