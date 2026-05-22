#!/usr/bin/env bash
# scripts/bench/run_benchmarks.sh
# Unified Spike benchmark runner.
#
# Modes (--mode <mode>):
#   mul     : polynomial-multiply micro-benchmark (default)
#   kem     : full KEM (keypair / encaps / decaps) benchmark
#   keccak  : SHAKE-256 micro-benchmark
#   decode  : Reed-Muller + Reed-Solomon decode micro-benchmark
#   suite   : run all variants across all ops and write per-op CSVs
#
# Usage:
#   run_benchmarks.sh [--mode <mul|kem|keccak|decode>] [--mul <variant>]
#                     [--keccak <ref|opt>] [--decode <ref|opt>]
#   run_benchmarks.sh --mode suite [--run-id ID] [--levels "128 192 256"]
#                     [--mul <regex>] [--promote NAME]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

bench_load_implemented_variants "$REPO_ROOT"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
SPIKE_RUNNER="${REPO_ROOT}/sim/spike/run.sh"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm"
BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"

# Shared/default flags.
MODE="mul"

# Suite mode flags.
RUN_ID=""
RUN_ID_OVERRIDE=0
LEVELS="128 192 256"
PROMOTE_FLAG=0
PROMOTE_NAME=""

usage() {
    cat <<'EOF'
Usage:
    run_benchmarks.sh [--mode <mul|kem|keccak|decode>] [--mul <variant>]
                      [--keccak <ref|opt>] [--decode <ref|opt>]
    run_benchmarks.sh --mode suite [--run-id ID] [--levels "128 192 256"]
                      [--mul <regex>] [--promote NAME]

Modes:
  mul     Polynomial-multiply micro-benchmark (default)
  kem     Full KEM (keypair/encaps/decaps) benchmark
  keccak  SHAKE-256 micro-benchmark
  decode  Reed-Muller + Reed-Solomon decode micro-benchmark
  suite   Run all ops for all variants and write per-op CSVs
EOF
}

# Parse args.
declare -a REBUILT_ARGS=()
TARGET_ARG=""
MUL_ARG_SET=0
KECCAK_ARG_SET=0
DECODE_ARG_SET=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)
            MODE="$2"
            shift 2
            if [[ $# -gt 0 && "$1" != --* ]]; then
                TARGET_ARG="$1"
                shift 1
            fi
            ;;
        --variant|--mul)
            REBUILT_ARGS+=("--mul" "$2")
            MUL_ARG_SET=1
            shift 2
            ;;
        --keccak|--decode)
            REBUILT_ARGS+=("$1" "$2")
            if [[ "$1" == "--keccak" ]]; then
                KECCAK_ARG_SET=1
            else
                DECODE_ARG_SET=1
            fi
            shift 2
            ;;
        --run-id)
            RUN_ID="$2"
            RUN_ID_OVERRIDE=1
            shift 2
            ;;
        --levels|--level)
            if [[ "$2" == "all" ]]; then
                LEVELS="128 192 256"
            else
                LEVELS="$2"
            fi
            shift 2
            ;;
        --promote)
            PROMOTE_FLAG=1
            PROMOTE_NAME=""
            if [[ $# -gt 1 && "$2" != --* ]]; then
                PROMOTE_NAME="$2"
                shift 2
            else
                shift 1
            fi
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
            echo "ERROR: Unexpected positional argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

# Map positional TARGET_ARG into the correct REBUILT_ARGS slot.
case "$MODE" in
    kem|mul|suite)
        if [[ -n "$TARGET_ARG" ]]; then
            REBUILT_ARGS+=("--mul" "$TARGET_ARG")
            MUL_ARG_SET=1
        fi
        if [[ "$MUL_ARG_SET" -eq 0 ]]; then
            REBUILT_ARGS+=("--mul" "karatsuba_zbc")
        fi
        ;;
    keccak)
        if [[ -n "$TARGET_ARG" ]]; then
            REBUILT_ARGS+=("--keccak" "$TARGET_ARG")
            KECCAK_ARG_SET=1
        fi
        if [[ "$KECCAK_ARG_SET" -eq 0 ]]; then
            REBUILT_ARGS+=("--keccak" "ref")
        fi
        if [[ "$MUL_ARG_SET" -eq 0 ]]; then
            REBUILT_ARGS+=("--mul" "karatsuba_zbc")
        fi
        ;;
    decode)
        if [[ -n "$TARGET_ARG" ]]; then
            REBUILT_ARGS+=("--decode" "$TARGET_ARG")
            DECODE_ARG_SET=1
        fi
        if [[ "$DECODE_ARG_SET" -eq 0 ]]; then
            REBUILT_ARGS+=("--decode" "ref")
        fi
        if [[ "$MUL_ARG_SET" -eq 0 ]]; then
            REBUILT_ARGS+=("--mul" "karatsuba_zbc")
        fi
        ;;
