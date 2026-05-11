#!/usr/bin/env bash
# scripts/bench/run_benchmarks.sh
# Unified Spike benchmark runner.
#
# Modes:
#   --mode async : background launch mode (formerly run_spike_bench_variant.sh)
#   --mode suite : synchronous suite mode (formerly run_spike_suite_sync.sh)
#
# Async usage:
#   run_benchmarks.sh --mode async <bench_set> [--variant <regex>] [--full] [--no-polymul] [--iter N]
#
# Suite usage:
#   run_benchmarks.sh --mode suite [--run-id ID] [--iter N] [--levels "128 192 256"]
#                     [--variant <regex>] [--promote NAME]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

bench_load_implemented_variants "$REPO_ROOT"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
SPIKE_RUNNER="${REPO_ROOT}/sim/spike/run.sh"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"
BUILD_DIR="${REPO_ROOT}/build/spike"
BENCH_TIME="$(date +'%Y%m%d_%H%M')"

# Shared/default flags.
MODE=""
ITER=1
VARIANT_FILTER=""

# Async mode flags.
BENCH_SET=""
DO_FULL=0
DO_POLYMUL=1

# Suite mode flags.
RUN_ID=""
RUN_ID_OVERRIDE=0
LEVELS="128 192 256"
PROMOTE_NAME=""

# Benchmark set mappings (async mode).
declare -A BENCH_SET_VARIANT_MAP
BENCH_SET_VARIANT_MAP=(
    [baseline_ref]="karatsuba_ref"
    [polymul_zbc]="karatsuba_zbc"
    [fafft_zbc]="fafft_zbc"
    [hybrid]="hybrid"
    [hybrid_zbc]="hybrid"
    [hybrid_zbc_v2]="hybrid"
    [hybrid_2way_zbc]="hybrid"
    [polymul_zvbc]="karatsuba_zvbc"
    [polymul_zvbc_v2]="fafft_zvbc"
)

declare -A BENCH_SET_TRACK_MAP
BENCH_SET_TRACK_MAP=(
    [baseline_ref]="scalar"
    [polymul_zbc]="scalar"
    [fafft_zbc]="scalar"
    [hybrid]="scalar"
    [hybrid_zbc]="scalar"
    [hybrid_zbc_v2]="scalar"
    [hybrid_2way_zbc]="scalar"
    [polymul_zvbc]="vector"
    [polymul_zvbc_v2]="vector"
    [fafft_zvbc]="vector"
)

usage() {
    cat <<'EOF'
Usage:
  run_benchmarks.sh --mode async <bench_set> [--variant <regex>] [--full] [--no-polymul] [--iter N]
  run_benchmarks.sh --mode suite [--run-id ID] [--iter N] [--levels "128 192 256"]
                    [--variant <regex>] [--promote NAME]

Modes:
  async  Launch Spike jobs in background and print PIDs.
  suite  Run polymul + KEM benchmarks synchronously and write summary.csv.
EOF
}

# Parse args.
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)
            MODE="$2"
            shift 2
            ;;
        --variant)
            VARIANT_FILTER="$2"
            shift 2
            ;;
        --iter)
            ITER="$2"
            shift 2
            ;;
        --full)
            DO_FULL=1
            shift
            ;;
        --no-polymul)
            DO_POLYMUL=0
            shift
            ;;
        --run-id)
            RUN_ID="$2"
            RUN_ID_OVERRIDE=1
            shift 2
            ;;
        --levels)
            LEVELS="$2"
            shift 2
            ;;
        --promote)
            PROMOTE_NAME="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --*)
            echo "ERROR: Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            # Positional bench_set for async mode.
            if [[ -z "$BENCH_SET" ]]; then
                BENCH_SET="$1"
                shift
            else
                echo "ERROR: Unexpected positional argument: $1" >&2
                usage >&2
                exit 2
            fi
            ;;
    esac
done

if [[ "$MODE" != "async" && "$MODE" != "suite" ]]; then
    echo "ERROR: --mode must be 'async' or 'suite'" >&2
    usage >&2
    exit 2
fi

