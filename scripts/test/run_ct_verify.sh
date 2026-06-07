#!/usr/bin/env bash
# scripts/test/run_ct_verify.sh
# Constant-time verification runner for HQC RISC-V (dudect methodology).
#
# Examples:
#   run_ct_verify.sh --all --mode dev
#   run_ct_verify.sh --target polymul --level 128 --polymul scalar_zb --runs 10

set -euo pipefail

# ── Defaults ─────────────────────────────────────────────────────────────────
ALL_MODE=0
TARGET=""
LEVEL=""
POLYMUL=""
MODE="dev"
RUNS=5
CORE=7
SEED=""
THRESHOLD="4.5"
BUILD_FAILURES=0
TOTAL_LEAKS=0
RESULTS=()

# ── Variant Matrix Constants ─────────────────────────────────────────────────
TARGETS=(polymul kem)
LEVELS=(128 192 256)
POLYMULS=(scalar_ref scalar_zb vector_rvv)

# ── Stub: usage (implemented in task 1.2) ────────────────────────────────────
usage() {
    echo "Usage: run_ct_verify.sh --all [OPTIONS]"
    echo "       run_ct_verify.sh --target TARGET --level LEVEL --polymul POLYMUL [OPTIONS]"
}

# ── Helper: die ──────────────────────────────────────────────────────────────
die() {
    echo "$1" >&2
    exit 1
}

# ── Validation ───────────────────────────────────────────────────────────────
validate_args() {
    # Mutual exclusivity: --all vs --target/--level/--polymul
    if [[ "$ALL_MODE" -eq 1 ]]; then
        if [[ -n "$TARGET" || -n "$LEVEL" || -n "$POLYMUL" ]]; then
            die "ERROR: --all is mutually exclusive with --target/--level/--polymul"
        fi
    else
        # All three required for single-variant mode
        if [[ -z "$TARGET" || -z "$LEVEL" || -z "$POLYMUL" ]]; then
            die "ERROR: Must specify --all OR all of --target, --level, --polymul"
        fi
    fi

    # Mode validation
    if [[ "$MODE" != "dev" && "$MODE" != "publish" ]]; then
        die "ERROR: --mode must be 'dev' or 'publish', got: '$MODE'"
    fi

    # Target validation (when specified)
    if [[ -n "$TARGET" && "$TARGET" != "polymul" && "$TARGET" != "kem" ]]; then
        die "ERROR: --target must be 'polymul' or 'kem', got: '$TARGET'"
    fi

    # Level validation (when specified)
    if [[ -n "$LEVEL" && "$LEVEL" != "128" && "$LEVEL" != "192" && "$LEVEL" != "256" ]]; then
        die "ERROR: --level must be 128, 192, or 256, got: '$LEVEL'"
    fi

    # Polymul validation (when specified)
    local valid_polymuls="scalar_ref scalar_zb vector_rvv"
    if [[ -n "$POLYMUL" ]] && ! echo "$valid_polymuls" | grep -qw "$POLYMUL"; then
        die "ERROR: --polymul must be one of: $valid_polymuls; got: '$POLYMUL'"
    fi
}

# ── Variant Expansion ────────────────────────────────────────────────────────
expand_variants() {
    if [[ "$ALL_MODE" -eq 1 ]]; then
        VARIANTS=()
        for t in "${TARGETS[@]}"; do
            for l in "${LEVELS[@]}"; do
                for p in "${POLYMULS[@]}"; do
                    VARIANTS+=("${t}:${l}:${p}")
                done
            done
        done
    else
        VARIANTS=("${TARGET}:${LEVEL}:${POLYMUL}")
    fi
}

# ── Configuration Display ────────────────────────────────────────────────────
_box_line() {
    local content="$1"
    local BOX_W=61
    printf "║  %-$(( BOX_W - 4 ))s  ║\n" "$content"
}

print_config() {
    local BOX_W=61
    echo "╔$(printf '═%.0s' $(seq 1 $BOX_W))╗"
    _box_line "run_ct_verify.sh — Constant-Time Verification"
    _box_line ""
    if [[ "$ALL_MODE" -eq 1 ]]; then
        _box_line "Scope:     full matrix (18 variants)"
    else
        _box_line "Target:    $TARGET"
        _box_line "Level:     $LEVEL"
        _box_line "Polymul:   $POLYMUL"
    fi
    _box_line "Mode:      $MODE"
    _box_line "Runs:      $RUNS"
    _box_line "Core:      $CORE"
    _box_line "Threshold: $THRESHOLD"
    if [[ -n "$SEED" ]]; then
        _box_line "Seed:      $SEED (fixed)"
    else
        _box_line "Seed:      /dev/urandom (per-trial)"
    fi
    echo "╚$(printf '═%.0s' $(seq 1 $BOX_W))╝"
}

