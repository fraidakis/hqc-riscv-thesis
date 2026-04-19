#!/usr/bin/env bash
# toolchain/build-llvm.sh
# (Re-)build LLVM/Clang for the RISC-V cross toolchain.
# This wraps the riscv-gnu-toolchain --enable-llvm make target.

set -euo pipefail

PREFIX="${RISCV:-$HOME/Thesis/installed-tools}"
SRCDIR="$HOME/Thesis/src/riscv-gnu-toolchain"

echo "[build-llvm] PREFIX=$PREFIX"
cd "$SRCDIR"
make -j"$(nproc)" build-llvm 2>&1 | tee /tmp/llvm-build.log
echo "[build-llvm] Done. Log: /tmp/llvm-build.log"
