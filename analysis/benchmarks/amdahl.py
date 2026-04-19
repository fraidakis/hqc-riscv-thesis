#!/usr/bin/env python3
"""analysis/benchmarks/amdahl.py
Compute projected speedup using Amdahl's law for the vectorised HQC poly_mul.

Given a measured fraction of runtime in poly_mul and a measured speedup of
the vectorised poly_mul over the baseline, estimates the overall system speedup.

Usage:
    python3 analysis/benchmarks/amdahl.py \\
        --fraction 0.75 \\
        --speedup 8.0
"""

import argparse
import math


def amdahl_speedup(parallel_fraction: float, speedup: float) -> float:
    """Classic Amdahl's law: S = 1 / ((1 - p) + p/s)"""
    serial = 1.0 - parallel_fraction
    return 1.0 / (serial + parallel_fraction / speedup)


def main() -> None:
    parser = argparse.ArgumentParser(description="Amdahl's law calculator for HQC")
    parser.add_argument(
        "--fraction", "-p", type=float, required=True,
        help="Fraction of runtime in the parallelised section (0.0–1.0)"
    )
    parser.add_argument(
        "--speedup", "-s", type=float, required=True,
        help="Speedup factor of the vectorised section"
    )
    parser.add_argument(
        "--sweep", action="store_true",
        help="Sweep speedup values from 1x to 32x"
    )
    args = parser.parse_args()

    if args.sweep:
        print(f"{'Speedup (s)':>14} {'Overall S':>12}")
        print("-" * 28)
        for s in [1, 2, 4, 8, 16, 32, 64, 128]:
            overall = amdahl_speedup(args.fraction, s)
            print(f"{s:>14}x {overall:>11.3f}x")
    else:
        overall = amdahl_speedup(args.fraction, args.speedup)
        print(f"Parallel fraction : {args.fraction:.1%}")
        print(f"Section speedup   : {args.speedup:.1f}x")
        print(f"Overall speedup   : {overall:.3f}x")
        limit = 1.0 / (1.0 - args.fraction)
        print(f"Theoretical limit : {limit:.3f}x")


if __name__ == "__main__":
    main()
