#!/usr/bin/env bash
# tests/kat/run_kat.sh
#
# Drive the bare-metal KAT binary under QEMU system-mode emulation,
# capture the HTIF console output, and diff against the reference .rsp.
#
# Usage:
#   run_kat.sh <elf-binary> <rsp-file> [<qemu-binary>]
#
# Arguments:
#   elf-binary   Path to the compiled hqc<LEVEL>_<POLYMUL> ELF (KAT mode).
#   rsp-file     Path to the reference PQCkemKAT_*.rsp file.
#   qemu-binary  (optional) Path to qemu-system-riscv64.
#                Defaults to: qemu-system-riscv64 on PATH, or
#                             ../../../installed-tools/bin/qemu-system-riscv64.
#
# Output:
#   Prints PASS/FAIL summary to stdout.
#   Exits 0 on success, 1 on KAT failure or QEMU error.
#
# QEMU invocation uses the virt machine with:
#   -nographic  : serial output captured to a temp file via -serial file:...
#   -bios none  : ELF loaded directly (no OpenSBI)
#   -kernel     : the bare-metal ELF
#
# Timeout: 120 s per security level (HQC-256 KAT on QEMU may take ~90 s).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 2 ]]; then
    echo "Usage: run_kat.sh <elf> <rsp> [qemu-system-riscv64]" >&2
    exit 2
fi

ELF="$1"
RSP="$2"
QEMU="${3:-}"

# ── Auto-build: rebuild the specific ELF target before running ────────────────
_BUILD_DIR="$(dirname "$ELF")"
_TARGET="$(basename "$ELF" .elf)"
if [[ -f "$_BUILD_DIR/CMakeCache.txt" ]]; then
    echo "[kat/run_kat] Auto-build: cmake --build $_BUILD_DIR --target $_TARGET" >&2
    cmake --build "$_BUILD_DIR" --target "$_TARGET" -j"$(nproc)" >&2
fi
unset _BUILD_DIR _TARGET

if [[ ! -x "${REPO_ROOT}/sim/qemu/run_kat.sh" ]]; then
    echo "ERROR: sim/qemu/run_kat.sh not found or not executable" >&2
    exit 2
fi

# ── Derive level from binary name (hqc128_*, hqc192_*, hqc256_*) ─────────────
BASENAME="$(basename "$ELF")"
if [[ "$BASENAME" == hqc128_* ]]; then LEVEL=128
elif [[ "$BASENAME" == hqc192_* ]]; then LEVEL=192
elif [[ "$BASENAME" == hqc256_* ]]; then LEVEL=256
else
    echo "WARNING: cannot infer level from binary name '$BASENAME'; omitting level label" >&2
    LEVEL=""
fi

# ── Temp file for QEMU serial output ─────────────────────────────────────────
LOG="$(mktemp /tmp/hqc_kat_XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

# ── Run under QEMU (via sim/qemu wrapper) ───────────────────────────────────
echo "Running: $ELF under QEMU..."
# Optional third argument is kept for backward-compatible CLI shape.
if [[ -n "$QEMU" ]]; then
    echo "[kat/run_kat] NOTE: custom qemu argument is ignored; using sim/qemu/run_kat.sh" >&2
fi
"${REPO_ROOT}/sim/qemu/run_kat.sh" "$ELF" > "$LOG" 2>&1 || true

# ── Check captured output ─────────────────────────────────────────────────────
if [[ ! -s "$LOG" ]]; then
    echo "FAIL: QEMU produced no output for $BASENAME" >&2
    exit 1
fi

# ── Diff against reference .rsp ───────────────────────────────────────────────
LEVEL_ARG=""
[[ -n "$LEVEL" ]] && LEVEL_ARG="--level $LEVEL"

python3 "${SCRIPT_DIR}/kat_check.py" \
    --actual "$LOG"  \
    --rsp    "$RSP"  \
    $LEVEL_ARG
