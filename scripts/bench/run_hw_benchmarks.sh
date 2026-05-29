#!/usr/bin/env bash
# scripts/bench/run_hw_benchmarks.sh
# Unified hardware benchmark runner for BPI-F3 (SpacemiT X60).
#
# Modes (--mode <mode>):
#   mul              : polymul micro-benchmark (default)
#   kem              : full KEM benchmark (keypair/encaps/decaps)
#   publish          : publish-quality measurement (N=10000, nice -n -20)
#   suite            : batch execution across all variants × levels × ops
#   sweep-threshold  : Karatsuba threshold sweep (T=8, T=16, T=32)
#   sweep-lmul       : LMUL sweep per operation class
#
# Execution targets:
#   --local          : run directly on the board (default if uname -m == riscv64)
#   --remote HOST    : deploy via rsync + run via SSH
#
# Examples:
#   run_hw_benchmarks.sh --mode mul --mul karatsuba_rvv --level 128
#   run_hw_benchmarks.sh --mode mul --mul karatsuba_rvv --level all
#   run_hw_benchmarks.sh --mode publish --mul karatsuba_rvv
#   run_hw_benchmarks.sh --mode sweep-threshold --mul karatsuba_rvv
#   run_hw_benchmarks.sh --mode sweep-lmul --mul karatsuba_rvv
#   run_hw_benchmarks.sh --mode suite
#   run_hw_benchmarks.sh --mode suite --publish
#   run_hw_benchmarks.sh --remote bpi-f3 --mode mul --mul karatsuba_rvv

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Source hardware-specific helpers (which in turn sources lib_bench.sh).
# shellcheck source=lib_hw_bench.sh
source "${SCRIPT_DIR}/lib_hw_bench.sh"

# ── Defaults ─────────────────────────────────────────────────────────────────
MODE="mul"
MUL_VARIANT="karatsuba_rvv"
LEVEL="128"
REMOTE_HOST=""
LOCAL_MODE=0
BUILD_ONLY=0
CORE=7
COMPARE=1
PUBLISH=0
THRESHOLDS="8 16 32"
TIMESTAMP="$(date +'%Y-%m-%d_%H-%M')"

BENCH_DIR="${REPO_ROOT}/tests/bench"
GEN_SCRIPT="${REPO_ROOT}/scripts/gen/gen_karatsuba_specialized.py"
OUT_DIR="${REPO_ROOT}/results/hardware/benchmarks"

# ── Auto-detect local mode ───────────────────────────────────────────────────
if [[ "$(uname -m)" == "riscv64" ]]; then
    LOCAL_MODE=1
fi

# ── Usage ────────────────────────────────────────────────────────────────────
usage() {
    cat <<'EOF'
Usage:
  run_hw_benchmarks.sh [--mode MODE] [--mul VARIANT] [--level LEVEL]
                       [--local | --remote HOST] [--build-only] [--no-compare]
                       [--core N] [--thresholds "T1 T2 ..."]

Modes:
  mul               Polymul micro-benchmark (N=100, default)
  kem               Full KEM benchmark (N=100)
  publish           Publish-quality polymul (N=10000, nice -n -20)
  suite             Batch: all variants × levels × ops (mul + kem)
  sweep-threshold   Karatsuba threshold sweep (T=8, T=16, T=32)
  sweep-lmul        LMUL sweep per operation class

Options:
  --mul VARIANT     Polymul variant (default: karatsuba_rvv)
  --level LEVEL     Security level: 128, 192, 256, or all (default: 128)
  --local           Run on this machine (auto-detected on riscv64)
  --remote HOST     Deploy and run via SSH
  --build-only      Build without running
  --publish         Use publish-quality settings (N=10000, nice -n -20)
  --no-compare      Skip comparison against karatsuba_zbc
  --core N          CPU core to pin to (default: 7)
  --thresholds "…"  Threshold values for sweep-threshold (default: "8 16 32")
  -h, --help        Show this help
EOF
}

