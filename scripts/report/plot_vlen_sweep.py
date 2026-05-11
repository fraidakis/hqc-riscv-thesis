#!/usr/bin/env python3
"""Plot throughput (ops/cycle) vs VLEN from benchmark results.

Parses benchmark CSV files for different VLEN configurations and generates
a throughput vs VLEN plot using matplotlib.

Usage:
    python plot_vlen_sweep.py <benchmark_dir> [output_png]
    Example: python plot_vlen_sweep.py analysis/benchmarks/isolated/vector latest_vlen_sweep.png
"""

import sys
import re
import csv
from pathlib import Path
from collections import defaultdict
from typing import Optional, Dict, List, Tuple

try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    sys.exit("Error: matplotlib and numpy are required. Install with: pip install matplotlib numpy")


def extract_vlen_from_csv(csv_path: Path) -> Optional[int]:
    """Extract VLEN value from CSV metadata or filename.

    Tries to find VLEN from:
    1. CSV file header comments
    2. Filename patterns like *_v256_, *_v512_, etc.
    3. Metadata in first lines

    Returns:
        VLEN value in bits, or None if not found
    """
    filename = csv_path.name

    # Try pattern: variant_v{VLEN}_{identifier}.csv
    match = re.search(r"_v(\d+)_", filename)
    if match:
        return int(match.group(1))

    # Try reading metadata from CSV header
    try:
        with open(csv_path, "r", encoding="utf-8") as f:
            for _ in range(10):  # Check first 10 lines
                line = f.readline().strip()
                if line.startswith("#"):
                    if "vlen" in line.lower() or "v=" in line.lower():
                        # Try to extract number
                        match = re.search(r"(\d+)", line)
                        if match:
                            return int(match.group(1))
    except Exception:
        pass

    return None


def extract_throughput_from_csv(csv_path: Path) -> Optional[float]:
    """Extract throughput (ops/cycle) from benchmark CSV.

    Calculates throughput as: instructions_retired / cycles

    Common column names:
    - median_cycles, median_instret: for cycle-level benchmarks
    - throughput, ops_per_cycle: if already calculated
    """
    try:
        with open(csv_path, "r", encoding="utf-8") as f:
            # Skip comments starting with #
            lines = [line for line in f if not line.startswith("#")]
            f.seek(0)

            # Read header first
            reader = csv.DictReader(lines)
            if reader is None or reader.fieldnames is None:
                return None

            for row in reader:
                if row is None:
                    continue

                # Look for pre-calculated throughput column
                for key in ["throughput", "ops_per_cycle", "throughput_ops_cycle", "tput"]:
                    if key in reader.fieldnames and row.get(key):
                        try:
                            return float(row[key])
                        except ValueError:
                            continue

                # Calculate throughput from cycles and instructions
                if "median_instret" in reader.fieldnames and "median_cycles" in reader.fieldnames:
                    try:
                        instret = float(row.get("median_instret", 0))
                        cycles = float(row.get("median_cycles", 0))
                        if cycles > 0:
                            return instret / cycles
                    except (ValueError, TypeError):
                        continue

                # Fallback: if we find vect_mul operation, use instret/cycles
                if row.get("op") == "vect_mul":
                    try:
                        instret = float(row.get("median_instret", 0))
                        cycles = float(row.get("median_cycles", 1))
                        if cycles > 0:
                            return instret / cycles
                    except (ValueError, TypeError):
                        continue

    except Exception as e:
        print(f"Warning: Could not read {csv_path}: {e}", file=sys.stderr)
        return None

    return None


