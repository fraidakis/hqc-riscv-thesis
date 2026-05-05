#!/usr/bin/env python3
"""Plot theoretical complexity curves vs empirical cycle counts.

Overlay theoretical O(n^α) complexity curves for different poly_mul algorithms:
- Schoolbook: O(n^2)
- Karatsuba: O(n^1.585)
- FA-FFT: O(n log n) with constant factors

Against empirical cycle count measurements from benchmarks.

Usage:
    python plot_complexity.py <benchmark_csv> [output_png]
    Example: python plot_complexity.py analysis/benchmarks/cycle_counts.csv complexity_plot.png
"""

import sys
import re
import csv
from pathlib import Path
from typing import Optional, Dict, List, Tuple

try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    sys.exit("Error: matplotlib and numpy are required. Install with: pip install matplotlib numpy")


def parse_benchmark_file(csv_path: Path) -> Dict[str, List[Tuple[float, float]]]:
    """Parse benchmark CSV to extract (input_size, cycles) data.

    Looks for columns like:
    - N, input_size, problem_size
    - cycles, median_cycles, total_cycles
    - algorithm, variant, polymul
    """
    data: Dict[str, List[Tuple[float, float]]] = {}

    try:
        with open(csv_path, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)

            if reader is None or reader.fieldnames is None:
                print(f"Error: Could not parse CSV headers from {csv_path}", file=sys.stderr)
                return data

            for row in reader:
                if not row:
                    continue

                # Extract algorithm/variant name
                algorithm = None
                for key in ["algorithm", "variant", "polymul", "category"]:
                    if key in reader.fieldnames:
                        algorithm = row.get(key)
                        if algorithm:
                            break

                if not algorithm:
                    algorithm = "unknown"

                # Extract problem size (N)
                problem_size = None
                for key in ["N", "input_size", "problem_size", "n"]:
                    if key in reader.fieldnames and row.get(key):
                        try:
                            problem_size = float(row[key])
                            break
                        except ValueError:
                            continue

                if problem_size is None:
                    continue

                # Extract cycle count
                cycles = None
                for key in ["cycles", "median_cycles", "total_cycles", "cycle_count"]:
                    if key in reader.fieldnames and row.get(key):
                        try:
                            cycles = float(row[key])
                            break
                        except ValueError:
                            continue

                if cycles is None:
                    continue

                if algorithm not in data:
                    data[algorithm] = []

                data[algorithm].append((problem_size, cycles))

    except Exception as e:
        print(f"Error reading benchmark file: {e}", file=sys.stderr)
        return {}

    # Sort each algorithm's data by problem size
    for algo in data:
        data[algo].sort(key=lambda x: x[0])

    return data


def theoretical_complexity(n: np.ndarray, algorithm: str, scale_factor: float = 1.0) -> np.ndarray:
    """Calculate theoretical cycle count based on algorithm complexity.

    Args:
        n: Array of problem sizes
        algorithm: Algorithm name (schoolbook, karatsuba, fafft, etc.)
        scale_factor: Scaling factor to align with empirical data

    Returns:
        Array of theoretical cycle counts
    """
    algo_lower = algorithm.lower()

    if "schoolbook" in algo_lower or "naive" in algo_lower:
        # O(n^2)
        return scale_factor * n ** 2

    elif "karatsuba" in algo_lower:
        # O(n^log_2(3)) ≈ O(n^1.585)
        log3_2 = np.log(3) / np.log(2)
        return scale_factor * n ** log3_2

    elif "fafft" in algo_lower or "fft" in algo_lower:
        # O(n log n) with a constant multiplier
        return scale_factor * n * np.log2(n)

    elif "hybrid" in algo_lower:
        # Hybrid: mix of Karatsuba and FFT
        # Approximate as O(n^1.7) as a compromise
        return scale_factor * n ** 1.7

    else:
        # Default: assume O(n^2)
        return scale_factor * n ** 2


