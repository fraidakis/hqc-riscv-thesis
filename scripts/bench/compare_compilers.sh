#!/usr/bin/env bash
# Compare Clang vs GCC across -O2/-O3/-Os for one variant.
# Outputs CSV summary under analysis/benchmarks/compiler_compare/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"
GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

VARIANT="${1:-}"
if [[ -z "$VARIANT" ]]; then
    echo "Usage: $0 <variant> [level]" >&2
    exit 2
fi

LEVEL="${2:-128}"

if ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
    echo "[compare_compilers] ERROR: level must be 128, 192, or 256" >&2
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

STAMP="$(date +'%Y-%m-%d_%H-%M')"
track="scalar"
if [[ "$VARIANT" == *"zvbc"* ]]; then track="vector"; fi
OUT_DIR="$REPO_ROOT/analysis/benchmarks/compiler_compare/${track}/${VARIANT}"
mkdir -p "$OUT_DIR"
CSV_OUT="${OUT_DIR}/${STAMP}.csv"

echo "# date: $(date +'%Y-%m-%d %H:%M')" > "$CSV_OUT"
echo "# commit: ${GIT_HASH}" >> "$CSV_OUT"
echo "compiler,opt,level,variant,status,instructions,text_bytes,elf,error" >> "$CSV_OUT"

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
    local toolchain
    local build_dir
    local toolchain_tmp
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
        -DHQC_SIM=spike >/dev/null 2>&1; then
        echo "${compiler},-${opt},${LEVEL},${VARIANT},FAIL,,,,cmake_configure_failed" >> "$CSV_OUT"
        echo "FAIL"
        return 1
    fi

    target="bench_polymul_hqc${LEVEL}_${VARIANT}"
    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        echo "${compiler},-${opt},${LEVEL},${VARIANT},FAIL,,,,build_failed" >> "$CSV_OUT"
        echo "FAIL"
        return 1
    fi

    elf="$build_dir/${target}.elf"
    run_out="$build_dir/${target}.stdout"
    run_err="$build_dir/${target}.stderr"

    if ! "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2> "$run_err"; then
        echo "${compiler},-${opt},${LEVEL},${VARIANT},FAIL,,,${elf},spike_run_failed" >> "$CSV_OUT"
        echo "FAIL"
        return 1
    fi
    instret="$(awk -F',' '$1=="vect_mul" {print $7; exit}' "$run_out")"

    size_tool="$RISCV/bin/llvm-size"
    if [[ ! -x "$size_tool" ]]; then
        size_tool="$RISCV/bin/riscv64-unknown-elf-size"
    fi
    text_bytes="$($size_tool "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
    text_bytes="${text_bytes:-0}"

    local fmt_ins=$(echo "$instret" | sed -e :a -e 's/\(.*[0-9]\)\([0-9]\{3\}\)/\1.\2/;ta')
    echo "${fmt_ins} instructions"
    echo "${compiler},-${opt},${LEVEL},${VARIANT},OK,${instret},${text_bytes},${elf}," >> "$CSV_OUT"
}

echo -e "\n📦 ${VARIANT} (compiler_compare)"
for compiler in clang gcc; do
    if [[ "$compiler" == "clang" ]]; then
        echo " ├── ${compiler}"
    else
        echo " └── ${compiler}"
    fi
    for opt in O2 O3 Os; do
        local_branch="├──"
        if [[ "$opt" == "Os" ]]; then local_branch="└──"; fi
        echo -n " │   ${local_branch} -${opt} ⋯ "
        run_one "$compiler" "$opt" || true
    done
done

if bench_refresh_tracked_csv "$CSV_OUT" "${OUT_DIR}/latest.csv"; then
    echo "↻ Refreshed: ${OUT_DIR}/latest.csv"
else
    echo "↻ Unchanged payload: ${OUT_DIR}/latest.csv"
fi
echo ""
echo "✔️ Saved: $CSV_OUT"
