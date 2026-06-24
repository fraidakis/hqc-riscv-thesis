#!/usr/bin/env python3
"""parse_perf_report.py — Parse `perf report --stdio` and `perf stat` output into CSV.

This is the hardware-profiling analogue of map_pcs.py (which parses Spike trace data).
Since the SpacemiT X60 PMU does not support sampling (no sscofpmf), this script
focuses on parsing:
  1. `gprof` flat-profile output → function-level CSV (instruction-weighted)
  2. `perf stat` output → aggregate counters CSV (cycles, instructions, IPC, etc.)

Usage:
  # Parse gprof flat profile into CSV
  parse_perf_report.py --mode gprof --input gmon_flat.txt --output hotspots.csv

  # Parse perf stat output into CSV
  parse_perf_report.py --mode perf-stat --input perf_stat.txt --output counters.csv

  # With provenance metadata
  parse_perf_report.py --mode perf-stat --input perf_stat.txt --output counters.csv \
      --variant scalar_zb --level 128 --commit abc1234 --date "2025-06-24 14:30"
"""

import argparse
import csv
import re
import sys
from typing import TextIO


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Parse hardware profiling output (gprof / perf stat) into CSV.",
    )
    parser.add_argument("--mode", required=True, choices=["gprof", "perf-stat"],
                        help="Input format to parse")
    parser.add_argument("--input", required=True, help="Input file path (or '-' for stdin)")
    parser.add_argument("--output", required=True, help="Output CSV file path (or '-' for stdout)")
    parser.add_argument("--variant", default="", help="Variant label for CSV metadata")
    parser.add_argument("--level", default="", help="Security level for CSV metadata")
    parser.add_argument("--commit", default="", help="Git commit hash for CSV metadata")
    parser.add_argument("--date", default="", help="Date string for CSV metadata")
    parser.add_argument("--kernel-version", default="", help="Kernel version")
    parser.add_argument("--gcc-version", default="", help="GCC version")
    parser.add_argument("--march", default="", help="-march flags used")
    parser.add_argument("--governor", default="", help="CPU governor setting")
    parser.add_argument("--core", default="", help="Pinned core number")
    parser.add_argument("--perf-version", default="", help="perf version")
    parser.add_argument("--paranoid", default="", help="perf_event_paranoid value")
    parser.add_argument("--op", default="", help="Operation being profiled (e.g. kem, mul)")
    return parser.parse_args()


def write_provenance(out: TextIO, args: argparse.Namespace) -> None:
    """Write provenance comment block at the top of the CSV."""
    if args.date:
        out.write(f"# date: {args.date}\n")
    if args.commit:
        out.write(f"# commit: {args.commit}\n")
    if args.variant:
        out.write(f"# variant: {args.variant}\n")
    if args.level:
        out.write(f"# level: {args.level}\n")
    if args.kernel_version:
        out.write(f"# kernel: {args.kernel_version}\n")
    if args.gcc_version:
        out.write(f"# gcc: {args.gcc_version}\n")
    if args.march:
        out.write(f"# march: {args.march}\n")
    if args.governor:
        out.write(f"# governor: {args.governor}\n")
    if args.core:
        out.write(f"# core: {args.core}\n")
    if args.perf_version:
        out.write(f"# perf: {args.perf_version}\n")
    if args.paranoid:
        out.write(f"# paranoid: {args.paranoid}\n")
    if args.op:
        out.write(f"# op: {args.op}\n")


