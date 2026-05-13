#!/usr/bin/env python3

import argparse
import csv
import os
import shutil
import subprocess
import sys
from collections import defaultdict


ADDR2LINE_CHUNK_SIZE = 500


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Map PC histogram entries to functions using addr2line.",
    )
    parser.add_argument("--lines", action="store_true", help="Include source file:line in labels when available")
    parser.add_argument("--csv-out", default="", help="Write output CSV file path")
    parser.add_argument("--variant", default="", help="Variant label for CSV preamble")
    parser.add_argument("--level", default="", help="Security level label for CSV preamble")
    parser.add_argument("--commit", default="", help="Commit hash for CSV preamble")
    parser.add_argument("--date", default="", help="Date string for CSV preamble")
    parser.add_argument("elf_file", help="Path to ELF file")
    parser.add_argument("pc_counts_file", help="Path to ""count pc"" histogram input file or '-' for stdin")
    return parser.parse_args()


def load_counts(pc_counts_file: str) -> dict[str, int]:
    counts: dict[str, int] = {}
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
    return counts


def main() -> int:
    parsed = parse_args()

    elf_file = parsed.elf_file
    pc_counts_file = parsed.pc_counts_file

    counts = load_counts(pc_counts_file)

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

    funcs = defaultdict(int)
    for index in range(0, len(pcs), ADDR2LINE_CHUNK_SIZE):
        chunk = pcs[index : index + ADDR2LINE_CHUNK_SIZE]
        command = [addr2line_bin, "-e", elf_file, "-f", "-p"] + chunk
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, check=False)
        output = result.stdout.splitlines()

        if len(output) < len(chunk):
            output.extend(["?? at ??:?"] * (len(chunk) - len(output)))

        for pc, line in zip(chunk, output):
            parts = line.split(" at ")
            func_name = parts[0].strip()
            if parsed.lines and len(parts) > 1:
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

    if parsed.csv_out:
        with open(parsed.csv_out, "w", encoding="utf-8", newline="") as handle:
            handle.write(f"# date: {parsed.date}\n")
            handle.write(f"# commit: {parsed.commit}\n")
            handle.write(f"# variant: {parsed.variant}\n")
            handle.write(f"# level: {parsed.level}\n")
            writer = csv.writer(handle)
            writer.writerow(["function", "instructions", "percentage"])
            writer.writerows(rows)
    else:
        writer = csv.writer(sys.stdout)
        writer.writerows(rows)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())