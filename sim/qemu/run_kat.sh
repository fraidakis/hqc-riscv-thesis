#!/usr/bin/env bash
# sim/qemu/run_kat.sh
# Run an HQC ELF under QEMU system-mode and check exit code.
# Usage: run_kat.sh <elf>

set -euo pipefail

ELF="${1:?Usage: $0 <elf>}"
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
QEMU="$RISCV/bin/qemu-system-riscv64"

# ── Auto-build: rebuild the specific ELF target before running ────────────────
_BUILD_DIR="$(dirname "$ELF")"
_TARGET="$(basename "$ELF" .elf)"
if [[ -f "$_BUILD_DIR/CMakeCache.txt" ]]; then
    echo "[qemu/run_kat] Auto-build: cmake --build $_BUILD_DIR --target $_TARGET" >&2
    cmake --build "$_BUILD_DIR" --target "$_TARGET" -j"$(nproc)" >&2
fi
unset _BUILD_DIR _TARGET

# The bare-metal ELF loads at 0x80000000; OpenSBI also loads there → collision.
# Always use -bios none so QEMU starts the ELF directly as the "kernel".
BIOS="none"

# Memory sizing: link.ld allocates exactly 128 MiB (ORIGIN=0x80000000, LENGTH=128M).
# Match -m to that value so QEMU doesn't waste host RAM.
#
# CPU flags:
#   vlen=256 — HQC FAFFT butterflies use LMUL=m2 over 64-bit elements; VLEN=256
#              gives 4 elements per register group, matching the implementation's
#              loop structure. Smaller (128) would halve throughput; larger (512)
#              would break the fixed-width grouping without code changes.
#   elen=64  — maximum element width; required for vd.vv 64-bit ops.
#   vext_spec=v1.0 — ratified vector ISA; older v0.7/v0.8 drafts behave differently.
#   zbkb/zbc/zvbc — bit-manipulation and carryless-multiply extensions used by
#                   the polynomial-mul kernels.
#
# Serial: plain -serial stdio (no mon: mux) so automated capture
# never sees QEMU monitor escape sequences in the output stream.
# Ctrl-A X is unavailable, but -no-reboot + the test-finisher MMIO write
# (bench.c → QEMU_FINISHER) ensure QEMU exits cleanly after every run.

echo "[run_kat] Running: $ELF"
"$QEMU" \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
    -m 128M \
    -nographic \
    -bios "$BIOS" \
    -kernel "$ELF" \
    -serial stdio \
    -no-reboot \
    2>&1

echo "[run_kat] Done."
