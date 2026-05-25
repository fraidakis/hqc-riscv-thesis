"""Comparative CPI Report: multi-variant analysis and delta tracking.

Discovers histogram CSVs across scalar/vector tracks, computes cycle estimates
for each variant, produces a comparative table with dominant class marking,
and optionally computes deltas against a previous report.
"""

from __future__ import annotations

import csv
import io
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

# Ensure project root is on sys.path for direct script invocation
_PROJECT_ROOT = str(Path(__file__).resolve().parent.parent.parent.parent)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from scripts.report.cpi.cost_table import CostTable
from scripts.report.cpi.cpi_model import (
    CycleEstimate,
    compute_cycles,
    parse_instruction_csv,
)


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------


@dataclass
class DeltaRow:
    """Signed difference between current and previous report for a variant."""

    delta_cycles: int  # negative = improvement
    delta_cpi: float


# ---------------------------------------------------------------------------
# Histogram discovery
# ---------------------------------------------------------------------------

# Canonical instruction class columns in report order
REPORT_CLASSES: list[str] = [
    "ALU_1CYCLE",
    "LOAD",
    "STORE",
    "MUL",
    "DIV",
    "CLMUL",
    "VECTOR_ALU",
    "VECTOR_MUL",
    "VECTOR_MEM",
    "VECTOR_CRYPTO",
    "VECTOR_PERM",
]


def discover_histograms(level: int, base_dir: Path) -> list[Path]:
    """Find all histogram_kem_L{level}_*_latest.csv files.

    Searches both scalar and vector track directories under:
        {base_dir}/analysis/profiles/histograms/{scalar,vector}/*/

    Args:
        level: security level (128, 192, or 256).
        base_dir: project root directory.

    Returns:
        Sorted list of Paths to matching histogram CSV files.
    """
    histograms_root = base_dir / "analysis" / "profiles" / "histograms"
    pattern = f"histogram_kem_L{level}_*_latest.csv"

    found: list[Path] = []
    for track in ("scalar", "vector"):
        track_dir = histograms_root / track
        if track_dir.is_dir():
            # Search in each variant subdirectory
            for variant_dir in sorted(track_dir.iterdir()):
                if variant_dir.is_dir():
                    for csv_file in variant_dir.glob(pattern):
                        found.append(csv_file)

    return sorted(found)


# ---------------------------------------------------------------------------
# Variant name extraction
# ---------------------------------------------------------------------------


def _extract_variant_from_histogram(csv_path: Path) -> str:
    """Extract variant name from a histogram CSV filename.

    Expects: histogram_kem_L{level}_{variant}_latest.csv
    Returns the variant portion (e.g., 'karatsuba-zbc', 'ref-scalar').
    """
    stem = csv_path.stem  # e.g., histogram_kem_L128_karatsuba-zbc_latest
    parts = stem.split("_")
    # Find the level marker and extract everything between it and 'latest'
    for i, part in enumerate(parts):
        if part.startswith("L") and part[1:].isdigit():
            remaining = parts[i + 1:]
            if remaining and remaining[-1] == "latest":
                remaining = remaining[:-1]
            if remaining:
                return "_".join(remaining)
    return stem


# ---------------------------------------------------------------------------
# Comparative table generation
# ---------------------------------------------------------------------------


@dataclass
class VariantRow:
    """A single row in the comparative report table."""

    variant: str
    total_instret: int
    estimated_cycles: int
    cpi: float
    class_percentages: dict[str, float]  # class_name -> percentage


def _build_variant_row(
    variant: str,
    estimate: CycleEstimate,
) -> VariantRow:
    """Build a VariantRow from a CycleEstimate."""
    class_pcts: dict[str, float] = {}
    for breakdown in estimate.per_class:
        class_pcts[breakdown.class_name] = breakdown.percentage

    return VariantRow(
        variant=variant,
        total_instret=estimate.total_instret,
        estimated_cycles=estimate.total_cycles,
        cpi=estimate.cpi,
        class_percentages=class_pcts,
    )


