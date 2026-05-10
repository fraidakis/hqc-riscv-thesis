#!/usr/bin/env bash
# tests/kat/run_kat.sh
#
# Drive the bare-metal KAT binary under the configured simulator backend
# (QEMU or Spike), capture output, and diff against the reference .rsp.
#
# Usage:
#   run_kat.sh <elf-binary> <rsp-file> [<sim-binary>]
#   run_kat.sh <elf-binary> <level> <polymul>
#
# Arguments:
#   elf-binary   Path to the compiled hqc<LEVEL>_<POLYMUL> ELF (KAT mode).
#   rsp-file     Path to the reference PQCkemKAT_*.rsp file.
#   level        Security level used by the current CTest wiring (128/192/256).
#   polymul      Polymul variant label, ignored here but accepted for
#                compatibility with the add_test() signature in CMake.
#   sim-binary   (optional) Backward-compatible optional argument. Ignored.
#
# Output:
#   Prints PASS/FAIL summary to stdout.
#   Exits 0 on success, 1 on KAT failure or simulator error.
#
# Timeout: 120 s per security level (higher for slower simulator backends).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 2 ]]; then
    echo "Usage: run_kat.sh <elf> <rsp> [sim-binary]" >&2
    echo "   or: run_kat.sh <elf> <level> <polymul>" >&2
    exit 2
fi

ELF="$1"
ARG2="$2"
ARG3="${3:-}"
SIM_BIN_OPT=""

if [[ "$ARG2" =~ ^(128|192|256)$ ]]; then
    LEVEL="$ARG2"
    POLYMUL="${ARG3:-}"
    RSP="${REPO_ROOT}/tests/kat/PQCkemKAT_${LEVEL}.rsp"
else
    RSP="$ARG2"
    SIM_BIN_OPT="$ARG3"
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

SIM_BACKEND=""
if [[ -f "$(dirname "$ELF")/CMakeCache.txt" ]]; then
    SIM_BACKEND="$(grep -E '^HQC_SIM:STRING=' "$(dirname "$ELF")/CMakeCache.txt" | head -n1 | cut -d= -f2 || true)"
fi
if [[ -z "$SIM_BACKEND" ]]; then
    case "$ELF" in
        */build/spike/*) SIM_BACKEND="spike" ;;
        *) SIM_BACKEND="qemu" ;;
    esac
fi

RUNNER="${REPO_ROOT}/sim/${SIM_BACKEND}/run_kat.sh"
if [[ ! -x "$RUNNER" ]]; then
    echo "ERROR: simulator runner not found or not executable: $RUNNER" >&2
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

# ── Temp file for simulator output ───────────────────────────────────────────
LOG="$(mktemp /tmp/hqc_kat_XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

# ── Run under selected simulator backend ─────────────────────────────────────
echo "Running: $ELF under ${SIM_BACKEND}..."
# Optional third argument is kept for backward-compatible CLI shape.
if [[ -n "$SIM_BIN_OPT" ]]; then
    echo "[kat/run_kat] NOTE: custom simulator argument is ignored; using ${RUNNER}" >&2
fi
"$RUNNER" "$ELF" > "$LOG" 2>&1 || true

# ── Check captured output ─────────────────────────────────────────────────────
if [[ ! -s "$LOG" ]]; then
    echo "FAIL: ${SIM_BACKEND} produced no output for $BASENAME" >&2
    exit 1
fi

# ── Diff against reference .rsp ───────────────────────────────────────────────
LEVEL_ARG=""
[[ -n "$LEVEL" ]] && LEVEL_ARG="--level $LEVEL"

python3 "${SCRIPT_DIR}/kat_check.py" \
    --actual "$LOG"  \
    --rsp    "$RSP"  \
    $LEVEL_ARG
