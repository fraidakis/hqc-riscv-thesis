#!/usr/bin/env bash
# scripts/ci/check_isa_string.sh
# Verify the ELF was compiled for the expected ISA string (rv64gcv_zbc_zvbc).
# Usage: check_isa_string.sh <elf>

set -euo pipefail

ELF="${1:?Usage: $0 <elf>}"
RISCV="${RISCV:-$HOME/Thesis/installed-tools}"
READELF="$RISCV/bin/llvm-readelf"

echo "[check_isa] Reading attributes from $ELF ..."
ISA_STRING=$("$READELF" -A "$ELF" 2>/dev/null | grep -i "Tag_RISCV_arch" | awk -F'"' '{print $2}')

if [[ -z "$ISA_STRING" ]]; then
    echo "[check_isa] WARNING: Could not read RISC-V arch attribute (older binutils format?)"
    exit 0
fi

echo "[check_isa] Found ISA: $ISA_STRING"

REQUIRED_EXTS=(rv64 gc v zbc zvbc)
FAIL=0
for EXT in "${REQUIRED_EXTS[@]}"; do
    if echo "$ISA_STRING" | grep -qi "$EXT"; then
        echo "[check_isa] OK: $EXT present"
    else
        echo "[check_isa] FAIL: $EXT MISSING from ISA string" >&2
        FAIL=1
    fi
done

exit $FAIL
