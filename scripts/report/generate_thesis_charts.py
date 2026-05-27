#!/usr/bin/env python3
"""Generate publication-quality PDF charts for the HQC RISC-V diploma thesis.

Consolidated chart generation script that produces 10 distinct chart types
as PDF figures suitable for LaTeX inclusion. Reads benchmark CSV data from
the analysis/ directory tree.

Usage:
    python3 scripts/report/generate_thesis_charts.py --all
    python3 scripts/report/generate_thesis_charts.py --chart three_stage_pie
    python3 scripts/report/generate_thesis_charts.py --list
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Callable

import matplotlib

matplotlib.use("pdf")

import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_DIR = REPO_ROOT / "analysis" / "reports" / "charts"

CHART_REGISTRY: dict[str, Callable] = {}

# ---------------------------------------------------------------------------
# Matplotlib Configuration
# ---------------------------------------------------------------------------

plt.rcParams.update(
    {
        "font.family": "serif",
        "font.serif": ["Libertinus Serif", "Computer Modern", "DejaVu Serif"],
        "font.size": 10,
        "axes.labelsize": 10,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
    }
)

# ---------------------------------------------------------------------------
# Category Classification (absorbed from generate_pie_charts.py)
# ---------------------------------------------------------------------------

CATEGORY_MAP: dict[str, str] = {
    # Multiplication patterns
    "schoolbook_mul": "Multiplication",
    "clmul": "Multiplication",
    "vclmul": "Multiplication",
    "kmul_": "Multiplication",
    "gf_reduce": "Multiplication",
    "gf_mul": "Multiplication",
    "gf_square": "Multiplication",
    "vect_mul": "Multiplication",
    "karatsuba": "Multiplication",
    # Keccak/Hashing patterns
    "Keccak": "Keccak/Hashing",
    "keccak": "Keccak/Hashing",
    "sha3_": "Keccak/Hashing",
    "shake": "Keccak/Hashing",
    "hash_": "Keccak/Hashing",
    "xof_": "Keccak/Hashing",
    # Decode patterns
    "reed_": "Decode",
    "fft": "Decode",
    "hadamard": "Decode",
    "find_peaks": "Decode",
    "compute_syndromes": "Decode",
    "compute_elp": "Decode",
    "compute_error_values": "Decode",
    "compute_z_poly": "Decode",
    "fft_retrieve_error_poly": "Decode",
    "correct_errors": "Decode",
    "code_decode": "Decode",
    "compute_roots": "Decode",
    "encode": "Decode",
    "code_encode": "Decode",
    "reed_muller": "Decode",
    # Memory/Utility patterns
    "memset": "Memory/Utility",
    "memcpy": "Memory/Utility",
    "rvv_xor": "Memory/Utility",
    "rvv_copy": "Memory/Utility",
    "rvv_xor3": "Memory/Utility",
    "vect_add": "Memory/Utility",
    "vect_compare": "Memory/Utility",
    "store64": "Memory/Utility",
    "x2_load64": "Memory/Utility",
}

CATEGORY_COLORS: dict[str, str] = {
    "Multiplication": "#e63946",
    "Keccak/Hashing": "#457b9d",
    "Decode": "#2a9d8f",
    "Memory/Utility": "#e9c46a",
    "Other": "#adb5bd",
}


# ---------------------------------------------------------------------------
# Helper Functions
# ---------------------------------------------------------------------------


def classify_function(name: str) -> str:
    """Return the category for a function name using substring matching.

    Iterates over CATEGORY_MAP items; if a pattern is found as a substring
    in the function name, returns the corresponding category. If no pattern
    matches, returns "Other".
    """
    for pattern, category in CATEGORY_MAP.items():
        if pattern in name:
            return category
    return "Other"


def categorize_entries(df: pd.DataFrame) -> dict[str, int]:
    """Aggregate instruction counts by category from a trace DataFrame.

    Expects columns: function, instructions.
    Returns dict mapping category name to total instruction count.
    """
    totals: dict[str, int] = {}
    for _, row in df.iterrows():
        category = classify_function(row["function"])
        totals[category] = totals.get(category, 0) + int(row["instructions"])
    return totals


def _autopct_threshold(threshold: float = 3.0):
    """Return an autopct function that suppresses labels for slices below threshold%.

    For any percentage value below threshold, returns an empty string.
    For any percentage value >= threshold, returns a formatted percentage string.
    """

    def _fmt(pct: float) -> str:
        return f"{pct:.1f}%" if pct >= threshold else ""

    return _fmt


def load_csv(path: Path) -> pd.DataFrame | None:
    """Parse a CSV file, skipping lines starting with '#'.

    Uses pd.read_csv with comment='#' to handle preamble lines.
    Returns None and prints error to stderr if file does not exist.
    """
    if not path.exists():
        print(f"Error: CSV file not found: {path}", file=sys.stderr)
        return None
    return pd.read_csv(path, comment="#")


def variant_slug(variant: str) -> str:
    """Convert a variant name from underscore form to hyphen form for filenames.

    Example: 'karatsuba_zbc' → 'karatsuba-zbc'
    """
    return variant.replace("_", "-")


# ---------------------------------------------------------------------------
# DataLoader
# ---------------------------------------------------------------------------


class DataLoader:
    """Central data layer that resolves CSV paths and provides parsed DataFrames."""

    def __init__(self, repo_root: Path) -> None:
        """Resolve all CSV paths relative to repo_root."""
        self._root = repo_root

    def trace_hotspots(self, track: str, variant: str, level: int) -> pd.DataFrame | None:
        """Load trace hotspot CSV for a given track/variant/level.

        Returns DataFrame with columns: function, instructions, percentage.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "analysis"
            / "profiles"
            / "traces"
            / track
            / variant
            / f"trace_kem_L{level}_{slug}_hotspots_latest.csv"
        )
        return load_csv(path)

    def isolated_kem(self, track: str, variant: str) -> pd.DataFrame | None:
        """Load isolated KEM benchmark CSV.

        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "analysis"
            / "benchmarks"
            / "isolated"
            / track
            / variant
            / f"isolated_kem_{slug}_latest.csv"
        )
        return load_csv(path)

    def isolated_polymul(self, track: str, variant: str) -> pd.DataFrame | None:
        """Load isolated polymul benchmark CSV.

        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "analysis"
            / "benchmarks"
            / "isolated"
            / track
            / variant
            / f"isolated_polymul_{slug}_latest.csv"
        )
        return load_csv(path)

    def isolated_keccak(self, variant: str) -> pd.DataFrame | None:
        """Load isolated keccak benchmark CSV.

        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        path = (
            self._root
            / "analysis"
            / "benchmarks"
            / "isolated"
            / "keccak"
            / f"isolated_keccak_{variant}_latest.csv"
        )
        return load_csv(path)

    def isolated_decode(self, variant: str) -> pd.DataFrame | None:
        """Load isolated decode benchmark CSV.

        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        path = (
            self._root
            / "analysis"
            / "benchmarks"
            / "isolated"
            / "decode"
            / f"isolated_decode_{variant}_latest.csv"
        )
        return load_csv(path)

    def vlen_sweep(self, variant: str, level: int) -> pd.DataFrame | None:
        """Load VLEN sweep CSV for a given variant/level.

        Returns DataFrame with columns: op, vlen, instructions.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "analysis"
            / "benchmarks"
            / "vlen_sweep"
            / "vector"
            / variant
            / f"vlen_sweep_L{level}_{slug}_latest.csv"
        )
        return load_csv(path)

    def compiler_compare(self, track: str, variant: str) -> pd.DataFrame | None:
        """Load compiler comparison CSV.

        Returns DataFrame with columns: compiler, opt, level, variant, status,
        instructions, text_bytes, elf, error.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "analysis"
            / "benchmarks"
            / "compiler_compare"
            / track
            / variant
            / f"compiler_compare_Lall_{slug}_latest.csv"
        )
        return load_csv(path)