def parse_gprof(input_file: str, output_file: str, args: argparse.Namespace) -> int:
    """Parse gprof flat profile into function,self_pct,cumulative_pct,calls,self_seconds CSV.
    
    gprof flat profile format (after header):
      %time  cumulative   self            self     total
              seconds    seconds  calls  ms/call  ms/call  name
     75.00      0.03     0.03        1    30.00    30.00  vect_mul
     25.00      0.04     0.01        1    10.00    10.00  keccak_f1600
    """
    lines = []
    if input_file == "-":
        lines = sys.stdin.readlines()
    else:
        with open(input_file, "r", encoding="utf-8") as f:
            lines = f.readlines()

    # Find the start of the flat profile data
    # Skip header lines until we find the separator line (all dashes)
    data_started = False
    rows = []

    for line in lines:
        stripped = line.strip()
        if not stripped:
            if data_started:
                break  # Empty line after data means end of flat profile
            continue

        # Look for the data rows (start after the column header line)
        # Data rows start with a number (percentage)
        if re.match(r'^\s*[\d.]+\s+[\d.]+\s+[\d.]+', stripped):
            data_started = True
            parts = stripped.split()
            if len(parts) >= 7:
                pct_time = parts[0]
                # cumulative_seconds = parts[1]
                self_seconds = parts[2]
                calls = parts[3]
                # self_ms_call = parts[4]
                # total_ms_call = parts[5]
                func_name = parts[6]
                rows.append({
                    "function": func_name,
                    "self_pct": pct_time,
                    "self_seconds": self_seconds,
                    "calls": calls,
                })
            elif len(parts) >= 4:
                # Sometimes gprof omits call count for inlined/special functions
                pct_time = parts[0]
                self_seconds = parts[2]
                func_name = parts[-1]
                rows.append({
                    "function": func_name,
                    "self_pct": pct_time,
                    "self_seconds": self_seconds,
                    "calls": "0",
                })

    # Write output
    out_handle: TextIO
    if output_file == "-":
        out_handle = sys.stdout
    else:
        out_handle = open(output_file, "w", encoding="utf-8", newline="")

    try:
        write_provenance(out_handle, args)
        writer = csv.writer(out_handle)
        writer.writerow(["function", "self_pct", "self_seconds", "calls"])
        for row in rows:
            writer.writerow([row["function"], row["self_pct"],
                           row["self_seconds"], row["calls"]])
    finally:
        if output_file != "-":
            out_handle.close()

    return 0


def parse_perf_stat(input_file: str, output_file: str, args: argparse.Namespace) -> int:
    """Parse `perf stat` output into metric,value CSV.
    
    perf stat format:
     Performance counter stats for 'command':
    
         1,234,567      cycles
           567,890      instructions              #    0.46  insn per cycle
             1,234      L1-dcache-load-misses
               567      branch-misses
    
       0.123456789 seconds time elapsed
    """
    lines = []
    if input_file == "-":
        lines = sys.stdin.readlines()
    else:
        with open(input_file, "r", encoding="utf-8") as f:
            lines = f.readlines()

    metrics = {}
    ipc = ""

    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("Performance counter") or stripped.startswith("#"):
            continue

        # Match lines like: "1,234,567      cycles"
        # or: "567,890      instructions              #    0.46  insn per cycle"
        match = re.match(r'^\s*([\d,]+)\s+(\S+)', stripped)
        if match:
            value_str = match.group(1).replace(",", "")
            event_name = match.group(2)
            try:
                value = int(value_str)
                metrics[event_name] = value
            except ValueError:
                continue

            # Extract IPC if present
            ipc_match = re.search(r'#\s+([\d.]+)\s+insn per cycle', stripped)
            if ipc_match:
                ipc = ipc_match.group(1)

        # Match time elapsed line
        time_match = re.match(r'^\s*([\d.]+)\s+seconds time elapsed', stripped)
        if time_match:
            metrics["time_elapsed_s"] = float(time_match.group(1))

    # Compute IPC if not already found
    if not ipc and "cycles" in metrics and "instructions" in metrics and metrics["cycles"] > 0:
        ipc = f"{metrics['instructions'] / metrics['cycles']:.4f}"

    # Write output
    out_handle: TextIO
    if output_file == "-":
        out_handle = sys.stdout
    else:
        out_handle = open(output_file, "w", encoding="utf-8", newline="")

    try:
        write_provenance(out_handle, args)
        writer = csv.writer(out_handle)
        writer.writerow(["metric", "value"])
        for event, value in metrics.items():
            writer.writerow([event, value])
        if ipc:
            writer.writerow(["ipc", ipc])
    finally:
        if output_file != "-":
            out_handle.close()

    return 0


def main() -> int:
    args = parse_args()

    if args.mode == "gprof":
        return parse_gprof(args.input, args.output, args)
    elif args.mode == "perf-stat":
        return parse_perf_stat(args.input, args.output, args)
    else:
        print(f"ERROR: Unknown mode '{args.mode}'", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