# ── Parse arguments ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)       MODE="$2"; shift 2 ;;
        --mul)        MUL_VARIANT="$2"; shift 2 ;;
        --level)
            if [[ "$2" == "all" ]]; then LEVEL="128 192 256"
            else LEVEL="$2"; fi
            shift 2 ;;
        --local)      LOCAL_MODE=1; shift ;;
        --remote)     REMOTE_HOST="$2"; LOCAL_MODE=0; shift 2 ;;
        --build-only) BUILD_ONLY=1; shift ;;
        --no-compare) COMPARE=0; shift ;;
        --publish)    PUBLISH=1; shift ;;
        --core)       CORE="$2"; shift 2 ;;
        --thresholds) THRESHOLDS="$2"; shift 2 ;;
        -h|--help)    usage; exit 0 ;;
        *)            echo "ERROR: Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ "$LOCAL_MODE" -eq 0 && -z "$REMOTE_HOST" ]]; then
    echo "ERROR: Not on riscv64 — specify --local or --remote HOST" >&2
    exit 1
fi

# ── Helpers ──────────────────────────────────────────────────────────────────
REMOTE_DIR="~/hqc-riscv"

run_cmd() {
    # Run a command locally or remotely
    if [[ "$LOCAL_MODE" -eq 1 ]]; then
        eval "$1"
    else
        ssh "$REMOTE_HOST" "$1"
    fi
}

deploy() {
    if [[ "$LOCAL_MODE" -eq 1 ]]; then return; fi
    echo "[deploy] Syncing to ${REMOTE_HOST}:${REMOTE_DIR} ..."
    rsync -az --delete \
        --exclude='.venv' --exclude='build*' --exclude='.git' \
        --exclude='__pycache__' --exclude='.hypothesis' \
        "${REPO_ROOT}/" "${REMOTE_HOST}:${REMOTE_DIR}/"
}

retrieve_results() {
    # Retrieve benchmark results from the remote board back to the local machine.
    # Only runs in remote mode (LOCAL_MODE=0).
    if [[ "$LOCAL_MODE" -eq 1 ]]; then return; fi
    echo "[retrieve] Syncing results from ${REMOTE_HOST}:${REMOTE_DIR}/results/hardware/benchmarks/ ..."
    rsync -az \
        "${REMOTE_HOST}:${REMOTE_DIR}/results/hardware/benchmarks/" \
        "${REPO_ROOT}/results/hardware/benchmarks/"
}

set_governor() {
    run_cmd "echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true"
}

hw_build() {
    local level="$1" variant="$2" make_mode="$3"
    local dir="$BENCH_DIR"
    if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}/tests/bench"; fi
    run_cmd "cd $dir && make -f Makefile.hw LEVEL=$level POLYMUL=$variant MODE=$make_mode clean >/dev/null 2>&1; make -f Makefile.hw LEVEL=$level POLYMUL=$variant MODE=$make_mode mul 2>&1 | tail -2"
}

hw_run() {
    local level="$1" variant="$2" use_nice="$3"
    local dir="$BENCH_DIR"
    if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}/tests/bench"; fi
    local bin="bench_hw_mul_hqc${level}_${variant}"
    local cmd="cd $dir && taskset -c $CORE ./$bin"
    if [[ "$use_nice" == "1" ]]; then
        cmd="cd $dir && sudo nice -n -20 taskset -c $CORE ./$bin"
    fi
    run_cmd "$cmd"
}

extract_median_cycles() { echo "$1" | awk -F',' '/^vect_mul,/ {print $4}'; }
extract_median_instret() { echo "$1" | awk -F',' '/^vect_mul,/ {print $9}'; }
extract_stddev_cycles() { echo "$1" | awk -F',' '/^vect_mul,/ {print $7}'; }

print_result() {
    local label="$1" output="$2"
    local cyc ins
    cyc="$(extract_median_cycles "$output")"
    ins="$(extract_median_instret "$output")"
    if [[ -n "$cyc" && -n "$ins" ]]; then
        local ipc
        ipc=$(awk "BEGIN {printf \"%.3f\", $ins / $cyc}")
        printf "  %-24s median_cycles=%-8s instret=%-8s IPC=%s\n" "$label" "$cyc" "$ins" "$ipc"
    else
        printf "  %-24s (no data)\n" "$label"
    fi
}