# ---------------------------------------------------------------------------
# Chart Registration Decorator
# ---------------------------------------------------------------------------


def register_chart(name: str) -> Callable:
    """Decorator that registers a chart function under the given name.

    The decorated function is added to CHART_REGISTRY[name] and returned
    unchanged.
    """

    def decorator(func: Callable) -> Callable:
        CHART_REGISTRY[name] = func
        return func

    return decorator


# ---------------------------------------------------------------------------
# Chart Functions
# ---------------------------------------------------------------------------


@register_chart("three_stage_pie")
def chart_three_stage_pie(loader: DataLoader, level: int) -> None:
    """Render three side-by-side pie charts showing instruction distribution evolution.

    Panels: Baseline_Scalar (ref_scalar), Tier_2 (karatsuba_rvv), Tier_3 (karatsuba_zvbc).
    Uses categorize_entries() for aggregation and CATEGORY_COLORS for consistent coloring.
    """
    # Define the three stages: (title, track, variant)
    stages = [
        ("Baseline (Scalar)", "scalar", "ref_scalar"),
        ("Tier 2 (Karatsuba RVV)", "vector", "karatsuba_rvv"),
        ("Tier 3 (Karatsuba Zvbc)", "vector", "karatsuba_zvbc"),
    ]

    # Load DataFrames for each stage
    dataframes = []
    for title, track, variant in stages:
        df = loader.trace_hotspots(track, variant, level)
        if df is None:
            print(
                f"Warning: Missing trace data for {variant} at level {level}, "
                f"skipping three_stage_pie chart.",
                file=sys.stderr,
            )
            return
        dataframes.append(df)

    # Create figure with three side-by-side panels
    fig, axes = plt.subplots(1, 3, figsize=(12, 4))

    for ax, df, (title, _, _) in zip(axes, dataframes, stages):
        # Aggregate by category
        category_totals = categorize_entries(df)

        # Sort categories for consistent ordering and filter out zero values
        categories = [c for c in CATEGORY_COLORS if c in category_totals and category_totals[c] > 0]
        sizes = [category_totals[c] for c in categories]
        colors = [CATEGORY_COLORS[c] for c in categories]

        ax.pie(
            sizes,
            labels=categories,
            colors=colors,
            autopct=_autopct_threshold(),
            startangle=90,
        )
        ax.set_title(title)

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "pie_three_stage_comparison.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


@register_chart("kem_speedup_table_chart")
def chart_kem_speedup_table_chart(loader: DataLoader, level: int) -> None:
    """Render grouped bar chart showing total KEM speedups across security levels.

    X-axis groups: HQC-128, HQC-192, HQC-256.
    Bars per group: Tier_1, Tier_2, Tier_3.
    Y-axis: speedup factor (×) relative to Baseline_Scalar.
    Each bar is annotated with its numeric speedup value.
    The `level` parameter is unused since this chart always shows all three levels.
    """
    levels = [128, 192, 256]
    tier_configs = [
        ("Tier 1", "scalar", "karatsuba_zbc"),
        ("Tier 2", "vector", "karatsuba_rvv"),
        ("Tier 3", "vector", "karatsuba_zvbc"),
    ]

    # Load baseline data
    baseline_df = loader.isolated_kem("scalar", "ref_scalar")
    if baseline_df is None or baseline_df.empty:
        print(
            "Warning: Missing baseline (ref_scalar) KEM data, "
            "skipping kem_speedup_table_chart.",
            file=sys.stderr,
        )
        return

    # Compute speedups for each level and tier
    speedups: dict[str, list[float]] = {name: [] for name, _, _ in tier_configs}

    for lvl in levels:
        # Get baseline instret for this level (op == "total")
        baseline_row = baseline_df[
            (baseline_df["level"] == lvl) & (baseline_df["op"] == "total")
        ]
        if baseline_row.empty:
            print(
                f"Warning: No baseline total KEM data for level {lvl}, "
                f"skipping kem_speedup_table_chart.",
                file=sys.stderr,
            )
            return
        baseline_instret = baseline_row["instructions"].iloc[0]

        for tier_name, track, variant in tier_configs:
            tier_df = loader.isolated_kem(track, variant)
            if tier_df is None or tier_df.empty:
                print(
                    f"Warning: Missing KEM data for {variant}, "
                    f"skipping kem_speedup_table_chart.",
                    file=sys.stderr,
                )
                return
            tier_row = tier_df[
                (tier_df["level"] == lvl) & (tier_df["op"] == "total")
            ]
            if tier_row.empty:
                print(
                    f"Warning: No total KEM data for {variant} at level {lvl}, "
                    f"skipping kem_speedup_table_chart.",
                    file=sys.stderr,
                )
                return
            tier_instret = tier_row["instructions"].iloc[0]
            speedup = baseline_instret / tier_instret
            speedups[tier_name].append(speedup)

    # Create grouped bar chart
    x_labels = [f"HQC-{lvl}" for lvl in levels]
    x = np.arange(len(x_labels))
    n_bars = len(tier_configs)
    bar_width = 0.25

    fig, ax = plt.subplots(figsize=(8, 5))

    for i, (tier_name, _, _) in enumerate(tier_configs):
        offset = (i - (n_bars - 1) / 2) * bar_width
        bars = ax.bar(x + offset, speedups[tier_name], bar_width, label=tier_name)
        # Annotate each bar with speedup value
        for bar, val in zip(bars, speedups[tier_name]):
            ax.annotate(
                f"{val:.2f}×",
                xy=(bar.get_x() + bar.get_width() / 2, bar.get_height()),
                xytext=(0, 3),
                textcoords="offset points",
                ha="center",
                va="bottom",
                fontsize=8,
            )

    ax.set_xlabel("Security Level")
    ax.set_ylabel("Speedup (×)")
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.legend()
    ax.set_ylim(bottom=0)

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "bar_kem_speedups.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# Chart: grouped_bar_micro
# ---------------------------------------------------------------------------


