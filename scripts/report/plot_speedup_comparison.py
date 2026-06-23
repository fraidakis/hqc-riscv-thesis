#!/usr/bin/env python3
"""
Plot: End-to-End KEM Speedup — karatsuba_zbc vs ref_scalar
Spike Instret Estimation vs Real Hardware (SpacemiT X60)

Reads speedup data from benchmark CSVs and produces a grouped bar chart
comparing Spike ISS (instret-based) and BPI-F3 hardware (cycle-based)
speedups across all three HQC security levels.

Usage:
    python3 scripts/report/plot_speedup_comparison.py
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_DIR = REPO_ROOT / "results" / "reports" / "figures"

SIM_KEM_DIR  = REPO_ROOT / "results" / "simulation" / "benchmarks" / "kem"
HW_KEM_DIR   = REPO_ROOT / "results" / "hardware"   / "benchmarks" / "kem"

SIM_REF_CSV  = SIM_KEM_DIR / "sim_kem_scalar-ref_latest.csv"
SIM_ZBC_CSV  = SIM_KEM_DIR / "sim_kem_scalar-zb_latest.csv"
HW_REF_CSV   = HW_KEM_DIR  / "hw_kem_scalar-ref_latest.csv"
HW_ZBC_CSV   = HW_KEM_DIR  / "hw_kem_scalar-zb_latest.csv"

LEVELS = [128, 192, 256]

# ---------------------------------------------------------------------------
# CSV helpers
# ---------------------------------------------------------------------------

def _read_csv(path: Path) -> list[dict[str, str]]:
    """Read a benchmark CSV, skipping comment lines starting with '#'."""
    if not path.exists():
        print(f"Error: file not found: {path}", file=sys.stderr)
        sys.exit(1)
    rows: list[dict[str, str]] = []
    with path.open(newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(line for line in fh if not line.startswith("#"))
        for row in reader:
            rows.append(row)
    return rows


def _total_instret(csv_path: Path) -> dict[int, float]:
    """Return {level: total_instructions} for op == 'total'."""
    rows = _read_csv(csv_path)
    result: dict[int, float] = {}
    for row in rows:
        if row.get("op", "").strip() == "total":
            lvl = int(row["level"])
            result[lvl] = float(row["instructions"])
    return result


def _total_cycles(csv_path: Path) -> dict[int, float]:
    """Return {level: total_median_cycles} by summing keypair+encaps+decaps."""
    rows = _read_csv(csv_path)
    totals: dict[int, float] = {}
    for row in rows:
        op = row.get("op", "").strip()
        if op in ("keypair", "encaps", "decaps"):
            lvl = int(row["level"])
            cycles = float(row["median_cycles"])
            totals[lvl] = totals.get(lvl, 0.0) + cycles
    return totals

# ---------------------------------------------------------------------------
# Compute speedups
# ---------------------------------------------------------------------------

def compute_speedups() -> tuple[list[float], list[float]]:
    sim_ref = _total_instret(SIM_REF_CSV)
    sim_zbc = _total_instret(SIM_ZBC_CSV)
    hw_ref  = _total_cycles(HW_REF_CSV)
    hw_zbc  = _total_cycles(HW_ZBC_CSV)

    spike_speedup: list[float] = []
    hw_speedup: list[float] = []

    for lvl in LEVELS:
        if lvl not in sim_ref or lvl not in sim_zbc:
            print(f"Error: missing simulation data for level {lvl}", file=sys.stderr)
            sys.exit(1)
        if lvl not in hw_ref or lvl not in hw_zbc:
            print(f"Error: missing hardware data for level {lvl}", file=sys.stderr)
            sys.exit(1)

        spike_speedup.append(sim_ref[lvl] / sim_zbc[lvl])
        hw_speedup.append(hw_ref[lvl] / hw_zbc[lvl])

    return spike_speedup, hw_speedup

# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------

def plot(spike_speedup: list[float], hw_speedup: list[float]) -> None:
    x_labels = [f"HQC-{lvl}" for lvl in LEVELS]
    x = np.arange(len(x_labels))
    bar_width = 0.32

    plt.style.use("seaborn-v0_8-whitegrid")
    fig, ax = plt.subplots(figsize=(9, 5.5))

    # Colors — academic-friendly palette
    color_spike = "#4C72B0"  # steel blue
    color_hw    = "#DD8452"  # warm orange

    bars_spike = ax.bar(
        x - bar_width / 2, spike_speedup, bar_width,
        label="Spike ISS (instret)", color=color_spike,
        edgecolor="white", linewidth=0.8, zorder=3,
    )
    bars_hw = ax.bar(
        x + bar_width / 2, hw_speedup, bar_width,
        label="Banana Pi BPI-F3 (cycles)", color=color_hw,
        edgecolor="white", linewidth=0.8, zorder=3,
    )

    # Value labels on bars
    for bar, val in zip(bars_spike, spike_speedup):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.8,
            f"\u00d7{val:.1f}",
            ha="center", va="bottom", fontsize=10,
            fontweight="bold", color=color_spike,
        )
    for bar, val in zip(bars_hw, hw_speedup):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.8,
            f"\u00d7{val:.1f}",
            ha="center", va="bottom", fontsize=10,
            fontweight="bold", color=color_hw,
        )

    ax.set_xlabel("Security Level", fontsize=11, labelpad=8)
    ax.set_ylabel("Speedup over ref_scalar (\u00d7)", fontsize=11, labelpad=8)
    ax.set_title(
        "End-to-End KEM Speedup: karatsuba_zbc vs ref_scalar\n"
        "Spike Instret Estimation vs Real Hardware (SpacemiT X60)",
        fontsize=12, fontweight="bold", pad=12,
    )
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels, fontsize=11)
    ax.set_ylim(0, max(hw_speedup) * 1.25)
    ax.yaxis.set_major_locator(plt.MultipleLocator(10))
    ax.legend(loc="upper left", fontsize=10, framealpha=0.9)
    ax.grid(axis="y", alpha=0.4, zorder=0)
    ax.grid(axis="x", visible=False)
    ax.set_axisbelow(True)

    plt.tight_layout()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out_pdf = OUTPUT_DIR / "speedup_cross_platform.pdf"
    out_png = OUTPUT_DIR / "speedup_cross_platform.png"
    fig.savefig(out_pdf, dpi=300, bbox_inches="tight")
    fig.savefig(out_png, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_pdf}")
    print(f"Saved: {out_png}")

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    spike_speedup, hw_speedup = compute_speedups()
    print("Spike speedups:", [f"{v:.2f}" for v in spike_speedup])
    print("HW speedups:   ", [f"{v:.2f}" for v in hw_speedup])
    plot(spike_speedup, hw_speedup)
