#!/usr/bin/env bash
# Compare Clang vs GCC across -O2/-O3/-Os for one variant.
# Outputs CSV summary under analysis/benchmarks/compiler_compare/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"
bench_load_implemented_variants "$REPO_ROOT"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

# Populate MUL_VARIANT, KECCAK_IMPL, DECODE_IMPL via shared auto-mapping.
bench_parse_track_args "$@"

LEVEL=""

RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm"
SPIKE_TIMEOUT_SEC="${SPIKE_TIMEOUT_SEC:-30}"

if [[ ! -x "$SPIKE" ]]; then
    echo "[compare_compilers] ERROR: Spike not found at $SPIKE" >&2
    exit 2
fi

usage() {
    cat <<'EOF'
Usage:
    compare_compilers.sh --mul <variant|all> [--level 128|192|256]
EOF
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

run_one() {
    local compiler="$1"
    local opt="$2"
    local level="$3"
    local variant="$4"
    local csv_out="$5"
    local toolchain
    local build_dir
    local target
    local elf
    local run_out
    local run_err
    local instret
    local size_tool
    local text_bytes

    case "$compiler" in
        clang) toolchain="toolchain/riscv-clang.cmake" ;;
        gcc) toolchain="toolchain/riscv-gcc.cmake" ;;
        *)
            echo "[compare_compilers] internal error: unknown compiler '$compiler'" >&2
            return 1
            ;;
    esac

    build_dir="$REPO_ROOT/build/compare_${compiler}_${opt}_hqc${level}_${variant}"
    rm -rf "$build_dir"
    mkdir -p "$build_dir"

    if ! cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/$toolchain" \
        -DRISCV="$RISCV" \
        -DRISCV_OPT_LEVEL="-${opt}" \
        -DHQC_SIM=spike \
        -DHQC_KECCAK_IMPL="${KECCAK_IMPL}" \
        -DHQC_DECODE_IMPL="${DECODE_IMPL}" >/dev/null 2>&1; then
        echo "${compiler},-${opt},${level},${variant},FAIL,,,,cmake_configure_failed" >> "$csv_out"
        echo "FAIL"
        return 1
    fi

    target="bench_hqc${level}_${variant}"
    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        echo "${compiler},-${opt},${level},${variant},FAIL,,,,build_failed" >> "$csv_out"
        echo "FAIL"
        return 1
    fi

    elf="$build_dir/${target}.elf"
    run_out="$build_dir/${target}.stdout"
    run_err="$build_dir/${target}.stderr"

    if command -v timeout >/dev/null 2>&1; then
        if ! timeout "${SPIKE_TIMEOUT_SEC}s" "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2> "$run_err"; then
            local rc="$?"
            local err_tag="spike_run_failed"
            if [[ "$rc" -eq 124 ]]; then
                err_tag="spike_run_timeout"
            fi
            echo "${compiler},-${opt},${level},${variant},FAIL,,,${elf},${err_tag}" >> "$csv_out"
            echo "FAIL"
            return 1
        fi
    else
        if ! "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2> "$run_err"; then
            echo "${compiler},-${opt},${level},${variant},FAIL,,,${elf},spike_run_failed" >> "$csv_out"
            echo "FAIL"
            return 1
        fi
    fi
    instret="$(awk -F',' '($1=="keypair" || $1=="encaps" || $1=="decaps") {sum += $7} END {print sum + 0}' "$run_out")"

    size_tool="$RISCV/bin/llvm-size"
    if [[ ! -x "$size_tool" ]]; then
        size_tool="$RISCV/bin/riscv64-unknown-elf-size"
    fi
    text_bytes="$($size_tool "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
    text_bytes="${text_bytes:-0}"

    local fmt_ins=$(echo "$instret" | sed -e :a -e 's/\(.*[0-9]\)\([0-9]\{3\}\)/\1.\2/;ta')
    echo "${fmt_ins} instructions"
    echo "${compiler},-${opt},${level},${variant},OK,${instret},${text_bytes},${elf}," >> "$csv_out"
}

run_for_variant() {
    local variant="$1"
    local levels=(128 192 256)
    if [[ -n "$LEVEL" ]]; then
        levels=("$LEVEL")
    fi

    local stamp out_dir track csv_out
    stamp="$(date +'%Y-%m-%d_%H-%M')"
    track="scalar"
    if [[ "$variant" == *"zvbc"* ]]; then track="vector"; fi
    out_dir="$REPO_ROOT/analysis/benchmarks/compiler_compare/${track}/${variant}"
    mkdir -p "$out_dir"
    csv_out="${out_dir}/${stamp}.csv"

    echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$csv_out"
    echo "# commit: ${GIT_HASH}" >> "$csv_out"
    echo "compiler,opt,level,variant,status,instructions,text_bytes,elf,error" >> "$csv_out"

    echo -e "\n📦 ${variant} (compiler_compare)"
    local last_level_index=$(( ${#levels[@]} - 1 ))
    local last_level="${levels[$last_level_index]}"
    for level in "${levels[@]}"; do
        local level_branch="├──"
        if [[ "$level" == "$last_level" ]]; then
            level_branch="└──"
        fi
        echo " ${level_branch} L${level}"
        for compiler in clang gcc; do
            echo " │   ├── ${compiler}"
            for opt in O2 O3 Os; do
                local opt_branch="├──"
                if [[ "$opt" == "Os" ]]; then opt_branch="└──"; fi
                echo -n " │   │   ${opt_branch} -${opt} ⋯ "
                run_one "$compiler" "$opt" "$level" "$variant" "$csv_out" || true
            done
        done
    done

    bench_refresh_tracked_csv "$csv_out" "${out_dir}/latest.csv" || true
    echo ""
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant|--mul)
            MUL_VARIANT="$2"
            shift 2
            ;;
        --keccak|--decode)
            shift 2
            ;;
        --level)
            LEVEL="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[compare_compilers] ERROR: unknown option '$1'" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$MUL_VARIANT" ]]; then
    echo "[compare_compilers] ERROR: --mul is required" >&2
    usage >&2
    exit 2
fi

if [[ -n "$LEVEL" ]] && ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
    echo "[compare_compilers] ERROR: level must be 128, 192, or 256" >&2
    exit 2
fi

if [[ "$MUL_VARIANT" == "all" ]]; then
    for v in "${IMPLEMENTED_VARIANTS[@]}"; do
        run_for_variant "$v"
    done
else
    if ! grep -qx "$MUL_VARIANT" "$REPO_ROOT/scripts/implemented_variants.txt"; then
        echo "[compare_compilers] ERROR: variant '$MUL_VARIANT' not in scripts/implemented_variants.txt" >&2
        exit 2
    fi
    run_for_variant "$MUL_VARIANT"
fi