@register_chart("grouped_bar_micro")
def chart_grouped_bar_micro(loader: DataLoader, level: int) -> None:
    """Render grouped bar chart comparing isolated micro-benchmarks.

    Groups: polymul (Baseline/Tier_1/Tier_2/Tier_3), keccak (Baseline/Optimized),
    decode (Baseline/RVV/Zvbc). Uses logarithmic y-axis and annotates optimized
    bars with speedup factors.
    """
    # --- Load polymul data ---
    polymul_baseline = loader.isolated_polymul("scalar", "ref_scalar")
    polymul_tier1 = loader.isolated_polymul("scalar", "karatsuba_zbc")
    polymul_tier2 = loader.isolated_polymul("vector", "karatsuba_rvv")
    polymul_tier3 = loader.isolated_polymul("vector", "karatsuba_zvbc")

    # --- Load keccak data ---
    keccak_baseline = loader.isolated_keccak("ref")
    keccak_opt = loader.isolated_keccak("opt")

    # --- Load decode data ---
    decode_baseline = loader.isolated_decode("ref")
    decode_rvv = loader.isolated_decode("rvv")
    decode_zvbc = loader.isolated_decode("zvbc")

    # Check that at least some data is available
    if all(
        df is None
        for df in [
            polymul_baseline, polymul_tier1, polymul_tier2, polymul_tier3,
            keccak_baseline, keccak_opt,
            decode_baseline, decode_rvv, decode_zvbc,
        ]
    ):
        print("Warning: No micro-benchmark data found, skipping grouped_bar_micro.", file=sys.stderr)
        return

    # --- Extract instret values for the requested level ---
    def get_instret(df: pd.DataFrame | None, level: int) -> float | None:
        """Extract instruction count for the given level from a DataFrame."""
        if df is None or df.empty:
            return None
        filtered = df[df["level"] == level]
        if filtered.empty:
            return None
        return float(filtered["instructions"].iloc[0])

    # Polymul values
    pm_base = get_instret(polymul_baseline, level)
    pm_t1 = get_instret(polymul_tier1, level)
    pm_t2 = get_instret(polymul_tier2, level)
    pm_t3 = get_instret(polymul_tier3, level)

    # Keccak values
    kc_base = get_instret(keccak_baseline, level)
    kc_opt = get_instret(keccak_opt, level)

    # Decode values
    dc_base = get_instret(decode_baseline, level)
    dc_rvv = get_instret(decode_rvv, level)
    dc_zvbc = get_instret(decode_zvbc, level)

    # Check we have at least baseline data for each group
    if pm_base is None and kc_base is None and dc_base is None:
        print("Warning: No baseline data available for any group, skipping grouped_bar_micro.", file=sys.stderr)
        return

    # --- Build bar data ---
    # Each group: (group_label, [(bar_label, value, is_baseline), ...])
    groups: list[tuple[str, list[tuple[str, float | None, bool]]]] = [
        ("Polymul", [
            ("Baseline", pm_base, True),
            ("Tier 1\n(Zbc)", pm_t1, False),
            ("Tier 2\n(RVV)", pm_t2, False),
            ("Tier 3\n(Zvbc)", pm_t3, False),
        ]),
        ("Keccak", [
            ("Baseline", kc_base, True),
            ("Optimized", kc_opt, False),
        ]),
        ("Decode", [
            ("Baseline", dc_base, True),
            ("RVV", dc_rvv, False),
            ("Zvbc", dc_zvbc, False),
        ]),
    ]

    # --- Compute bar positions ---
    bar_width = 0.6
    gap_within_group = 0.15
    gap_between_groups = 1.5

    positions: list[float] = []
    labels: list[str] = []
    values: list[float] = []
    colors: list[str] = []
    speedups: list[str | None] = []

    tier_colors = {
        "Baseline": "#6c757d",
        "Tier 1\n(Zbc)": "#e63946",
        "Tier 2\n(RVV)": "#457b9d",
        "Tier 3\n(Zvbc)": "#2a9d8f",
        "Optimized": "#2a9d8f",
        "RVV": "#457b9d",
        "Zvbc": "#2a9d8f",
    }

    x = 0.0
    group_centers: list[tuple[float, str]] = []

    for group_label, bars in groups:
        group_start = x
        for bar_label, value, is_baseline in bars:
            if value is not None:
                positions.append(x)
                labels.append(bar_label)
                values.append(value)
                colors.append(tier_colors.get(bar_label, "#adb5bd"))

                # Compute speedup for non-baseline bars
                if not is_baseline:
                    # Find baseline for this group
                    baseline_val = bars[0][1]  # first bar is always baseline
                    if baseline_val is not None and value > 0:
                        sp = baseline_val / value
                        speedups.append(f"{sp:.1f}×")
                    else:
                        speedups.append(None)
                else:
                    speedups.append(None)

                x += bar_width + gap_within_group
            else:
                # Skip missing data bars
                x += bar_width + gap_within_group

        group_end = x - gap_within_group
        group_centers.append(((group_start + group_end) / 2, group_label))
        x += gap_between_groups

    if not values:
        print("Warning: No valid data to plot for grouped_bar_micro.", file=sys.stderr)
        return

    # --- Create figure ---
    fig, ax = plt.subplots(figsize=(10, 5))

    bars_rendered = ax.bar(positions, values, width=bar_width, color=colors, edgecolor="black", linewidth=0.5)

    # Logarithmic y-axis
    ax.set_yscale("log")
    ax.set_ylabel("Instructions (Instret)")
    ax.set_title(f"Isolated Micro-benchmark Comparison (HQC-{level})")

    # Set x-tick labels for individual bars
    ax.set_xticks(positions)
    ax.set_xticklabels(labels, fontsize=7)

    # Add group labels below
    for center, group_label in group_centers:
        ax.text(center, -0.12, group_label, transform=ax.get_xaxis_transform(),
                ha="center", va="top", fontsize=9, fontweight="bold")

    # Annotate speedup factors above optimized bars
    for bar, sp in zip(bars_rendered, speedups):
        if sp is not None:
            ax.annotate(
                sp,
                xy=(bar.get_x() + bar.get_width() / 2, bar.get_height()),
                xytext=(0, 4),
                textcoords="offset points",
                ha="center",
                va="bottom",
                fontsize=7,
                fontweight="bold",
                color="#2a9d8f",
            )

    ax.grid(axis="y", alpha=0.3, which="both")
    ax.set_axisbelow(True)

    # Remove top and right spines
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    plt.tight_layout()
    plt.subplots_adjust(bottom=0.18)

    # --- Save ---
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUTPUT_DIR / "bar_micro_benchmarks.pdf"
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {out_path}")


