#!/usr/bin/env bash
# toolchain/build-llvm.sh
# Build LLVM/Clang via riscv-gnu-toolchain.

set -euo pipefail

PREFIX="${RISCV:-$HOME/riscv-tools}"
export RISCV="$PREFIX"
DEFAULT_ARCH="${RISCV_ARCH:-rv64gcv}"
DEFAULT_ABI="${RISCV_ABI:-lp64d}"
BUILD_TARGET="${RISCV_MAKE_TARGET:-all}"
TOOLCHAIN_URL="${RISCV_LLVM_PREBUILT_URL:-}"
TMP_ROOT="${RUNNER_TEMP:-/tmp}"
BUILD_LOG="$TMP_ROOT/llvm-build.log"

if [ -z "${SRCDIR:-}" ]; then
	if [ -n "${GITHUB_WORKSPACE:-}" ] && [ -d "$GITHUB_WORKSPACE/riscv-gnu-toolchain" ]; then
		SRCDIR="$GITHUB_WORKSPACE/riscv-gnu-toolchain"
	elif [ -d "$(dirname "$PREFIX")/riscv-gnu-toolchain" ]; then
		SRCDIR="$(dirname "$PREFIX")/riscv-gnu-toolchain"
	elif [ -d "$HOME/riscv-tools/src/riscv-gnu-toolchain" ]; then
		SRCDIR="$HOME/riscv-tools/src/riscv-gnu-toolchain"
	elif [ -d "$HOME/Thesis/src/riscv-gnu-toolchain" ]; then
		SRCDIR="$HOME/Thesis/src/riscv-gnu-toolchain"
	elif [ -d "../src/riscv-gnu-toolchain" ]; then
		SRCDIR="$(cd ../src/riscv-gnu-toolchain && pwd)"
	elif [ -f "configure" ] && [ -d "qemu" ]; then
		SRCDIR="$(pwd)"
	else
		echo "[build-llvm] ERROR: Could not locate riscv-gnu-toolchain source directory" >&2
		exit 1
	fi
fi

if [ ! -d "$SRCDIR" ]; then
	echo "[build-llvm] ERROR: Source directory does not exist: $SRCDIR" >&2
	exit 1
fi

if [ ! -f "$SRCDIR/configure" ]; then
	echo "[build-llvm] ERROR: Not a valid riscv-gnu-toolchain source: missing configure in $SRCDIR" >&2
	exit 1
fi

download_prebuilt_toolchain() {
	local archive_dir archive_path extract_dir found_root
	if [ -z "$TOOLCHAIN_URL" ]; then
		echo "[build-llvm] No RISCV_LLVM_PREBUILT_URL provided; skipping prebuilt download"
		return 1
	fi

	archive_dir="$(mktemp -d "$TMP_ROOT/riscv-prebuilt.XXXXXX")"
	archive_path="$archive_dir/toolchain.tar.gz"

	echo "[build-llvm] Attempting prebuilt toolchain download: $TOOLCHAIN_URL"
	if ! wget -q -O "$archive_path" "$TOOLCHAIN_URL"; then
		echo "[build-llvm] Prebuilt download failed; falling back to source build"
		rm -rf "$archive_dir"
		return 1
	fi

	extract_dir="$archive_dir/extract"
	mkdir -p "$extract_dir"
	if ! tar -xf "$archive_path" -C "$extract_dir"; then
		echo "[build-llvm] Prebuilt archive extraction failed; falling back to source build"
		rm -rf "$archive_dir"
		return 1
	fi

	found_root="$(find "$extract_dir" -mindepth 1 -maxdepth 2 -type d \( -path '*/bin' -o -path '*/.content/bin' \) | head -n 1)"
	if [ -z "$found_root" ]; then
		echo "[build-llvm] Prebuilt archive layout was not recognized; falling back to source build"
		rm -rf "$archive_dir"
		return 1
	fi

	if [ "$(basename "$found_root")" = "bin" ]; then
		found_root="$(dirname "$found_root")"
	fi
	if [ "$(basename "$found_root")" = ".content" ]; then
		found_root="$found_root"
	fi

	rm -rf "$PREFIX"
	mkdir -p "$PREFIX"
	shopt -s dotglob
	mv "$found_root"/* "$PREFIX"/
	shopt -u dotglob
	rm -rf "$archive_dir"

	if [ -x "$PREFIX/bin/clang" ]; then
		echo "[build-llvm] Using prebuilt toolchain at $PREFIX"
		return 0
	fi

	echo "[build-llvm] Prebuilt toolchain did not provide clang at $PREFIX/bin/clang"
	return 1
}

ensure_configured() {
	local config_args=()
	config_args+=("--prefix=$PREFIX")
	config_args+=("--enable-llvm")
	config_args+=("--disable-linux")
	config_args+=("--with-arch=$DEFAULT_ARCH")
	config_args+=("--with-abi=$DEFAULT_ABI")

	if [ ! -f "$SRCDIR/Makefile" ] || ! grep -q -- '--enable-llvm' "$SRCDIR/config.status" 2>/dev/null; then
		echo "[build-llvm] Configuring riscv-gnu-toolchain with LLVM enabled"
		(cd "$SRCDIR" && ./configure "${config_args[@]}")
	else
		echo "[build-llvm] Reusing existing riscv-gnu-toolchain configuration"
	fi
}

echo "[build-llvm] PREFIX=$PREFIX"
echo "[build-llvm] SRCDIR=$SRCDIR"

if [ -x "$PREFIX/bin/clang" ]; then
	echo "[build-llvm] Existing clang found at $PREFIX/bin/clang"
	exit 0
fi

download_prebuilt_toolchain || true

if [ -x "$PREFIX/bin/clang" ]; then
	exit 0
fi

ensure_configured

cd "$SRCDIR"
echo "[build-llvm] Building target '$BUILD_TARGET'"
make -j"$(nproc)" "$BUILD_TARGET" 2>&1 | tee "$BUILD_LOG"
echo "[build-llvm] Done. Log: $BUILD_LOG"
