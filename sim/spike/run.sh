#!/usr/bin/env bash
# sim/spike/run.sh
# Run an HQC ELF under Spike with commit log and histogram enabled.
#
# !! PROFILING / INSTRUCTION-MIX TOOL — NOT FOR BENCHMARKING !!
# --log-commits  logs every committed instruction + register writes to stderr.
# --histogram    collects per-instruction execution counts.
# -l             dumps a disassembly trace of every decoded instruction.
#
# These three flags together can produce 10–50 GB of log data on a full HQC
# benchmark run and are the primary cause of the observed 360× wall-clock
# slowdown versus QEMU.  Use sim/spike/run_bench.sh for cycle-count benchmarks.
#
# Usage: run.sh <elf> [extra_spike_flags...]

set -euo pipefail

ELF="${1:?Usage: $0 <elf> [flags...]}"
shift
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="$RISCV/bin/spike"

# The bench harness reads mcycle (CSR 0xb00), always accessible in M-mode.
# _zicntr/_zihpm are not needed.
ISA="rv64gcv_zbc_zbb_zbkb_zvbc"

echo "[spike/run] ELF: $ELF"                                        >&2
echo "[spike/run] ISA: $ISA"                                        >&2
echo "[spike/run] WARNING: --log-commits + -l active (SLOW)"        >&2
echo "[spike/run] Use sim/spike/run_bench.sh for timing benchmarks" >&2

# Commit log and instruction trace go to a file for post-processing.
LOGFILE="${ELF%.elf}.commitlog"
"$SPIKE" \
    --isa="$ISA" \
    --log-commits \
    --histogram \
    -l "$@" \
    "$ELF" \
    2>"$LOGFILE"

echo "[spike/run] Commit log written to: $LOGFILE" >&2
