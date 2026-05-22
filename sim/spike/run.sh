#!/usr/bin/env bash
# sim/spike/run.sh
# Unified Spike runner for HQC ELFs.
#
# Modes:
#   --clean    clean run (no logging; default for timing and KATs)
#   --profile  commit-log + histogram + disassembly trace (slow)
#
# Profiling mode can produce very large logs (10–50 GB on full runs).
#
# Usage:
#   run.sh --clean <elf> [extra_spike_flags...]
#   run.sh --profile <elf> [extra_spike_flags...]
#   run.sh <elf> [extra_spike_flags...]         # same as --clean

set -euo pipefail

MODE="clean"
if [[ "${1:-}" == "--profile" ]]; then
    MODE="profile"
    shift
elif [[ "${1:-}" == "--clean" ]]; then
    MODE="clean"
    shift
elif [[ "${1:-}" == "--bench" ]]; then
    MODE="clean"
    shift
fi

ELF="${1:?Usage: $0 [--clean|--profile] <elf> [flags...]}"
shift || true
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="$RISCV/bin/spike"

# ── Auto-build: rebuild the specific ELF target before running ────────────────
_BUILD_DIR="$(dirname "$ELF")"
_TARGET="$(basename "$ELF" .elf)"
if [[ -f "$_BUILD_DIR/CMakeCache.txt" ]]; then
    echo "[spike/run] Auto-build: cmake --build $_BUILD_DIR --target $_TARGET" >&2
    cmake --build "$_BUILD_DIR" --target "$_TARGET" -j"$(nproc)" >&2
fi
unset _BUILD_DIR _TARGET

# The bench harness reads mcycle (CSR 0xb00), always accessible in M-mode.
# _zicntr/_zihpm are added defensively so that any ELF still using the
# rdcycle pseudoinstruction (CSR 0xc00) does not trap → hang.
ISA="rv64gcv_zbc_zbb_zvbc_zvbb_zvl256b_zicntr_zihpm"

echo "[spike/run] ELF: $ELF"                                        >&2
echo "[spike/run] ISA: $ISA"                                        >&2
echo "[spike/run] MODE: $MODE"                                      >&2

if [[ "$MODE" == "profile" ]]; then
    # Commit log and instruction trace go to a file for post-processing.
    LOGFILE="${ELF%.elf}.commitlog"
    echo "[spike/run] WARNING: profiling flags enabled (slow)" >&2
    "$SPIKE" \
        --isa="$ISA" \
        --log-commits \
        --histogram \
        -l "$@" \
        "$ELF" \
        2>"$LOGFILE"
    echo "[spike/run] Commit log written to: $LOGFILE" >&2
else
    "$SPIKE" \
        --isa="$ISA" \
        "$@" \
        "$ELF"
fi