esac
bench_parse_track_args "${REBUILT_ARGS[@]}"
BUILD_DIR="${REPO_ROOT}/build/spike/bench_k${KECCAK_IMPL}_d${DECODE_IMPL}"

if [[ "$MODE" != "mul" && "$MODE" != "kem" && "$MODE" != "keccak" && "$MODE" != "decode" && "$MODE" != "suite" ]]; then
    echo "ERROR: --mode must be one of: mul, kem, keccak, decode, suite" >&2
    usage >&2
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
    awk -F',' -v op="$op" '$1==op {print $7; exit}' "$csv"
}

format_int() {
    local n="${1:-0}"
    if [[ ! "$n" =~ ^[0-9]+$ ]]; then
        echo "$n"
        return
    fi
    awk -v n="$n" 'BEGIN {
        out = ""
        while (n >= 1000) {
            out = sprintf(",%03d", n % 1000) out
            n = int(n / 1000)
        }
        printf "%d%s", n, out
    }'
}

format_int_dot() {
    local n="${1:-0}"
    if [[ ! "$n" =~ ^[0-9]+$ ]]; then
        echo "$n"
        return
    fi
    awk -v n="$n" 'BEGIN {
        out = ""
        while (n >= 1000) {
            out = sprintf(".%03d", n % 1000) out
            n = int(n / 1000)
        }
        printf "%d%s", n, out
    }'
}

# Parse raw Spike output into clean CSV format.
# Arguments: raw_log csv_out level codesize
#   raw_log: raw Spike output file
#   csv_out: output CSV file path
#   level: security level (128, 192, 256)
#   codesize: ELF .text bytes
parse_spike_to_csv() {
    local raw_log="$1" csv_out="$2" level="$3" codesize="$4"

    # Parse raw log and append clean rows
    awk -F',' -v level="$level" -v codesize="$codesize" '
        /^(vect_mul|keccak|decode|keypair|encaps|decaps),/ {
            op = $1
            instructions = $7
            printf "%s,%s,%s,%s\n", level, op, instructions, codesize >> csv_out
            if (op == "keypair" || op == "encaps" || op == "decaps") {
                total += instructions
                kem_rows++
            }
        }
        END {
            if (kem_rows > 0) {
                printf "%s,total,%s,%s\n", level, total, codesize >> csv_out
            }
        }
    ' csv_out="$csv_out" "$raw_log"
}

