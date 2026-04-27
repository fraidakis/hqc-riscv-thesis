#!/usr/bin/env bash
# sim/spike/run_bench.sh
#
# Clean Spike runner for BENCHMARKING — no instruction logging.
#
# All three logging flags present in run.sh (--log-commits, --histogram, -l)
# are intentionally omitted here.  On a full HQC benchmark those flags cause
# Spike to emit ~10–50 GB of log data per run, serialising every instruction
# through the host file system and producing the 360× wall-clock slowdown
# observed against QEMU.
#
# Usage:
#   ./sim/spike/run_bench.sh <elf> [extra_spike_flags...]
#
# Examples:
#   # Micro-bench timing only:
#   ./sim/spike/run_bench.sh build-mini/mini_bench_spike.elf
#
#   # Full KEM benchmark (N_ITER=1, outputs CSV to stdout):
#   ./sim/spike/run_bench.sh build-spike-probe/bench_hqc128_karatsuba_ref.elf
#
#   # With a non-default VLEN:
#   ./sim/spike/run_bench.sh build-mini/mini_bench_spike.elf --varch=vlen:512,elen:64

set -euo pipefail

ELF="${1:?Usage: $0 <elf> [extra_spike_flags...]}"
shift

RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
SPIKE="$RISCV/bin/spike"

# The bench harness reads mcycle (CSR 0xb00), which is always accessible in
# M-mode and requires no ISA extension.  _zicntr/_zihpm are not needed.
ISA="rv64gcv_zbc_zbb_zbkb_zvbc"

echo "[spike/bench] ELF : $ELF"  >&2
echo "[spike/bench] ISA : $ISA"  >&2
echo "[spike/bench] Logging flags: NONE (benchmark mode)"  >&2

# Note: Spike 1.1.1-dev configures VLEN at build time.
# The --varch flag was introduced in later versions; omit it here.
"$SPIKE" \
    --isa="$ISA" \
    "$@" \
    "$ELF"
