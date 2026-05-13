#!/usr/bin/env python3

import csv
import os
import shutil
import subprocess
import sys
from collections import defaultdict


def main() -> int:
    use_lines = False
    csv_out = ""
    args = sys.argv[1:]

    while args and args[0].startswith("--"):
        if args[0] == "--lines":
            use_lines = True
            args = args[1:]
            continue
        if args[0] == "--csv-out":
            if len(args) < 2:
                print("Usage: map_pcs.py [--lines] [--csv-out <csv_file>] <elf_file> <pc_counts_file|->")
                return 1
            csv_out = args[1]
            args = args[2:]
            continue
        print(f"Unknown option: {args[0]}", file=sys.stderr)
        print("Usage: map_pcs.py [--lines] [--csv-out <csv_file>] <elf_file> <pc_counts_file|->")
        return 1

    if len(args) != 2:
        print("Usage: map_pcs.py [--lines] [--csv-out <csv_file>] <elf_file> <pc_counts_file|->")
        return 1

    elf_file = args[0]
    pc_counts_file = args[1]

    counts = {}
    use_stdin = pc_counts_file == "-" or not sys.stdin.isatty()
    if use_stdin:
        lines = sys.stdin
        for line in lines:
            parts = line.strip().split()
            if len(parts) == 2:
                counts[parts[1]] = int(parts[0])
    else:
        with open(pc_counts_file, "r", encoding="utf-8") as handle:
            for line in handle:
                parts = line.strip().split()
                if len(parts) == 2:
                    counts[parts[1]] = int(parts[0])

    if not counts:
        return 0

    pcs = list(counts.keys())

    riscv_root = os.environ.get("RISCV", "")
    addr2line_candidates = [
        "riscv64-unknown-elf-addr2line",
        "llvm-addr2line",
    ]
    if riscv_root:
        addr2line_candidates.extend(
            [
                os.path.join(riscv_root, "bin", "riscv64-unknown-elf-addr2line"),
                os.path.join(riscv_root, "bin", "llvm-addr2line"),
            ]
        )

    addr2line_bin = ""
    for candidate in addr2line_candidates:
        if os.path.isabs(candidate):
            if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                addr2line_bin = candidate
                break
        elif shutil.which(candidate):
            addr2line_bin = candidate
            break

    if not addr2line_bin:
        print(
            "ERROR: addr2line tool not found. Set RISCV or add llvm-addr2line/riscv64-unknown-elf-addr2line to PATH.",
            file=sys.stderr,
        )
        return 2

    command = [addr2line_bin, "-e", elf_file, "-f", "-p"] + pcs

    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, check=False)
    output = result.stdout.strip().split("\n")

    funcs = defaultdict(int)
    for pc, line in zip(pcs, output):
        parts = line.split(" at ")
        func_name = parts[0].strip()
        if use_lines and len(parts) > 1:
            location = parts[1].strip()
            file_line = os.path.basename(location)
            if file_line != "??:?":
                func_name = f"{func_name} at {file_line}"
        funcs[func_name] += counts[pc]

    total_instructions = sum(funcs.values())
    htif_total = 0
    htif_keys = [
        key
        for key in funcs
        if key.startswith("hqc_htif_puts") or key.startswith("hqc_htif_putu64") or key.startswith("bench_putchar")
    ]
    for key in htif_keys:
        htif_total += funcs.pop(key, 0)
    clean_total_instructions = total_instructions - htif_total

    rows = []
    for func, count in sorted(funcs.items(), key=lambda item: item[1], reverse=True):
        pct = (count * 100.0 / clean_total_instructions) if clean_total_instructions else 0.0
        rows.append([func, count, f"{pct:.2f}"])

    if csv_out:
        with open(csv_out, "a", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle)
            writer.writerows(rows)
    else:
        writer = csv.writer(sys.stdout)
        writer.writerows(rows)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())