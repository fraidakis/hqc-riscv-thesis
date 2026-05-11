#!/usr/bin/env python3

import subprocess
import sys
from collections import defaultdict


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: map_pcs.py <elf_file> <out_dir>")
        return 1

    elf_file = sys.argv[1]
    out_dir = sys.argv[2]

    counts = {}
    with open(f"{out_dir}/pc_counts_all.txt", "r") as handle:
        for line in handle:
            parts = line.strip().split()
            if len(parts) == 2:
                counts[parts[1]] = int(parts[0])

    pcs = list(counts.keys())
    command = ["riscv64-unknown-elf-addr2line", "-e", elf_file, "-f", "-p"] + pcs

    result = subprocess.run(command, stdout=subprocess.PIPE, text=True, check=False)
    output = result.stdout.strip().split("\n")

    funcs = defaultdict(int)
    for pc, line in zip(pcs, output):
        func_name = line.split(" at ")[0].strip()
        funcs[func_name] += counts[pc]

    with open(f"{out_dir}/final_function_counts.txt", "w") as handle:
        handle.write("--- FUNCTION TOTALS ---\n")
        for func, count in sorted(funcs.items(), key=lambda item: item[1], reverse=True):
            handle.write(f"{func}: {count}\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())