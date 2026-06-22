#!/usr/bin/env bash
# scripts/bench/run_compiler_compare.sh
# Re-run compiler comparison (Clang vs GCC, -O2/-O3/-Os) for one variant.
# Writes to results/simulation/benchmarks/compiler_compare/sim_compiler_compare_<slug>_latest.csv
#
# Usage:
#   run_compiler_compare.sh --variant vector_zvk [--level 128|192|256]
#
# GCC is automatically SKIP-guarded for vector_zvk (Zvbb incompatible).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/lib_bench.sh"

# ── Toolchain / environment ───────────────────────────────────────────────────
RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
SPIKE="${RISCV}/bin/spike"
ISA="rv64gcv_zbc_zbb_zba_zbs_zvbc_zvbb_zvl256b"
SPIKE_TIMEOUT_SEC="${SPIKE_TIMEOUT_SEC:-120}"

if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found at $SPIKE" >&2
    echo "       Source toolchain/toolchain.env or set RISCV" >&2
    exit 2
fi

GIT_HASH="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
BENCH_TIME="$(date +'%Y-%m-%d %H:%M')"

# ── Argument parsing ─────────────────────────────────────────────────────────
MUL_VARIANT=""
LEVEL=""

usage() {
    cat <<'EOF'
Usage:
  run_compiler_compare.sh --variant <variant> [--level 128|192|256]

Variants: scalar_ref, scalar_zb, vector_ref, vector_rvv, vector_zvk
EOF
}

bench_parse_track_args "$@"   # sets MUL_VARIANT KECCAK_IMPL DECODE_IMPL

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant|--mul) shift 2 ;;
        --keccak|--decode) shift 2 ;;
        --level)
            LEVEL="$2"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) shift ;;
    esac
done

if [[ -z "$MUL_VARIANT" ]]; then
    echo "ERROR: --variant is required" >&2
    usage; exit 2
fi

LEVELS=(128 192 256)
if [[ -n "$LEVEL" ]]; then
    if ! [[ "$LEVEL" =~ ^(128|192|256)$ ]]; then
        echo "ERROR: --level must be 128, 192, or 256" >&2; exit 2
    fi
    LEVELS=("$LEVEL")
fi

# ── Output file ───────────────────────────────────────────────────────────────
OUT_DIR="${REPO_ROOT}/results/simulation/benchmarks/compiler_compare"
mkdir -p "$OUT_DIR"
VARIANT_SLUG="$(bench_variant_slug "$MUL_VARIANT")"
STAMP="$(date +'%Y-%m-%d_%H-%M')"
CSV_OUT="${OUT_DIR}/sim_compiler_compare_${VARIANT_SLUG}_${STAMP}.csv"
CSV_LATEST="${OUT_DIR}/sim_compiler_compare_${VARIANT_SLUG}_latest.csv"

{
    echo "# date: ${BENCH_TIME}"
    echo "# commit: ${GIT_HASH}"
    echo "compiler,opt,level,variant,status,instructions,text_bytes,elf,error"
} > "$CSV_OUT"

# Canonical variant name embedded in CSV rows (matches old naming for compatibility)
# e.g. vector_zvk → karatsuba_zvbc  (what the old compare_compilers.sh used)
variant_csv_name() {
    case "$1" in
        scalar_ref) echo "ref_scalar" ;;
        scalar_zb)  echo "karatsuba_zbc" ;;
        vector_ref) echo "ref_vector" ;;
        vector_rvv) echo "karatsuba_rvv" ;;
        vector_zvk) echo "karatsuba_zvbc" ;;
        *)          echo "$1" ;;
    esac
}
VARIANT_CSV_NAME="$(variant_csv_name "$MUL_VARIANT")"