def _mark_dominant_class(pcts: dict[str, float]) -> dict[str, str]:
    """Format percentage values, appending '*' to the dominant (highest) class.

    Returns a dict mapping class_name -> formatted string (e.g., "32.1*" or "12.0").
    """
    if not pcts:
        return {}

    # Find the dominant class (highest percentage)
    max_class = max(pcts, key=lambda k: pcts[k])

    formatted: dict[str, str] = {}
    for cls in REPORT_CLASSES:
        pct = pcts.get(cls, 0.0)
        value = f"{pct:.1f}"
        if cls == max_class and pct > 0.0:
            value += "*"
        formatted[cls] = value

    return formatted


def generate_comparative_table(
    estimates: dict[str, CycleEstimate],
) -> str:
    """Generate the comparative report CSV content.

    Args:
        estimates: mapping of variant name -> CycleEstimate.

    Returns:
        CSV string with header and data rows sorted by estimated_cycles ascending.
    """
    # Build rows
    rows: list[VariantRow] = []
    for variant, estimate in estimates.items():
        rows.append(_build_variant_row(variant, estimate))

    # Sort by estimated_cycles ascending
    rows.sort(key=lambda r: r.estimated_cycles)

    # Generate CSV
    output = io.StringIO()
    # Header
    class_cols = [f"{cls}%" for cls in REPORT_CLASSES]
    header = ["variant", "total_instret", "estimated_cycles", "CPI"] + class_cols
    output.write(",".join(header) + "\n")

    # Data rows
    for row in rows:
        formatted_pcts = _mark_dominant_class(row.class_percentages)
        fields = [
            row.variant,
            str(row.total_instret),
            str(row.estimated_cycles),
            f"{row.cpi:.2f}",
        ]
        for cls in REPORT_CLASSES:
            fields.append(formatted_pcts.get(cls, "0.0"))
        output.write(",".join(fields) + "\n")

    return output.getvalue()


# ---------------------------------------------------------------------------
# Delta computation
# ---------------------------------------------------------------------------


def compute_deltas(
    current: dict[str, CycleEstimate],
    previous: dict[str, CycleEstimate],
) -> dict[str, DeltaRow]:
    """Compute signed differences for variants present in both reports.

    Args:
        current: current report estimates by variant.
        previous: previous report estimates by variant.

    Returns:
        Dict mapping variant -> DeltaRow for common variants.
        Negative delta_cycles means improvement.
    """
    common_variants = set(current.keys()) & set(previous.keys())
    deltas: dict[str, DeltaRow] = {}

    for variant in common_variants:
        cur = current[variant]
        prev = previous[variant]
        deltas[variant] = DeltaRow(
            delta_cycles=cur.total_cycles - prev.total_cycles,
            delta_cpi=round(cur.cpi - prev.cpi, 2),
        )

    return deltas


# ---------------------------------------------------------------------------
# Previous report discovery and parsing
# ---------------------------------------------------------------------------


def _find_previous_report(output_dir: Path, current_filename: str) -> Path | None:
    """Find the most recent previous report CSV in the output directory.

    Reports are named with timestamps: cpi_report_L{level}_{timestamp}.csv
    Returns the most recent file that is NOT the current one, or None.
    """
    if not output_dir.is_dir():
        return None

    report_files = sorted(output_dir.glob("cpi_report_L*_*.csv"))
    # Exclude current file, take the most recent remaining
    candidates = [f for f in report_files if f.name != current_filename]
    if candidates:
        return candidates[-1]  # last in sorted order = most recent timestamp
    return None


