#!/usr/bin/env bash
# toolchain/build-gcc-sysroot.sh
# (Re-)build the riscv-gnu-toolchain GCC/newlib sysroot.
# This is a convenience wrapper; the full build was already done via
# the riscv-gnu-toolchain Makefile.

set -euo pipefail

PREFIX="${RISCV:-$HOME/Thesis/installed-tools}"
SRCDIR="$HOME/Thesis/src/riscv-gnu-toolchain"

echo "[build-gcc-sysroot] PREFIX=$PREFIX"
echo "[build-gcc-sysroot] Source: $SRCDIR"

cd "$SRCDIR"
make -j"$(nproc)" 2>&1 | tee /tmp/gcc-sysroot-build.log
echo "[build-gcc-sysroot] Done. Log: /tmp/gcc-sysroot-build.log"