# ── Build Phase ───────────────────────────────────────────────────────────────
build_variant() {
    local target="$1" level="$2" polymul="$3"

    local make_args="make -f Makefile.hw SUITE=dudect TARGET=$target LEVEL=$level POLYMUL=$polymul MODE=$MODE"

    if [[ -n "$SEED" ]]; then
        make_args+=" SEED=$SEED"
    fi
    if [[ "$THRESHOLD" != "4.5" ]]; then
        make_args+=" T_THRESH=$THRESHOLD"
    fi

    echo "[build] $target/hqc${level}/${polymul} (MODE=$MODE)"
    if ! eval "$make_args" 2>&1; then
        echo "[build] FAILED: $target/hqc${level}/${polymul}" >&2
        BUILD_FAILURES=$((BUILD_FAILURES + 1))
        return 1
    fi
}

# ── Execution Phase (Multi-Run Trials) ────────────────────────────────────────
run_trials() {
    local target="$1" level="$2" polymul="$3"
    local binary="build-hw/dudect_${target}_hqc${level}_${polymul}"
    local variant_dir="results/ct_verify/dudect/${target}_hqc${level}_${polymul}"

    mkdir -p "$variant_dir"

    local -a t1_vals t2_vals verdicts
    local leakage_count=0

    for ((r=1; r<=RUNS; r++)); do
        local run_outdir="${variant_dir}/run_${r}"
        mkdir -p "$run_outdir"

        # Process isolation: sudo nice -n -20 taskset -c $CORE
        local output
        output=$(sudo nice -n -20 taskset -c "$CORE" "./$binary" --outdir "$run_outdir" 2>&1)

        # Parse result line
        local result_line t1 t2 verdict
        result_line=$(echo "$output" | grep '\[dudect\] RESULT:' || true)

        if echo "$result_line" | grep -q "LEAKAGE DETECTED"; then
            verdict="LEAK"
            leakage_count=$((leakage_count + 1))
        else
            verdict="PASS"
        fi

        t1=$(echo "$result_line" | grep -oP 't1=\K[-0-9.]+' || echo "0.0")
        t2=$(echo "$result_line" | grep -oP 't2=\K[-0-9.]+' || echo "0.0")

        t1_vals+=("$t1")
        t2_vals+=("$t2")
        verdicts+=("$verdict")

        echo "  run $r/$RUNS: $verdict  t1=$t1  t2=$t2"
    done

    # Store results in global arrays for compute_summary_stats
    TRIAL_T1S=("${t1_vals[@]}")
    TRIAL_T2S=("${t2_vals[@]}")
    TRIAL_VERDICTS=("${verdicts[@]}")
    TRIAL_LEAKAGE_COUNT=$leakage_count
    TRIAL_VARIANT_DIR="$variant_dir"
}