save_csv() {
    # Write a structured CSV with metadata header, column header, and data rows.
    # Uses hw_csv_header() from lib_hw_bench.sh for the metadata comment block,
    # then emits the canonical column header and prepends the level to each data row.
    #
    # Arguments:
    #   $1 - raw benchmark output (comment line + old header + data rows)
    #   $2 - security level (128, 192, 256)
    #   $3 - variant name (e.g. karatsuba_rvv)
    #   $4 - mode (dev | publish) — determines header fields and CoV inclusion
    local output="$1" level="$2" variant="$3" mode_suffix="$4"

    # Determine iteration counts from mode for the CSV header.
    local n_iter n_warmup
    if [[ "$mode_suffix" == "publish" ]]; then
        n_iter=10000
        n_warmup=100
    else
        n_iter=100
        n_warmup=10
    fi

    # Compute CoV for publish mode header.
    local cov_arg=""
    if [[ "$mode_suffix" == "publish" ]]; then
        local cyc std
        cyc="$(extract_median_cycles "$output")"
        std="$(extract_stddev_cycles "$output")"
        if [[ -n "$cyc" && -n "$std" ]]; then
            cov_arg=$(awk "BEGIN {printf \"%.4f\", $std / $cyc}")
        fi
    fi

    # Build full CSV content: metadata header + column header + data rows.
    local csv_content
    csv_content="$(hw_csv_header "$variant" "$n_iter" "$n_warmup" "$mode_suffix" "$cov_arg")"

    # Append the canonical CSV column header (with level prepended).
    csv_content="${csv_content}"$'\n'"level,op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"

    # Append data rows: skip comment lines and the old header from raw output,
    # prepend the level to each data row.
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        [[ "$line" == \#* ]] && continue
        [[ "$line" == op,* ]] && continue
        csv_content="${csv_content}"$'\n'"${level},${line}"
    done <<< "$output"

    # Store via hw_store_result (handles per-variant subdir, symlink, archive).
    hw_store_result "$csv_content" "$variant" "mul" "$TIMESTAMP"
}

# ── Mode: mul / kem ──────────────────────────────────────────────────────────
run_mode_mul() {
    local make_mode="dev" use_nice=0
    if [[ "$MODE" == "publish" ]]; then
        make_mode="publish"
        use_nice=1
    fi

    echo "╔══════════════════════════════════════════════════════════════╗"
    printf "║  Hardware Benchmark — %-36s  ║\n" "$MUL_VARIANT"
    printf "║  Mode: %-6s  Level: %-8s  Core: %-3s               ║\n" "$make_mode" "$LEVEL" "$CORE"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    deploy
    set_governor

    for level in $LEVEL; do
        echo "━━━ HQC-${level} ━━━"

        # Build and run target variant
        hw_build "$level" "$MUL_VARIANT" "$make_mode"
        if [[ "$BUILD_ONLY" -eq 1 ]]; then
            echo "  [build-only] Done."
            continue
        fi

        local output
        output="$(hw_run "$level" "$MUL_VARIANT" "$use_nice")"
        print_result "$MUL_VARIANT" "$output"
        save_csv "$output" "$level" "$MUL_VARIANT" "$make_mode"

        # CoV check for publish mode
        if [[ "$make_mode" == "publish" ]]; then
            local cyc std cov
            cyc="$(extract_median_cycles "$output")"
            std="$(extract_stddev_cycles "$output")"
            if [[ -n "$cyc" && -n "$std" ]]; then
                cov=$(awk "BEGIN {printf \"%.4f\", $std / $cyc}")
                local pct=$(awk "BEGIN {printf \"%.2f\", $cov * 100}")
                local verdict="✅"
                if (( $(awk "BEGIN {print ($cov > 0.02)}") )); then verdict="❌ (>2%)"; fi
                echo "  CoV: ${pct}% ${verdict}"
            fi
        fi

        # Comparison run
        if [[ "$COMPARE" -eq 1 && "$MUL_VARIANT" != "karatsuba_zbc" ]]; then
            hw_build "$level" "karatsuba_zbc" "$make_mode"
            local zbc_output
            zbc_output="$(hw_run "$level" "karatsuba_zbc" "$use_nice")"
            print_result "karatsuba_zbc" "$zbc_output"

            # Compute delta
            local rvv_cyc zbc_cyc
            rvv_cyc="$(extract_median_cycles "$output")"
            zbc_cyc="$(extract_median_cycles "$zbc_output")"
            if [[ -n "$rvv_cyc" && -n "$zbc_cyc" ]]; then
                local delta
                delta=$(awk "BEGIN {printf \"%+.1f\", ($rvv_cyc / $zbc_cyc - 1) * 100}")
                echo "  Δ vs zbc: ${delta}%"
            fi
        fi
        echo ""
    done

    # Retrieve results from the remote board after all benchmarks complete.
    retrieve_results
}

# ── Mode: sweep-threshold ────────────────────────────────────────────────────
run_mode_sweep_threshold() {
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║  Threshold Sweep — ${MUL_VARIANT}, HQC-128                      ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    # This mode requires running on the board (code generation + build + run)
    if [[ "$LOCAL_MODE" -eq 0 ]]; then
        deploy
        echo "[sweep] Running threshold sweep remotely..."
        # Generate + build + run for each threshold on the board
        for T in $THRESHOLDS; do
            echo "━━━ T=${T} ━━━"
            run_cmd "cd ${REMOTE_DIR} && python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv_opt --threshold $T"
            run_cmd "cd ${REMOTE_DIR}/tests/bench && make -f Makefile.hw LEVEL=128 POLYMUL=$MUL_VARIANT MODE=dev clean >/dev/null 2>&1; make -f Makefile.hw LEVEL=128 POLYMUL=$MUL_VARIANT MODE=dev mul 2>&1 | tail -1"
            local output
            output="$(run_cmd "cd ${REMOTE_DIR}/tests/bench && taskset -c $CORE ./bench_hw_mul_hqc128_${MUL_VARIANT}")"
            print_result "T=${T}" "$output"
            echo ""
        done
        # Restore default
        run_cmd "cd ${REMOTE_DIR} && python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv_opt --threshold 16"
        echo "[sweep] Restored default T=16."
    else
        set_governor
        for T in $THRESHOLDS; do
            echo "━━━ T=${T} ━━━"
            python3 "$GEN_SCRIPT" --mode rvv_opt --threshold "$T"
            (cd "$BENCH_DIR" && make -f Makefile.hw LEVEL=128 POLYMUL="$MUL_VARIANT" MODE=dev clean >/dev/null 2>&1 && make -f Makefile.hw LEVEL=128 POLYMUL="$MUL_VARIANT" MODE=dev mul 2>&1 | tail -1)
            local output
            output="$(cd "$BENCH_DIR" && taskset -c "$CORE" "./bench_hw_mul_hqc128_${MUL_VARIANT}")"
            print_result "T=${T}" "$output"
            echo ""
        done
        python3 "$GEN_SCRIPT" --mode rvv_opt --threshold 16
        echo "[sweep] Restored default T=16."
    fi
}

# ── Mode: sweep-lmul ────────────────────────────────────────────────────────
run_mode_sweep_lmul() {
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║  LMUL Sweep — ${MUL_VARIANT}, HQC-128                           ║"
    echo "║  Validates: Requirement 3.3                                ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""
    echo "  This mode patches the code generator to test alternative LMUL"
    echo "  values for each operation class. Must run on the board."
    echo ""

    if [[ "$LOCAL_MODE" -eq 0 ]]; then
        echo "ERROR: sweep-lmul must run on the board (--local)." >&2
        echo "       It patches source files and requires native compilation." >&2
        echo "       Deploy the repo and run: scripts/bench/run_hw_benchmarks.sh --local --mode sweep-lmul" >&2
        exit 1
    fi

    set_governor

    # Backup originals
    local gen_bak="${GEN_SCRIPT}.lmul_bak"
    local src_file="${REPO_ROOT}/src/mult/karatsuba_rvv/gf2x_clmul.c"
    local src_bak="${src_file}.lmul_bak"
    cp "$GEN_SCRIPT" "$gen_bak"
    cp "$src_file" "$src_bak"

    cleanup_lmul() {
        cp "$gen_bak" "$GEN_SCRIPT"
        cp "$src_bak" "$src_file"
        rm -f "$gen_bak" "$src_bak"
        python3 "$GEN_SCRIPT" --mode rvv_opt --threshold 16
        echo "[cleanup] Restored original files."
    }
    trap cleanup_lmul EXIT

    run_lmul_variant() {
        local label="$1"
        python3 "$GEN_SCRIPT" --mode rvv_opt --threshold 16
        (cd "$BENCH_DIR" && make -f Makefile.hw LEVEL=128 POLYMUL="$MUL_VARIANT" MODE=dev clean >/dev/null 2>&1 && make -f Makefile.hw LEVEL=128 POLYMUL="$MUL_VARIANT" MODE=dev mul >/dev/null 2>&1)
        local output
        output="$(cd "$BENCH_DIR" && taskset -c "$CORE" "./bench_hw_mul_hqc128_${MUL_VARIANT}")"
        print_result "$label" "$output"
    }

    # Sweep 1: ta/tb XOR — m4 vs m8
    echo "━━━ ta/tb XOR preparation (large operands) ━━━"
    echo "  Current: m8 for n1>=28"
    echo ""

    # Force m4 for all
    sed -i 's/if n1 >= 28:/if False:/' "$GEN_SCRIPT"
    run_lmul_variant "ta/tb LMUL=m4"

    # Force m8 for all (restore condition, force m8 in else)
    cp "$gen_bak" "$GEN_SCRIPT"
    sed -i 's/if n1 >= 28:/if True:/' "$GEN_SCRIPT"
    run_lmul_variant "ta/tb LMUL=m8"
    cp "$gen_bak" "$GEN_SCRIPT"
    echo ""

    # Sweep 2: XOR3 — m1 vs m2 vs m4
    echo "━━━ XOR3 accumulation (Loop 3) ━━━"
    echo "  Current: m2"
    echo ""

    for lmul in m1 m2 m4; do
        cp "$gen_bak" "$GEN_SCRIPT"
        if [[ "$lmul" != "m2" ]]; then
            sed -i "s/__riscv_vsetvl_e64m2/__riscv_vsetvl_e64${lmul}/g" "$GEN_SCRIPT"
            sed -i "s/vuint64m2_t/vuint64${lmul}_t/g" "$GEN_SCRIPT"
            sed -i "s/_v_u64m2/_v_u64${lmul}/g" "$GEN_SCRIPT"
            sed -i "s/_vv_u64m2/_vv_u64${lmul}/g" "$GEN_SCRIPT"
        fi
        run_lmul_variant "XOR3 LMUL=${lmul}"
    done
    cp "$gen_bak" "$GEN_SCRIPT"
    echo ""

    # Sweep 3: Reduction — m4 vs m8
    echo "━━━ Reduction phase ━━━"
    echo "  Current: m8"
    echo ""

    for lmul in m4 m8; do
        cp "$src_bak" "$src_file"
        if [[ "$lmul" != "m8" ]]; then
            sed -i '/^static void reduce_rvv/,/^}/{
                s/__riscv_vsetvl_e64m8/__riscv_vsetvl_e64'"${lmul}"'/g
                s/vuint64m8_t/vuint64'"${lmul}"'_t/g
                s/_v_u64m8/_v_u64'"${lmul}"'/g
                s/_vv_u64m8/_vv_u64'"${lmul}"'/g
                s/_vx_u64m8/_vx_u64'"${lmul}"'/g
                s/_v_x_u64m8/_v_x_u64'"${lmul}"'/g
            }' "$src_file"
        fi
        run_lmul_variant "reduce LMUL=${lmul}"
    done
    cp "$src_bak" "$src_file"
    echo ""

    trap - EXIT
    cleanup_lmul
}

# ── Mode: suite ───────────────────────────────────────────────────────────────
# Batch execution: iterate over all hardware-compatible variants × levels × ops.
# Reuses the same build/run/store logic as mul and kem modes.
SUITE_VARIANTS="ref_scalar karatsuba_zbc karatsuba_rvv"
SUITE_LEVELS="128 192 256"
SUITE_OPS="mul kem"

hw_build_target() {
    # Build a benchmark binary for a given target (mul or kem).
    # Arguments: $1=level, $2=variant, $3=make_mode (dev|publish), $4=target (mul|kem)
    local level="$1" variant="$2" make_mode="$3" target="$4"
    local dir="$BENCH_DIR"
    if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}/tests/bench"; fi
    run_cmd "cd $dir && make -f Makefile.hw LEVEL=$level POLYMUL=$variant MODE=$make_mode clean >/dev/null 2>&1; make -f Makefile.hw LEVEL=$level POLYMUL=$variant MODE=$make_mode $target 2>&1 | tail -2"
}