# ── Single build+run ──────────────────────────────────────────────────────────
run_one() {
    local compiler="$1" opt="$2" level="$3"
    local toolchain elf target run_out run_err instret text_bytes

    case "$compiler" in
        clang) toolchain="toolchain/riscv-clang.cmake" ;;
        gcc)   toolchain="toolchain/riscv-gcc.cmake" ;;
        *) echo "ERROR: unknown compiler $compiler" >&2; return 1 ;;
    esac

    # GCC is incompatible with the Zvbb x2 SoA Keccak backend.
    # Emit a SKIP row and proceed.
    if [[ "$compiler" == "gcc" && "$KECCAK_IMPL" == "zvbb_x2_soa" ]]; then
        local build_placeholder="${REPO_ROOT}/build/spike/bench_k${KECCAK_IMPL}_d${DECODE_IMPL}_${compiler}_${opt}"
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},SKIP,,,${build_placeholder}/bench_hqc${level}_${VARIANT_CSV_NAME}.elf,gcc_zvbb_incompatible" >> "$CSV_OUT"
        echo "SKIP (gcc_zvbb_incompatible)"
        return 0
    fi

    local build_dir="${REPO_ROOT}/build/spike/bench_k${KECCAK_IMPL}_d${DECODE_IMPL}_${compiler}_${opt}"
    mkdir -p "$build_dir"

    # Configure (always fresh for opt-level isolation)
    if ! cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/${toolchain}" \
        -DRISCV="${RISCV}" \
        -DRISCV_OPT_LEVEL="-${opt}" \
        -DHQC_SIM=spike \
        -DHQC_KECCAK_IMPL="${KECCAK_IMPL}" \
        -DHQC_DECODE_IMPL="${DECODE_IMPL}" \
        -DBENCH_N_ITER=1 \
        >/dev/null 2>&1; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${build_dir}/bench_hqc${level}_${VARIANT_CSV_NAME}.elf,cmake_configure_failed" >> "$CSV_OUT"
        echo "FAIL (cmake configure)"
        return 1
    fi

    target="bench_hqc${level}_${MUL_VARIANT}"
    if ! cmake --build "$build_dir" --target "$target" -j"$(nproc)" >/dev/null 2>&1; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${build_dir}/${target}.elf,build_failed" >> "$CSV_OUT"
        echo "FAIL (build)"
        return 1
    fi

    elf="${build_dir}/${target}.elf"
    if [[ ! -f "$elf" ]]; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${elf},elf_missing" >> "$CSV_OUT"
        echo "FAIL (elf missing)"
        return 1
    fi

    run_out="$(mktemp)"
    run_err="$(mktemp)"

    local rc=0
    if command -v timeout >/dev/null 2>&1; then
        timeout "${SPIKE_TIMEOUT_SEC}s" "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2>"$run_err" || rc=$?
    else
        "$SPIKE" --isa="$ISA" "$elf" > "$run_out" 2>"$run_err" || rc=$?
    fi

    if [[ "$rc" -eq 124 ]]; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${elf},spike_run_timeout" >> "$CSV_OUT"
        rm -f "$run_out" "$run_err"
        echo "FAIL (spike timeout)"
        return 1
    elif [[ "$rc" -ne 0 ]]; then
        echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},FAIL,,,${elf},spike_run_failed" >> "$CSV_OUT"
        rm -f "$run_out" "$run_err"
        echo "FAIL (spike error rc=$rc)"
        return 1
    fi

    # Sum keypair + encaps + decaps instret (column 7 in Spike CSV output)
    instret="$(awk -F',' '($1=="keypair" || $1=="encaps" || $1=="decaps") {sum += $7} END {print sum + 0}' "$run_out")"

    local size_tool="${RISCV}/bin/llvm-size"
    if [[ ! -x "$size_tool" ]]; then
        size_tool="${RISCV}/bin/riscv64-unknown-elf-size"
    fi
    text_bytes="$("$size_tool" "$elf" 2>/dev/null | awk 'NR==2{print $1}')"
    text_bytes="${text_bytes:-0}"

    echo "${compiler},-${opt},${level},${VARIANT_CSV_NAME},OK,${instret},${text_bytes},${elf}," >> "$CSV_OUT"
    rm -f "$run_out" "$run_err"
    printf "%s instructions" "$instret"
}

# ── Main loop ─────────────────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════╗"
printf "║  %-56s  ║\n" "run_compiler_compare.sh"
printf "║  %-56s  ║\n" "Variant: ${MUL_VARIANT}  Keccak: ${KECCAK_IMPL}  Decode: ${DECODE_IMPL}"
printf "║  %-56s  ║\n" "Commit:  ${GIT_HASH}  Date: ${BENCH_TIME}"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

for level in "${LEVELS[@]}"; do
    echo "── Level HQC-${level} ──────────────────────────────────────────────"
    for compiler in clang gcc; do
        for opt in O2 O3 Os; do
            printf "   %-6s %-3s  ⋯  " "$compiler" "-$opt"
            run_one "$compiler" "$opt" "$level" || true
            echo ""
        done
    done
done

# Atomically update the _latest symlink/copy
bench_refresh_tracked_csv "$CSV_OUT" "$CSV_LATEST" || true

echo ""
echo "✔  Written: ${CSV_LATEST}"
echo ""

# Regenerate the compiler analysis summary
echo "Regenerating compiler_analysis.md ..."
python3 "${REPO_ROOT}/scripts/report/generate_compiler_compare.py"
