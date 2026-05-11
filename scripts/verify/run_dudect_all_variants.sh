#!/usr/bin/env bash
# Run dudect workflow with hardware-aware routing.
#
# Paths:
#   1) Reference variant(s): native x86 build + execute via check_ct.sh --dudect
#   2) zbc scalar intrinsics: cross-compile RISC-V dudect ELF, do not execute
#   3) zvbc vector intrinsics: skip (no silicon support; emulation invalid)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

LEVELS="${LEVELS:-128}"
REPORT_DIR="$REPO_ROOT/analysis/ct_verify/dudect"
STAMP="$(date +%Y%m%d_%H%M%S)"
SUMMARY="$REPORT_DIR/dudect_summary_${STAMP}.csv"
RISCV="${RISCV:-$HOME/Thesis/toolchains/installed-tools}"
RISCV_CC="${RISCV}/bin/riscv64-unknown-elf-gcc"
RISCV_OUT_DIR="$REPORT_DIR/riscv_binaries"

ANSI_BOLD_RED='\033[1;31m'
ANSI_RESET='\033[0m'

mkdir -p "$REPORT_DIR" "$RISCV_OUT_DIR"

echo "level,variant,path,status,artifact_or_reason" > "$SUMMARY"

variant_path() {
    local variant="$1"
    case "$variant" in
        karatsuba_ref)
            echo "reference"
            ;;
        karatsuba_zbc|fafft_zbc|hybrid)
            echo "zbc"
            ;;
        fafft_zvbc|karatsuba_zvbc)
            echo "zvbc"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

level_variant_name() {
    local level="$1"
    case "$level" in
        128) echo "hqc-1" ;;
        192) echo "hqc-3" ;;
        256) echo "hqc-5" ;;
        *) return 1 ;;
    esac
}

cross_compile_dudect_zbc() {
    local level="$1"
    local variant="$2"
    local hqc_variant
    hqc_variant="$(level_variant_name "$level")"

    local vendor="$REPO_ROOT/vendor/hqc-ref"
    local out_elf="$RISCV_OUT_DIR/dudect_hqc${level}_${variant}.elf"
    local src_list=()

    case "$variant" in
        karatsuba_zbc)
            src_list+=("$REPO_ROOT/src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c")
            ;;
        fafft_zbc)
            src_list+=(
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/fafft.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/fafft_v5.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/fafft_rvv.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/reduce.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/gfmul_fft.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/bitconversion.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/btfy_consts_gft.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/btfy_ffft.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/encode.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/gft_mul_vi.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/CRT_fft_hqc1.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/CRT_fft_hqc3.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/radix16_base.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/radix16_81.c"
                "$REPO_ROOT/src/poly_mul/scalar/fafft_zbc/ref/radix16_193.c"
            )
            ;;
        hybrid)
            src_list+=(
                "$REPO_ROOT/src/poly_mul/scalar/hybrid_zbc/hybrid.c"
                "$REPO_ROOT/src/poly_mul/scalar/hybrid_zbc/hybrid_v3.c"
            )
            ;;
        *)
            return 1
            ;;
    esac

    local harness_c
    harness_c="$(mktemp "$REPORT_DIR/dudect_riscv_${variant}_XXXXXX.c")"
    cat > "$harness_c" << 'DUDECT_RISCV_HARNESS'
#include <stdint.h>

extern void vect_mul(uint64_t *o, const uint64_t *v1, const uint64_t *v2);

int main(void) {
    static uint64_t a[64] = {0};
    static uint64_t b[64] = {0};
    static uint64_t o[128] = {0};

    for (int i = 0; i < 1024; i++) {
        a[i & 63] ^= (uint64_t)i * 0x9e3779b97f4a7c15ULL;
        b[(i * 7) & 63] ^= (uint64_t)i * 0xbf58476d1ce4e5b9ULL;
        vect_mul(o, a, b);
    }

    return (int)(o[0] & 1ULL);
}
DUDECT_RISCV_HARNESS

    local compile_log="$REPORT_DIR/dudect_cross_hqc${level}_${variant}.log"

    "$RISCV_CC" -O2 \
        -march=rv64gcv_zbc_zbb_zbkb_zvbc \
        -mabi=lp64d \
        -ffreestanding -fno-builtin -fno-tree-vectorize \
        -I"$vendor/src/common/${hqc_variant}" \
        -I"$vendor/src/common" \
        -I"$vendor/src/ref/${hqc_variant}" \
        -I"$vendor/src/ref" \
        -I"$vendor/lib/fips202" \
        -I"$REPO_ROOT/include" \
        -I"$REPO_ROOT/src/common" \
        -I"$REPO_ROOT/src/poly_mul/scalar/fafft_zbc" \
        "$harness_c" \
        "${src_list[@]}" \
        "$vendor/src/common/symmetric.c" \
        "$vendor/lib/fips202/fips202.c" \
        "$vendor/src/ref/vector.c" \
        "$vendor/src/ref/gf.c" \
        -o "$out_elf" >"$compile_log" 2>&1

    local rc=$?
    if [[ $rc -ne 0 || ! -f "$out_elf" ]]; then
        rm -f "$harness_c"
        return 1
    fi

    rm -f "$harness_c"
    printf "%s" "$out_elf"
}

while IFS= read -r variant; do
    [[ -z "$variant" ]] && continue
    for level in $LEVELS; do
        path="$(variant_path "$variant")"
        echo "[dudect_all] level=${level} variant=${variant} path=${path}"

        if [[ "$path" == "reference" ]]; then
            out_file="$REPORT_DIR/dudect_hqc${level}_${variant}.txt"
            set +e
            cmd_out="$($REPO_ROOT/scripts/ci/check_ct.sh --dudect "$level" "$variant" 2>&1)"
            rc=$?
            set -e
            printf "%s\n" "$cmd_out" > "$out_file"
            if [[ $rc -eq 0 ]]; then
                echo "${level},${variant},${path},PASS,${out_file}" >> "$SUMMARY"
            else
                echo "${level},${variant},${path},FAIL,${out_file}" >> "$SUMMARY"
            fi
            continue
        fi

        if [[ "$path" == "zbc" ]]; then
            if [[ ! -x "$RISCV_CC" ]]; then
                echo "${level},${variant},${path},FAIL,missing_cross_compiler" >> "$SUMMARY"
                echo "[dudect_all] ERROR: missing cross compiler at $RISCV_CC" >&2
                continue
            fi
            set +e
            elf_path="$(cross_compile_dudect_zbc "$level" "$variant")"
            rc=$?
            set -e
            if [[ $rc -eq 0 && -n "$elf_path" ]]; then
                printf "${ANSI_BOLD_RED}[READY FOR SILICON] Cross-compiled dudect binary ready. Please SCP to the Banana Pi for native timing analysis.${ANSI_RESET}\n"
                echo "${level},${variant},${path},READY_FOR_SILICON,${elf_path}" >> "$SUMMARY"
            else
                echo "${level},${variant},${path},FAIL,dudect_cross_hqc${level}_${variant}.log" >> "$SUMMARY"
            fi
            continue
        fi

        if [[ "$path" == "zvbc" ]]; then
            msg="[SKIPPED] Emulation is invalid for timing analysis, and current physical silicon lacks zvbc support."
            echo "$msg"
            echo "${level},${variant},${path},SKIPPED,no_zvbc_silicon" >> "$SUMMARY"
            continue
        fi

        echo "${level},${variant},unknown,SKIPPED,unclassified_variant" >> "$SUMMARY"
    done
done < "$REPO_ROOT/scripts/implemented_variants.txt"

echo "[dudect_all] summary: $SUMMARY"
