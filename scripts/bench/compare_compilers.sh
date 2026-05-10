#!/usr/bin/env bash
# Compare Clang vs GCC across -O2/-O3/-Os for one variant.
# Outputs CSV + Markdown summary under analysis/benchmarks/compiler_compare/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

VARIANT="${1:-}"
if [[ -z "$VARIANT" ]]; then
    echo "Usage: $0 <variant> [level] [iter]" >&2
    exit 2
fi

LEVEL="${2:-128}"
ITER="${3:-1}"

if ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
    echo "[compare_compilers] ERROR: level must be 128, 192, or 256" >&2
    exit 2
fi
if ! [[ "$ITER" =~ ^[1-9][0-9]*$ ]]; then
    echo "[compare_compilers] ERROR: iter must be a positive integer" >&2
    exit 2
fi

if ! grep -qx "$VARIANT" "$REPO_ROOT/scripts/implemented_variants.txt"; then
    echo "[compare_compilers] ERROR: variant '$VARIANT' not in scripts/implemented_variants.txt" >&2
    exit 2
fi

RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"

if [[ ! -x "$SPIKE" ]]; then
    echo "[compare_compilers] ERROR: Spike not found at $SPIKE" >&2
    exit 2
fi

STAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="$REPO_ROOT/analysis/benchmarks/compiler_compare"
mkdir -p "$OUT_DIR"
CSV_OUT="$OUT_DIR/compare_hqc${LEVEL}_${VARIANT}_${STAMP}.csv"
MD_OUT="$OUT_DIR/compare_hqc${LEVEL}_${VARIANT}_${STAMP}.md"

echo "compiler,opt,level,variant,status,median_cycles,median_instret,text_bytes,elf,error" > "$CSV_OUT"

run_one() {
    local compiler="$1"
    local opt="$2"
    local toolchain
    local build_dir
    local toolchain_tmp
    local target
    local elf
    local run_out
    local run_err
    local row
    local cycles
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

    build_dir="$REPO_ROOT/build/compare_${compiler}_${opt}_hqc${LEVEL}_${VARIANT}"
    rm -rf "$build_dir"
    toolchain_tmp="$build_dir/toolchain_${compiler}_${opt}.cmake"
    mkdir -p "$build_dir"
    cp "$REPO_ROOT/$toolchain" "$toolchain_tmp"

    # Replace the default -O3 with requested optimization level in the temp toolchain.
    sed -i "s/\"-O3\"/\"-${opt}\"/g" "$toolchain_tmp"

    # GCC toolchain cannot accept several Clang-only flags in this repo snapshot.
    if [[ "$compiler" == "gcc" ]]; then
        sed -i 's/--target=${RISCV_TARGET_TRIPLE}//g' "$toolchain_tmp"
        sed -i 's/-menable-experimental-extensions//g' "$toolchain_tmp"
        sed -i 's/-fuse-ld=lld//g' "$toolchain_tmp"
    fi

    if ! cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain_tmp" \
        -DRISCV="$RISCV" \
        -DHQC_SIM=spike \
        -DBENCH_N_ITER="$ITER" >/dev/null 2>&1; then
        echo "${compiler},-${opt},${LEVEL},${VARIANT},FAIL,,,,,cmake_configure_failed" >> "$CSV_OUT"
        return 1
    fi

    target="bench_polymul_hqc${LEVEL}_${VARIANT}"
    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        echo "${compiler},-${opt},${LEVEL},${VARIANT},FAIL,,,,,build_failed" >> "$CSV_OUT"
        return 1
    fi

    elf="$build_dir/${target}.elf"
    run_out="$build_dir/${target}.stdout"
    run_err="$build_dir/${target}.stderr"

    if ! "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2> "$run_err"; then
        echo "${compiler},-${opt},${LEVEL},${VARIANT},FAIL,,,,${elf},spike_run_failed" >> "$CSV_OUT"
        return 1
    fi
    row="$(awk -F',' '$1=="vect_mul" {print $4 "," $7; exit}' "$run_out")"
    cycles="${row%%,*}"
    instret="${row##*,}"

    size_tool="$RISCV/bin/llvm-size"
    if [[ ! -x "$size_tool" ]]; then
        size_tool="$RISCV/bin/riscv64-unknown-elf-size"
    fi
    text_bytes="$($size_tool "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
    text_bytes="${text_bytes:-0}"

    echo "${compiler},-${opt},${LEVEL},${VARIANT},OK,${cycles},${instret},${text_bytes},${elf}," >> "$CSV_OUT"
}

for compiler in clang gcc; do
    for opt in O2 O3 Os; do
        echo "[compare_compilers] running ${compiler} -${opt} ..."
        run_one "$compiler" "$opt" || true
    done
done

    best_cycles_clang="$(awk -F',' '$1=="clang" && $5=="OK" {if(min=="" || $6<min) min=$6} END{print min}' "$CSV_OUT")"
    best_cycles_gcc="$(awk -F',' '$1=="gcc" && $5=="OK" {if(min=="" || $6<min) min=$6} END{print min}' "$CSV_OUT")"

{
    echo "# Compiler Comparison (HQC-${LEVEL} / ${VARIANT})"
    echo
    echo "| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |"
    echo "|---|---:|---|---:|---:|---:|---|"
    awk -F',' 'NR>1 {printf("| %s | %s | %s | %s | %s | %s | %s |\n", $1, $2, $5, $6, $7, $8, $10)}' "$CSV_OUT"
    echo
    echo "Best cycles (clang): ${best_cycles_clang}"
    echo
    echo "Best cycles (gcc): ${best_cycles_gcc}"
} > "$MD_OUT"

echo "[compare_compilers] CSV: $CSV_OUT"
echo "[compare_compilers] MD : $MD_OUT"
