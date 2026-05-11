#!/usr/bin/env bash
# scripts/test/run_kat.sh
#
# Drive the bare-metal KAT binary under a simulator (Spike by default, QEMU optional),
# capture the console output, and diff against the reference .rsp.
#
# Usage:
#   run_kat.sh [--sim qemu|spike] <elf-binary> <rsp-file>
#   run_kat.sh [--sim qemu|spike] <elf-binary> <level> <polymul>
#
# Arguments:
#   elf-binary   Path to the compiled hqc<LEVEL>_<POLYMUL> ELF (KAT mode).
#   rsp-file     Path to the reference PQCkemKAT_*.rsp file.
#   level        Security level used by the current CTest wiring (128/192/256).
#   polymul      Polymul variant label, ignored here but accepted for
#                compatibility with the add_test() signature in CMake.
#
# Simulator selection (defaults to Spike if not specified):
#   --sim spike  : Run under Spike ISS (fast, deterministic)
#   --sim qemu   : Run under QEMU system-mode emulation
#
# Output:
#   Prints PASS/FAIL summary to stdout.
#   Exits 0 on success, 1 on KAT failure or simulator error.
#
# Timeout: 120 s per security level (HQC-256 KAT on QEMU may take ~90 s).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Default to Spike; can be overridden with --sim
SIM="spike"
if [[ "${1:-}" == "--sim" ]]; then
    SIM="${2:-spike}"
    shift 2 || true
fi

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 2 ]]; then
    echo "Usage: run_kat.sh [--sim qemu|spike] <elf> <rsp>" >&2
    echo "   or: run_kat.sh [--sim qemu|spike] <elf> <level> <polymul>" >&2
    exit 2
fi

ELF="$1"
ARG2="$2"
ARG3="${3:-}"

if [[ "$ARG2" =~ ^(128|192|256)$ ]]; then
    LEVEL="$ARG2"
    POLYMUL="${ARG3:-}"
    RSP="${REPO_ROOT}/tests/kat/PQCkemKAT_${LEVEL}.rsp"
else
    RSP="$ARG2"
    POLYMUL=""
    LEVEL=""
fi

# ── Auto-build: rebuild the specific ELF target before running ────────────────
_BUILD_DIR="$(dirname "$ELF")"
_TARGET="$(basename "$ELF" .elf)"
if [[ -f "$_BUILD_DIR/CMakeCache.txt" ]]; then
    echo "[kat/run_kat] Auto-build: cmake --build $_BUILD_DIR --target $_TARGET" >&2
    cmake --build "$_BUILD_DIR" --target "$_TARGET" -j"$(nproc)" >&2
fi
unset _BUILD_DIR _TARGET

if [[ "$SIM" != "qemu" && "$SIM" != "spike" ]]; then
    echo "ERROR: --sim must be 'qemu' or 'spike'" >&2
    exit 2
fi

if [[ "$SIM" == "qemu" && ! -x "${REPO_ROOT}/sim/qemu/run.sh" ]]; then
    echo "ERROR: sim/qemu/run.sh not found or not executable" >&2
    exit 2
fi
if [[ "$SIM" == "spike" && ! -x "${REPO_ROOT}/sim/spike/run.sh" ]]; then
    echo "ERROR: sim/spike/run.sh not found or not executable" >&2
    exit 2
fi

# ── Derive level from binary name when not provided explicitly ───────────────
BASENAME="$(basename "$ELF")"
if [[ -z "$LEVEL" ]]; then
    if [[ "$BASENAME" == hqc128_* ]]; then LEVEL=128
    elif [[ "$BASENAME" == hqc192_* ]]; then LEVEL=192
    elif [[ "$BASENAME" == hqc256_* ]]; then LEVEL=256
    else
        echo "WARNING: cannot infer level from binary name '$BASENAME'; omitting level label" >&2
        LEVEL=""
    fi
fi

if [[ ! -f "$RSP" ]]; then
    echo "ERROR: reference RSP file '$RSP' not found" >&2
    exit 2
fi

# ── Temp file for simulator output ────────────────────────────────────────────
LOG="$(mktemp /tmp/hqc_kat_XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

# ── Run under simulator ───────────────────────────────────────────────────────
if [[ "$SIM" == "qemu" ]]; then
    echo "Running: $ELF under QEMU..."
    "${REPO_ROOT}/sim/qemu/run.sh" "$ELF" > "$LOG" 2>&1 || true
else
    echo "Running: $ELF under Spike..."
    "${REPO_ROOT}/sim/spike/run.sh" --clean "$ELF" > "$LOG" 2>&1 || true
fi

# ── Check captured output ─────────────────────────────────────────────────────
if [[ ! -s "$LOG" ]]; then
    echo "FAIL: Simulator produced no output for $BASENAME" >&2
    exit 1
fi

# ── Diff against reference .rsp ───────────────────────────────────────────────
LEVEL_ARG=""
[[ -n "$LEVEL" ]] && LEVEL_ARG="--level $LEVEL"

python3 "${REPO_ROOT}/tests/kat/kat_check.py" \
    --actual "$LOG"  \
    --rsp    "$RSP"  \
    $LEVEL_ARG
