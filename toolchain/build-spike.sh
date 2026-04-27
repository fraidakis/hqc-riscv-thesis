#!/usr/bin/env bash
# toolchain/build-spike.sh
# Clone and build Spike (riscv-isa-sim) optimised for benchmarking.
#
# Key flags:
#   CXXFLAGS="-O3 -march=native"   — maximise host simulation throughput
#   --enable-commitlog             — keep the capability (disabled at runtime
#                                    by not passing --log-commits / -l)
#
# Omitting -O3/-march=native leaves Spike at the autoconf default of -O2,
# which combined with an unstripped debug binary (201 MB) can add 20–40%
# extra overhead on top of any logging-related slowdowns.

set -euo pipefail

PREFIX="${RISCV:-$HOME/Thesis/installed-tools}"
SRCDIR="$HOME/Thesis/src/riscv-isa-sim"

if [[ ! -d "$SRCDIR" ]]; then
    echo "[build-spike] Cloning riscv-isa-sim ..."
    git clone https://github.com/riscv-software-src/riscv-isa-sim.git "$SRCDIR"
fi

echo "[build-spike] Configuring with -O3 -march=native ..."
mkdir -p "$SRCDIR/build"
(cd "$SRCDIR/build" && \
    CXXFLAGS="-O3 -march=native" \
    CFLAGS="-O3 -march=native" \
    "$SRCDIR/configure" \
        --prefix="$PREFIX" \
        --enable-commitlog \
        2>&1 | tail -5)

echo "[build-spike] Building ..."
make -j"$(nproc)" -C "$SRCDIR/build" 2>&1 | tail -5

echo "[build-spike] Installing ..."
make install -C "$SRCDIR/build"

echo "[build-spike] Stripping debug symbols to reduce binary size ..."
strip "$PREFIX/bin/spike" || true

echo "[build-spike] Done. Spike: $PREFIX/bin/spike"
"$PREFIX/bin/spike" --version 2>&1 | head -2 || \
    "$PREFIX/bin/spike" --help 2>&1 | head -3
