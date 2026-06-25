#!/usr/bin/env python3
"""
scripts/report/gen_roofline.py

Generates roofline data CSV and a pgfplots-consumable data file from:
  1. Peak ceiling measurements (from roofline_peak binary)
  2. Per-variant derived counters (arith_intensity from compute_derived_counters.py)

Operation unit:
  "Useful arithmetic operations" = retired instructions (instret).
  This is an integer/carryless-multiply kernel (not FLOP). We use total
  retired instructions as the operation count, consistent across all variants.
  The arith_intensity is then instret / dram_bytes_est (ops/byte).

Output:
  results/hardware/profiling/roofline/roofline_latest.csv

Columns:
  variant,level,arith_intensity,achieved_ops_per_s,
  peak_compute_scalar,peak_compute_vector,peak_dram_bw

Usage:
  python3 scripts/report/gen_roofline.py \\
      --ceilings <ceilings.csv> \\
      --counters-dir <path_to_counters> \\
      --output <roofline_latest.csv>
"""

import argparse
import csv
import sys
from pathlib import Path


def parse_ceilings(filepath: str) -> dict:
    """Parse the roofline_peak output CSV."""
    ceilings = {}
    with open(filepath) as f:
        reader = csv.DictReader(f)
        for row in reader:
            ceilings[row["ceiling"]] = float(row["value"])
    return ceilings


def parse_derived_section(filepath: str) -> list[dict]:
    """Parse a counters CSV's '# --- derived metrics ---' section."""
    rows = []
    in_derived = False
    header = None

    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if line == "# --- derived metrics ---":
                in_derived = True
                continue
            if not in_derived:
                continue
            if line.startswith("#") or not line:
                continue
            cols = [c.strip() for c in line.split(",")]
            if header is None:
                header = cols
                continue
            row = {}
            for i, h in enumerate(header):
                if i < len(cols):
                    row[h] = cols[i]
            rows.append(row)

    return rows


def extract_variant_from_comments(filepath: str) -> str:
    """Extract variant name from CSV comment headers."""
    with open(filepath) as f:
        for line in f:
            if line.startswith("# variant:"):
                return line.split(":", 1)[1].strip()
    return "unknown"


def extract_level_from_comments(filepath: str) -> str:
    """Extract level from CSV comment headers."""
    with open(filepath) as f:
        for line in f:
            if line.startswith("# level:"):
                return line.split(":", 1)[1].strip()
    return "128"


def main():
    parser = argparse.ArgumentParser(description="Generate roofline data CSV")
    parser.add_argument("--ceilings", required=True, help="Path to ceilings CSV (roofline_peak output)")
    parser.add_argument("--counters-dir", required=True, help="Base directory for counters results")
    parser.add_argument("--output", required=True, help="Output roofline CSV path")
    parser.add_argument("--freq-ghz", type=float, default=1.6, help="CPU frequency in GHz (for ops/s)")
    args = parser.parse_args()

    # Parse ceilings
    ceilings = parse_ceilings(args.ceilings)
    peak_scalar = ceilings.get("peak_compute_scalar", 0)
    peak_vector = ceilings.get("peak_compute_vector", 0)
    peak_bw = ceilings.get("peak_dram_bw", 0)

    # Find all counters CSV files
    counters_base = Path(args.counters_dir)
    if not counters_base.exists():
        print(f"ERROR: counters directory not found: {counters_base}", file=sys.stderr)
        sys.exit(1)

    # Collect data points from derived metrics in counter CSVs
    points = []
    for csv_path in sorted(counters_base.rglob("*_latest.csv")):
        variant = extract_variant_from_comments(str(csv_path))
        level = extract_level_from_comments(str(csv_path))

        derived_rows = parse_derived_section(str(csv_path))
        if not derived_rows:
            continue

        for row in derived_rows:
            op = row.get("op", "unknown")
            arith_intensity = row.get("arith_intensity", "NA")
            if arith_intensity == "NA" or arith_intensity == "inf":
                continue

            try:
                ai = float(arith_intensity)
            except (ValueError, TypeError):
                continue

            # Compute achieved ops/s from median_cycles and instret
            # Read raw counters from the main (non-derived) section
            # For now, use the IPC × frequency approximation
            ipc = row.get("ipc", "0")
            try:
                ipc_val = float(ipc)
            except (ValueError, TypeError):
                ipc_val = 0

            # achieved ops/s ≈ IPC × frequency (ops/cycle × cycles/s)
            achieved = ipc_val * args.freq_ghz * 1e9

            points.append({
                "variant": variant,
                "level": level,
                "op": op,
                "arith_intensity": ai,
                "achieved_ops_per_s": achieved,
            })

    # Write output CSV
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, "w", newline="") as f:
        f.write(f"# Roofline model data for HQC-RISC-V on BPI-F3 (SpacemiT X60)\n")
        f.write(f"# peak_compute_scalar: {peak_scalar:.0f} ops/s\n")
        f.write(f"# peak_compute_vector: {peak_vector:.0f} ops/s\n")
        f.write(f"# peak_dram_bw: {peak_bw:.0f} bytes/s\n")
        f.write(f"# freq_ghz: {args.freq_ghz}\n")
        f.write(f"# operation_unit: retired instructions (integer/GF2 crypto, not FLOP)\n")
        f.write("variant,level,op,arith_intensity,achieved_ops_per_s,"
                "peak_compute_scalar,peak_compute_vector,peak_dram_bw\n")
        for pt in points:
            f.write(f"{pt['variant']},{pt['level']},{pt['op']},"
                    f"{pt['arith_intensity']:.4f},{pt['achieved_ops_per_s']:.0f},"
                    f"{peak_scalar:.0f},{peak_vector:.0f},{peak_bw:.0f}\n")

    print(f"Roofline CSV: {output_path}")
    print(f"  {len(points)} data points")
    print(f"  Ceilings: scalar={peak_scalar/1e9:.2f} Gops/s, "
          f"vector={peak_vector/1e9:.2f} Gops/s, "
          f"DRAM={peak_bw/1e9:.2f} GB/s")

    # Also write a pgfplots-ready .dat file (space-separated, no comments mixed in)
    dat_path = output_path.with_suffix(".dat")
    with open(dat_path, "w") as f:
        f.write("% Roofline pgfplots data\n")
        f.write("% Columns: arith_intensity achieved_gops variant level op\n")
        f.write("% Ceilings (Gops/s): scalar={:.3f} vector={:.3f}\n".format(
            peak_scalar/1e9, peak_vector/1e9))
        f.write("% DRAM BW (GB/s): {:.3f}\n".format(peak_bw/1e9))
        f.write("ai achieved_gops variant level op\n")
        for pt in points:
            f.write(f"{pt['arith_intensity']:.4f} "
                    f"{pt['achieved_ops_per_s']/1e9:.6f} "
                    f"{pt['variant']} {pt['level']} {pt['op']}\n")

    print(f"  pgfplots data: {dat_path}")


if __name__ == "__main__":
    main()
