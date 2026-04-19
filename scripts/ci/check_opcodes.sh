#!/usr/bin/env bash
# scripts/ci/check_opcodes.sh
# Verify that the target ELF contains vclmul/vclmulh vector opcodes.
# Usage: check_opcodes.sh <objdump-binary> <elf>

set -euo pipefail

OBJDUMP="${1:?Usage: $0 <objdump> <elf>}"
ELF="${2:?Usage: $0 <objdump> <elf>}"

REPORT="${ELF%.elf}.objdump.txt"

echo "[check_opcodes] Disassembling $ELF ..."
"$OBJDUMP" -d "$ELF" > "$REPORT"

echo "[check_opcodes] Checking for vclmul / vclmulh opcodes ..."
if grep -qE '\bvclmul(h)?\b' "$REPORT"; then
    echo "[check_opcodes] PASS: vclmul/vclmulh found in $ELF"
else
    echo "[check_opcodes] FAIL: vclmul/vclmulh NOT found in $ELF" >&2
    exit 1
fi

echo "[check_opcodes] Checking for clmul (scalar Zbc) opcodes ..."
grep -cE '\bclmul(h|r)?\b' "$REPORT" | xargs -I{} echo "[check_opcodes] {} clmul instructions found"
