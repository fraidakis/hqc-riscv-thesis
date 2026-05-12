#!/usr/bin/env python3
"""parse_histogram.py – Convert raw Spike -g PC counts to an Instruction Mix table.

Usage:
    parse_histogram.py <raw_hist> <elf_file> <objdump_bin>

Arguments:
    raw_hist     Path to the Spike -g histogram file (stderr output).
    elf_file     Path to the ELF binary to disassemble.
    objdump_bin  Path to the llvm-objdump (or riscv64-unknown-elf-objdump) binary.

Output:
    Prints a formatted table of instruction mnemonics, counts, and percentages
    sorted in descending order of execution count, to stdout.
"""

import re
import subprocess
import sys
from collections import defaultdict


def parse_histogram(hist_path: str) -> dict:
    """Parse Spike -g histogram output.

    Spike writes lines of the form:
        <hex_pc> <count>
    Returns a dict mapping stripped hex PC (no leading zeros) -> count.
    """
    counts: dict = {}
    pat = re.compile(r"^([0-9a-fA-F]+)\s+(\d+)")
    with open(hist_path) as fh:
        for line in fh:
            m = pat.search(line)
            if m:
                pc = m.group(1).lstrip("0") or "0"
                counts[pc] = int(m.group(2))
    return counts


def disassemble(elf_path: str, objdump_bin: str) -> dict:
    """Run objdump -d and return {hex_pc_no_leading_zeros: mnemonic}."""
    result = subprocess.run(
        [objdump_bin, "-d", "--no-show-raw-insn", elf_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=False,
    )
    pc_to_mnem: dict = {}
    # Lines look like: "    80001234:  vmulhu.vv  v0, v0, v0"
    pat = re.compile(r"^\s*([0-9a-fA-F]+):\s+(\S+)")
    for line in result.stdout.splitlines():
        m = pat.match(line)
        if m:
            pc = m.group(1).lstrip("0") or "0"
            mnem = m.group(2)
            pc_to_mnem[pc] = mnem
    return pc_to_mnem


def main() -> int:
    if len(sys.argv) != 4:
        print(
            "Usage: parse_histogram.py <raw_hist> <elf_file> <objdump_bin>",
            file=sys.stderr,
        )
        return 1

    raw_hist, elf_file, objdump_bin = sys.argv[1], sys.argv[2], sys.argv[3]

    pc_counts = parse_histogram(raw_hist)
    if not pc_counts:
        print("WARNING: no PC counts found in histogram", file=sys.stderr)
        return 0

    pc_to_mnem = disassemble(elf_file, objdump_bin)

    mnem_counts: dict = defaultdict(int)
    for pc, count in pc_counts.items():
        mnem = pc_to_mnem.get(pc, f"<unknown@{pc}>")
        mnem_counts[mnem] += count

    total = sum(mnem_counts.values())
    rows = sorted(mnem_counts.items(), key=lambda x: x[1], reverse=True)

    col_w = 28
    print(f"{'Instruction':<{col_w}} {'Count':>14} {'Pct':>8}")
    print("-" * (col_w + 25))
    for mnem, count in rows:
        pct = 100.0 * count / total if total else 0.0
        print(f"{mnem:<{col_w}} {count:>14,} {pct:>7.2f}%")
    print("-" * (col_w + 25))
    print(f"{'TOTAL':<{col_w}} {total:>14,} {'100.00%':>8}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
