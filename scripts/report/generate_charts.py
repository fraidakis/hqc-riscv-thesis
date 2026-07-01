#!/usr/bin/env python3
"""Generate publication-quality PDF charts for the HQC RISC-V project.

Consolidated chart generation script that produces 10 distinct chart types
as PDF figures suitable for LaTeX inclusion. Reads benchmark CSV data from
the results/ directory tree.

Usage:
    python3 scripts/report/generate_charts.py --all
    python3 scripts/report/generate_charts.py --chart three_stage_pie
    python3 scripts/report/generate_charts.py --list
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
OUTPUT_DIR = REPO_ROOT / "results" / "reports" / "figures"

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


_VARIANT_SLUG_MAP: dict[str, str] = {
    "scalar_ref": "scalar-ref",
    "scalar_zb": "scalar-zb",
    "vector_rvv": "vector-rvv",
    "vector_zvk": "vector-zvk",
}


def variant_slug(variant: str) -> str:
    """Convert a variant name to ISA-family-based slug for filenames.

    Example: 'scalar_zb' → 'scalar-zb'
    """
    return _VARIANT_SLUG_MAP.get(variant, variant.replace("_", "-"))


# ---------------------------------------------------------------------------
# DataLoader
# ---------------------------------------------------------------------------


class DataLoader:
    """Central data layer that resolves CSV paths and provides parsed DataFrames."""

    # Variant name mapping: old internal names → new directory names
    VARIANT_MAP: dict[str, str] = {
        "scalar_ref": "scalar_ref",
        "scalar_zb": "scalar_Zb",
        "vector_rvv": "vector_rvv",
        "vector_zvk": "vector_Zvk",
    }

    def __init__(self, repo_root: Path) -> None:
        """Resolve all CSV paths relative to repo_root."""
        self._root = repo_root

    def _variant_dir(self, variant: str) -> str:
        """Map a variant name to its directory name under results/."""
        return self.VARIANT_MAP.get(variant, variant)

    def trace_hotspots(self, track: str, variant: str, level: int) -> pd.DataFrame | None:
        """Load trace hotspot CSV for a given variant/level.

        The `track` parameter is accepted for API compatibility but ignored;
        the variant name encodes the execution model.
        Returns DataFrame with columns: function, instructions, percentage.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        var_dir = self._variant_dir(variant)
        path = (
            self._root
            / "results"
            / "simulation"
            / "profiling"
            / "hotspots"
            / var_dir
            / f"hotspots_kem_L{level}_{slug}_latest.csv"
        )
        return load_csv(path)

    def isolated_kem(self, track: str, variant: str) -> pd.DataFrame | None:
        """Load isolated KEM benchmark CSV.

        The `track` parameter is accepted for API compatibility but ignored;
        the variant name encodes the execution model.
        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "results"
            / "simulation"
            / "benchmarks"
            / "kem"
            / f"sim_kem_{slug}_latest.csv"
        )
        return load_csv(path)

    def isolated_polymul(self, track: str, variant: str) -> pd.DataFrame | None:
        """Load isolated polymul benchmark CSV.

        The `track` parameter is accepted for API compatibility but ignored;
        the variant name encodes the execution model.
        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "results"
            / "simulation"
            / "benchmarks"
            / "components"
            / "poly_mul"
            / f"sim_mul_{slug}_latest.csv"
        )
        return load_csv(path)

    def isolated_keccak(self, variant: str) -> pd.DataFrame | None:
        """Load isolated keccak benchmark CSV.

        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        path = (
            self._root
            / "results"
            / "simulation"
            / "benchmarks"
            / "components"
            / "keccak"
            / f"sim_keccak_{variant}_latest.csv"
        )
        return load_csv(path)

    def isolated_decode(self, variant: str) -> pd.DataFrame | None:
        """Load isolated decode benchmark CSV.

        Returns DataFrame with columns: level, op, instructions, codesize.
        Returns None and prints to stderr if file not found.
        """
        path = (
            self._root
            / "results"
            / "simulation"
            / "benchmarks"
            / "components"
            / "decode"
            / f"sim_decode_{variant}_latest.csv"
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
            / "results"
            / "simulation"
            / "benchmarks"
            / "vlen_scaling"
            / f"sim_vlen_l{level}-{slug}_latest.csv"
        )
        return load_csv(path)

    def compiler_compare(self, track: str, variant: str) -> pd.DataFrame | None:
        """Load compiler comparison CSV (hardware).

        The `track` parameter is accepted for API compatibility but ignored;
        the variant name encodes the execution model.
        Returns DataFrame with columns: compiler, opt, level, variant, status,
        cycles, text_bytes, binary, error.
        Returns None and prints to stderr if file not found.
        """
        slug = variant_slug(variant)
        path = (
            self._root
            / "results"
            / "hardware"
            / "benchmarks"
            / "compiler_compare"
            / f"hw_compiler_compare_{slug}_latest.csv"
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

    Panels: Baseline_Scalar (scalar_ref), Tier_2 (vector_rvv), Tier_3 (vector_zvk).
    Uses categorize_entries() for aggregation and CATEGORY_COLORS for consistent coloring.
    """
    # Define the three stages: (title, track, variant)
    stages = [
        ("Baseline (Scalar)", "scalar", "scalar_ref"),
        ("Tier 2 (Karatsuba RVV)", "vector", "vector_rvv"),
        ("Tier 3 (Karatsuba Zvbc)", "vector", "vector_zvk"),
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
        ("Tier 1", "scalar", "scalar_zb"),
        ("Tier 2", "vector", "vector_rvv"),
        ("Tier 3", "vector", "vector_zvk"),
    ]

    # Load baseline data
    baseline_df = loader.isolated_kem("scalar", "scalar_ref")
    if baseline_df is None or baseline_df.empty:
        print(
            "Warning: Missing baseline (scalar_ref) KEM data, "
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
    polymul_baseline = loader.isolated_polymul("scalar", "scalar_ref")
    polymul_tier1 = loader.isolated_polymul("scalar", "scalar_zb")
    polymul_tier2 = loader.isolated_polymul("vector", "vector_rvv")
    polymul_tier3 = loader.isolated_polymul("vector", "vector_zvk")

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

    Plots separate lines for each vector variant (RVV, Zvk) at each security
    level that has data. Marks the minimum Instret point on each line.
    The `level` parameter is unused since this chart shows all three levels.
    """
    levels = [128, 192, 256]
    # Plot configurations per variant
    variant_configs = [
        ("vector_rvv", "RVV"),
        ("vector_zvk", "Zvk"),
    ]
    level_markers = {128: "o", 192: "s", 256: "^"}
    level_colors = {128: "#e63946", 192: "#457b9d", 256: "#2a9d8f"}

    fig, ax = plt.subplots(figsize=(8, 5))
    any_plotted = False

    for variant, variant_label in variant_configs:
        for lvl in levels:
            df = loader.vlen_sweep(variant, lvl)
            if df is None or df.empty:
                continue

            # Filter for total KEM operation
            total_df = df[df["op"] == "total"].copy()
            if total_df.empty:
                continue

            total_df = total_df.sort_values("vlen")
            vlens = total_df["vlen"].values
            instrets = total_df["instructions"].values

            color = level_colors[lvl]
            marker = level_markers[lvl]
            linestyle = "-" if variant == "vector_rvv" else "--"
            label = f"{variant_label} HQC-{lvl}"

            ax.plot(
                vlens,
                instrets,
                color=color,
                marker=marker,
                linestyle=linestyle,
                label=label,
                linewidth=1.5,
                markersize=6,
            )

            # Mark the minimum Instret point
            min_idx = np.argmin(instrets)
            ax.scatter(
                vlens[min_idx],
                instrets[min_idx],
                marker="*",
                color=color,
                s=200,
                zorder=5,
                edgecolors="black",
                linewidths=0.5,
            )

            any_plotted = True

    if not any_plotted:
        print(
            "Warning: No VLEN sweep data available for any variant/level, "
            "skipping vlen_sweep chart.",
            file=sys.stderr,
        )
        plt.close(fig)
        return

    ax.set_xlabel("VLEN")
    ax.set_ylabel("Instructions (Instret)")
    ax.set_title("Total KEM Instret vs VLEN")
    ax.set_xticks([256, 512, 1024, 2048])
    ax.get_xaxis().set_major_formatter(plt.FuncFormatter(lambda x, _: f"{int(x)}"))
    ax.legend(fontsize=8)
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
        ("Baseline (Scalar)", "scalar", "scalar_ref"),
        ("Tier 1 (Zbc)", "scalar", "scalar_zb"),
        ("Tier 2 (RVV)", "vector", "vector_rvv"),
        ("Tier 3 (Zvbc)", "vector", "vector_zvk"),
    ]

    # Load baseline data first to check availability
    baseline_df = loader.isolated_kem("scalar", "scalar_ref")
    if baseline_df is None or baseline_df.empty:
        print(
            "Warning: Missing baseline (scalar_ref) KEM data, "
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

    X-axis groups: scalar_ref, scalar_zb, vector_rvv (HW-available variants only).
    Two bars per group: best Clang result and best GCC result for total KEM cycles.
    SKIP entries are rendered with hatched pattern and grey color.
    Each pair is annotated with the winning compiler and margin percentage.
    """
    # Variant configurations: only HW-available variants (vector_zvk cannot run on HW)
    variant_configs = [
        ("scalar_ref", "scalar"),
        ("scalar_zb", "scalar"),
        ("vector_rvv", "vector"),
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

        # Find best Clang result (lowest cycles where status != SKIP)
        clang_rows = df_level[df_level["compiler"] == "clang"]
        clang_ok = clang_rows[clang_rows["status"] != "SKIP"]
        if clang_ok.empty:
            # All Clang entries are SKIP
            clang_values.append(None)
            clang_skip.append(True)
        else:
            best_clang = clang_ok["cycles"].min()
            clang_values.append(float(best_clang))
            clang_skip.append(False)

        # Find best GCC result (lowest cycles where status != SKIP)
        gcc_rows = df_level[df_level["compiler"] == "gcc"]
        gcc_ok = gcc_rows[gcc_rows["status"] != "SKIP"]
        if gcc_ok.empty:
            # All GCC entries are SKIP
            gcc_values.append(None)
            gcc_skip.append(True)
        else:
            best_gcc = gcc_ok["cycles"].min()
            gcc_values.append(float(best_gcc))
            gcc_skip.append(False)

        # Format variant name for display
        label = variant.replace("_", " ").replace("ref scalar", "Ref Scalar")
        label = label.title() if "ref" not in variant else label
        x_labels.append(variant.replace("_", "\\_") if False else variant.replace("_", " ").title())

    # Nicer labels
    x_labels = ["Ref Scalar", "Karatsuba Zbc", "Karatsuba RVV"]

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
    ax.set_ylabel("Cycles (Hardware)")
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
        ("T1", "scalar", "scalar_zb"),
        ("T2", "vector", "vector_rvv"),
        ("T3", "vector", "vector_zvk"),
    ]

    # Column labels: T1-128, T1-192, T1-256, T2-128, ...
    col_labels = [f"{tier_name}-{lvl}" for tier_name, _, _ in tier_configs for lvl in levels]

    # Load baseline data
    baseline_df = loader.isolated_kem("scalar", "scalar_ref")
    if baseline_df is None or baseline_df.empty:
        print(
            "Warning: Missing baseline (scalar_ref) KEM data, "
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
    baseline_df = loader.isolated_kem("scalar", "scalar_ref")
    tier1_df = loader.isolated_kem("scalar", "scalar_zb")
    tier2_df = loader.isolated_kem("vector", "vector_rvv")
    tier3_df = loader.isolated_kem("vector", "vector_zvk")

    # Check data availability
    for name, df in [
        ("scalar_ref", baseline_df),
        ("scalar_zb", tier1_df),
        ("vector_rvv", tier2_df),
        ("vector_zvk", tier3_df),
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

    baseline_val = get_total_instret(baseline_df, "scalar_ref")
    tier1_val = get_total_instret(tier1_df, "scalar_zb")
    tier2_val = get_total_instret(tier2_df, "vector_rvv")
    tier3_val = get_total_instret(tier3_df, "vector_zvk")

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
    output_path = OUTPUT_DIR / "waterfall_polymul_scalar_zbc.pdf"
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
    Uses Tier_3 (vector_zvk) data at the specified security level.
    Derives per-operation category breakdowns by applying the combined trace's category
    proportions to each operation's total instruction count from isolated KEM data.
    """
    # Load the combined trace for category proportions
    trace_df = loader.trace_hotspots("vector", "vector_zvk", level)
    if trace_df is None or trace_df.empty:
        print(
            f"Warning: Missing trace data for vector_zvk at level {level}, "
            f"skipping stacked_breakdown chart.",
            file=sys.stderr,
        )
        return

    # Load isolated KEM data for per-operation instruction counts
    kem_df = loader.isolated_kem("vector", "vector_zvk")
    if kem_df is None or kem_df.empty:
        print(
            "Warning: Missing isolated KEM data for vector_zvk, "
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
        ("scalar_ref", "scalar"),
        ("scalar_zb", "scalar"),
        ("vector_rvv", "vector"),
        ("vector_zvk", "vector"),
    ]

    # Colors per variant
    variant_colors = {
        "scalar_ref": "#6c757d",
        "scalar_zb": "#e63946",
        "vector_rvv": "#457b9d",
        "vector_zvk": "#2a9d8f",
    }

    # Markers per security level
    level_markers = {
        128: "o",
        192: "s",
        256: "^",
    }

    # Display names for variants
    variant_display = {
        "scalar_ref": "Scalar Ref",
        "scalar_zb": "Scalar Zb",
        "vector_rvv": "Vector RVV",
        "vector_zvk": "Vector Zvk",
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
# Hardware Benchmark Charts (BPI-F3 SpacemiT K1 X60)
# ---------------------------------------------------------------------------

# Hardware-specific constants
HW_CLOCK_HZ = 1_600_000_000  # 1.6 GHz SpacemiT K1 X60

# Hardware chart registry (separate from simulation charts)
HW_CHART_REGISTRY: dict[str, Callable] = {}


def register_hw_chart(name: str) -> Callable:
    """Decorator that registers a hardware chart function under the given name."""

    def decorator(func: Callable) -> Callable:
        HW_CHART_REGISTRY[name] = func
        return func

    return decorator


# Variant display names and colors for hardware charts
HW_VARIANT_DISPLAY: dict[str, str] = {
    "scalar_ref": "Scalar Ref",
    "scalar_zb": "Scalar Zb",
    "vector_rvv": "Vector RVV",
}

HW_VARIANT_COLORS: dict[str, str] = {
    "scalar_ref": "#6c757d",
    "scalar_zb": "#457b9d",
    "vector_rvv": "#2a9d8f",
}

# IPC chart color coding: scalar=blue family, vector=green family
HW_IPC_COLORS: dict[str, str] = {
    "scalar_ref": "#1f77b4",
    "scalar_zb": "#6baed6",
    "vector_rvv": "#2ca02c",
}

# Hardware variant → directory mapping (only variants that run on hardware)
HW_VARIANT_DIRS: dict[str, str] = {
    "scalar_ref": "scalar_ref",
    "scalar_zb": "scalar_Zb",
    "vector_rvv": "vector_rvv",
}

# Operations in canonical order
HW_OPS = ["keypair", "encaps", "decaps"]


def _load_hw_kem_data(repo_root: Path) -> pd.DataFrame | None:
    """Load and merge all hardware KEM benchmark CSVs into a single DataFrame.

    Returns DataFrame with columns:
        variant, op, median_cycles, mean_cycles, stddev_cycles,
        min_cycles, max_cycles, median_instret, mean_instret, stddev_instret,
        min_instret, max_instret
    Returns None if no data files are found.
    """
    frames: list[pd.DataFrame] = []

    for variant, dir_name in HW_VARIANT_DIRS.items():
        slug = variant_slug(variant)
        # Try flat layout first (files directly in kem/), then subdirectory layout
        path_flat = (
            repo_root
            / "results"
            / "hardware"
            / "benchmarks"
            / "kem"
            / f"hw_kem_{slug}_latest.csv"
        )
        path_subdir = (
            repo_root
            / "results"
            / "hardware"
            / "benchmarks"
            / "kem"
            / dir_name
            / f"hw_kem_{slug}_latest.csv"
        )
        path = path_flat if path_flat.exists() else path_subdir
        df = load_csv(path)
        if df is None or df.empty:
            continue
        # Normalize column names for consistent downstream access
        if "op" in df.columns:
            df = df.rename(columns={"op": "operation"})
        if "instret" in df.columns and "median_instret" not in df.columns:
            df = df.rename(columns={"instret": "median_instret"})
        df["variant"] = variant
        frames.append(df)

    if not frames:
        print(
            "Error: No hardware KEM benchmark data found in "
            "results/hardware/benchmarks/kem/",
            file=sys.stderr,
        )
        return None

    combined = pd.concat(frames, ignore_index=True)
    return combined


@register_hw_chart("spike_vs_hw_bars")
def plot_spike_vs_hw_bars(loader: DataLoader, level: int) -> None:
    """Grouped bar chart: Spike instret vs HW cycles side-by-side per variant.

    Annotates each pair with IPC ratio (instret/cycles).
    One subplot per security level would require multi-level data; currently
    generates a single figure for the available level (128).
    """
    df = _load_hw_kem_data(loader._root)
    if df is None:
        return

    # Filter by requested security level
    if "level" in df.columns:
        df = df[df["level"] == level]

    variants = [v for v in HW_VARIANT_DIRS if v in df["variant"].unique()]
    operations = [op for op in HW_OPS if op in df["operation"].unique()]

    if not variants or not operations:
        print("Warning: Insufficient data for spike_vs_hw_bars chart.", file=sys.stderr)
        return

    n_groups = len(variants) * len(operations)
    x = np.arange(n_groups)
    bar_width = 0.35

    fig, ax = plt.subplots(figsize=(12, 6))

    instret_vals: list[float] = []
    cycle_vals: list[float] = []
    labels: list[str] = []

    for variant in variants:
        for op in operations:
            row = df[(df["variant"] == variant) & (df["operation"] == op)]
            if row.empty:
                instret_vals.append(0)
                cycle_vals.append(0)
            else:
                instret_vals.append(float(row["median_instret"].iloc[0]))
                cycle_vals.append(float(row["median_cycles"].iloc[0]))
            labels.append(f"{HW_VARIANT_DISPLAY[variant]}\n{op}")

    instret_arr = np.array(instret_vals)
    cycle_arr = np.array(cycle_vals)

    bars_instret = ax.bar(
        x - bar_width / 2, instret_arr, bar_width,
        label="Spike Instret", color="#457b9d", edgecolor="black", linewidth=0.5,
    )
    bars_cycles = ax.bar(
        x + bar_width / 2, cycle_arr, bar_width,
        label="HW Cycles", color="#e63946", edgecolor="black", linewidth=0.5,
    )

    # Annotate IPC ratio above each pair
    for i in range(n_groups):
        if cycle_arr[i] > 0:
            ipc = instret_arr[i] / cycle_arr[i]
            mid_x = x[i]
            top_y = max(instret_arr[i], cycle_arr[i])
            ax.annotate(
                f"IPC={ipc:.2f}",
                xy=(mid_x, top_y),
                xytext=(0, 8),
                textcoords="offset points",
                ha="center",
                va="bottom",
                fontsize=7,
                fontstyle="italic",
            )

    ax.set_xlabel("Variant / Operation")
    ax.set_ylabel("Count")
    ax.set_title(f"Spike Instret vs Hardware Cycles (HQC-{level}, BPI-F3 K1 X60)")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=7)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    fig.tight_layout()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "hw_spike_vs_hw_bars.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


@register_hw_chart("ipc_chart")
def plot_ipc_chart(loader: DataLoader, level: int) -> None:
    """Horizontal bar chart showing IPC per variant per operation.

    Color-coded: scalar variants in blue, RVV variants in green.
    Vertical reference line at IPC=1.0 (ideal single-issue).
    """
    df = _load_hw_kem_data(loader._root)
    if df is None:
        return

    # Filter by requested security level
    if "level" in df.columns:
        df = df[df["level"] == level]

    variants = [v for v in HW_VARIANT_DIRS if v in df["variant"].unique()]
    operations = [op for op in HW_OPS if op in df["operation"].unique()]

    if not variants or not operations:
        print("Warning: Insufficient data for ipc_chart.", file=sys.stderr)
        return

    # Build IPC data
    bar_labels: list[str] = []
    ipc_values: list[float] = []
    bar_colors: list[str] = []

    for variant in variants:
        for op in operations:
            row = df[(df["variant"] == variant) & (df["operation"] == op)]
            if row.empty:
                continue
            instret = float(row["median_instret"].iloc[0])
            cycles = float(row["median_cycles"].iloc[0])
            if cycles > 0:
                ipc = instret / cycles
                bar_labels.append(f"{HW_VARIANT_DISPLAY[variant]} – {op}")
                ipc_values.append(ipc)
                bar_colors.append(HW_IPC_COLORS[variant])

    if not ipc_values:
        print("Warning: No IPC data computed for ipc_chart.", file=sys.stderr)
        return

    y = np.arange(len(bar_labels))

    fig, ax = plt.subplots(figsize=(9, 6))

    ax.barh(y, ipc_values, color=bar_colors, edgecolor="black", linewidth=0.5)
    ax.set_yticks(y)
    ax.set_yticklabels(bar_labels, fontsize=8)
    ax.set_xlabel("Instructions Per Cycle (IPC)")
    ax.set_title("IPC per Variant and Operation (BPI-F3 K1 X60)")

    # Reference line at IPC=1.0
    ax.axvline(x=1.0, color="red", linestyle="--", linewidth=1.0, label="IPC = 1.0")

    # Annotate IPC values on bars
    for i, val in enumerate(ipc_values):
        ax.text(val + 0.02, i, f"{val:.3f}", va="center", fontsize=7)

    # Caption annotation
    ax.annotate(
        "IPC > 1 indicates dual-issue utilization",
        xy=(0.98, 0.02),
        xycoords="axes fraction",
        ha="right",
        va="bottom",
        fontsize=8,
        fontstyle="italic",
        color="#555555",
    )

    ax.legend(loc="lower right", fontsize=8)
    ax.grid(axis="x", alpha=0.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    fig.tight_layout()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "hw_ipc_chart.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


@register_hw_chart("wallclock_bars")
def plot_wallclock_bars(loader: DataLoader, level: int) -> None:
    """Bar chart showing wall-clock time (μs) per KEM operation per variant.

    Derives wall-clock = hw_cycles_median / 1_600_000_000 → seconds → μs.
    Annotates decaps bar with "HQC-128 Decaps in X μs".
    """
    df = _load_hw_kem_data(loader._root)
    if df is None:
        return

    # Filter by requested security level
    if "level" in df.columns:
        df = df[df["level"] == level]

    variants = [v for v in HW_VARIANT_DIRS if v in df["variant"].unique()]
    operations = [op for op in HW_OPS if op in df["operation"].unique()]

    if not variants or not operations:
        print("Warning: Insufficient data for wallclock_bars chart.", file=sys.stderr)
        return

    n_ops = len(operations)
    x = np.arange(n_ops)
    n_variants = len(variants)
    bar_width = 0.8 / n_variants

    fig, ax = plt.subplots(figsize=(9, 5))

    for i, variant in enumerate(variants):
        times_us: list[float] = []
        for op in operations:
            row = df[(df["variant"] == variant) & (df["operation"] == op)]
            if row.empty:
                times_us.append(0)
            else:
                cycles = float(row["median_cycles"].iloc[0])
                time_s = cycles / HW_CLOCK_HZ
                times_us.append(time_s * 1_000_000)  # Convert to μs

        offset = (i - (n_variants - 1) / 2) * bar_width
        bars = ax.bar(
            x + offset, times_us, bar_width,
            label=HW_VARIANT_DISPLAY[variant],
            color=HW_VARIANT_COLORS[variant],
            edgecolor="black",
            linewidth=0.5,
        )

        # Annotate each bar with value
        for bar, val in zip(bars, times_us):
            ax.annotate(
                f"{val:.0f} μs",
                xy=(bar.get_x() + bar.get_width() / 2, bar.get_height()),
                xytext=(0, 3),
                textcoords="offset points",
                ha="center",
                va="bottom",
                fontsize=7,
            )

    ax.set_xlabel("KEM Operation")
    ax.set_ylabel("Wall-Clock Time (μs)")
    ax.set_title(f"HQC-{level} Wall-Clock Latency @ 1.6 GHz (BPI-F3 K1 X60)")
    ax.set_xticks(x)
    ax.set_xticklabels([op.capitalize() for op in operations])
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.set_ylim(bottom=0)

    fig.tight_layout()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "hw_wallclock_bars.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


@register_hw_chart("spike_hw_scatter")
def plot_spike_hw_scatter(loader: DataLoader, level: int) -> None:
    """Scatter plot: x=Spike speedup, y=HW speedup (scalar_ref as baseline).

    Draws ideal line y=x (dashed). Annotates each point with variant name.
    Caption: "Points above the line = HW outperforms Spike prediction".
    """
    df = _load_hw_kem_data(loader._root)
    if df is None:
        return

    # Filter by requested security level
    if "level" in df.columns:
        df = df[df["level"] == level]

    # Need scalar_ref as baseline
    baseline_rows = df[df["variant"] == "scalar_ref"]
    if baseline_rows.empty:
        print(
            "Warning: No scalar_ref baseline data for spike_hw_scatter chart.",
            file=sys.stderr,
        )
        return

    operations = [op for op in HW_OPS if op in df["operation"].unique()]
    optimized_variants = [v for v in ["scalar_zb", "vector_rvv"]
                          if v in df["variant"].unique()]

    if not optimized_variants:
        print("Warning: No optimized variant data for spike_hw_scatter.", file=sys.stderr)
        return

    # Compute speedups
    points: list[dict] = []

    for op in operations:
        base_row = baseline_rows[baseline_rows["operation"] == op]
        if base_row.empty:
            continue
        base_instret = float(base_row["median_instret"].iloc[0])
        base_cycles = float(base_row["median_cycles"].iloc[0])

        for variant in optimized_variants:
            row = df[(df["variant"] == variant) & (df["operation"] == op)]
            if row.empty:
                continue
            var_instret = float(row["median_instret"].iloc[0])
            var_cycles = float(row["median_cycles"].iloc[0])

            if var_instret > 0 and var_cycles > 0:
                spike_speedup = base_instret / var_instret
                hw_speedup = base_cycles / var_cycles

                points.append({
                    "variant": variant,
                    "operation": op,
                    "spike_speedup": spike_speedup,
                    "hw_speedup": hw_speedup,
                })

    if not points:
        print("Warning: No speedup data for spike_hw_scatter chart.", file=sys.stderr)
        return

    fig, ax = plt.subplots(figsize=(7, 7))

    # Plot each point
    for pt in points:
        color = HW_VARIANT_COLORS[pt["variant"]]
        ax.scatter(
            pt["spike_speedup"], pt["hw_speedup"],
            color=color, s=100, edgecolors="black", linewidths=0.5, zorder=5,
        )
        ax.annotate(
            f"{HW_VARIANT_DISPLAY[pt['variant']]}\n({pt['operation']})",
            xy=(pt["spike_speedup"], pt["hw_speedup"]),
            xytext=(6, 6),
            textcoords="offset points",
            fontsize=7,
            color=color,
        )

    # Ideal line y=x
    all_speedups = [pt["spike_speedup"] for pt in points] + [pt["hw_speedup"] for pt in points]
    lim_min = 0
    lim_max = max(all_speedups) * 1.15
    ax.plot([lim_min, lim_max], [lim_min, lim_max], "k--", linewidth=1.0, alpha=0.6, label="y = x (ideal)")

    ax.set_xlabel("Spike Instret Speedup (×)")
    ax.set_ylabel("Hardware Cycle Speedup (×)")
    ax.set_title("Spike vs Hardware Speedup Correlation")
    ax.set_xlim(lim_min, lim_max)
    ax.set_ylim(lim_min, lim_max)
    ax.set_aspect("equal")
    ax.legend(loc="upper left", fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    # Caption annotation
    ax.annotate(
        "Points above the line = HW outperforms Spike prediction",
        xy=(0.98, 0.02),
        xycoords="axes fraction",
        ha="right",
        va="bottom",
        fontsize=8,
        fontstyle="italic",
        color="#555555",
    )

    fig.tight_layout()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT_DIR / "hw_spike_hw_scatter.pdf"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    """Build argparse parser with --all, --chart, --list, --hardware, --level flags."""
    parser = argparse.ArgumentParser(
        description="Generate publication-quality PDF charts for the HQC RISC-V thesis."
    )
    action_group = parser.add_mutually_exclusive_group()
    action_group.add_argument(
        "--all",
        action="store_true",
        help="Generate all registered charts (simulation only).",
    )
    action_group.add_argument(
        "--chart",
        type=str,
        choices=list(CHART_REGISTRY.keys()),
        metavar="NAME",
        help="Generate a single simulation chart by name.",
    )
    action_group.add_argument(
        "--list",
        action="store_true",
        help="List all available chart names and exit.",
    )
    action_group.add_argument(
        "--hardware",
        action="store_true",
        help="Generate all hardware benchmark charts (BPI-F3 K1 X60).",
    )
    action_group.add_argument(
        "--hw-chart",
        type=str,
        choices=list(HW_CHART_REGISTRY.keys()),
        metavar="NAME",
        help="Generate a single hardware chart by name.",
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
        print("Simulation charts:")
        for name in sorted(CHART_REGISTRY.keys()):
            print(f"  {name}")
        print("\nHardware charts (use --hardware or --hw-chart):")
        for name in sorted(HW_CHART_REGISTRY.keys()):
            print(f"  {name}")
        return

    # --hardware: generate all hardware charts
    if args.hardware:
        loader = DataLoader(REPO_ROOT)
        for name, func in HW_CHART_REGISTRY.items():
            func(loader, args.level)
        return

    # --hw-chart NAME: generate a single hardware chart
    if args.hw_chart:
        loader = DataLoader(REPO_ROOT)
        HW_CHART_REGISTRY[args.hw_chart](loader, args.level)
        return

    # --all: generate all simulation charts
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