def fit_scale_factor(
    n_values: np.ndarray, empirical_cycles: np.ndarray, algorithm: str
) -> float:
    """Fit scale factor to match empirical data using least squares.

    Args:
        n_values: Problem sizes
        empirical_cycles: Measured cycle counts
        algorithm: Algorithm name for complexity function

    Returns:
        Scale factor that minimizes error
    """
    if len(n_values) == 0 or len(empirical_cycles) == 0:
        return 1.0

    # Calculate theoretical complexity without scaling
    theory = theoretical_complexity(n_values, algorithm, scale_factor=1.0)

    # Fit scale factor: minimize sum((empirical - scale*theory)^2)
    # Optimal scale = sum(empirical * theory) / sum(theory^2)
    if np.sum(theory ** 2) > 0:
        scale = np.sum(empirical_cycles * theory) / np.sum(theory ** 2)
        return max(scale, 0.1)  # Prevent negative scales
    return 1.0


def plot_complexity(data: Dict[str, List[Tuple[float, float]]], output_path: Optional[Path] = None) -> None:
    """Generate complexity analysis plot.

    Args:
        data: Dictionary mapping algorithm to (problem_size, cycles) points
        output_path: Path to save the plot
    """
    if not data:
        print("Error: No data to plot", file=sys.stderr)
        return

    plt.figure(figsize=(14, 8))

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

    for idx, (algorithm, points) in enumerate(sorted(data.items())):
        if not points or len(points) < 2:
            continue

        n_values = np.array([p[0] for p in points])
        cycles = np.array([p[1] for p in points])

        color = colors[idx % len(colors)]

        # Plot empirical data
        plt.scatter(n_values, cycles, s=100, alpha=0.7, color=color, label=f"{algorithm} (empirical)", zorder=3)

        # Fit and plot theoretical curve
        scale = fit_scale_factor(n_values, cycles, algorithm)
        n_theory = np.linspace(n_values.min(), n_values.max(), 200)
        cycles_theory = theoretical_complexity(n_theory, algorithm, scale_factor=scale)

        plt.plot(
            n_theory,
            cycles_theory,
            linestyle="--",
            alpha=0.6,
            color=color,
            label=f"{algorithm} (theory: {complexity_label(algorithm)})",
            linewidth=2,
        )

    plt.xlabel("Problem Size (N)", fontsize=12, fontweight="bold")
    plt.ylabel("Cycle Count", fontsize=12, fontweight="bold")
    plt.title("Poly-Mul Complexity Analysis: Theory vs Empirical", fontsize=14, fontweight="bold")
    plt.legend(loc="best", fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.yscale("log")
    plt.xscale("log")
    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150)
        print(f"[plot_complexity] Plot saved to: {output_path}")
    else:
        plt.show()


def complexity_label(algorithm: str) -> str:
    """Get theoretical complexity label for algorithm.

    Args:
        algorithm: Algorithm name

    Returns:
        Complexity notation string
    """
    algo_lower = algorithm.lower()

    if "schoolbook" in algo_lower or "naive" in algo_lower:
        return "O(n²)"
    elif "karatsuba" in algo_lower:
        return "O(n¹·⁵⁸⁵)"
    elif "fafft" in algo_lower or "fft" in algo_lower:
        return "O(n log n)"
    elif "hybrid" in algo_lower:
        return "O(n¹·⁷)"
    else:
        return "O(n²)"


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python plot_complexity.py <benchmark_csv> [output_png]", file=sys.stderr)
        print("", file=sys.stderr)
        print("Expected CSV format:", file=sys.stderr)
        print("  Columns: algorithm, N, cycles", file=sys.stderr)
        print("           or: variant, input_size, median_cycles", file=sys.stderr)
        sys.exit(1)

    csv_path = Path(sys.argv[1])
    if not csv_path.exists():
        print(f"Error: File not found: {csv_path}", file=sys.stderr)
        sys.exit(1)

    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else None

    print(f"[plot_complexity] Parsing benchmark data: {csv_path}")
    data = parse_benchmark_file(csv_path)

    if not data:
        print("Error: No benchmark data found in file", file=sys.stderr)
        sys.exit(1)

    print(f"[plot_complexity] Found {len(data)} algorithms/variants")
    for algo, points in sorted(data.items()):
        print(f"  {algo}: {len(points)} data points")

    plot_complexity(data, output_path)


if __name__ == "__main__":
    main()