# ── Summary Statistics Computation ────────────────────────────────────────────
compute_summary_stats() {
    local variant_dir="$TRIAL_VARIANT_DIR"

    # Compute mean |t1|, max |t1|
    local mean_t1 max_t1 mean_t2 max_t2
    mean_t1=$(printf '%s\n' "${TRIAL_T1S[@]}" | awk 'BEGIN{s=0;n=0}{v=($1<0?-$1:$1);s+=v;n++}END{printf "%.3f",s/n}')
    max_t1=$(printf '%s\n' "${TRIAL_T1S[@]}" | awk 'BEGIN{m=0}{v=($1<0?-$1:$1);if(v>m)m=v}END{printf "%.3f",m}')

    # Compute mean |t2|, max |t2|
    mean_t2=$(printf '%s\n' "${TRIAL_T2S[@]}" | awk 'BEGIN{s=0;n=0}{v=($1<0?-$1:$1);s+=v;n++}END{printf "%.3f",s/n}')
    max_t2=$(printf '%s\n' "${TRIAL_T2S[@]}" | awk 'BEGIN{m=0}{v=($1<0?-$1:$1);if(v>m)m=v}END{printf "%.3f",m}')

    # Write summary CSV: header + N data rows
    echo "run,verdict,t1,t2" > "${variant_dir}/summary.csv"
    for ((i=0; i<${#TRIAL_VERDICTS[@]}; i++)); do
        echo "$((i+1)),${TRIAL_VERDICTS[$i]},${TRIAL_T1S[$i]},${TRIAL_T2S[$i]}" >> "${variant_dir}/summary.csv"
    done

    # Determine overall variant verdict: LEAK if any trial leaked, else PASS
    if [[ "$TRIAL_LEAKAGE_COUNT" -gt 0 ]]; then
        VARIANT_VERDICT="LEAK"
        TOTAL_LEAKS=$((TOTAL_LEAKS + 1))
    else
        VARIANT_VERDICT="PASS"
    fi

    # Use max of mean_t1/mean_t2 as representative mean |t| for summary table
    VARIANT_MEAN_T=$(awk "BEGIN{printf \"%.3f\", ($mean_t1 > $mean_t2 ? $mean_t1 : $mean_t2)}")
    VARIANT_MAX_T=$(awk "BEGIN{printf \"%.3f\", ($max_t1 > $max_t2 ? $max_t1 : $max_t2)}")
}

# ── Summary Table Printer ─────────────────────────────────────────────────────
print_summary_table() {
    echo ""
    echo "════════════════════════════════════════════════════════════════════════════"
    echo " CT VERIFICATION SUMMARY"
    echo "════════════════════════════════════════════════════════════════════════════"
    printf " %-10s %-5s %-15s %-8s %-10s %-10s\n" \
        "TARGET" "LEVEL" "POLYMUL" "VERDICT" "mean|t|" "max|t|"
    echo " ──────────────────────────────────────────────────────────────────────────"

    for entry in "${RESULTS[@]}"; do
        # entry format: "target:level:polymul:verdict:mean_t:max_t"
        IFS=':' read -r t l p v mt xt <<< "$entry"
        local marker=""
        if [[ "$v" == "LEAK" ]]; then
            marker="⚠"
        else
            marker="✓"
        fi
        printf " %-10s %-5s %-15s %s %-6s %-10s %-10s\n" "$t" "$l" "$p" "$marker" "$v" "$mt" "$xt"
    done
    echo "════════════════════════════════════════════════════════════════════════════"
}

# ── Exit Code Logic ───────────────────────────────────────────────────────────
determine_exit_code() {
    # Non-zero if any variant leaked OR any variant failed to build
    if [[ "$TOTAL_LEAKS" -gt 0 || "$BUILD_FAILURES" -gt 0 ]]; then
        exit 1
    fi
    exit 0
}

# ── Argument Parsing ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)
            ALL_MODE=1
            shift
            ;;
        --target)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --target requires a value" >&2
                exit 1
            fi
            TARGET="$2"
            shift 2
            ;;
        --level)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --level requires a value" >&2
                exit 1
            fi
            LEVEL="$2"
            shift 2
            ;;
        --polymul)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --polymul requires a value" >&2
                exit 1
            fi
            POLYMUL="$2"
            shift 2
            ;;
        --mode)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --mode requires a value" >&2
                exit 1
            fi
            MODE="$2"
            shift 2
            ;;
        --runs)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --runs requires a value" >&2
                exit 1
            fi
            RUNS="$2"
            shift 2
            ;;
        --core)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --core requires a value" >&2
                exit 1
            fi
            CORE="$2"
            shift 2
            ;;
        --seed)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --seed requires a value" >&2
                exit 1
            fi
            SEED="$2"
            shift 2
            ;;
        --threshold)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --threshold requires a value" >&2
                exit 1
            fi
            THRESHOLD="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# ── Validate parsed arguments ────────────────────────────────────────────────
validate_args

# ── Main Pipeline ────────────────────────────────────────────────────────────
main() {
    # Stage 3: Print configuration summary
    print_config

    # Stage 4: Expand variants (populates VARIANTS array)
    expand_variants

    # Stage 5: Per-variant loop (build → execute → accumulate)
    for variant in "${VARIANTS[@]}"; do
        IFS=':' read -r target level polymul <<< "$variant"

        # Build phase
        if ! build_variant "$target" "$level" "$polymul"; then
            # Build failed — skip execution, continue to next variant
            RESULTS+=("${target}:${level}:${polymul}:BUILD_FAIL:N/A:N/A")
            continue
        fi

        # Execution phase: run trials and compute summary stats
        run_trials "$target" "$level" "$polymul"
        compute_summary_stats

        # Accumulate results
        RESULTS+=("${target}:${level}:${polymul}:${VARIANT_VERDICT}:${VARIANT_MEAN_T}:${VARIANT_MAX_T}")
    done

    # Stage 6: Print summary table
    print_summary_table

    # Stage 7: Determine exit code
    determine_exit_code
}

# Run the main pipeline
main
