#!/usr/bin/env bash
# sim/qemu/run.sh
# Run an HQC ELF under QEMU system-mode and check exit code.
# Usage: run.sh <elf>

set -euo pipefail

ELF="${1:?Usage: $0 <elf>}"
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
if [[ -n "${QEMU:-}" ]]; then
    QEMU_BIN="$QEMU"
elif command -v qemu-system-riscv64 >/dev/null 2>&1; then
    QEMU_BIN="$(command -v qemu-system-riscv64)"
elif [[ -x "$RISCV/bin/qemu-system-riscv64" ]]; then
    QEMU_BIN="$RISCV/bin/qemu-system-riscv64"
else
    echo "ERROR: qemu-system-riscv64 not found (PATH or RISCV/bin)." >&2
    exit 2
fi

# ── Auto-build: rebuild the specific ELF target before running ────────────────
_BUILD_DIR="$(dirname "$ELF")"
_TARGET="$(basename "$ELF" .elf)"
if [[ -f "$_BUILD_DIR/CMakeCache.txt" ]]; then
    # Guard: skip auto-build when the build dir is configured for Spike.
    # A Spike build uses HTIF tohost/fromhost as its exit mechanism; that MMIO
    # region does not exist on QEMU's virt machine, so the ELF would never signal
    # completion and would hang indefinitely.  Such ELFs must not be run via QEMU.
    _CACHED_SIM=$(grep -E '^HQC_SIM:STRING=' "$_BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || true)
    if [[ "$_CACHED_SIM" == "spike" ]]; then
        echo "[qemu/run] WARNING: skipping auto-build — $_BUILD_DIR is Spike-configured (HQC_SIM=spike)." >&2
        echo "[qemu/run] This ELF may be incompatible with QEMU.  Re-run from a QEMU build dir." >&2
    else
        echo "[qemu/run] Auto-build: cmake --build $_BUILD_DIR --target $_TARGET" >&2
        # Non-fatal: if the build fails (e.g. cached Keccak impl name no longer
        # valid after a CMakeLists.txt rename), warn and fall through to run
        # whatever ELF is already present on disk.
        cmake --build "$_BUILD_DIR" --target "$_TARGET" -j"$(nproc)" >&2 || \
            echo "[qemu/run] WARNING: auto-build failed, attempting to run existing ELF." >&2
    fi
fi
unset _BUILD_DIR _TARGET _CACHED_SIM

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

echo "[qemu/run] Running: $ELF"
"$QEMU_BIN" \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
    -m 128M \
    -nographic \
    -monitor none \
    -bios "$BIOS" \
    -kernel "$ELF" \
    -serial stdio \
    -no-reboot \
    2>&1

echo "[qemu/run] Done."
