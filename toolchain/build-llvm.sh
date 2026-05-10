#!/usr/bin/env bash
# toolchain/build-llvm.sh
# (Re-)build LLVM/Clang for the RISC-V cross toolchain.
# This wraps the riscv-gnu-toolchain --enable-llvm make target.
#
# Usage:
#   export RISCV=/path/to/install
#   export SRCDIR=/path/to/riscv-gnu-toolchain (optional)
#   bash toolchain/build-llvm.sh
#
# Environment variables:
#   RISCV        - Installation prefix (default: $HOME/riscv-tools)
#   SRCDIR       - Source directory for riscv-gnu-toolchain (auto-detected if not set)
#   GITHUB_WORKSPACE - GitHub Actions workspace root (used for CI path detection)

set -euo pipefail

# Determine installation prefix
PREFIX="${RISCV:-$HOME/riscv-tools}"
export RISCV="$PREFIX"

# Determine source directory
if [ -z "${SRCDIR:-}" ]; then
	# Try GitHub Actions workspace first (CI environment)
	if [ -n "${GITHUB_WORKSPACE:-}" ] && [ -d "$GITHUB_WORKSPACE/riscv-gnu-toolchain" ]; then
		SRCDIR="$GITHUB_WORKSPACE/riscv-gnu-toolchain"
	# Try RISCV parent directory (if RISCV is in a workspace)
	elif [ -d "$(dirname "$PREFIX")/riscv-gnu-toolchain" ]; then
		SRCDIR="$(dirname "$PREFIX")/riscv-gnu-toolchain"
	# Try standard local development paths
	elif [ -d "$HOME/riscv-tools/src/riscv-gnu-toolchain" ]; then
		SRCDIR="$HOME/riscv-tools/src/riscv-gnu-toolchain"
	elif [ -d "$HOME/Thesis/src/riscv-gnu-toolchain" ]; then
		SRCDIR="$HOME/Thesis/src/riscv-gnu-toolchain"
	# Try relative path from toolchain directory
	elif [ -d "../src/riscv-gnu-toolchain" ]; then
		SRCDIR="$(cd ../src/riscv-gnu-toolchain && pwd)"
	# Try current directory as fallback
	elif [ -f "configure" ] && [ -d "qemu" ]; then
		SRCDIR="$(pwd)"
	else
		echo "[build-llvm] ERROR: Could not locate riscv-gnu-toolchain source directory" >&2
		echo "[build-llvm] Searched:" >&2
		echo "[build-llvm]   - \$GITHUB_WORKSPACE/riscv-gnu-toolchain" >&2
		echo "[build-llvm]   - $(dirname "$PREFIX")/riscv-gnu-toolchain" >&2
		echo "[build-llvm]   - \$HOME/riscv-tools/src/riscv-gnu-toolchain" >&2
		echo "[build-llvm]   - \$HOME/Thesis/src/riscv-gnu-toolchain" >&2
		echo "[build-llvm]   - ../src/riscv-gnu-toolchain (relative)" >&2
		echo "[build-llvm]" >&2
		echo "[build-llvm] Set SRCDIR environment variable to override:" >&2
		echo "[build-llvm]   export SRCDIR=/path/to/riscv-gnu-toolchain" >&2
		exit 1
	fi
fi

# Verify source directory exists and has expected structure
if [ ! -d "$SRCDIR" ]; then
	echo "[build-llvm] ERROR: Source directory does not exist: $SRCDIR" >&2
	exit 1
fi

if [ ! -f "$SRCDIR/configure" ]; then
	echo "[build-llvm] ERROR: Not a valid riscv-gnu-toolchain source: missing configure in $SRCDIR" >&2
	exit 1
fi

echo "[build-llvm] PREFIX=$PREFIX"
echo "[build-llvm] SRCDIR=$SRCDIR"
cd "$SRCDIR"
make -j"$(nproc)" build-llvm 2>&1 | tee /tmp/llvm-build.log
echo "[build-llvm] Done. Log: /tmp/llvm-build.log"