run_single_mode() {
    local variant="${MUL_VARIANT}"
    local variant_slug="${variant//_/-}"

    # Determine track type (scalar / vector) for mul/kem output dirs
    local track
    track="$(bench_variant_track "$variant")"

    local variant_label
    if [[ -z "$variant" ]]; then
        variant_label="all"
    else
        variant_label="$(bench_sanitize_label "${variant}")"
    fi

    # Determine output directory based on MODE
    local out_dir
    case "$MODE" in
        mul|kem)
            out_dir="${REPO_ROOT}/analysis/benchmarks/isolated/${track}/${variant_label}"
            ;;
        keccak)
            out_dir="${REPO_ROOT}/analysis/benchmarks/isolated/keccak"
            ;;
        decode)
            out_dir="${REPO_ROOT}/analysis/benchmarks/isolated/decode"
            ;;
    esac
    mkdir -p "$out_dir"

    # Single CSV file per run containing all levels
    local csv
    local csv_latest
    local variant_header
    variant_header="# Multiplication variant: ${variant}"
    if [[ "$MODE" == "keccak" ]]; then
        local impl_flag="ref"
        case "$KECCAK_IMPL" in
            opt|zvbb_* ) impl_flag="opt" ;;
        esac
        csv="${out_dir}/isolated_keccak_${impl_flag}_${BENCH_TIME}.csv"
        csv_latest="${out_dir}/isolated_keccak_${impl_flag}_latest.csv"
        variant_header="# Keccak variant: ${KECCAK_IMPL}"
    elif [[ "$MODE" == "decode" ]]; then
        local impl_flag="ref"
        if [[ "$DECODE_IMPL" == "rvv" ]]; then impl_flag="opt"; fi
        csv="${out_dir}/isolated_decode_${impl_flag}_${BENCH_TIME}.csv"
        csv_latest="${out_dir}/isolated_decode_${impl_flag}_latest.csv"
        variant_header="# Decode variant: ${DECODE_IMPL}"
    else
        csv="${out_dir}/isolated_${MODE}_${variant_slug}_${BENCH_TIME}.csv"
        csv_latest="${out_dir}/isolated_${MODE}_${variant_slug}_latest.csv"
    fi
    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv"
    echo "# commit: ${GIT_HASH}" >> "$csv"
    echo "$variant_header" >> "$csv"
    echo "level,op,instructions,codesize" >> "$csv"

    if [[ "$MODE" == "keccak" ]]; then
        echo -e "\n📦 keccak_${KECCAK_IMPL} (isolated)"
    elif [[ "$MODE" == "decode" ]]; then
        echo -e "\n📦 decode_${DECODE_IMPL} (isolated)"
    else
        echo -e "\n📦 ${variant} (${MODE})"
    fi

    # Loop through security levels
    for level in 128 192 256; do
        local elf target
        case "$MODE" in
            kem)
                elf="${BUILD_DIR}/bench_hqc${level}_${variant}.elf"
                target="bench_hqc${level}_${variant}"
                ;;
            mul)
                elf="${BUILD_DIR}/bench_polymul_hqc${level}_${variant}.elf"
                target="bench_polymul_hqc${level}_${variant}"
                ;;
            keccak)
                elf="${BUILD_DIR}/bench_keccak_hqc${level}.elf"
                target="bench_keccak_hqc${level}"
                ;;
            decode)
                elf="${BUILD_DIR}/bench_decode_hqc${level}.elf"
                target="bench_decode_hqc${level}"
                ;;
        esac

        bench_build_target_if_configured "$BUILD_DIR" "$target" "bench" || continue
        if ! bench_auto_configure_and_build "$BUILD_DIR" "$REPO_ROOT" "$target" "spike" "0" "bench"; then
            continue
        fi

        if [[ ! -f "$elf" ]]; then
            echo "[bench] SKIP  ${target}: ELF build failed" >&2
            continue
        fi

        # Capture timing
        local t0_ns t1_ns elapsed_s
        local tmp_log
        local size_tool text_bytes
        t0_ns=$(date +%s.%N)

        # Run Spike silently, capture raw output
        tmp_log=$(mktemp)
        "$SPIKE_RUNNER" --clean "$elf" > "$tmp_log" 2>/dev/null

        t1_ns=$(date +%s.%N)
        elapsed_s=$(awk -v t0="$t0_ns" -v t1="$t1_ns" 'BEGIN {
            printf "%.2f", t1 - t0
        }')

        size_tool="$RISCV/bin/llvm-size"
        if [[ ! -x "$size_tool" ]]; then
            size_tool="$RISCV/bin/riscv64-unknown-elf-size"
        fi
        text_bytes="$($size_tool "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
        text_bytes="${text_bytes:-0}"

        # Parse and append to master CSV
        parse_spike_to_csv "$tmp_log" "$csv" "$level" "$text_bytes"

        local ins_str=""
        if [[ "$MODE" == "kem" ]]; then
            local kp_raw enc_raw dec_raw total
            kp_raw="$(awk -F',' '/^keypair,/ {print $7; exit}' "$tmp_log")"
            enc_raw="$(awk -F',' '/^encaps,/ {print $7; exit}' "$tmp_log")"
            dec_raw="$(awk -F',' '/^decaps,/ {print $7; exit}' "$tmp_log")"

            kp_raw="${kp_raw:-0}"
            enc_raw="${enc_raw:-0}"
            dec_raw="${dec_raw:-0}"
            total=$((kp_raw + enc_raw + dec_raw))

            local kp enc dec total_fmt
            kp="$(format_int_dot "$kp_raw")"
            enc="$(format_int_dot "$enc_raw")"
            dec="$(format_int_dot "$dec_raw")"
            total_fmt="$(format_int_dot "$total")"

            ins_str="$(printf 'Kp: %11s | Enc: %11s | Dec: %11s | Total: %11s' "$kp" "$enc" "$dec" "$total_fmt")"
        else
            local vm_raw vm
            case "$MODE" in
                mul)    vm_raw="$(awk -F',' '/^vect_mul,/ {print $7; exit}' "$tmp_log")" ;;
                keccak) vm_raw="$(awk -F',' '/^keccak,/   {print $7; exit}' "$tmp_log")" ;;
                decode) vm_raw="$(awk -F',' '/^decode,/   {print $7; exit}' "$tmp_log")" ;;
            esac
            vm_raw="${vm_raw:-0}"
            vm="$(format_int_dot "$vm_raw")"
            ins_str="${vm} instructions"
        fi

        local prefix=" ├──"
        if [[ "$level" == "256" ]]; then prefix=" └──"; fi
        echo "${prefix} L${level} ⋯ ${ins_str}"
        rm -f "$tmp_log"
    done

    echo ""
    bench_refresh_tracked_csv "$csv" "$csv_latest" || true
}

