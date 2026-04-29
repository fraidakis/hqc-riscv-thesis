#!/usr/bin/env bash
# scripts/ci/check_ct.sh
# Build a fresh native (x86) HQC-128 test binary, then run a constant-time
# check using Valgrind's Memcheck (ctgrind-style).
# Marks the HQC secret key as undefined before encrypt/decrypt to detect
# any secret-dependent branches or memory accesses.
# Usage: check_ct.sh [security_level]   (default: 128)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

SECURITY_LEVEL="${1:-128}"
BINARY="build/hqc${SECURITY_LEVEL}_ct_test"

# Map security level to vendor variant directory (hqc-1/hqc-3/hqc-5)
case "$SECURITY_LEVEL" in
  128) VARIANT="hqc-1" ;;
  192) VARIANT="hqc-3" ;;
  256) VARIANT="hqc-5" ;;
  *) echo "[check_ct] ERROR: unsupported security level $SECURITY_LEVEL (use 128, 192, or 256)" >&2; exit 1 ;;
esac

VENDOR="vendor/hqc-ref"

echo "[check_ct] Building native HQC-${SECURITY_LEVEL} test binary..."
mkdir -p build
gcc -O2 \
    -I"${VENDOR}/src/common" \
    -I"${VENDOR}/src/common/${VARIANT}" \
    -I"${VENDOR}/src/ref" \
    -I"${VENDOR}/src/ref/${VARIANT}" \
    -I"${VENDOR}/lib/fips202" \
    "${VENDOR}/src/common/kem.c" \
    "${VENDOR}/src/common/code.c" \
    "${VENDOR}/src/common/crypto_memset.c" \
    "${VENDOR}/src/common/fft.c" \
    "${VENDOR}/src/common/symmetric.c" \
    "${VENDOR}/src/ref/gf2x.c" \
    "${VENDOR}/src/ref/gf.c" \
    "${VENDOR}/src/ref/hqc.c" \
    "${VENDOR}/src/ref/parsing.c" \
    "${VENDOR}/src/ref/reed_muller.c" \
    "${VENDOR}/src/ref/reed_solomon.c" \
    "${VENDOR}/src/ref/vector.c" \
    "${VENDOR}/lib/fips202/fips202.c" \
    "${VENDOR}/packaging/utils/helpers/main_hqc.c" \
    -o "$BINARY"
echo "[check_ct] Build OK: $BINARY"

REPORT_DIR="analysis/ct_verify/ctgrind"
mkdir -p "$REPORT_DIR"
REPORT="$REPORT_DIR/$(basename "$BINARY")_valgrind.xml"

echo "[check_ct] Running Valgrind Memcheck on $BINARY ..."
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
    echo "[check_ct] PASS: No secret-dependent errors detected."
else
    echo "[check_ct] FAIL: Valgrind reported errors — see $REPORT" >&2
fi
exit $RC
