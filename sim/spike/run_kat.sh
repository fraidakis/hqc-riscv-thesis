#!/usr/bin/env bash
# sim/spike/run_kat.sh
# Run an HQC KAT ELF under Spike (no instruction logging).
# Usage: run_kat.sh <elf>

set -euo pipefail

ELF="${1:?Usage: $0 <elf>}"
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"

if [[ -n "${SPIKE:-}" ]]; then
    SPIKE_BIN="$SPIKE"
elif command -v spike >/dev/null 2>&1; then
    SPIKE_BIN="$(command -v spike)"
elif [[ -x "$RISCV/bin/spike" ]]; then
    SPIKE_BIN="$RISCV/bin/spike"
else
    echo "ERROR: spike not found (PATH or RISCV/bin)." >&2
    exit 2
fi

# Keep ISA aligned with the build architecture.
ISA="rv64gcv_zbc_zbb_zbkb_zvbc"

echo "[spike/run_kat] Running: $ELF" >&2
echo "[spike/run_kat] ISA: $ISA" >&2

"$SPIKE_BIN" \
    --isa="$ISA" \
    "$ELF"
