#!/usr/bin/env bash
# scripts/ci/check_ct.sh
#
# Build a native (x86-64) CT test binary for a given HQC security level and
# polymul variant, then run Valgrind Memcheck to detect any secret-dependent
# branches or memory accesses in the KEM decapsulation path.
#
# Source selection mirrors the KAT target in CMakeLists.txt:
#   - Invariant core     : cmake/hqc_core.cmake
#   - Vendor invariant   : CMakeLists.txt VENDOR_INVARIANT_SOURCES
#   - Keccak             : always vendor reference (fips202.c)
#   - Decode             : always vendor reference (reed_muller + reed_solomon)
#   - Polymul            : variant-specific (see mapping below)
#
# Variants that require RISC-V Zbc / RVV intrinsics cannot be compiled on
# x86-64.  The script skips them with exit 0 so CI remains green.
# RISC-V binary-level CT analysis uses binsec (analysis/ct_verify/binsec/).
#
# Usage:
#   check_ct.sh <security_level> <variant>
#     security_level : 128 | 192 | 256
#     variant        : karatsuba_ref | karatsuba_zbc | fafft_zbc |
#                      hybrid | fafft_zvbc

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

# ── Arguments ────────────────────────────────────────────────────────────────
if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <security_level> <variant>" >&2
    exit 1
fi

LEVEL="$1"
VARIANT="$2"

# ── Security level → vendor HQC variant directory ────────────────────────────
case "$LEVEL" in
    128) HQC_VARIANT="hqc-1" ;;
    192) HQC_VARIANT="hqc-3" ;;
    256) HQC_VARIANT="hqc-5" ;;
    *)
        echo "[check_ct] ERROR: unsupported security level '$LEVEL' (use 128, 192, or 256)" >&2
        exit 1
        ;;
esac

VENDOR="vendor/hqc-ref"

# ── Polymul variant → source file(s) ─────────────────────────────────────────
# RISCV_ONLY=1 means the variant requires RISC-V Zbc/RVV intrinsics and cannot
# be compiled on x86-64.  Update this table when new variants land.
RISCV_ONLY=0

case "$VARIANT" in
    karatsuba_ref)
        POLYMUL_SRCS=("${VENDOR}/src/ref/gf2x.c")
        ;;
    karatsuba_zbc)
        POLYMUL_SRCS=("src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c")
        RISCV_ONLY=1
        ;;
    fafft_zbc)
        POLYMUL_SRCS=(
            "src/poly_mul/scalar/fafft_zbc/fafft.c"
            "src/poly_mul/scalar/fafft_zbc/fafft_rvv.c"
        )
        RISCV_ONLY=1
        ;;
    hybrid|hybrid_zbc)
        POLYMUL_SRCS=("src/poly_mul/scalar/hybrid_zbc/hybrid.c")
        RISCV_ONLY=1
        ;;
    fafft_zvbc)
        POLYMUL_SRCS=("src/poly_mul/vector/fafft_zvbc/fafft_vclmul.c")
        RISCV_ONLY=1
        ;;
    *)
        echo "[check_ct] ERROR: unknown variant '$VARIANT'" >&2
        exit 1
        ;;
esac

if [[ "$RISCV_ONLY" -eq 1 ]]; then
    echo "[check_ct] SKIP: $VARIANT uses RISC-V intrinsics — cannot compile natively."
    echo "[check_ct]        Binary-level CT analysis for this variant: analysis/ct_verify/binsec/"
    exit 0
fi

# ── Source lists — mirrors CMakeLists.txt target composition ─────────────────

# Invariant core (cmake/hqc_core.cmake)
CORE_SRCS=(
    "${VENDOR}/src/common/kem.c"
    "${VENDOR}/src/common/crypto_memset.c"
    "${VENDOR}/src/ref/parsing.c"
    "${VENDOR}/src/ref/vector.c"
    "${VENDOR}/src/ref/gf.c"
)

# Vendor invariant sources (CMakeLists.txt VENDOR_INVARIANT_SOURCES)
VENDOR_INVARIANT_SRCS=(
    "${VENDOR}/src/ref/hqc.c"
    "${VENDOR}/src/common/code.c"
    "${VENDOR}/src/common/fft.c"
    "${VENDOR}/src/common/symmetric.c"
)

# Always use reference Keccak and Decode for native CT builds.
# Phase 2/3 RVV implementations require binary-level analysis via binsec.
KECCAK_SRCS=("${VENDOR}/lib/fips202/fips202.c")
DECODE_SRCS=(
    "${VENDOR}/src/ref/reed_muller.c"
    "${VENDOR}/src/ref/reed_solomon.c"
)

# ── Output paths ─────────────────────────────────────────────────────────────
BUILD_DIR="build/ct"
BINARY="${BUILD_DIR}/ct_hqc${LEVEL}_${VARIANT}"
REPORT_DIR="analysis/ct_verify/ctgrind"
REPORT="${REPORT_DIR}/ct_hqc${LEVEL}_${VARIANT}_valgrind.xml"

mkdir -p "$BUILD_DIR" "$REPORT_DIR"

# ── Build ─────────────────────────────────────────────────────────────────────
echo "[check_ct] Building native HQC-${LEVEL}/${VARIANT} CT harness..."

gcc -O2 -DNDEBUG \
    -I"${VENDOR}/src/common/${HQC_VARIANT}" \
    -I"${VENDOR}/src/common" \
    -I"${VENDOR}/src/ref/${HQC_VARIANT}" \
    -I"${VENDOR}/src/ref" \
    -I"${VENDOR}/lib/fips202" \
    -Iinclude \
    "${CORE_SRCS[@]}" \
    "${VENDOR_INVARIANT_SRCS[@]}" \
    "${KECCAK_SRCS[@]}" \
    "${DECODE_SRCS[@]}" \
    "${POLYMUL_SRCS[@]}" \
    tests/ct/ct_harness.c \
    -o "$BINARY"

echo "[check_ct] Build OK: $BINARY"

# ── Valgrind Memcheck ─────────────────────────────────────────────────────────
echo "[check_ct] Running Valgrind Memcheck — HQC-${LEVEL}/${VARIANT}..."

valgrind \
    --tool=memcheck \
    --error-exitcode=1 \
    --xml=yes \
    --xml-file="$REPORT" \
    --track-origins=yes \
    --leak-check=no \
    "$BINARY"

RC=$?
if [[ $RC -eq 0 ]]; then
    echo "[check_ct] PASS — HQC-${LEVEL}/${VARIANT}: no secret-dependent errors detected."
else
    echo "[check_ct] FAIL — HQC-${LEVEL}/${VARIANT}: Valgrind reported errors (see ${REPORT})." >&2
fi
exit $RC
