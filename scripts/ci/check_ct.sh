#!/usr/bin/env bash
# scripts/ci/check_ct.sh
# Run a constant-time check using Valgrind's Memcheck (ctgrind-style).
# Marks the HQC secret key as undefined before encrypt/decrypt to detect
# any secret-dependent branches or memory accesses.
# Usage: check_ct.sh <test_binary>

set -euo pipefail

BINARY="${1:?Usage: $0 <binary>}"
REPORT_DIR="analysis/ct_verify/ctgrind"
mkdir -p "$REPORT_DIR"
REPORT="$REPORT_DIR/$(basename "$BINARY" .elf)_valgrind.xml"

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
