#!/usr/bin/env python3
"""
Plot: Full KEM Speedup — scalar_zb & vector_rvv vs scalar_ref

Design goal: show that the Spike simulator (instret-based) PREDICTS the real
hardware speedup for the *scalar* path (delta < ~1%), but OVER-PREDICTS the
speedup for the *vector* path.

Encoding (marker-overlay, not stacked):
  - Solid bar  : real hardware speedup (BPI-F3, cycle-based)  -> the ground truth
  - Diamond + horizontal cap : Spike prediction (instret) at its own height
  - Gap bar top -> marker     : prediction error
      * |delta| <= MATCH_THRESHOLD_PCT -> green "match" badge (no gap region)
      * delta  >  MATCH_THRESHOLD_PCT  -> red hatched over-prediction region + arrow

Usage:
    python3 scripts/report/plot_speedup_comparison.py
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D
from matplotlib.patches import Patch, Rectangle

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_DIR = REPO_ROOT / "results" / "reports" / "figures"

SIM_KEM_DIR = REPO_ROOT / "results" / "simulation" / "benchmarks" / "kem"
HW_KEM_DIR  = REPO_ROOT / "results" / "hardware"   / "benchmarks" / "kem"

# Baseline (scalar_ref)
SIM_REF_CSV = SIM_KEM_DIR / "sim_kem_scalar-ref_latest.csv"
HW_REF_CSV  = HW_KEM_DIR  / "hw_kem_scalar-ref_latest.csv"

# scalar_zb
SIM_ZBC_CSV = SIM_KEM_DIR / "sim_kem_scalar-zb_latest.csv"
HW_ZBC_CSV  = HW_KEM_DIR  / "hw_kem_scalar-zb_latest.csv"

# vector_rvv
SIM_RVV_CSV = SIM_KEM_DIR / "sim_kem_vector-rvv_latest.csv"
HW_RVV_CSV  = HW_KEM_DIR  / "hw_kem_vector-rvv_latest.csv"

LEVELS = [128, 192, 256]

# ---------------------------------------------------------------------------
# Presentation config (edit display names to match the thesis controlled
# vocabulary / canonical tier naming used in intro/results/conclusion).
# ---------------------------------------------------------------------------

# Variant identity colour (encodes the *series*, kept stable across the thesis).
VARIANT_DISPLAY = {
    "scalar_zb":  "Zbc (scalar)",
    "vector_rvv": "RVV (vector)",
}
VARIANT_COLOR = {
    "scalar_zb":  "#4C72B0",  # blue
    "vector_rvv": "#DD8452",  # orange
}

# Semantic colours encode the *prediction quality* (delta), NOT the tier.
COLOR_MATCH = "#55A868"  # green  -> Spike agrees with HW
COLOR_OVER  = "#C44E52"  # red    -> Spike over-predicts
COLOR_SPIKE = "#2F2F2F"  # dark   -> Spike prediction marker

# A |delta| at or below this (percent) is treated as agreement.
MATCH_THRESHOLD_PCT = 2.0

ONE_DP = "{:.1f}"  # single, consistent decimal precision everywhere

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
# Compute speedups (both variants vs scalar_ref)
# ---------------------------------------------------------------------------

def compute_speedups() -> dict[str, tuple[list[float], list[float]]]:
    """
    Returns dict: variant -> (spike_speedups, hw_speedups)
    Both measured against scalar_ref baseline on full KEM.
    """
    sim_ref = _total_instret(SIM_REF_CSV)
    hw_ref  = _total_cycles(HW_REF_CSV)

    sim_zbc = _total_instret(SIM_ZBC_CSV)
    hw_zbc  = _total_cycles(HW_ZBC_CSV)

    sim_rvv = _total_instret(SIM_RVV_CSV)
    hw_rvv  = _total_cycles(HW_RVV_CSV)

    results: dict[str, tuple[list[float], list[float]]] = {}

    for name, sim_opt, hw_opt in [
        ("scalar_zb",  sim_zbc, hw_zbc),
        ("vector_rvv", sim_rvv, hw_rvv),
    ]:
        spike_sp: list[float] = []
        hw_sp: list[float] = []
        for lvl in LEVELS:
            if lvl not in sim_ref or lvl not in sim_opt:
                print(f"Error: missing simulation data for {name} level {lvl}", file=sys.stderr)
                sys.exit(1)
            if lvl not in hw_ref or lvl not in hw_opt:
                print(f"Error: missing hardware data for {name} level {lvl}", file=sys.stderr)
                sys.exit(1)
            spike_sp.append(sim_ref[lvl] / sim_opt[lvl])
            hw_sp.append(hw_ref[lvl] / hw_opt[lvl])
        results[name] = (spike_sp, hw_sp)

    return results

# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------

def plot(speedups: dict[str, tuple[list[float], list[float]]]) -> None:
    n_levels = len(LEVELS)
    variant_names = list(speedups.keys())

    x_labels = [f"HQC-{lvl}" for lvl in LEVELS]
    x = np.arange(n_levels)
    bar_width = 0.34
    offsets = [-bar_width / 2 - 0.02, bar_width / 2 + 0.02]
    cap_half = bar_width * 0.52  # spike marker cap half-width

    plt.style.use("seaborn-v0_8-whitegrid")
    fig, ax = plt.subplots(figsize=(10, 6.5))

    all_tops: list[float] = []
    saw_over = False
    saw_match = False

    for v_idx, variant in enumerate(variant_names):
        spike_sp, hw_sp = speedups[variant]
        color = VARIANT_COLOR[variant]
        offset = offsets[v_idx]

        # Ground-truth hardware bars.
        ax.bar(
            x + offset, hw_sp, bar_width,
            color=color, edgecolor="white", linewidth=0.8, zorder=3,
        )

        for i in range(n_levels):
            s_val = spike_sp[i]
            h_val = hw_sp[i]
            bar_x = x[i] + offset
            delta_pct = (s_val - h_val) / h_val * 100.0
            is_match = abs(delta_pct) <= MATCH_THRESHOLD_PCT
            top_y = max(s_val, h_val)
            all_tops.append(top_y)

            # HW value inside the solid bar.
            ax.text(
                bar_x, h_val / 2,
                ("\u00d7" + ONE_DP).format(h_val),
                ha="center", va="center", fontsize=9.5,
                fontweight="bold", color="white", zorder=6,
            )

            # Over-prediction gap region (only when Spike clearly exceeds HW).
            if not is_match and s_val > h_val:
                saw_over = True
                ax.add_patch(Rectangle(
                    (bar_x - bar_width / 2, h_val), bar_width, s_val - h_val,
                    facecolor=COLOR_OVER, alpha=0.16, edgecolor=COLOR_OVER,
                    linewidth=0.8, hatch="////", zorder=2,
                ))
                # Double-headed arrow spanning the error.
                ax.annotate(
                    "", xy=(bar_x, s_val), xytext=(bar_x, h_val),
                    arrowprops=dict(arrowstyle="<->", color=COLOR_OVER, lw=1.4),
                    zorder=5,
                )
            else:
                saw_match = True

            # Spike prediction marker: horizontal cap + diamond at its height.
            ax.plot(
                [bar_x - cap_half, bar_x + cap_half], [s_val, s_val],
                color=COLOR_SPIKE, lw=1.8, zorder=7, solid_capstyle="round",
            )
            ax.plot(
                bar_x, s_val, marker="D", markersize=7,
                markerfacecolor="white", markeredgecolor=COLOR_SPIKE,
                markeredgewidth=1.6, zorder=8,
            )

            # Delta badge above the marker, coloured by prediction quality.
            if is_match:
                badge_txt = ("Spike \u00d7" + ONE_DP + "  match ({:+.1f}%)").format(
                    s_val, delta_pct,
                )
                badge_color = COLOR_MATCH
            else:
                badge_txt = ("Spike \u00d7" + ONE_DP + "  over +{:.1f}%").format(
                    s_val, delta_pct,
                )
                badge_color = COLOR_OVER
            ax.text(
                bar_x, s_val + 1.4, badge_txt,
                ha="center", va="bottom", fontsize=8, fontweight="bold",
                color="white",
                bbox=dict(boxstyle="round,pad=0.28", facecolor=badge_color,
                          edgecolor="none", alpha=0.92),
                zorder=9,
            )

    # Axes & titles.
    ax.set_xlabel("Security level", fontsize=11, labelpad=8)
    ax.set_ylabel("Full-KEM speedup over scalar_ref (\u00d7)", fontsize=11, labelpad=8)
    ax.set_title(
        "Spike (instret) tracks hardware for the scalar path, "
        "but over-predicts the vector speedup",
        fontsize=12.5, fontweight="bold", pad=14,
    )
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels, fontsize=11)

    y_max = max(all_tops) * 1.22
    ax.set_ylim(0, y_max)
    ax.yaxis.set_major_locator(plt.MultipleLocator(10))
    ax.grid(axis="y", alpha=0.30, zorder=0)
    ax.grid(axis="x", visible=False)
    ax.set_axisbelow(True)

    # Lean, semantic legend built from proxy handles.
    handles: list = [
        Patch(facecolor=VARIANT_COLOR[v], edgecolor="white",
              label=f"{VARIANT_DISPLAY[v]} \u2014 BPI-F3 hardware (cycles)")
        for v in variant_names
    ]
    handles.append(Line2D(
        [0], [0], color=COLOR_SPIKE, lw=1.8, marker="D",
        markerfacecolor="white", markeredgecolor=COLOR_SPIKE, markersize=7,
        label="Spike prediction (instret)",
    ))
    if saw_over:
        handles.append(Patch(
            facecolor=COLOR_OVER, alpha=0.16, edgecolor=COLOR_OVER,
            hatch="////", label="Spike over-prediction (gap)",
        ))
    if saw_match:
        handles.append(Patch(
            facecolor=COLOR_MATCH, edgecolor="none",
            label=f"Spike \u2248 hardware (|\u0394| \u2264 {MATCH_THRESHOLD_PCT:.0f}%)",
        ))
    ax.legend(handles=handles, loc="upper left", fontsize=9, framealpha=0.92,
              ncol=1, borderpad=0.8)

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
    speedups = compute_speedups()
    for variant, (spike_sp, hw_sp) in speedups.items():
        print(f"{variant}:")
        print(f"  Spike speedups: {[f'{v:.2f}' for v in spike_sp]}")
        print(f"  HW speedups:    {[f'{v:.2f}' for v in hw_sp]}")
        deltas = [(s - h) / h * 100 for s, h in zip(spike_sp, hw_sp)]
        print(f"  Spike extra:    {[f'{d:+.2f}%' for d in deltas]}")
    plot(speedups)