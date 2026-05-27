#!/usr/bin/env python3
"""Generate PDF pie charts showing instruction breakdown by category for HQC variants.

Reads trace hotspot CSVs from analysis/profiles/traces/{track}/{variant}/,
classifies functions into high-level categories via a hardcoded pattern map,
and renders PDF pie charts using matplotlib.

Usage:
    python3 scripts/report/generate_pie_charts.py --track scalar --variant karatsuba_zbc
    python3 scripts/report/generate_pie_charts.py --all
    python3 scripts/report/generate_pie_charts.py --all --level 192
"""

from __future__ import annotations

import argparse
import csv
import sys
from collections import defaultdict
from dataclasses import dataclass
from math import ceil
from pathlib import Path

import matplotlib

matplotlib.use("pdf")
import matplotlib.pyplot as plt  # noqa: E402

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parents[2]
TRACES_DIR = REPO_ROOT / "analysis" / "profiles" / "traces"
OUTPUT_DIR = REPO_ROOT / "analysis" / "reports" / "charts"

CATEGORY_MAP: dict[str, str] = {
    # Multiplication patterns
    "schoolbook_mul": "Multiplication",
    "clmul":        "Multiplication",
    "vclmul":       "Multiplication",
    "kmul_":        "Multiplication",
    "gf_reduce":    "Multiplication",
    "gf_mul":       "Multiplication",
    "gf_square":    "Multiplication",
    "vect_mul":     "Multiplication",
    "karatsuba":    "Multiplication",
    # Keccak/Hashing patterns
    "Keccak":       "Keccak/Hashing",
    "keccak":       "Keccak/Hashing",
    "sha3_":        "Keccak/Hashing",
    "shake":        "Keccak/Hashing",
    "hash_":        "Keccak/Hashing",
    "xof_":         "Keccak/Hashing",
    # Decode patterns
    "reed_":        "Decode",
    "fft":          "Decode",
    "hadamard":     "Decode",
    "find_peaks":   "Decode",
    "compute_syndromes": "Decode",
    "compute_elp":  "Decode",
    "compute_error_values": "Decode",
    "compute_z_poly": "Decode",
    "fft_retrieve_error_poly": "Decode",
    "correct_errors": "Decode",
    "code_decode":  "Decode",
    "compute_roots": "Decode",
    "encode":       "Decode",
    "code_encode":  "Decode",
    "reed_muller":  "Decode",
    # Memory/Utility patterns
    "memset":       "Memory/Utility",
    "memcpy":       "Memory/Utility",
    "rvv_xor":      "Memory/Utility",
    "rvv_copy":     "Memory/Utility",
    "rvv_xor3":     "Memory/Utility",
    "vect_add":     "Memory/Utility",
    "vect_compare": "Memory/Utility",
    "store64":      "Memory/Utility",
    "x2_load64":    "Memory/Utility",
}

CATEGORY_COLORS: dict[str, str] = {
    "Multiplication": "#e63946",
    "Keccak/Hashing": "#457b9d",
    "Decode":         "#2a9d8f",
    "Memory/Utility": "#e9c46a",
    "Other":          "#adb5bd",
}

# ---------------------------------------------------------------------------
# Data Models
# ---------------------------------------------------------------------------


@dataclass
class TraceEntry:
    """A single row from a trace hotspot CSV."""

    function: str
    instructions: int
    percentage: float


@dataclass
class TraceData:
    """Parsed trace CSV data including variant metadata."""

    variant: str
    entries: list[TraceEntry]


# ---------------------------------------------------------------------------
# CSV Parsing
# ---------------------------------------------------------------------------


def parse_trace_csv(path: Path) -> TraceData:
    """Parse a trace hotspot CSV, skipping preamble lines.

    Extracts variant name from '# variant: ...' preamble line.
    Parses data rows as function,instructions,percentage.

    If the file does not exist, prints an error message and exits with code 1.
    """
    if not path.exists():
        print(f"Error: trace CSV not found: {path}", file=sys.stderr)
        sys.exit(1)

    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    preamble_lines: list[str] = []
    data_lines: list[str] = []
    for line in lines:
        if line.startswith("#"):
            preamble_lines.append(line)
        else:
            data_lines.append(line)

    # Extract variant name from preamble
    variant = ""
    for pline in preamble_lines:
        if pline.startswith("# variant:"):
            variant = pline[len("# variant:"):].strip()
            break

    # Parse data rows using csv module
    entries: list[TraceEntry] = []
    reader = csv.reader(data_lines)
    for row in reader:
        if len(row) < 3:
            continue
        func_name = row[0].strip()
        # Skip the header row
        if func_name == "function":
            continue
        instructions = int(row[1].strip())
        percentage = float(row[2].strip())
        entries.append(TraceEntry(function=func_name, instructions=instructions, percentage=percentage))

    return TraceData(variant=variant, entries=entries)