def _parse_previous_report(report_path: Path) -> dict[str, CycleEstimate]:
    """Parse a previous comparative report CSV to extract estimates per variant.

    Returns a dict mapping variant -> CycleEstimate (with minimal fields for delta).
    """
    estimates: dict[str, CycleEstimate] = {}

    with open(report_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            variant = row.get("variant", "")
            if not variant:
                continue

            total_instret = int(row.get("total_instret", 0))
            estimated_cycles = int(row.get("estimated_cycles", 0))
            cpi_str = row.get("CPI", "0.0")
            cpi = float(cpi_str)

            estimates[variant] = CycleEstimate(
                total_cycles=estimated_cycles,
                total_instret=total_instret,
                cpi=cpi,
                per_class=[],
                vector_summary=None,
            )

    return estimates


# ---------------------------------------------------------------------------
# Report output with deltas
# ---------------------------------------------------------------------------


def generate_report_with_deltas(
    estimates: dict[str, CycleEstimate],
    output_dir: Path,
    level: int,
) -> str:
    """Generate comparative table and append delta columns if previous report exists.

    Args:
        estimates: current variant -> CycleEstimate mapping.
        output_dir: directory where reports are stored.
        level: security level.

    Returns:
        Complete CSV report content (with or without delta columns).
    """
    # Write base comparative table
    base_csv = generate_comparative_table(estimates)

    # Find previous report
    now = datetime.now()
    current_filename = f"cpi_report_L{level}_{now.strftime('%Y-%m-%d_%H-%M')}.csv"
    previous_path = _find_previous_report(output_dir, current_filename)

    if previous_path is None:
        return base_csv

    # Parse previous and compute deltas
    try:
        previous_estimates = _parse_previous_report(previous_path)
    except (OSError, ValueError, KeyError):
        # If previous report is unreadable, skip deltas
        return base_csv

    deltas = compute_deltas(estimates, previous_estimates)
    if not deltas:
        return base_csv

    # Append delta columns to existing CSV
    output = io.StringIO()
    lines = base_csv.strip().split("\n")

    # Augment header
    header = lines[0] + ",Δ_estimated_cycles,Δ_CPI"
    output.write(header + "\n")

    # Augment data rows
    for line in lines[1:]:
        variant = line.split(",")[0]
        if variant in deltas:
            delta = deltas[variant]
            sign_cycles = f"+{delta.delta_cycles}" if delta.delta_cycles > 0 else str(delta.delta_cycles)
            sign_cpi = f"+{delta.delta_cpi:.2f}" if delta.delta_cpi > 0 else f"{delta.delta_cpi:.2f}"
            line += f",{sign_cycles},{sign_cpi}"
        else:
            line += ",,"
        output.write(line + "\n")

    return output.getvalue()


# ---------------------------------------------------------------------------
# Main report entry point
# ---------------------------------------------------------------------------


def run_report(level: int, base_dir: Path, cost_table: CostTable) -> int:
    """Execute comparative report mode.

    Discovers histograms, computes estimates, generates comparative table,
    writes output CSV, and prints to stdout.

    Args:
        level: security level (128, 192, or 256).
        base_dir: project root directory.
        cost_table: the CostTable to use for estimation.

    Returns:
        Exit code (0 for success, 1 for errors).
    """
    # Discover histograms
    histograms = discover_histograms(level, base_dir)

    if not histograms:
        searched_dirs = [
            str(base_dir / "analysis" / "profiles" / "histograms" / "scalar"),
            str(base_dir / "analysis" / "profiles" / "histograms" / "vector"),
        ]
        print(
            f"Error: no histogram CSVs found for level {level}. "
            f"Searched directories: {', '.join(searched_dirs)}",
            file=sys.stderr,
        )
        return 1

    # Process each histogram
    estimates: dict[str, CycleEstimate] = {}
    for hist_path in histograms:
        try:
            csv_content = hist_path.read_text(encoding="utf-8")
            instruction_mix = parse_instruction_csv(csv_content, filename=str(hist_path))
            estimate = compute_cycles(instruction_mix, cost_table)
            variant = _extract_variant_from_histogram(hist_path)
            estimates[variant] = estimate
        except (OSError, ValueError) as e:
            print(f"Warning: skipping {hist_path}: {e}", file=sys.stderr)
            continue

    if not estimates:
        print(
            f"Error: all histogram CSVs for level {level} failed to process.",
            file=sys.stderr,
        )
        return 1

    # Generate report
    output_dir = base_dir / "analysis" / "profiles" / "cpi" / "reports"
    output_dir.mkdir(parents=True, exist_ok=True)

    report_csv = generate_report_with_deltas(estimates, output_dir, level)

    # Write output file
    now = datetime.now()
    filename = f"cpi_report_L{level}_{now.strftime('%Y-%m-%d_%H-%M')}.csv"
    output_path = output_dir / filename
    output_path.write_text(report_csv, encoding="utf-8")

    # Print to stdout
    print(report_csv, end="")
    print(f"\nReport written to: {output_path}", file=sys.stderr)

    # Emit any unmatched mnemonic warnings
    cost_table.emit_unmatched_warnings()

    return 0
