#!/usr/bin/env bash
# sim/spike/run.sh
# Run an HQC ELF under Spike with commit log and histogram enabled.
# Usage: run.sh <elf> [extra_spike_flags...]

set -euo pipefail

ELF="${1:?Usage: $0 <elf> [flags...]}"
shift
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="$RISCV/bin/spike"
PK="$RISCV/riscv64-unknown-elf/bin/pk"

ISA="rv64gcv_zbc_zbb_zbkb_zvbc"

echo "[spike/run] ELF: $ELF"
echo "[spike/run] ISA: $ISA"

# Run with commit log output to a file for post-processing
LOGFILE="${ELF%.elf}.commitlog"
"$SPIKE" \
    --isa="$ISA" \
    --log-commits \
    --histogram \
    -l "$@" \
    "$ELF" \
    2>"$LOGFILE"

echo "[spike/run] Commit log written to: $LOGFILE"