def collect_vlen_data(benchmark_dir: Path) -> Dict[str, List[Tuple[int, float]]]:
    """Collect throughput vs VLEN data from benchmark directory.

    Scans for CSV files with naming patterns like:
    - hqc{level}_{variant}_polymul_N1.csv
    - Name format suggests different VLEN configurations in different directories

    Returns:
        Dictionary mapping variant name to list of (VLEN, throughput) tuples
    """
    data: Dict[str, List[Tuple[int, float]]] = defaultdict(list)

    if not benchmark_dir.exists():
        print(f"Error: Benchmark directory not found: {benchmark_dir}", file=sys.stderr)
        return data

    # Recursively find all CSV files
    for csv_file in benchmark_dir.rglob("*.csv"):
        # Skip non-benchmark files
        if csv_file.name.startswith("final_") or csv_file.name in ["instruction_mix.csv", "cycle_counts.csv"]:
            continue

        # Extract variant and level from filename
        # Pattern: hqc{level}_{variant}_polymul_N1.csv
        match = re.search(r"hqc(\d+)_([a-z0-9_]+)_polymul", csv_file.name)
        if not match:
            continue

        level = match.group(1)
        variant_base = match.group(2)

        # Infer VLEN from directory name or use a default
        # Directory names like: karatsuba_zvbc_20260503_1944 or karatsuba_zvbc_v2_20260503_2005
        dir_name = csv_file.parent.name
        vlen = extract_vlen_from_csv(csv_file)

        # If no VLEN extracted, try to infer from variant name
        if vlen is None:
            # Try common VLEN values
            if "zvbc" in variant_base or "zvbc" in dir_name:
                vlen = 256  # Default to 256 for zvbc (common for RVV)
            elif "zbc" in variant_base:
                vlen = 64  # Default to 64 for scalar
            else:
                vlen = 128

        throughput = extract_throughput_from_csv(csv_file)

        if throughput is None:
            continue

        # Use variant name as key
        variant_key = f"{variant_base}_hqc{level}"
        data[variant_key].append((vlen, throughput))

    # If no data collected, return empty
    if not data:
        print("Warning: No data extracted from CSV files. Checking available files...", file=sys.stderr)
        csv_count = len(list(benchmark_dir.rglob("*.csv")))
        print(f"  Found {csv_count} CSV files total", file=sys.stderr)

    # Sort by VLEN for each variant
    for variant in data:
        data[variant].sort(key=lambda x: x[0])

    return data


def plot_vlen_sweep(data: Dict[str, List[Tuple[int, float]]], output_path: Optional[Path] = None) -> None:
    """Generate VLEN sweep plot.

    Args:
        data: Dictionary mapping variant to (VLEN, throughput) data
        output_path: Path to save the plot, or None to display
    """
    if not data:
        print("Error: No data to plot", file=sys.stderr)
        return

    plt.figure(figsize=(12, 7))

    # Color palette for different variants
    colors = [
        "#1f77b4",
        "#ff7f0e",
        "#2ca02c",
        "#d62728",
        "#9467bd",
        "#8c564b",
        "#e377c2",
        "#7f7f7f",
    ]

    for idx, (variant, points) in enumerate(sorted(data.items())):
        if not points:
            continue

        vlens = [p[0] for p in points]
        throughputs = [p[1] for p in points]

        color = colors[idx % len(colors)]
        plt.plot(
            vlens,
            throughputs,
            marker="o",
            linestyle="-",
            linewidth=2,
            markersize=8,
            label=variant,
            color=color,
        )

    plt.xlabel("VLEN (bits)", fontsize=12, fontweight="bold")
    plt.ylabel("Throughput (ops/cycle)", fontsize=12, fontweight="bold")
    plt.title("Throughput vs VLEN Sweep", fontsize=14, fontweight="bold")
    plt.legend(loc="best", fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150)
        print(f"[plot_vlen_sweep] Plot saved to: {output_path}")
    else:
        plt.show()


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python plot_vlen_sweep.py <benchmark_dir> [output_png]", file=sys.stderr)
        sys.exit(1)

    benchmark_dir = Path(sys.argv[1])
    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else None

    print(f"[plot_vlen_sweep] Scanning benchmark directory: {benchmark_dir}")
    data = collect_vlen_data(benchmark_dir)

    if not data:
        print("Error: No benchmark data found", file=sys.stderr)
        sys.exit(1)

    print(f"[plot_vlen_sweep] Found {len(data)} variants")
    for variant, points in sorted(data.items()):
        print(f"  {variant}: {len(points)} data points")

    plot_vlen_sweep(data, output_path)


if __name__ == "__main__":
    main()