# ---------------------------------------------------------------------------
# Chart: vlen_sweep
# ---------------------------------------------------------------------------


@register_chart("vlen_sweep")
def chart_vlen_sweep(loader: DataLoader, level: int) -> None:
    """Render line chart showing how total KEM Instret varies with VLEN.

    Plots separate lines for HQC-128, HQC-192, HQC-256 using Tier_3
    (karatsuba_zvbc) data. Marks the minimum Instret point on each line
    with a distinct marker. The `level` parameter is unused since this
    chart always shows all three security levels.
    """
    levels = [128, 192, 256]
    line_styles = {
        128: {"color": "#e63946", "marker": "o", "label": "HQC-128"},
        192: {"color": "#457b9d", "marker": "s", "label": "HQC-192"},
        256: {"color": "#2a9d8f", "marker": "^", "label": "HQC-256"},
    }
    min_markers = {
        128: {"marker": "*", "color": "#e63946", "size": 200},
        192: {"marker": "D", "color": "#457b9d", "size": 120},
        256: {"marker": "P", "color": "#2a9d8f", "size": 150},
    }

    fig, ax = plt.subplots(figsize=(8, 5))
    any_plotted = False

    for lvl in levels:
        df = loader.vlen_sweep("karatsuba_zvbc", lvl)
        if df is None or df.empty:
            print(
                f"Warning: Missing VLEN sweep data for karatsuba_zvbc at level {lvl}, "
                f"skipping this line.",
                file=sys.stderr,
            )
            continue

        # Filter for total KEM operation
        total_df = df[df["op"] == "total"].copy()
        if total_df.empty:
            print(
                f"Warning: No 'total' op data in VLEN sweep for level {lvl}, "
                f"skipping this line.",
                file=sys.stderr,
            )
            continue

        total_df = total_df.sort_values("vlen")
        vlens = total_df["vlen"].values
        instrets = total_df["instructions"].values

        style = line_styles[lvl]
        ax.plot(
            vlens,
            instrets,
            color=style["color"],
            marker=style["marker"],
            label=style["label"],
            linewidth=1.5,
            markersize=6,
        )

        # Mark the minimum Instret point with a distinct marker
        min_idx = np.argmin(instrets)
        mm = min_markers[lvl]
        ax.scatter(
            vlens[min_idx],
            instrets[min_idx],
            marker=mm["marker"],
            color=mm["color"],
            s=mm["size"],
            zorder=5,
            edgecolors="black",
            linewidths=0.5,
        )

        any_plotted = True

    if not any_plotted:
        print(
            "Warning: No VLEN sweep data available for any level, "
            "skipping vlen_sweep chart.",
            file=sys.stderr,
        )
        plt.close(fig)
        return

    ax.set_xlabel("VLEN")
    ax.set_ylabel("Instructions (Instret)")
    ax.set_title("Total KEM Instret vs VLEN (Tier 3 — karatsuba_zvbc)")
    ax.set_xticks([256, 512, 1024, 2048])
    ax.get_xaxis().set_major_formatter(plt.FuncFormatter(lambda x, _: f"{int(x)}"))
    ax.legend()
    ax.grid(alpha=0.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "line_vlen_sweep.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# Chart: security_scaling
# ---------------------------------------------------------------------------


@register_chart("security_scaling")
def chart_security_scaling(loader: DataLoader, level: int) -> None:
    """Render line chart showing total KEM instret scaling across security levels.

    X-axis: security levels [128, 192, 256].
    Lines: Baseline_Scalar, Tier_1, Tier_2, Tier_3 total KEM instret.
    Y-axis: logarithmic scale.
    Includes a dashed O(N²) reference line normalized to baseline at level 128.
    The `level` parameter is unused since this chart always shows all three levels.
    """
    levels = [128, 192, 256]
    tier_configs = [
        ("Baseline (Scalar)", "scalar", "ref_scalar"),
        ("Tier 1 (Zbc)", "scalar", "karatsuba_zbc"),
        ("Tier 2 (RVV)", "vector", "karatsuba_rvv"),
        ("Tier 3 (Zvbc)", "vector", "karatsuba_zvbc"),
    ]

    # Load baseline data first to check availability
    baseline_df = loader.isolated_kem("scalar", "ref_scalar")
    if baseline_df is None or baseline_df.empty:
        print(
            "Warning: Missing baseline (ref_scalar) KEM data, "
            "skipping security_scaling chart.",
            file=sys.stderr,
        )
        return

    # Collect instret values for each tier across all levels
    tier_data: dict[str, list[float]] = {}

    for tier_name, track, variant in tier_configs:
        df = loader.isolated_kem(track, variant)
        if df is None or df.empty:
            print(
                f"Warning: Missing KEM data for {variant}, "
                f"skipping security_scaling chart.",
                file=sys.stderr,
            )
            return

        values = []
        for lvl in levels:
            row = df[(df["level"] == lvl) & (df["op"] == "total")]
            if row.empty:
                print(
                    f"Warning: No total KEM data for {variant} at level {lvl}, "
                    f"skipping security_scaling chart.",
                    file=sys.stderr,
                )
                return
            values.append(float(row["instructions"].iloc[0]))
        tier_data[tier_name] = values

    # --- Create figure ---
    fig, ax = plt.subplots(figsize=(7, 5))

    tier_colors_line = {
        "Baseline (Scalar)": "#6c757d",
        "Tier 1 (Zbc)": "#e63946",
        "Tier 2 (RVV)": "#457b9d",
        "Tier 3 (Zvbc)": "#2a9d8f",
    }
    tier_markers = {
        "Baseline (Scalar)": "o",
        "Tier 1 (Zbc)": "s",
        "Tier 2 (RVV)": "^",
        "Tier 3 (Zvbc)": "D",
    }

    for tier_name, _, _ in tier_configs:
        ax.plot(
            levels,
            tier_data[tier_name],
            marker=tier_markers[tier_name],
            color=tier_colors_line[tier_name],
            label=tier_name,
            linewidth=1.5,
            markersize=6,
        )

    # Add O(N²) reference line normalized to baseline at level 128
    ref_128 = tier_data["Baseline (Scalar)"][0]
    ref_values = [ref_128 * (n / 128) ** 2 for n in levels]
    ax.plot(
        levels,
        ref_values,
        linestyle="--",
        color="grey",
        label="O(N²) reference",
        linewidth=1.2,
    )

    ax.set_yscale("log")
    ax.set_xlabel("Security Level")
    ax.set_ylabel("Total KEM Instret")
    ax.set_xticks(levels)
    ax.set_xticklabels([str(lvl) for lvl in levels])
    ax.legend()
    ax.grid(True, alpha=0.3, which="both")

    fig.tight_layout()

    # --- Save ---
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUTPUT_DIR / "line_security_scaling.pdf"
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {out_path}")


@register_chart("compiler_comparison")
def chart_compiler_comparison(loader: DataLoader, level: int) -> None:
    """Render grouped bar chart comparing Clang vs GCC for each variant.

    X-axis groups: ref_scalar, karatsuba_zbc, karatsuba_rvv, karatsuba_zvbc.
    Two bars per group: best Clang result and best GCC result for total KEM Instret.
    SKIP entries are rendered with hatched pattern and grey color.
    Each pair is annotated with the winning compiler and margin percentage.
    """
    # Variant configurations: (variant_name, track)
    variant_configs = [
        ("ref_scalar", "scalar"),
        ("karatsuba_zbc", "scalar"),
        ("karatsuba_rvv", "vector"),
        ("karatsuba_zvbc", "vector"),
    ]

    # Collect best results per compiler per variant
    clang_values: list[float | None] = []
    gcc_values: list[float | None] = []
    clang_skip: list[bool] = []
    gcc_skip: list[bool] = []
    x_labels: list[str] = []

    for variant, track in variant_configs:
        df = loader.compiler_compare(track, variant)
        if df is None or df.empty:
            print(
                f"Warning: Missing compiler comparison data for {variant}, "
                f"skipping compiler_comparison chart.",
                file=sys.stderr,
            )
            return

        # Filter by requested level
        df_level = df[df["level"] == level]
        if df_level.empty:
            print(
                f"Warning: No data for level {level} in {variant}, "
                f"skipping compiler_comparison chart.",
                file=sys.stderr,
            )
            return

        # Find best Clang result (lowest instructions where status != SKIP)
        clang_rows = df_level[df_level["compiler"] == "clang"]
        clang_ok = clang_rows[clang_rows["status"] != "SKIP"]
        if clang_ok.empty:
            # All Clang entries are SKIP
            clang_values.append(None)
            clang_skip.append(True)
        else:
            best_clang = clang_ok["instructions"].min()
            clang_values.append(float(best_clang))
            clang_skip.append(False)

        # Find best GCC result (lowest instructions where status != SKIP)
        gcc_rows = df_level[df_level["compiler"] == "gcc"]
        gcc_ok = gcc_rows[gcc_rows["status"] != "SKIP"]
        if gcc_ok.empty:
            # All GCC entries are SKIP
            gcc_values.append(None)
            gcc_skip.append(True)
        else:
            best_gcc = gcc_ok["instructions"].min()
            gcc_values.append(float(best_gcc))
            gcc_skip.append(False)

        # Format variant name for display
        label = variant.replace("_", " ").replace("ref scalar", "Ref Scalar")
        label = label.title() if "ref" not in variant else label
        x_labels.append(variant.replace("_", "\\_") if False else variant.replace("_", " ").title())

    # Nicer labels
    x_labels = ["Ref Scalar", "Karatsuba Zbc", "Karatsuba RVV", "Karatsuba Zvbc"]

    # Create grouped bar chart
    x = np.arange(len(x_labels))
    bar_width = 0.35

    fig, ax = plt.subplots(figsize=(9, 5))

    # Colors
    clang_color = "#e63946"
    gcc_color = "#457b9d"
    skip_color = "#adb5bd"

    # Plot Clang bars
    for i, (val, is_skip) in enumerate(zip(clang_values, clang_skip)):
        if is_skip:
            # Render SKIP bar with hatched pattern and grey color
            ax.bar(
                x[i] - bar_width / 2, 0, bar_width,
                color=skip_color, edgecolor="black", linewidth=0.5,
                hatch="///", label="Clang (SKIP)" if i == 0 else "",
            )
        else:
            ax.bar(
                x[i] - bar_width / 2, val, bar_width,
                color=clang_color, edgecolor="black", linewidth=0.5,
                label="Clang" if i == 0 else "",
            )

    # Plot GCC bars
    for i, (val, is_skip) in enumerate(zip(gcc_values, gcc_skip)):
        if is_skip:
            # Render SKIP bar with hatched pattern and grey color
            ax.bar(
                x[i] + bar_width / 2, 0, bar_width,
                color=skip_color, edgecolor="black", linewidth=0.5,
                hatch="///", label="GCC (SKIP)" if i == 0 or not any(gcc_skip[:i]) else "",
            )
        else:
            ax.bar(
                x[i] + bar_width / 2, val, bar_width,
                color=gcc_color, edgecolor="black", linewidth=0.5,
                label="GCC" if i == 0 or all(gcc_skip[:i]) else "",
            )

    # Annotate pairs with winning compiler and margin
    for i in range(len(x_labels)):
        c_val = clang_values[i]
        g_val = gcc_values[i]
        c_skip = clang_skip[i]
        g_skip = gcc_skip[i]

        if c_skip and g_skip:
            # Both SKIP — no annotation
            continue
        elif c_skip:
            # Only GCC available
            annotation = "GCC only"
            y_pos = g_val
        elif g_skip:
            # Only Clang available
            annotation = "Clang only"
            y_pos = c_val
        else:
            # Both available — compute margin
            margin = abs(c_val - g_val) / max(c_val, g_val) * 100
            if c_val < g_val:
                annotation = f"Clang wins\n{margin:.1f}%"
            elif g_val < c_val:
                annotation = f"GCC wins\n{margin:.1f}%"
            else:
                annotation = "Tie"
            y_pos = max(c_val, g_val)

        ax.annotate(
            annotation,
            xy=(x[i], y_pos),
            xytext=(0, 8),
            textcoords="offset points",
            ha="center",
            va="bottom",
            fontsize=7,
            fontweight="bold",
        )

    ax.set_xlabel("Variant")
    ax.set_ylabel("Instructions (Instret)")
    ax.set_title(f"Compiler Comparison — HQC-{level} Total KEM")
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.legend()
    ax.set_ylim(bottom=0)

    # Remove top and right spines
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    ax.grid(axis="y", alpha=0.3)
    ax.set_axisbelow(True)

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "bar_compiler_comparison.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# Chart: heatmap_speedup
# ---------------------------------------------------------------------------


@register_chart("heatmap_speedup")
def chart_heatmap_speedup(loader: DataLoader, level: int) -> None:
    """Render heatmap showing speedups across all tier/level/operation combinations.

    Rows: KEM operations (keypair, encaps, decaps, total).
    Columns: 9 combinations of tier × security level (T1-128, T1-192, ..., T3-256).
    Each cell is annotated with the numeric speedup value.
    Uses a sequential colormap (light-to-dark) for low-to-high speedup.
    The `level` parameter is unused since this chart shows all levels.
    """
    operations = ["keypair", "encaps", "decaps", "total"]
    levels = [128, 192, 256]
    tier_configs = [
        ("T1", "scalar", "karatsuba_zbc"),
        ("T2", "vector", "karatsuba_rvv"),
        ("T3", "vector", "karatsuba_zvbc"),
    ]

    # Column labels: T1-128, T1-192, T1-256, T2-128, ...
    col_labels = [f"{tier_name}-{lvl}" for tier_name, _, _ in tier_configs for lvl in levels]

    # Load baseline data
    baseline_df = loader.isolated_kem("scalar", "ref_scalar")
    if baseline_df is None or baseline_df.empty:
        print(
            "Warning: Missing baseline (ref_scalar) KEM data, "
            "skipping heatmap_speedup chart.",
            file=sys.stderr,
        )
        return

    # Build 2D speedup array: rows = operations, cols = tier × level
    data = np.zeros((len(operations), len(col_labels)))

    for row_idx, op in enumerate(operations):
        col_idx = 0
        for _tier_name, track, variant in tier_configs:
            tier_df = loader.isolated_kem(track, variant)
            if tier_df is None or tier_df.empty:
                print(
                    f"Warning: Missing KEM data for {variant}, "
                    f"skipping heatmap_speedup chart.",
                    file=sys.stderr,
                )
                return

            for lvl in levels:
                # Get baseline instret for this op and level
                baseline_row = baseline_df[
                    (baseline_df["level"] == lvl) & (baseline_df["op"] == op)
                ]
                if baseline_row.empty:
                    print(
                        f"Warning: No baseline data for op={op}, level={lvl}, "
                        f"skipping heatmap_speedup chart.",
                        file=sys.stderr,
                    )
                    return
                baseline_instret = float(baseline_row["instructions"].iloc[0])

                # Get tier instret for this op and level
                tier_row = tier_df[
                    (tier_df["level"] == lvl) & (tier_df["op"] == op)
                ]
                if tier_row.empty:
                    print(
                        f"Warning: No data for {variant} op={op}, level={lvl}, "
                        f"skipping heatmap_speedup chart.",
                        file=sys.stderr,
                    )
                    return
                tier_instret = float(tier_row["instructions"].iloc[0])

                # Compute speedup
                speedup = baseline_instret / tier_instret if tier_instret > 0 else 0.0
                data[row_idx, col_idx] = speedup
                col_idx += 1

    # --- Create heatmap figure ---
    fig, ax = plt.subplots(figsize=(10, 4))

    im = ax.imshow(data, cmap=plt.cm.YlOrRd, aspect="auto")

    # Set tick labels
    ax.set_xticks(np.arange(len(col_labels)))
    ax.set_xticklabels(col_labels)
    ax.set_yticks(np.arange(len(operations)))
    ax.set_yticklabels(operations)

    # Annotate each cell with the speedup value
    for i in range(len(operations)):
        for j in range(len(col_labels)):
            val = data[i, j]
            # Choose text color based on background intensity
            text_color = "white" if val > (data.max() + data.min()) / 2 else "black"
            ax.text(
                j, i, f"{val:.2f}×",
                ha="center", va="center",
                fontsize=8, color=text_color,
            )

    ax.set_title("KEM Speedup Heatmap (vs Baseline Scalar)")
    fig.colorbar(im, ax=ax, label="Speedup (×)")

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "heatmap_speedups.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# Chart: ablation_waterfall
# ---------------------------------------------------------------------------


@register_chart("ablation_waterfall")
def chart_ablation_waterfall(loader: DataLoader, level: int) -> None:
    """Render waterfall chart for HQC total KEM starting from Baseline_Scalar.

    Shows reduction steps: +Zbc (Tier_1), +RVV (Tier_2), +Zvbc/Zvbb (Tier_3).
    Each delta bar represents the instruction count reduction from the previous stage.
    Bars are annotated with the absolute instruction count saved.
    """
    # Load data for each tier (filter by level and op == "total")
    baseline_df = loader.isolated_kem("scalar", "ref_scalar")
    tier1_df = loader.isolated_kem("scalar", "karatsuba_zbc")
    tier2_df = loader.isolated_kem("vector", "karatsuba_rvv")
    tier3_df = loader.isolated_kem("vector", "karatsuba_zvbc")

    # Check data availability
    for name, df in [
        ("ref_scalar", baseline_df),
        ("karatsuba_zbc", tier1_df),
        ("karatsuba_rvv", tier2_df),
        ("karatsuba_zvbc", tier3_df),
    ]:
        if df is None or df.empty:
            print(
                f"Warning: Missing KEM data for {name}, "
                f"skipping ablation_waterfall chart.",
                file=sys.stderr,
            )
            return

    # Extract total instret for the requested level
    def get_total_instret(df: pd.DataFrame, variant_name: str) -> int | None:
        row = df[(df["level"] == level) & (df["op"] == "total")]
        if row.empty:
            print(
                f"Warning: No total KEM data for {variant_name} at level {level}, "
                f"skipping ablation_waterfall chart.",
                file=sys.stderr,
            )
            return None
        return int(row["instructions"].iloc[0])

    baseline_val = get_total_instret(baseline_df, "ref_scalar")
    tier1_val = get_total_instret(tier1_df, "karatsuba_zbc")
    tier2_val = get_total_instret(tier2_df, "karatsuba_rvv")
    tier3_val = get_total_instret(tier3_df, "karatsuba_zvbc")

    if any(v is None for v in [baseline_val, tier1_val, tier2_val, tier3_val]):
        return

    # Compute deltas (reductions)
    delta1 = baseline_val - tier1_val  # +Zbc reduction
    delta2 = tier1_val - tier2_val  # +RVV reduction
    delta3 = tier2_val - tier3_val  # +Zvbc/Zvbb reduction

    # Waterfall bar data
    # "Start" bar: bottom=0, height=baseline_val
    # "+Zbc" bar: bottom=tier1_val, height=delta1 (shows reduction)
    # "+RVV" bar: bottom=tier2_val, height=delta2
    # "+Zvbc/Zvbb" bar: bottom=tier3_val, height=delta3
    # "Final" bar: bottom=0, height=tier3_val
    x_labels = ["Baseline", "+Zbc", "+RVV", "+Zvbc/Zvbb", "Final"]
    bottoms = [0, tier1_val, tier2_val, tier3_val, 0]
    heights = [baseline_val, delta1, delta2, delta3, tier3_val]
    colors = ["#6c757d", "#2a9d8f", "#2a9d8f", "#2a9d8f", "#6c757d"]

    # Create figure
    fig, ax = plt.subplots(figsize=(8, 5))

    x = np.arange(len(x_labels))
    bars = ax.bar(x, heights, bottom=bottoms, color=colors, edgecolor="black", linewidth=0.5)

    # Annotate each delta bar with absolute instruction count saved
    for i in range(1, 4):  # indices 1, 2, 3 are the delta bars
        bar = bars[i]
        saved = heights[i]
        # Position annotation at the center of the delta bar
        y_center = bottoms[i] + heights[i] / 2
        ax.annotate(
            f"-{saved:,}",
            xy=(bar.get_x() + bar.get_width() / 2, y_center),
            ha="center",
            va="center",
            fontsize=8,
            fontweight="bold",
            color="white",
        )

    ax.set_xlabel("Optimization Stage")
    ax.set_ylabel("Instructions (Instret)")
    ax.set_title(f"ISA Extension Ablation — HQC-{level} Total KEM")
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.set_ylim(bottom=0)

    # Remove top and right spines
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    ax.grid(axis="y", alpha=0.3)
    ax.set_axisbelow(True)

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "waterfall_ablation.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# Chart: stacked_breakdown
# ---------------------------------------------------------------------------


@register_chart("stacked_breakdown")
def chart_stacked_breakdown(loader: DataLoader, level: int) -> None:
    """Render stacked bar chart showing instruction breakdown by operation and category.

    X-axis: keypair, encaps, decaps.
    Stacks colored by category: Multiplication, Keccak/Hashing, Decode, Memory/Utility, Other.
    Uses Tier_3 (karatsuba_zvbc) data at the specified security level.
    Derives per-operation category breakdowns by applying the combined trace's category
    proportions to each operation's total instruction count from isolated KEM data.
    """
    # Load the combined trace for category proportions
    trace_df = loader.trace_hotspots("vector", "karatsuba_zvbc", level)
    if trace_df is None or trace_df.empty:
        print(
            f"Warning: Missing trace data for karatsuba_zvbc at level {level}, "
            f"skipping stacked_breakdown chart.",
            file=sys.stderr,
        )
        return

    # Load isolated KEM data for per-operation instruction counts
    kem_df = loader.isolated_kem("vector", "karatsuba_zvbc")
    if kem_df is None or kem_df.empty:
        print(
            "Warning: Missing isolated KEM data for karatsuba_zvbc, "
            "skipping stacked_breakdown chart.",
            file=sys.stderr,
        )
        return

    # Get per-operation instruction counts for the requested level
    operations = ["keypair", "encaps", "decaps"]
    op_instrets: dict[str, int] = {}
    for op in operations:
        row = kem_df[(kem_df["level"] == level) & (kem_df["op"] == op)]
        if row.empty:
            print(
                f"Warning: No isolated KEM data for {op} at level {level}, "
                f"skipping stacked_breakdown chart.",
                file=sys.stderr,
            )
            return
        op_instrets[op] = int(row["instructions"].iloc[0])

    # Compute category proportions from the combined trace
    category_totals = categorize_entries(trace_df)
    total_instret = sum(category_totals.values())
    if total_instret == 0:
        print(
            "Warning: Trace data has zero total instructions, "
            "skipping stacked_breakdown chart.",
            file=sys.stderr,
        )
        return

    category_fractions: dict[str, float] = {
        cat: count / total_instret for cat, count in category_totals.items()
    }

    # Derive per-operation category breakdowns by scaling proportions
    # Use consistent category ordering from CATEGORY_COLORS
    categories = [c for c in CATEGORY_COLORS if c in category_totals and category_totals[c] > 0]

    # Build data matrix: rows = categories, columns = operations
    data: dict[str, list[float]] = {cat: [] for cat in categories}
    for op in operations:
        op_total = op_instrets[op]
        for cat in categories:
            data[cat].append(op_total * category_fractions.get(cat, 0.0))

    # --- Create stacked bar chart ---
    fig, ax = plt.subplots(figsize=(7, 5))

    x = np.arange(len(operations))
    bar_width = 0.5
    bottom = np.zeros(len(operations))

    for cat in categories:
        values = data[cat]
        ax.bar(
            x,
            values,
            bar_width,
            bottom=bottom,
            label=cat,
            color=CATEGORY_COLORS[cat],
            edgecolor="white",
            linewidth=0.5,
        )
        bottom += np.array(values)

    ax.set_xlabel("KEM Operation")
    ax.set_ylabel("Instructions (Instret)")
    ax.set_title(f"Instruction Breakdown by Category — Tier 3 (HQC-{level})")
    ax.set_xticks(x)
    ax.set_xticklabels([op.capitalize() for op in operations])
    ax.legend(loc="upper left", fontsize=8)
    ax.set_ylim(bottom=0)

    # Remove top and right spines
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    ax.grid(axis="y", alpha=0.3)
    ax.set_axisbelow(True)

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "bar_stacked_breakdown.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# Chart: code_size_tradeoff
# ---------------------------------------------------------------------------


@register_chart("code_size_tradeoff")
def chart_code_size_tradeoff(loader: DataLoader, level: int) -> None:
    """Render scatter plot showing code size vs performance tradeoff.

    X-axis: .text section size in KB.
    Y-axis: total KEM Instret.
    Points for each variant at each security level, labeled with variant name.
    Uses distinct colors per variant and distinct markers per security level.
    The `level` parameter is unused since this chart shows all levels.
    """
    levels = [128, 192, 256]
    variant_configs = [
        ("ref_scalar", "scalar"),
        ("karatsuba_zbc", "scalar"),
        ("karatsuba_rvv", "vector"),
        ("karatsuba_zvbc", "vector"),
    ]

    # Colors per variant
    variant_colors = {
        "ref_scalar": "#6c757d",
        "karatsuba_zbc": "#e63946",
        "karatsuba_rvv": "#457b9d",
        "karatsuba_zvbc": "#2a9d8f",
    }

    # Markers per security level
    level_markers = {
        128: "o",
        192: "s",
        256: "^",
    }

    # Display names for variants
    variant_display = {
        "ref_scalar": "Ref Scalar",
        "karatsuba_zbc": "Karatsuba Zbc",
        "karatsuba_rvv": "Karatsuba RVV",
        "karatsuba_zvbc": "Karatsuba Zvbc",
    }

    # Collect data points
    points: list[dict] = []

    for variant, track in variant_configs:
        df = loader.isolated_kem(track, variant)
        if df is None or df.empty:
            print(
                f"Warning: Missing isolated KEM data for {variant}, "
                f"skipping this variant in code_size_tradeoff chart.",
                file=sys.stderr,
            )
            continue

        for lvl in levels:
            row = df[(df["level"] == lvl) & (df["op"] == "total")]
            if row.empty:
                print(
                    f"Warning: No total KEM data for {variant} at level {lvl}, "
                    f"skipping this point.",
                    file=sys.stderr,
                )
                continue

            try:
                instret = float(row["instructions"].iloc[0])
                codesize = float(row["codesize"].iloc[0])
            except KeyError as e:
                print(
                    f"Warning: Missing column {e} for {variant} at level {lvl}, "
                    f"skipping this point.",
                    file=sys.stderr,
                )
                continue

            points.append({
                "variant": variant,
                "level": lvl,
                "codesize_kb": codesize / 1024,
                "instret": instret,
            })

    if not points:
        print(
            "Warning: No data available for code_size_tradeoff chart.",
            file=sys.stderr,
        )
        return

    # --- Create scatter plot ---
    fig, ax = plt.subplots(figsize=(8, 6))

    # Plot points grouped by variant (for legend) with markers by level
    # Track which variants and levels have been added to legend
    legend_variants: set[str] = set()
    legend_levels: set[int] = set()

    for pt in points:
        variant = pt["variant"]
        lvl = pt["level"]
        color = variant_colors[variant]
        marker = level_markers[lvl]

        # Only add variant label to legend once
        label = variant_display[variant] if variant not in legend_variants else ""
        legend_variants.add(variant)

        ax.scatter(
            pt["codesize_kb"],
            pt["instret"],
            color=color,
            marker=marker,
            s=80,
            edgecolors="black",
            linewidths=0.5,
            label=label,
            zorder=5,
        )

        # Annotate each point with variant name
        ax.annotate(
            variant_display[variant],
            xy=(pt["codesize_kb"], pt["instret"]),
            xytext=(5, 5),
            textcoords="offset points",
            fontsize=7,
            color=color,
            ha="left",
            va="bottom",
        )

    # Add marker legend for security levels
    for lvl, marker in level_markers.items():
        ax.scatter(
            [], [],
            color="grey",
            marker=marker,
            s=60,
            label=f"HQC-{lvl}",
        )

    ax.set_xlabel(".text Size (KB)")
    ax.set_ylabel("Total KEM Instret")
    ax.set_title("Code Size vs Performance Tradeoff")
    ax.legend(loc="best", fontsize=8)
    ax.grid(True, alpha=0.3)

    # Remove top and right spines
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    fig.tight_layout()

    # Save output
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "scatter_code_size.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    """Build argparse parser with --all, --chart, --list, --level flags."""
    parser = argparse.ArgumentParser(
        description="Generate publication-quality PDF charts for the HQC RISC-V thesis."
    )
    action_group = parser.add_mutually_exclusive_group()
    action_group.add_argument(
        "--all",
        action="store_true",
        help="Generate all registered charts.",
    )
    action_group.add_argument(
        "--chart",
        type=str,
        choices=list(CHART_REGISTRY.keys()),
        metavar="NAME",
        help="Generate a single chart by name.",
    )
    action_group.add_argument(
        "--list",
        action="store_true",
        help="List all available chart names and exit.",
    )
    parser.add_argument(
        "--level",
        type=int,
        choices=[128, 192, 256],
        default=128,
        help="Security level for charts that support level filtering (default: 128).",
    )
    return parser


def main() -> None:
    """Entry point: parse args, create DataLoader, dispatch chart generation."""
    parser = build_parser()
    args = parser.parse_args()

    # --list: print chart names and exit
    if args.list:
        for name in sorted(CHART_REGISTRY.keys()):
            print(name)
        return

    # --all: generate all charts
    if args.all:
        loader = DataLoader(REPO_ROOT)
        for name, func in CHART_REGISTRY.items():
            func(loader, args.level)
        return

    # --chart NAME: generate a single chart
    if args.chart:
        loader = DataLoader(REPO_ROOT)
        CHART_REGISTRY[args.chart](loader, args.level)
        return

    # No action flag provided → print usage to stderr, exit code 2
    parser.print_usage(sys.stderr)
    sys.exit(2)


if __name__ == "__main__":
    main()