# ---------------------------------------------------------------------------
# Category Aggregation
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


def categorize_entries(entries: list[TraceEntry]) -> dict[str, int]:
    """Aggregate instruction counts by category.

    For each entry, classifies the function name and adds its instruction
    count to the corresponding category total.

    Returns a dict mapping category names to total instruction counts.
    """
    totals: dict[str, int] = defaultdict(int)
    for entry in entries:
        category = classify_function(entry.function)
        totals[category] += entry.instructions
    return dict(totals)


# ---------------------------------------------------------------------------
# Chart Rendering
# ---------------------------------------------------------------------------


def _autopct_threshold(threshold: float = 3.0):
    """Return an autopct function that only shows labels for slices >= threshold%."""
    def _fmt(pct: float) -> str:
        return f"{pct:.1f}%" if pct >= threshold else ""
    return _fmt


def render_pie_chart(
    category_totals: dict[str, int],
    variant_name: str,
    output_path: Path,
) -> None:
    """Render a single pie chart to PDF.

    Creates a figure with figsize=(6, 6), sorts categories by instruction
    count descending, applies CATEGORY_COLORS, and saves as PDF.
    Slices below 3% have their percentage label suppressed to avoid overlap.
    A legend is placed below the chart for clarity.
    """
    # Sort categories by instruction count descending
    sorted_categories = sorted(category_totals.items(), key=lambda x: x[1], reverse=True)
    labels = [cat for cat, _ in sorted_categories]
    sizes = [count for _, count in sorted_categories]
    colors = [CATEGORY_COLORS.get(cat, "#adb5bd") for cat in labels]

    fig, ax = plt.subplots(figsize=(7, 7))
    wedges, texts, autotexts = ax.pie(
        sizes,
        colors=colors,
        autopct=_autopct_threshold(3.0),
        startangle=90,
        pctdistance=0.8,
    )
    ax.set_title(variant_name, fontsize=13, fontweight="bold", pad=12)

    # Use a legend instead of inline labels to avoid text overlap on small slices
    ax.legend(
        wedges,
        labels,
        loc="lower center",
        bbox_to_anchor=(0.5, -0.12),
        ncol=min(len(labels), 3),
        fontsize="small",
        frameon=False,
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def render_combined_figure(
    all_data: list[tuple[str, dict[str, int]]],
    output_path: Path,
) -> None:
    """Render a combined subplot grid of pie charts to PDF.

    Creates a grid layout with up to 3 columns, renders each variant's pie
    chart in its subplot using consistent CATEGORY_COLORS, adds subplot titles
    and a shared legend, then saves as PDF.
    """
    n = len(all_data)
    if n == 0:
        return

    ncols = min(n, 3)
    nrows = ceil(n / ncols)

    fig, axes = plt.subplots(nrows, ncols, figsize=(5 * ncols, 5 * nrows))

    # Normalize axes to a flat array for uniform indexing
    if nrows == 1 and ncols == 1:
        axes_flat = [axes]
    else:
        import numpy as np

        axes_flat = np.array(axes).flatten().tolist()

    # Render each variant's pie chart
    for idx, (variant_name, category_totals) in enumerate(all_data):
        ax = axes_flat[idx]
        # Sort categories by instruction count descending
        sorted_categories = sorted(
            category_totals.items(), key=lambda x: x[1], reverse=True
        )
        labels = [cat for cat, _ in sorted_categories]
        sizes = [count for _, count in sorted_categories]
        colors = [CATEGORY_COLORS.get(cat, "#adb5bd") for cat in labels]

        ax.pie(
            sizes,
            colors=colors,
            autopct=_autopct_threshold(3.0),
            startangle=90,
            pctdistance=0.8,
        )
        ax.set_title(variant_name, fontsize=11, fontweight="bold")

    # Hide unused subplots if grid has empty cells
    for idx in range(n, len(axes_flat)):
        axes_flat[idx].set_visible(False)

    # Add shared legend using all known categories
    legend_handles = [
        plt.matplotlib.patches.Patch(color=color, label=cat)
        for cat, color in CATEGORY_COLORS.items()
    ]
    fig.legend(
        handles=legend_handles,
        loc="lower center",
        ncol=len(CATEGORY_COLORS),
        frameon=False,
        fontsize="small",
    )

    fig.tight_layout(rect=[0, 0.05, 1, 1])

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    """Build the argument parser for the pie chart generator."""
    parser = argparse.ArgumentParser(
        description="Generate PDF pie charts showing instruction breakdown by category."
    )
    parser.add_argument(
        "--track",
        choices=["scalar", "vector"],
        help="Track to process (scalar or vector).",
    )
    parser.add_argument(
        "--variant",
        type=str,
        help="Variant name within the selected track.",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Process all discovered variants across all tracks.",
    )
    parser.add_argument(
        "--level",
        type=int,
        default=128,
        help="Security level for CSV filename (default: 128).",
    )
    return parser


def validate_args(args: argparse.Namespace) -> None:
    """Validate CLI arguments: require --all OR both --track and --variant."""
    if args.all:
        return
    if args.track and args.variant:
        return
    print(
        "Error: must specify either --all or both --track and --variant.",
        file=sys.stderr,
    )
    sys.exit(2)


# ---------------------------------------------------------------------------
# Variant Discovery and Path Resolution
# ---------------------------------------------------------------------------


def variant_slug(variant: str) -> str:
    """Convert variant name to slug (underscores → hyphens)."""
    return variant.replace("_", "-")


def discover_all_variants() -> list[tuple[str, str]]:
    """Scan TRACES_DIR for all (track, variant) pairs.

    Expects directory structure: TRACES_DIR/{track}/{variant}/
    Returns a sorted list of (track, variant) tuples.
    """
    variants: list[tuple[str, str]] = []
    if not TRACES_DIR.is_dir():
        return variants
    for track_dir in sorted(TRACES_DIR.iterdir()):
        if not track_dir.is_dir():
            continue
        for variant_dir in sorted(track_dir.iterdir()):
            if not variant_dir.is_dir():
                continue
            variants.append((track_dir.name, variant_dir.name))
    return variants


def resolve_trace_path(track: str, variant: str, level: int = 128) -> Path:
    """Build the path to a trace CSV for a given track/variant/level."""
    slug = variant_slug(variant)
    return (
        TRACES_DIR
        / track
        / variant
        / f"trace_kem_L{level}_{slug}_hotspots_latest.csv"
    )


# ---------------------------------------------------------------------------
# Main Orchestration
# ---------------------------------------------------------------------------


def main() -> None:
    """Entry point for the pie chart generator."""
    parser = build_parser()
    args = parser.parse_args()
    validate_args(args)

    # Determine variant list
    if getattr(args, "all"):
        variants = discover_all_variants()
    else:
        variants = [(args.track, args.variant)]

    # Create output directory
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Process each variant
    all_data: list[tuple[str, dict[str, int]]] = []
    for track, variant in variants:
        csv_path = resolve_trace_path(track, variant, args.level)
        trace_data = parse_trace_csv(csv_path)

        # Handle empty CSV (no data rows) by skipping with a warning
        if not trace_data.entries:
            print(
                f"Warning: no data rows in {csv_path}, skipping.",
                file=sys.stderr,
            )
            continue

        # Categorize entries
        category_totals = categorize_entries(trace_data.entries)

        # Determine display name
        display_name = trace_data.variant if trace_data.variant else variant

        # Render individual chart
        individual_path = OUTPUT_DIR / f"pie_{variant_slug(variant)}.pdf"
        render_pie_chart(category_totals, display_name, individual_path)
        print(f"Generated: {individual_path}")

        # Collect for combined figure
        all_data.append((display_name, category_totals))

    # Render combined figure if multiple variants were processed
    if len(all_data) > 1:
        combined_path = OUTPUT_DIR / "pie_combined.pdf"
        render_combined_figure(all_data, combined_path)
        print(f"Generated: {combined_path}")


if __name__ == "__main__":
    main()
