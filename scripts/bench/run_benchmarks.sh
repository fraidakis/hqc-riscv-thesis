#!/usr/bin/env bash
# scripts/bench/run_benchmarks.sh
# Unified Spike benchmark runner.
#
# Modes (--mode <mode>):
#   mul     : polynomial-multiply micro-benchmark (default)
#   kem     : full KEM (keypair / encaps / decaps) benchmark
#   keccak  : SHAKE-256 micro-benchmark
#   decode  : Reed-Muller + Reed-Solomon decode micro-benchmark
#
# Usage:
#   run_benchmarks.sh [--mode <mul|kem|keccak|decode>] [--mul <variant>]
#                     [--keccak <ref|opt>] [--decode <ref|rvv|zvbc|opt>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

bench_load_implemented_variants "$REPO_ROOT"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

# Map old variant names to new directory names (extension-family tier naming).
variant_to_dir() {
    case "$1" in
        ref_scalar)      echo "scalar_ref" ;;
        karatsuba_zbc)   echo "scalar_Zb" ;;
        ref_vector)      echo "vector_ref" ;;
        karatsuba_rvv)   echo "vector_rvv" ;;
        karatsuba_zvbc)  echo "vector_Zvk" ;;
        *)               echo "$1" ;;  # fallback
    esac
}

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"
SPIKE_RUNNER="${REPO_ROOT}/sim/spike/run.sh"
ISA="rv64gcv_zbc_zbb_zba_zvbc_zvbb_zvl256b_zicntr_zihpm"
BENCH_EXPECT_HQC_SCALAR_MARCH="${BENCH_EXPECT_HQC_SCALAR_MARCH:-rv64gc_zbc_zbb}"
export BENCH_EXPECT_HQC_SCALAR_MARCH
BENCH_TIME="$(date +'%Y-%m-%d_%H-%M')"

# Shared/default flags.
MODE="mul"

usage() {
    cat <<'EOF'
Usage:
    run_benchmarks.sh [--mode <mul|kem|keccak|decode>] [--mul <variant>]
                      [--keccak <ref|opt>] [--decode <ref|rvv|zvbc|opt>]

Modes:
  mul     Polynomial-multiply micro-benchmark (default)
  kem     Full KEM (keypair/encaps/decaps) benchmark
  keccak  SHAKE-256 micro-benchmark
  decode  Reed-Muller + Reed-Solomon decode micro-benchmark
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
    kem|mul)
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

if [[ "$MODE" != "mul" && "$MODE" != "kem" && "$MODE" != "keccak" && "$MODE" != "decode" ]]; then
    echo "ERROR: --mode must be one of: mul, kem, keccak, decode" >&2
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

    # Map variant to new directory name
    local variant_dir
    variant_dir="$(variant_to_dir "$variant")"

    # Determine output directory based on MODE
    local out_dir
    case "$MODE" in
        mul|kem)
            out_dir="${REPO_ROOT}/results/simulation/benchmarks/components/poly_mul/${variant_dir}"
            ;;
        keccak)
            out_dir="${REPO_ROOT}/results/simulation/benchmarks/components/keccak"
            ;;
        decode)
            out_dir="${REPO_ROOT}/results/simulation/benchmarks/components/decode"
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
        if [[ "$DECODE_IMPL" == "rvv" ]]; then
            impl_flag="rvv"
        elif [[ "$DECODE_IMPL" == "zvbc" || "$DECODE_IMPL" == "rs_zvbc_exp" ]]; then
            impl_flag="zvbc"
        fi
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
esac
