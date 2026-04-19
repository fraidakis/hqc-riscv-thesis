#!/usr/bin/env bash
# sim/qemu/run_kat.sh
# Run an HQC ELF under QEMU system-mode and check exit code.
# Usage: run_kat.sh <elf>

set -euo pipefail

ELF="${1:?Usage: $0 <elf>}"
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
QEMU="$RISCV/bin/qemu-system-riscv64"

BIOS="$RISCV/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin"
[[ -f "$BIOS" ]] || BIOS="none"

echo "[run_kat] Running: $ELF"
"$QEMU" \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
    -m 256M \
    -nographic \
    -bios "$BIOS" \
    -kernel "$ELF" \
    -serial mon:stdio \
    -no-reboot \
    -plugin "$RISCV/bin/libinsn.so" \
    2>&1

echo "[run_kat] Done."
