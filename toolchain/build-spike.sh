#!/usr/bin/env bash
# toolchain/build-spike.sh
# Clone and build Spike (riscv-isa-sim) with commitlog and histogram.

set -euo pipefail

PREFIX="${RISCV:-$HOME/Thesis/installed-tools}"
SRCDIR="$HOME/Thesis/src/riscv-isa-sim"

if [[ ! -d "$SRCDIR" ]]; then
    echo "[build-spike] Cloning riscv-isa-sim ..."
    git clone https://github.com/riscv-software-src/riscv-isa-sim.git "$SRCDIR"
fi

echo "[build-spike] Configuring ..."
mkdir -p "$SRCDIR/build"
(cd "$SRCDIR/build" && \
    "$SRCDIR/configure" \
        --prefix="$PREFIX" \
        2>&1 | tail -5)

echo "[build-spike] Building ..."
make -j"$(nproc)" -C "$SRCDIR/build" 2>&1 | tail -5

echo "[build-spike] Installing ..."
make install -C "$SRCDIR/build"

echo "[build-spike] Done. Spike: $PREFIX/bin/spike"
"$PREFIX/bin/spike" --help 2>&1 | head -3