if ! [[ "$ITER" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: --iter must be a positive integer" >&2
    exit 2
fi

if ! bench_require_executable "$SPIKE" "Spike not found"; then
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

if ! bench_require_executable "$SPIKE_RUNNER" "Spike runner not found"; then
    exit 2
fi

extract_median_row() {
    local csv="$1" op="$2"
    awk -F',' -v op="$op" '$1==op {print $4 "," $7; exit}' "$csv"
}

run_async_mode() {
    if [[ -z "$BENCH_SET" ]]; then
        echo "ERROR: async mode requires <bench_set>" >&2
        usage >&2
        exit 2
    fi

    if [[ $DO_POLYMUL -eq 0 && $DO_FULL -eq 0 ]]; then
        echo "ERROR: --no-polymul requires --full (nothing to do otherwise)" >&2
        exit 2
    fi

    if [[ -z "$VARIANT_FILTER" ]]; then
        if [[ -v BENCH_SET_VARIANT_MAP["$BENCH_SET"] ]]; then
            VARIANT_FILTER="${BENCH_SET_VARIANT_MAP[$BENCH_SET]}"
            echo "[bench] Benchmark set '${BENCH_SET}' -> canonical variant '${VARIANT_FILTER}'"
        else
            echo "WARNING: Unknown benchmark set '${BENCH_SET}' -- no canonical variant; running all implemented variants." >&2
            echo "         Known benchmark sets: ${!BENCH_SET_VARIANT_MAP[*]}" >&2
        fi
    else
        echo "[bench] Benchmark set '${BENCH_SET}' -> variant override '${VARIANT_FILTER}'"
    fi

    local variant_label run_label bench_track out_dir
    variant_label="$(bench_sanitize_label "${VARIANT_FILTER:-all}")"
    run_label="${variant_label}_${BENCH_SET}_${BENCH_TIME}"

    bench_track="${BENCH_SET_TRACK_MAP[$BENCH_SET]:-}"
    if [[ -n "$bench_track" ]]; then
        out_dir="${REPO_ROOT}/analysis/benchmarks/${bench_track}/${BENCH_SET}/${run_label}"
    else
        out_dir="${REPO_ROOT}/analysis/benchmarks/${BENCH_SET}/${run_label}"
    fi
    mkdir -p "$out_dir"

    local launched=0
    local -a job_lines=()

    for level in 128 192 256; do
        for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
            bench_variant_matches "$variant" "$VARIANT_FILTER" || continue

            if [[ $DO_POLYMUL -eq 1 ]]; then
                local elf target prefix csv label pid
                elf="${BUILD_DIR}/bench_polymul_hqc${level}_${variant}.elf"
                target="bench_polymul_hqc${level}_${variant}"
                prefix="hqc${level}_${variant}"
                csv="${out_dir}/${prefix}_polymul_N${ITER}.csv"

                bench_build_target_if_configured "$BUILD_DIR" "$target" "bench" || continue
                if ! bench_require_elf_or_hint "$elf" "$target" \
                    "cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}" \
                    "bench"; then
                    continue
                fi

                label="polymul_hqc${level}_${variant}"
                nohup "$SPIKE_RUNNER" --clean "$elf" > "$csv" 2>/dev/null &
                pid=$!
                job_lines+=("  PID ${pid}  ${label}  ->  $(basename "$csv")")
                launched=$((launched + 1))
            fi

            if [[ $DO_FULL -eq 1 ]]; then
                local elf target prefix csv label pid
                elf="${BUILD_DIR}/bench_hqc${level}_${variant}.elf"
                target="bench_hqc${level}_${variant}"
                prefix="hqc${level}_${variant}"
                csv="${out_dir}/${prefix}_N${ITER}.csv"

                bench_build_target_if_configured "$BUILD_DIR" "$target" "bench" || continue
                if ! bench_require_elf_or_hint "$elf" "$target" \
                    "cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}" \
                    "bench"; then
                    continue
                fi

                label="kem_hqc${level}_${variant}"
                nohup "$SPIKE_RUNNER" --clean "$elf" > "$csv" 2>/dev/null &
                pid=$!
                job_lines+=("  PID ${pid}  ${label}  ->  $(basename "$csv")")
                launched=$((launched + 1))
            fi
        done
    done

    echo ""
    if [[ $launched -eq 0 ]]; then
        echo "[bench] No jobs launched. Check that ELFs are built in ${BUILD_DIR}/"
        echo "        cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=${ITER}"
        echo "        cmake --build build/spike -j$(nproc)"
        return
    fi

    echo "[bench] Launched ${launched} Spike job(s) for ${BENCH_SET}:"
    printf "%s\n" "${job_lines[@]}"
    echo ""
    echo "Monitor:"
    echo "  watch -n 30 \"wc -c ${out_dir}/*.csv\""
    echo "  ps aux | grep '[s]pike.*bench'"
    echo ""
    echo "When done, extract results:"
    echo "  python3 scripts/report/extract_results.py --bench-set ${BENCH_SET}"
}

run_suite_mode() {
    if [[ "$RUN_ID_OVERRIDE" -eq 0 ]]; then
        local variant_label
        variant_label="$(bench_sanitize_label "${VARIANT_FILTER:-all}")"
        RUN_ID="${variant_label}_suite_${BENCH_TIME}"
    fi

    if ! [[ "$RUN_ID" =~ ^[A-Za-z0-9_]+$ ]]; then
        echo "ERROR: --run-id must contain only [A-Za-z0-9_]" >&2
        exit 2
    fi

    local run_root raw_dir curated_dir summary_csv
    run_root="${REPO_ROOT}/analysis/benchmarks/runs/${RUN_ID}"
    raw_dir="${run_root}/raw"
    curated_dir="${run_root}/curated"
    mkdir -p "$raw_dir" "$curated_dir"

    summary_csv="${run_root}/summary.csv"
    echo "level,variant,kind,op,wall_seconds,median_cycles,median_instret,csv_path" > "$summary_csv"

    run_one_suite() {
        local level="$1" variant="$2" kind="$3" target="$4"

        local elf base raw_out raw_err curated_csv
        elf="${BUILD_DIR}/${target}.elf"
        base="hqc${level}_${variant}_${kind}_N${ITER}"
        raw_out="${raw_dir}/${base}.stdout"
        raw_err="${raw_dir}/${base}.stderr"
        curated_csv="${curated_dir}/${base}.csv"

        cmake --build "$BUILD_DIR" --target "$target" -j"$(nproc)" >/dev/null

        local elapsed_s t0_ns t1_ns elapsed_ms
        t0_ns=$(date +%s%N)
        "$SPIKE_RUNNER" --clean "$elf" > "$raw_out" 2> "$raw_err"
        t1_ns=$(date +%s%N)
        elapsed_ms=$(( (t1_ns - t0_ns) / 1000000 ))
        elapsed_s=$(awk -v ms="$elapsed_ms" 'BEGIN { printf "%.3f", ms/1000.0 }')

        awk 'BEGIN{keep=0}
             /^# level=/ {print; keep=1; next}
             /^op,iterations,/ {print; keep=1; next}
             keep && /^[a-z_]+,[0-9]+,/ {print}
        ' "$raw_out" > "$curated_csv"

        if [[ "$kind" == "polymul" ]]; then
            local row med_cyc med_ins
            row="$(extract_median_row "$curated_csv" "vect_mul")"
            med_cyc="${row%%,*}"
            med_ins="${row##*,}"
            echo "${level},${variant},${kind},vect_mul,${elapsed_s},${med_cyc},${med_ins},${curated_csv}" >> "$summary_csv"
        else
            for op in keypair encaps decaps; do
                local row med_cyc med_ins
                row="$(extract_median_row "$curated_csv" "$op")"
                med_cyc="${row%%,*}"
                med_ins="${row##*,}"
                echo "${level},${variant},${kind},${op},${elapsed_s},${med_cyc},${med_ins},${curated_csv}" >> "$summary_csv"
            done
        fi

        echo "[suite] ${kind} L${level} ${variant}: ${elapsed_s}s"
    }

    echo "[suite] run-id: ${RUN_ID}"
    echo "[suite] build dir: ${BUILD_DIR}"
    echo "[suite] output: ${run_root}"

    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DHQC_SIM=spike \
        -DBENCH_N_ITER="$ITER" >/dev/null

    local level variant
    for level in $LEVELS; do
        for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
            bench_variant_matches "$variant" "$VARIANT_FILTER" || continue
            run_one_suite "$level" "$variant" "polymul" "bench_polymul_hqc${level}_${variant}"
            run_one_suite "$level" "$variant" "kem" "bench_hqc${level}_${variant}"
        done
    done

    if [[ -n "$PROMOTE_NAME" ]]; then
        local canon_dir
        canon_dir="${REPO_ROOT}/analysis/benchmarks/canonical/${PROMOTE_NAME}"
        mkdir -p "$canon_dir"
        cp -f "${curated_dir}"/*.csv "$canon_dir"/
        cp -f "$summary_csv" "${canon_dir}/summary.csv"
        echo "[suite] promoted curated outputs to ${canon_dir}"
    fi

    echo "[suite] done: ${summary_csv}"
}

if [[ "$MODE" == "async" ]]; then
    run_async_mode
else
    run_suite_mode
fi
