#!/usr/bin/env bash
# scripts/bench/spike_kat.sh
#
# Run a KAT ELF under Spike and check its output against the reference .rsp.
# Use this for fafft_zvbc variants — QEMU does not support the Zvbc extension
# required for vclmul.vv / vclmulh.vv.
#
# Usage:
#   scripts/bench/spike_kat.sh <elf> <rsp-file>
#
# Examples:
#   scripts/bench/spike_kat.sh \
#       build/hqc128_fafft_zvbc.elf \
#       vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
#
#   scripts/bench/spike_kat.sh \
#       build/hqc256_fafft_zvbc.elf \
#       vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp
#
# Timing estimates (rough):
#   HQC-128 KAT (100 vectors): ~10–30 s (depends on fafft_zvbc speedup)
#   HQC-192 KAT (100 vectors): ~30–90 s
#   HQC-256 KAT (100 vectors): ~60–180 s

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="${RISCV}/bin/spike"

# ── Argument parsing ──────────────────────────────────────────────────────────
if [[ $# -lt 2 ]]; then
    echo "Usage: spike_kat.sh <elf> <rsp-file>" >&2
    exit 2
fi

ELF="$1"
RSP="$2"

if [[ ! -f "$ELF" ]]; then
    echo "ERROR: ELF not found: $ELF" >&2
    exit 2
fi
if [[ ! -f "$RSP" ]]; then
    echo "ERROR: RSP file not found: $RSP" >&2
    exit 2
fi
if [[ ! -x "$SPIKE" ]]; then
    echo "ERROR: Spike not found: $SPIKE" >&2
    echo "       Source toolchain/toolchain.env or check RISCV path" >&2
    exit 2
fi

# ── Derive security level from binary name ────────────────────────────────────
BASENAME="$(basename "$ELF")"
if   [[ "$BASENAME" == hqc128_* ]]; then LEVEL=128
elif [[ "$BASENAME" == hqc192_* ]]; then LEVEL=192
elif [[ "$BASENAME" == hqc256_* ]]; then LEVEL=256
else
    echo "WARNING: cannot infer level from binary name '$BASENAME'" >&2
    LEVEL=""
fi

ISA="rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm"

# Estimated time based on level (very rough, assumes fafft_zvbc speedup)
case "$LEVEL" in
    128) ESTIMATE="~10–30 s" ;;
    192) ESTIMATE="~30–90 s" ;;
    256) ESTIMATE="~60–180 s" ;;
    *)   ESTIMATE="unknown" ;;
esac

echo "[spike_kat] ELF  : $ELF" >&2
echo "[spike_kat] RSP  : $RSP" >&2
echo "[spike_kat] ISA  : $ISA" >&2
echo "[spike_kat] ETA  : $ESTIMATE" >&2
echo "[spike_kat] Running Spike (no --histogram, no --log-commits)..." >&2

# ── Temp file for Spike output ────────────────────────────────────────────────
LOG="$(mktemp /tmp/hqc_spike_kat_XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

# ── Run under Spike ───────────────────────────────────────────────────────────
# Spike routes guest UART (HTIF tohost/fromhost) directly to its own stdout.
# The KAT harness emits "ss: <hex>" lines followed by "PASS N/N" or "FAIL".
# Spike may exit non-zero on guest ecall halt — treat as non-fatal.
"$SPIKE" \
    --isa="$ISA" \
    "$ELF" \
    > "$LOG" 2>/dev/null || true

# ── Check captured output ─────────────────────────────────────────────────────
if [[ ! -s "$LOG" ]]; then
    echo "[spike_kat] FAIL: Spike produced no output for $BASENAME" >&2
    echo "            Possible causes: ISA mismatch, ELF crash at startup" >&2
    exit 1
fi

# ── Diff against reference .rsp ───────────────────────────────────────────────
LEVEL_ARG=""
[[ -n "$LEVEL" ]] && LEVEL_ARG="--level $LEVEL"

python3 "${REPO_ROOT}/tests/kat/kat_check.py" \
    --actual "$LOG" \
    --rsp    "$RSP" \
    $LEVEL_ARG
