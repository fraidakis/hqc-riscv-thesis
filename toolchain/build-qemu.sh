#!/usr/bin/env bash
# toolchain/build-qemu.sh
# (Re-)build QEMU system emulator and TCG plugins via riscv-gnu-toolchain.

set -euo pipefail

PREFIX="${RISCV:-$HOME/Thesis/installed-tools}"
SRCDIR="$HOME/Thesis/src/riscv-gnu-toolchain"

echo "[build-qemu] PREFIX=$PREFIX"
cd "$SRCDIR"
make -j"$(nproc)" build-sim SIM=qemu 2>&1 | tee /tmp/qemu-build.log

echo "[build-qemu] Copying TCG plugins ..."
PLUGIN_DIR="$PREFIX/lib/qemu/plugins"
mkdir -p "$PLUGIN_DIR"
BUILDDIR="$SRCDIR/build-qemu/contrib/plugins"
if [[ -d "$BUILDDIR" ]]; then
    cp "$BUILDDIR"/*.so "$PLUGIN_DIR/"
    cp "$BUILDDIR/libexeclog.so" "$PREFIX/bin/libinsn.so"
    echo "[build-qemu] Plugins installed to $PLUGIN_DIR"
fi
echo "[build-qemu] Done."
