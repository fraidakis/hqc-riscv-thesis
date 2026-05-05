#!/usr/bin/env bash
# scripts/ci/lint.sh — Unified clang-tidy static analysis gate.
#
# Runs bugprone-* and cert-* checks over C source files in src/ that are
# lintable by the host clang-tidy. Files whose compile_commands entry
# contains -mrvv-vector-bits (VLEN-specific TUs requiring cross-compiler)
# are skipped automatically.
#
# For the remaining scalar files, --extra-arg=-march=rv64gcv_zbc_zbb_zbkb
# strips unsupported extensions that host clang-tidy (LLVM 16) rejects.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source toolchain env if present (sets $RISCV for sysroot).
TOOLCHAIN_ENV="$REPO_ROOT/toolchain/toolchain.env"
if [[ -f "$TOOLCHAIN_ENV" ]]; then
    # shellcheck source=/dev/null
    source "$TOOLCHAIN_ENV"
fi

CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
BUILD_DIR="${BUILD_DIR:-build/qemu}"

# Disabled checks:
#   bugprone-easily-swappable-parameters — high false-positive rate on
#     fixed-signature crypto functions; suppressed project-wide.
#   clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling —
#     bare-metal target; memcpy_s / memset_s are not available. Secret-data
#     zeroization is handled explicitly via memset_zero (volatile fn-ptr).
CHECKS="bugprone-*,cert-*,-bugprone-easily-swappable-parameters,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling"

cd "$REPO_ROOT"

# Build the list of lintable files: src/*.c files present in compile_commands
# whose compile command does NOT contain -mrvv-vector-bits.
# Files with -mrvv-vector-bits require the cross-compiler and cannot be
# processed by host clang-tidy.
LINTABLE_FILES=$(python3 - <<'PYEOF'
import json, os, sys

build_dir = os.environ.get("BUILD_DIR", "build/qemu")
cc_path = os.path.join(build_dir, "compile_commands.json")

try:
    with open(cc_path) as f:
        db = json.load(f)
except FileNotFoundError:
    print(f"lint.sh: compile_commands.json not found at {cc_path}", file=sys.stderr)
    sys.exit(1)

# Two-pass: conservative exclusion — if ANY compile_commands entry for a file
# contains -mrvv-vector-bits (VLEN-specific TU), exclude the file entirely.
# This prevents clang-tidy from picking a VLEN entry when there are duplicates.
vlen_files = set()
all_src_files = set()

for entry in db:
    fpath = os.path.relpath(entry["file"])
    if not fpath.startswith("src/"):
        continue
    cmd = entry.get("command") or " ".join(entry.get("arguments", []))
    all_src_files.add(fpath)
    if "-mrvv-vector-bits" in cmd:
        vlen_files.add(fpath)

for fpath in sorted(all_src_files - vlen_files):
    print(fpath)
PYEOF
)

if [[ -z "$LINTABLE_FILES" ]]; then
    echo "lint.sh: no lintable files found — check BUILD_DIR=${BUILD_DIR}" >&2
    exit 1
fi

echo "lint.sh: linting $(echo "$LINTABLE_FILES" | wc -l) files (VLEN-specific TUs excluded)"

FAILED=0

while IFS= read -r FILE; do
    ARGS=(
        "$FILE"
        -p "$BUILD_DIR"
        "--checks=$CHECKS"
        "--warnings-as-errors=$CHECKS"
        "--extra-arg=-march=rv64gcv_zbc_zbb_zbkb"
    )

    if ! "$CLANG_TIDY" "${ARGS[@]}"; then
        FAILED=1
    fi
done <<< "$LINTABLE_FILES"

exit "$FAILED"
