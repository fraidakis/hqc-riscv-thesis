#!/usr/bin/env python3
"""scripts/debug/zvbc_trace.py
Filter a Spike --log-commits file for Zvbc instructions (vclmul, vclmulh)
and report their count, arguments, and PC distribution.

Usage:
    python3 scripts/debug/zvbc_trace.py run.commitlog
"""

import argparse
import re
from collections import Counter, defaultdict


ZVBC_MNEMONICS = {"vclmul.vv", "vclmul.vx", "vclmulh.vv", "vclmulh.vx"}

COMMIT_RE = re.compile(
    r"core\s+\d+:\s+(0x[0-9a-f]+)\s+\(0x[0-9a-f]+\)\s+(\S+)\s*(.*)"
)


def main() -> None:
    parser = argparse.ArgumentParser(description="Zvbc instruction tracer")
    parser.add_argument("logfile")
    args = parser.parse_args()

    total_insns = 0
    zvbc_hits: Counter = Counter()
    pc_hits: defaultdict = defaultdict(int)

    with open(args.logfile, "r", errors="replace") as fh:
        for line in fh:
            m = COMMIT_RE.match(line)
            if not m:
                continue
            total_insns += 1
            pc, mnemonic, operands = m.group(1), m.group(2).lower(), m.group(3)
            if mnemonic in ZVBC_MNEMONICS:
                zvbc_hits[mnemonic] += 1
                pc_hits[pc] += 1

    total_zvbc = sum(zvbc_hits.values())
    print(f"Total instructions : {total_insns:,}")
    print(f"Total Zvbc insns   : {total_zvbc:,}  "
          f"({100*total_zvbc/max(total_insns,1):.2f}%)")
    print()
    print("Zvbc breakdown:")
    for mnemonic, count in zvbc_hits.most_common():
        print(f"  {mnemonic:<20} {count:>10,}")
    print()
    print("Top 10 Zvbc PCs:")
    for pc, count in sorted(pc_hits.items(), key=lambda x: -x[1])[:10]:
        print(f"  {pc}  {count:>10,}")


if __name__ == "__main__":
    main()
