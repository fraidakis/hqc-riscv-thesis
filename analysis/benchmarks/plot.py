#!/usr/bin/env python3
"""analysis/benchmarks/plot.py
Plot cycle count comparisons from cycle_counts.csv and instruction_mix.csv.

Usage:
    python3 analysis/benchmarks/plot.py
"""

import csv
import os
import sys

try:
    import matplotlib.pyplot as plt
    import matplotlib.ticker as mticker
except ImportError:
    sys.exit("matplotlib not installed. Run: pip install matplotlib")

SCRIPT_DIR = os.path.dirname(__file__)
CYCLE_CSV  = os.path.join(SCRIPT_DIR, "cycle_counts.csv")
MIX_CSV    = os.path.join(SCRIPT_DIR, "instruction_mix.csv")
FIGURE_DIR = os.path.join(SCRIPT_DIR, "../../docs/figures")
os.makedirs(FIGURE_DIR, exist_ok=True)


def load_csv(path: str) -> list[dict]:
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


def plot_cycle_counts() -> None:
    if not os.path.exists(CYCLE_CSV):
        print(f"[plot] {CYCLE_CSV} not found, skipping cycle-count plot.")
        return

    rows = load_csv(CYCLE_CSV)
    # Expected columns: impl, hqc128, hqc192, hqc256
    impls  = [r["impl"]  for r in rows]
    hqc128 = [int(r.get("hqc128", 0)) for r in rows]
    hqc192 = [int(r.get("hqc192", 0)) for r in rows]
    hqc256 = [int(r.get("hqc256", 0)) for r in rows]

    x = range(len(impls))
    width = 0.25
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.bar([i - width for i in x], hqc128, width, label="HQC-128")
    ax.bar([i          for i in x], hqc192, width, label="HQC-192")
    ax.bar([i + width  for i in x], hqc256, width, label="HQC-256")

    ax.set_title("Cycle Counts by Implementation")
    ax.set_xlabel("Implementation")
    ax.set_ylabel("Cycles")
    ax.set_xticks(list(x))
    ax.set_xticklabels(impls)
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{int(v):,}"))
    ax.legend()
    fig.tight_layout()
    out = os.path.join(FIGURE_DIR, "cycle_counts.pdf")
    fig.savefig(out)
    print(f"[plot] Saved: {out}")


def plot_instruction_mix() -> None:
    if not os.path.exists(MIX_CSV):
        print(f"[plot] {MIX_CSV} not found, skipping instruction-mix plot.")
        return

    rows = load_csv(MIX_CSV)
    # Expected columns: category, baseline, scalar_zbc, vector_zvbc
    categories = [r["category"]     for r in rows]
    baseline   = [float(r.get("baseline",    0)) for r in rows]
    scalar_zbc = [float(r.get("scalar_zbc",  0)) for r in rows]
    vector_zvbc= [float(r.get("vector_zvbc", 0)) for r in rows]

    x = range(len(categories))
    width = 0.25
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar([i - width for i in x], baseline,    width, label="baseline")
    ax.bar([i          for i in x], scalar_zbc,  width, label="scalar_zbc")
    ax.bar([i + width  for i in x], vector_zvbc, width, label="vector_zvbc")

    ax.set_title("Instruction Mix (% of total)")
    ax.set_xlabel("Category")
    ax.set_ylabel("Percentage (%)")
    ax.set_xticks(list(x))
    ax.set_xticklabels(categories, rotation=20, ha="right")
    ax.legend()
    fig.tight_layout()
    out = os.path.join(FIGURE_DIR, "instruction_mix.pdf")
    fig.savefig(out)
    print(f"[plot] Saved: {out}")


if __name__ == "__main__":
    plot_cycle_counts()
    plot_instruction_mix()
