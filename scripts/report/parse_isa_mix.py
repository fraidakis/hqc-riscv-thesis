#!/usr/bin/env python3
"""
scripts/report/parse_isa_mix.py

Parse objdump disassembly and emit instruction-mix CSV.
Reads objdump output from stdin.

Output sections:
  1. Per-mnemonic counts grouped by category, sorted descending by count
     within each group (ties: alphabetical).
  2. total,all_instructions,N
  3. Per-category summary totals.
  4. Flat top-N view: top_n,<rank>,<mnemonic>,<count>  (top 20 by default)

Usage:
    objdump -d ./binary | python3 parse_isa_mix.py [--top N]
"""

import sys
import re
import argparse
from collections import Counter

# ── Instruction category definitions ─────────────────────────────────────────
CATEGORIES: dict[str, list[str]] = {
    "zbc":        ["clmul", "clmulh", "clmulr"],
    "zbb":        ["andn", "orn", "xnor", "clz", "ctz", "cpop", "max", "maxu", "min", "minu",
                   "orc.b", "rev8", "rol", "ror", "rori", "sext.b", "sext.h", "zext.h"],
    "zba":        ["sh1add", "sh2add", "sh3add", "add.uw", "sh1add.uw", "sh2add.uw",
                   "sh3add.uw", "slli.uw"],
    "zbs":        ["bclr", "bclri", "bext", "bexti", "binv", "binvi", "bset", "bseti"],
    "rvv_arith":  ["vadd", "vsub", "vand", "vor", "vxor", "vnot", "vsll", "vsrl", "vsra",
                   "vmerge", "vmv", "vslideup", "vslidedown", "vslide1up", "vslide1down",
                   "vrgather", "vcompress", "vredxor", "vredand", "vredor"],
    "rvv_load":   ["vle8", "vle16", "vle32", "vle64", "vlse8", "vlse16", "vlse32", "vlse64",
                   "vl1re8", "vl1re16", "vl1re32", "vl1re64", "vl2re8", "vl4re8", "vl8re8"],
    "rvv_store":  ["vse8", "vse16", "vse32", "vse64", "vsse8", "vsse16", "vsse32", "vsse64",
                   "vs1r", "vs2r", "vs4r", "vs8r"],
    "rvv_config": ["vsetvli", "vsetivli", "vsetvl"],
    "zvbc":       ["vclmul", "vclmulh"],
    "scalar_mul": ["mul", "mulh", "mulhu", "mulhsu", "mulw", "div", "divu", "rem", "remu"],
    "branch":     ["beq", "bne", "blt", "bge", "bltu", "bgeu", "jal", "jalr"],
    "load":       ["lb", "lbu", "lh", "lhu", "lw", "lwu", "ld", "flw", "fld"],
    "store":      ["sb", "sh", "sw", "sd", "fsw", "fsd"],
}


def count_mnemonic(mnemonic: str, counts: Counter) -> int:
    """Sum counts for all objdump tokens that match this mnemonic (prefix match for suffixed variants)."""
    total = 0
    for key, cnt in counts.items():
        if key == mnemonic or key.startswith(mnemonic + "."):
            total += cnt
    return total


def main() -> None:
    parser = argparse.ArgumentParser(description="Parse objdump ISA mix")
    parser.add_argument("--top", type=int, default=20, help="Number of mnemonics in flat top-N view")
    args = parser.parse_args()

    # Count mnemonics from stdin
    counts: Counter = Counter()
    for line in sys.stdin:
        m = re.match(r"^\s+[0-9a-f]+:\s+[0-9a-f ]+\s+(\S+)", line)
        if not m:
            continue
        counts[m.group(1)] += 1

    # ── 1. Per-mnemonic rows, grouped by category, sorted desc by count ──────
    for cat, mnemonics in CATEGORIES.items():
        # Compute (count, mnemonic) for non-zero entries
        entries = []
        for mn in mnemonics:
            total = count_mnemonic(mn, counts)
            if total > 0:
                entries.append((total, mn))
        # Sort: descending count, then alphabetical on mnemonic for ties
        entries.sort(key=lambda x: (-x[0], x[1]))
        for total, mn in entries:
            print(f"{cat},{mn},{total}")

    # ── 2. Total instruction count ────────────────────────────────────────────
    total_insns = sum(counts.values())
    print(f"total,all_instructions,{total_insns}")

    # ── 3. Category summary totals ────────────────────────────────────────────
    for cat, mnemonics in CATEGORIES.items():
        cat_total = sum(count_mnemonic(mn, counts) for mn in mnemonics)
        if cat_total > 0:
            print(f"summary,{cat}_total,{cat_total}")

    # ── 4. Flat top-N view ────────────────────────────────────────────────────
    # Build a flat dict of all mnemonics that appear in any category
    all_categorized: dict[str, int] = {}
    for mnemonics in CATEGORIES.values():
        for mn in mnemonics:
            total = count_mnemonic(mn, counts)
            if total > 0:
                # Use the bare mnemonic name (not suffixed variants)
                all_categorized[mn] = all_categorized.get(mn, 0) + total

    # Also include uncategorized mnemonics that appear in the binary
    categorized_set: set[str] = set()
    for mnemonics in CATEGORIES.values():
        for mn in mnemonics:
            categorized_set.add(mn)

    flat: dict[str, int] = {}
    for key, cnt in counts.items():
        # Map back to bare mnemonic
        bare = key.split(".")[0] if "." in key else key
        matched = bare in categorized_set or key in categorized_set
        if matched:
            # Use the canonical bare form already in all_categorized
            canon = bare if bare in all_categorized else key
            flat[canon] = flat.get(canon, 0) + cnt
        else:
            flat[key] = flat.get(key, 0) + cnt

    # Sort flat by count descending, then alphabetical
    ranked = sorted(flat.items(), key=lambda x: (-x[1], x[0]))
    for rank, (mn, cnt) in enumerate(ranked[: args.top], start=1):
        print(f"top_n,{rank},{mn},{cnt}")


if __name__ == "__main__":
    main()