hw_run_target() {
    # Run a benchmark binary for a given target (mul or kem).
    # Arguments: $1=level, $2=variant, $3=use_nice (0|1), $4=target (mul|kem)
    local level="$1" variant="$2" use_nice="$3" target="$4"
    local dir="$BENCH_DIR"
    if [[ "$LOCAL_MODE" -eq 0 ]]; then dir="${REMOTE_DIR}/tests/bench"; fi
    local bin="bench_hw_${target}_hqc${level}_${variant}"
    local cmd="cd $dir && taskset -c $CORE ./$bin"
    if [[ "$use_nice" == "1" ]]; then
        cmd="cd $dir && sudo nice -n -20 taskset -c $CORE ./$bin"
    fi
    run_cmd "$cmd"
}

save_csv_generic() {
    # Write a structured CSV with metadata header for any op type (mul or kem).
    # Arguments:
    #   $1 - raw benchmark output (comment line + old header + data rows)
    #   $2 - security level (128, 192, 256)
    #   $3 - variant name (e.g. karatsuba_rvv)
    #   $4 - mode (dev | publish)
    #   $5 - op type (mul | kem)
    local output="$1" level="$2" variant="$3" mode_suffix="$4" op="$5"

    # Determine iteration counts from mode for the CSV header.
    local n_iter n_warmup
    if [[ "$mode_suffix" == "publish" ]]; then
        n_iter=10000
        n_warmup=100
    else
        n_iter=100
        n_warmup=10
    fi

    # Compute CoV for publish mode header (use first data row's cycles).
    local cov_arg=""
    if [[ "$mode_suffix" == "publish" ]]; then
        local cyc std
        cyc="$(echo "$output" | awk -F',' 'NR>2 && !/^#/ && !/^op,/ {print $4; exit}')"
        std="$(echo "$output" | awk -F',' 'NR>2 && !/^#/ && !/^op,/ {print $7; exit}')"
        if [[ -n "$cyc" && -n "$std" ]]; then
            cov_arg=$(awk "BEGIN {printf \"%.4f\", $std / $cyc}")
        fi
    fi

    # Build full CSV content: metadata header + column header + data rows.
    local csv_content
    csv_content="$(hw_csv_header "$variant" "$n_iter" "$n_warmup" "$mode_suffix" "$cov_arg")"

    # Append the canonical CSV column header (with level prepended).
    csv_content="${csv_content}"$'\n'"level,op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"

    # Append data rows: skip comment lines and the old header from raw output,
    # prepend the level to each data row.
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        [[ "$line" == \#* ]] && continue
        [[ "$line" == op,* ]] && continue
        csv_content="${csv_content}"$'\n'"${level},${line}"
    done <<< "$output"

    # Store via hw_store_result (handles per-variant subdir, symlink, archive).
    hw_store_result "$csv_content" "$variant" "$op" "$TIMESTAMP"
}

run_mode_suite() {
    local make_mode="dev" use_nice=0
    if [[ "$PUBLISH" -eq 1 ]]; then
        make_mode="publish"
        use_nice=1
    fi

    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║  Hardware Benchmark Suite                                   ║"
    printf "║  Mode: %-6s  Core: %-3s                                   ║\n" "$make_mode" "$CORE"
    echo "║  Variants: ref_scalar, karatsuba_zbc, karatsuba_rvv        ║"
    echo "║  Levels:   128, 192, 256                                   ║"
    echo "║  Ops:      mul, kem                                        ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    deploy
    set_governor

    # Track failures for the summary table (array of "variant/level/op:message").
    SUITE_FAILURES=()
    # Track successes with median cycles: "variant/level/op:median_cycles".
    SUITE_RESULTS=()

    for variant in $SUITE_VARIANTS; do
        echo "┌──────────────────────────────────────────────────────────────┐"
        printf "│  Variant: %-49s│\n" "$variant"
        echo "└──────────────────────────────────────────────────────────────┘"

        for op in $SUITE_OPS; do
            # Collect data rows across all levels for the consolidated CSV.
            local consolidated_data_rows=""

            for level in $SUITE_LEVELS; do
                echo "  ━━━ HQC-${level} / ${op} ━━━"

                echo "    [${variant}/${level}/${op}] Building..."

                # Wrap build step in error handling.
                local build_output
                if ! build_output="$(hw_build_target "$level" "$variant" "$make_mode" "$op" 2>&1)"; then
                    local errmsg="Build failed: ${build_output##*$'\n'}"
                    echo "    [${variant}/${level}/${op}] ✗ BUILD FAILED" >&2
                    echo "      Error: ${errmsg}" >&2
                    SUITE_FAILURES+=("${variant}/${level}/${op}:${errmsg}")
                    continue
                fi

                if [[ "$BUILD_ONLY" -eq 1 ]]; then
                    echo "    [build-only] Done."
                    SUITE_RESULTS+=("${variant}/${level}/${op}:—")
                    continue
                fi

                echo "    [${variant}/${level}/${op}] Running..."

                # Wrap run step in error handling.
                local output
                if ! output="$(hw_run_target "$level" "$variant" "$use_nice" "$op" 2>&1)"; then
                    local errmsg="Execution failed: ${output##*$'\n'}"
                    echo "    [${variant}/${level}/${op}] ✗ RUN FAILED" >&2
                    echo "      Error: ${errmsg}" >&2
                    SUITE_FAILURES+=("${variant}/${level}/${op}:${errmsg}")
                    continue
                fi

                # Store the per-level result CSV (wrap in error handling).
                if ! save_csv_generic "$output" "$level" "$variant" "$make_mode" "$op"; then
                    local errmsg="CSV storage failed for per-level result"
                    echo "    [${variant}/${level}/${op}] ✗ STORE FAILED" >&2
                    echo "      Error: ${errmsg}" >&2
                    SUITE_FAILURES+=("${variant}/${level}/${op}:${errmsg}")
                    continue
                fi
                echo "    [${variant}/${level}/${op}] ✓ Stored per-level CSV."

                # Extract median cycles for the summary.
                local median_cyc
                median_cyc="$(echo "$output" | awk -F',' 'NR>2 && !/^#/ && !/^op,/ {print $4; exit}')"
                SUITE_RESULTS+=("${variant}/${level}/${op}:${median_cyc:-—}")

                # Accumulate data rows for the consolidated multi-level CSV.
                # Extract data rows: skip comment lines and the old header, prepend level.
                while IFS= read -r line; do
                    [[ -z "$line" ]] && continue
                    [[ "$line" == \#* ]] && continue
                    [[ "$line" == op,* ]] && continue
                    if [[ -z "$consolidated_data_rows" ]]; then
                        consolidated_data_rows="${level},${line}"
                    else
                        consolidated_data_rows="${consolidated_data_rows}"$'\n'"${level},${line}"
                    fi
                done <<< "$output"
            done

            # After all levels for this variant+op: generate consolidated multi-level CSV.
            if [[ "$BUILD_ONLY" -eq 0 && -n "$consolidated_data_rows" ]]; then
                # Determine iteration counts for the CSV header.
                local n_iter n_warmup
                if [[ "$make_mode" == "publish" ]]; then
                    n_iter=10000
                    n_warmup=100
                else
                    n_iter=100
                    n_warmup=10
                fi

                # Compute CoV for publish mode (use first data row's cycles).
                local cov_arg=""
                if [[ "$make_mode" == "publish" ]]; then
                    local cyc std
                    cyc="$(echo "$consolidated_data_rows" | awk -F',' 'NR==1 {print $5}')"
                    std="$(echo "$consolidated_data_rows" | awk -F',' 'NR==1 {print $8}')"
                    if [[ -n "$cyc" && -n "$std" ]]; then
                        cov_arg=$(awk "BEGIN {printf \"%.4f\", $std / $cyc}")
                    fi
                fi

                # Build consolidated CSV: metadata header + column header + all-level data rows.
                local consolidated_csv
                consolidated_csv="$(hw_csv_header "$variant" "$n_iter" "$n_warmup" "$make_mode" "$cov_arg")"
                consolidated_csv="${consolidated_csv}"$'\n'"level,op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret"
                consolidated_csv="${consolidated_csv}"$'\n'"${consolidated_data_rows}"

                # Store consolidated CSV as hw_{op}_{variant_slug}_{timestamp}.csv
                if ! hw_store_result "$consolidated_csv" "$variant" "$op" "$TIMESTAMP"; then
                    echo "    [${variant}/${op}] ✗ Consolidated CSV storage failed" >&2
                fi
                echo "    [${variant}/${op}] ✓ Stored consolidated multi-level CSV."
            fi
            echo ""
        done
        echo ""
    done

    # ── Summary Table ────────────────────────────────────────────────────────
    echo "═══════════════════════════════════════════════════════════════"
    echo ""
    printf "| %-16s | %-5s | %-3s | %-6s | %-13s |\n" "Variant" "Level" "Op" "Status" "Median Cycles"
    printf "|%-18s|%-7s|%-5s|%-8s|%-15s|\n" "------------------" "-------" "-----" "--------" "---------------"

    # Build a lookup of failed combinations for quick status check.
    declare -A _suite_failed_map
    for failure in "${SUITE_FAILURES[@]}"; do
        local combo="${failure%%:*}"
        _suite_failed_map["$combo"]=1
    done

    # Print one row per variant/level/op combination (in suite order).
    for variant in $SUITE_VARIANTS; do
        for level in $SUITE_LEVELS; do
            for op in $SUITE_OPS; do
                local combo="${variant}/${level}/${op}"
                local status="PASS"
                local median_display="—"

                if [[ -n "${_suite_failed_map[$combo]:-}" ]]; then
                    status="FAIL"
                    median_display="—"
                else
                    # Find the median cycles from SUITE_RESULTS.
                    for entry in "${SUITE_RESULTS[@]}"; do
                        if [[ "${entry%%:*}" == "$combo" ]]; then
                            local raw_cycles="${entry#*:}"
                            if [[ -n "$raw_cycles" && "$raw_cycles" != "—" ]]; then
                                # Format with thousand-separators.
                                median_display="$(printf "%'.0f" "${raw_cycles%.*}" 2>/dev/null || echo "$raw_cycles")"
                            fi
                            break
                        fi
                    done
                fi

                printf "| %-16s | %-5s | %-3s | %-6s | %-13s |\n" \
                    "$variant" "$level" "$op" "$status" "$median_display"
            done
        done
    done
    echo ""

    # ── Final Status ──────────────────────────────────────────────────────────
    local total_failures=${#SUITE_FAILURES[@]}
    echo "═══════════════════════════════════════════════════════════════"
    if [[ "$total_failures" -gt 0 ]]; then
        echo "  Suite complete with ${total_failures} failure(s)."
        echo ""
        echo "  Failed combinations:"
        for failure in "${SUITE_FAILURES[@]}"; do
            local combo="${failure%%:*}"
            local msg="${failure#*:}"
            echo "    ✗ ${combo} — ${msg}"
        done
    else
        echo "  Suite complete."
    fi
    echo "═══════════════════════════════════════════════════════════════"

    # Retrieve results from the remote board after the full suite completes.
    retrieve_results
}

# ── SSH connection validation ─────────────────────────────────────────────────
# When running in remote mode, validate SSH connectivity before any deployment
# or execution to provide a clear, early error message.
if [[ "$LOCAL_MODE" -eq 0 && -n "$REMOTE_HOST" ]]; then
    if ! ssh -o ConnectTimeout=5 -o BatchMode=yes "$REMOTE_HOST" true 2>/dev/null; then
        echo "ERROR: Cannot connect to ${REMOTE_HOST} via SSH. Check that the host is reachable and SSH keys are configured." >&2
        exit 1
    fi
fi

# ── Dispatch ─────────────────────────────────────────────────────────────────
case "$MODE" in
    mul|kem|publish)  run_mode_mul ;;
    suite)            run_mode_suite ;;
    sweep-threshold)  run_mode_sweep_threshold ;;
    sweep-lmul)       run_mode_sweep_lmul ;;
    *)
        echo "ERROR: Unknown mode '$MODE'" >&2
        usage >&2
        exit 2
        ;;
esac