run_suite_mode() {
    if [[ "$RUN_ID_OVERRIDE" -eq 0 ]]; then
        local variant_label
        variant_label="$(bench_sanitize_label "${MUL_VARIANT:-all}")"
        RUN_ID="${variant_label}_suite_${BENCH_TIME}"
    fi

    if [[ "$PROMOTE_FLAG" -eq 1 && -z "$PROMOTE_NAME" ]]; then
        PROMOTE_NAME="$RUN_ID"
    fi

    if ! [[ "$RUN_ID" =~ ^[A-Za-z0-9_-]+$ ]]; then
        echo "ERROR: --run-id must contain only [A-Za-z0-9_-]" >&2
        exit 2
    fi

    local run_root
    run_root="${REPO_ROOT}/analysis/benchmarks/suites/${RUN_ID}"
    mkdir -p "$run_root"

    local suite_variant_slug
    suite_variant_slug="$(bench_sanitize_label "${MUL_VARIANT:-all}")"
    suite_variant_slug="${suite_variant_slug//_/-}"

    local vect_mul_csv keypair_csv encaps_csv decaps_csv
    vect_mul_csv="${run_root}/suite_vect-mul_Lall_${suite_variant_slug}.csv"
    keypair_csv="${run_root}/suite_keypair_Lall_${suite_variant_slug}.csv"
    encaps_csv="${run_root}/suite_encaps_Lall_${suite_variant_slug}.csv"
    decaps_csv="${run_root}/suite_decaps_Lall_${suite_variant_slug}.csv"

    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$vect_mul_csv"
    echo "# commit: ${GIT_HASH}" >> "$vect_mul_csv"
    echo "# run_id: ${RUN_ID}" >> "$vect_mul_csv"
    echo "level,variant,op,instructions,codesize" >> "$vect_mul_csv"
    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$keypair_csv"
    echo "# commit: ${GIT_HASH}" >> "$keypair_csv"
    echo "# run_id: ${RUN_ID}" >> "$keypair_csv"
    echo "level,variant,op,instructions,codesize" >> "$keypair_csv"
    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$encaps_csv"
    echo "# commit: ${GIT_HASH}" >> "$encaps_csv"
    echo "# run_id: ${RUN_ID}" >> "$encaps_csv"
    echo "level,variant,op,instructions,codesize" >> "$encaps_csv"
    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$decaps_csv"
    echo "# commit: ${GIT_HASH}" >> "$decaps_csv"
    echo "# run_id: ${RUN_ID}" >> "$decaps_csv"
    echo "level,variant,op,instructions,codesize" >> "$decaps_csv"

    run_one_suite() {
        local level="$1" variant="$2" kind="$3" target="$4" result_csv="$5"

        local elf base raw_log details_dir details_csv
        elf="${BUILD_DIR}/${target}.elf"
        base="hqc${level}_${variant}_${kind}"
        raw_log="$(mktemp "${run_root}/.${base}.XXXXXX.log")"
        details_dir="${run_root}/details"
        mkdir -p "$details_dir"
        details_csv="${details_dir}/${base}.csv"

        if [[ ! -f "$elf" ]]; then
            echo "[suite] ERROR missing ELF: ${elf}" >&2
            rm -f "$raw_log"
            return 1
        fi

        local t0_ns t1_ns
        t0_ns=$(date +%s%N)
        "$SPIKE_RUNNER" --clean "$elf" > "$raw_log" 2>/dev/null
        t1_ns=$(date +%s%N)

        local size_tool text_bytes
        size_tool="$RISCV/bin/llvm-size"
        if [[ ! -x "$size_tool" ]]; then
            size_tool="$RISCV/bin/riscv64-unknown-elf-size"
        fi
        text_bytes="$($size_tool "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
        text_bytes="${text_bytes:-0}"

        # Use unified parser to generate pristine details CSV
        echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$details_csv"
        echo "# commit: ${GIT_HASH}" >> "$details_csv"
        echo "# variant: ${variant}" >> "$details_csv"
        echo "level,op,instructions,codesize" >> "$details_csv"
        parse_spike_to_csv "$raw_log" "$details_csv" "$level" "$text_bytes"
        rm -f "$raw_log"

        : > "$result_csv"
        local -a ins_arr=()
        if [[ "$kind" == "polymul" ]]; then
            local med_ins
            med_ins="$(awk -F',' '$2=="vect_mul" {print $3; exit}' "$details_csv")"
            echo "${level},${variant},vect_mul,${med_ins},${text_bytes}" >> "$result_csv"
            ins_arr+=("$(echo "$med_ins" | sed -e :a -e 's/\(.*[0-9]\)\([0-9]\{3\}\)/\1.\2/;ta')")
        else
            for op in keypair encaps decaps; do
                local med_ins
                med_ins="$(awk -F',' -v op="$op" '$2==op {print $3; exit}' "$details_csv")"
                echo "${level},${variant},${op},${med_ins},${text_bytes}" >> "$result_csv"
                ins_arr+=("$(echo "$med_ins" | sed -e :a -e 's/\(.*[0-9]\)\([0-9]\{3\}\)/\1.\2/;ta')")
            done
        fi

        local ins_str=""
        if [[ "$kind" == "kem" ]]; then
            ins_str="Kp: ${ins_arr[0]} | Enc: ${ins_arr[1]} | Dec: ${ins_arr[2]}"
        else
            ins_str="${ins_arr[0]} instructions"
        fi
        local prefix="  ├──"
        if [[ "$kind" == "kem" && "$level" == "256" ]]; then prefix="  └──"; fi
        echo "${prefix} ${kind} L${level} ⋯ ${ins_str}"
    }

    echo "[suite] run-id: ${RUN_ID}"
    echo "[suite] build dir: ${BUILD_DIR}"
    echo "[suite] output: ${run_root}"

    if ! command -v parallel >/dev/null 2>&1; then
        echo "ERROR: GNU parallel is required for --suite mode" >&2
        echo "       Install parallel and retry" >&2
        exit 2
    fi

    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
        -DHQC_SIM=spike >/dev/null 2>&1

    local level variant
    local -a suite_targets=()
    local suite_jobs_file job_results_dir
    local job_idx=0
    suite_jobs_file="$(mktemp "${run_root}/.suite_jobs.XXXXXX.tsv")"
    job_results_dir="${run_root}/job_results"
    mkdir -p "$job_results_dir"

    for variant in "${IMPLEMENTED_VARIANTS[@]}"; do
        bench_variant_matches "$variant" "$MUL_VARIANT" || continue
        for level in $LEVELS; do
            local poly_target kem_target result_csv
            poly_target="bench_polymul_hqc${level}_${variant}"
            kem_target="bench_hqc${level}_${variant}"
            suite_targets+=("$poly_target" "$kem_target")

            job_idx=$((job_idx + 1))
            result_csv="${job_results_dir}/$(printf '%04d' "$job_idx")_hqc${level}_${variant}_polymul.csv"
            printf '%s\t%s\t%s\t%s\t%s\n' "$level" "$variant" "polymul" "$poly_target" "$result_csv" >> "$suite_jobs_file"

            job_idx=$((job_idx + 1))
            result_csv="${job_results_dir}/$(printf '%04d' "$job_idx")_hqc${level}_${variant}_kem.csv"
            printf '%s\t%s\t%s\t%s\t%s\n' "$level" "$variant" "kem" "$kem_target" "$result_csv" >> "$suite_jobs_file"
        done
    done

    if [[ ! -s "$suite_jobs_file" ]]; then
        rm -f "$suite_jobs_file"
        echo "ERROR: No suite jobs matched current filters" >&2
        exit 1
    fi

    cmake --build "$BUILD_DIR" --target "${suite_targets[@]}" -j"$(nproc)" >/dev/null

    export BUILD_DIR SPIKE_RUNNER RISCV run_root GIT_HASH
    export -f parse_spike_to_csv run_one_suite
    parallel --jobs 4 --colsep '\t' \
        'run_one_suite {1} {2} {3} {4} {5}' :::: "$suite_jobs_file"
    rm -f "$suite_jobs_file"

    local result_file
    for result_file in "${job_results_dir}"/*.csv; do
        [[ -f "$result_file" ]] || continue
        while IFS=',' read -r level variant op instructions codesize; do
            case "$op" in
                vect_mul)
                    echo "${level},${variant},${op},${instructions},${codesize}" >> "$vect_mul_csv"
                    ;;
                keypair)
                    echo "${level},${variant},${op},${instructions},${codesize}" >> "$keypair_csv"
                    ;;
                encaps)
                    echo "${level},${variant},${op},${instructions},${codesize}" >> "$encaps_csv"
                    ;;
                decaps)
                    echo "${level},${variant},${op},${instructions},${codesize}" >> "$decaps_csv"
                    ;;
            esac
        done < "$result_file"
    done

    if [[ -n "$PROMOTE_NAME" ]]; then
        local canon_dir
        local promoted_count=0
        canon_dir="${REPO_ROOT}/analysis/benchmarks/canonical/${PROMOTE_NAME}"
        mkdir -p "$canon_dir"
        local csv_file dst_file
        for csv_file in "${run_root}"/*.csv; do
            dst_file="${canon_dir}/$(basename "$csv_file")"
            if bench_csv_payload_matches "$csv_file" "$dst_file"; then
                echo "[suite] skipped unchanged $(basename "$csv_file")"
            else
                cp -f "$csv_file" "$dst_file"
                promoted_count=$((promoted_count + 1))
                echo "[suite] promoted $(basename "$csv_file")"
            fi
        done
        echo "[suite] promotion complete: ${promoted_count} file(s) updated in ${canon_dir}"
    fi

    echo ""
    echo "✔️ Saved: ${run_root}"
}

case "$MODE" in
    mul|kem|keccak|decode)
        if [[ "${MUL_VARIANT:-}" == "all" ]]; then
            if ! command -v parallel >/dev/null 2>&1; then
                echo "ERROR: GNU parallel is required for --mul all" >&2
                echo "       Install parallel and retry" >&2
                exit 2
            fi

            declare -a cmds=()
            local_mode="$MODE"
            for v in "${IMPLEMENTED_VARIANTS[@]}"; do
                cmds+=("${SCRIPT_DIR}/run_benchmarks.sh --mode ${local_mode} --mul ${v}")
            done

            printf '%s\n' "${cmds[@]}" | parallel --jobs 4
        else
            if [[ -z "${MUL_VARIANT:-}" ]]; then MUL_VARIANT="karatsuba_zbc"; fi
            if [[ ! " ${IMPLEMENTED_VARIANTS[*]} " =~ " ${MUL_VARIANT} " ]]; then
                echo "ERROR: Variant '${MUL_VARIANT}' is not valid." >&2
                exit 1
            fi
            run_single_mode
        fi
        ;;
    suite)
        run_suite_mode
        ;;
esac
