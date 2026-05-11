#!/usr/bin/env python3
"""scripts/report/parse_commits.py
Parse a Spike --log-commits output and produce an instruction-mix summary.

Usage:
    spike --log-commits ... 2> run.commitlog
    python3 scripts/report/parse_commits.py run.commitlog [--csv output.csv]
"""

import argparse
import csv
import re
from collections import Counter


# Regex for a Spike commit log line:
#   core   0: 0x0000000080000000 (0x00000297) auipc   t0, 0x0
COMMIT_RE = re.compile(
    r"core\s+\d+:\s+0x[0-9a-f]+\s+\(0x[0-9a-f]+\)\s+(\S+)"
)


def parse(path: str) -> Counter:
    counts: Counter = Counter()
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            m = COMMIT_RE.match(line)
            if m:
                mnemonic = m.group(1).lower()
                counts[mnemonic] += 1
    return counts


def main() -> None:
    parser = argparse.ArgumentParser(description="Parse Spike commit log")
    parser.add_argument("logfile", help="Spike commit log file")
    parser.add_argument("--csv", default=None, help="Write CSV to this path")
    parser.add_argument("--top", type=int, default=30, help="Show top N mnemonics")
    args = parser.parse_args()

    counts = parse(args.logfile)
    total = sum(counts.values())

    print(f"Total instructions retired: {total:,}")
    print(f"\nTop {args.top} instruction mnemonics:")
    print(f"{'Mnemonic':<24} {'Count':>12} {'%':>8}")
    print("-" * 46)
    for mnemonic, count in counts.most_common(args.top):
        print(f"{mnemonic:<24} {count:>12,} {100*count/total:>7.2f}%")

    if args.csv:
        with open(args.csv, "w", newline="") as fh:
            writer = csv.writer(fh)
            writer.writerow(["mnemonic", "count", "percent"])
            for mnemonic, count in counts.most_common():
                writer.writerow([mnemonic, count, f"{100*count/total:.4f}"])
        print(f"\nCSV written to: {args.csv}")


if __name__ == "__main__":
    main()
