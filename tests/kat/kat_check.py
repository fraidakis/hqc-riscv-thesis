#!/usr/bin/env python3
"""
tests/kat/kat_check.py

Compare the shared-secret output emitted by the bare-metal KAT runner
(captured from Spike / QEMU console) against the reference .rsp file.

Usage:
    kat_check.py --actual <log file or "-" for stdin> \
                 --rsp    <path to PQCkemKAT_*.rsp>   \
                 [--level <128|192|256>]

Exit codes:
    0 — all vectors matched
    1 — mismatch or wrong number of vectors
    2 — usage error

The "actual" file should contain lines of the form:
    ss: <128-char hex string>
exactly 100 such lines, interspersed with other diagnostic output which is
silently ignored.
"""

import argparse
import sys
import re


def parse_actual(path: str):
    """Return list of bytes from "ss: <hex>" lines in the log."""
    ss_list = []
    src = sys.stdin if path == "-" else open(path)
    with src as f:
        for line in f:
            m = re.match(r"^ss:\s*([0-9a-fA-F]{128})\s*$", line)
            if m:
                ss_list.append(bytes.fromhex(m.group(1)))
    return ss_list


def parse_rsp(path: str):
    """Return list of expected ss bytes from the .rsp file."""
    ss_list = []
    with open(path) as f:
        for line in f:
            m = re.match(r"^ss\s*=\s*([0-9a-fA-F]+)\s*$", line)
            if m:
                ss_list.append(bytes.fromhex(m.group(1)))
    return ss_list


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--actual", required=True, help='Log file or "-" for stdin')
    ap.add_argument("--rsp",    required=True, help="Reference .rsp file")
    ap.add_argument("--level",  default=None,  help="Security level (informational only)")
    args = ap.parse_args()

    actual = parse_actual(args.actual)
    expected = parse_rsp(args.rsp)

    label = f"HQC-{args.level}" if args.level else "HQC"

    if len(actual) != len(expected):
        print(f"FAIL [{label}]: got {len(actual)} ss lines, expected {len(expected)}",
              file=sys.stderr)
        sys.exit(1)

    failures = 0
    for i, (a, e) in enumerate(zip(actual, expected)):
        if a != e:
            print(f"FAIL [{label}] vector {i:3d}:")
            print(f"  expected: {e.hex()}")
            print(f"  actual:   {a.hex()}")
            failures += 1

    if failures:
        print(f"FAIL [{label}]: {failures}/{len(expected)} vectors mismatched")
        sys.exit(1)
    else:
        print(f"PASS [{label}]: {len(expected)}/{len(expected)} vectors matched")
        sys.exit(0)


if __name__ == "__main__":
    main()
