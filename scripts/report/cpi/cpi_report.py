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

# Baseline variant name for each track used when computing speedups.
# Speedup = baseline_value / variant_value  (> 1 means variant is faster).
TRACK_BASELINES: dict[str, str] = {
    "scalar": "scalar_ref",
    "vector": "vector_ref",
}


def discover_histograms(level: int, base_dir: Path) -> list[Path]:
    """Find all histogram_kem_L{level}_*_latest.csv files.

    Searches variant directories under:
        {base_dir}/results/simulation/profiling/histograms/{variant}/

    Args:
        level: security level (128, 192, or 256).
        base_dir: project root directory.

    Returns:
        Sorted list of Paths to matching histogram CSV files.
    """
    histograms_root = base_dir / "results" / "simulation" / "profiling" / "histograms"
    pattern = f"histogram_kem_L{level}_*_latest.csv"

    found: list[Path] = []
    if histograms_root.is_dir():
        for variant_dir in sorted(histograms_root.iterdir()):
            if variant_dir.is_dir():
                for csv_file in variant_dir.glob(pattern):
                    found.append(csv_file)

    return sorted(found)


# ---------------------------------------------------------------------------
# Variant name / track extraction
# ---------------------------------------------------------------------------


def _extract_track_from_histogram(csv_path: Path) -> str:
    """Return 'vector' or 'scalar' based on the variant directory name.

    Under the new structure, the parent directory is the variant name
    (e.g., 'vector_ref', 'scalar_Zb'). Track is encoded in the prefix.
    """
    variant_dir = csv_path.parent.name
    if variant_dir.startswith("vector"):
        return "vector"
    return "scalar"


def _extract_variant_from_histogram(csv_path: Path) -> str:
    """Extract variant name from a histogram CSV path.

    Under the new structure, the parent directory IS the variant name
    (e.g., 'scalar_ref', 'vector_Zvk'). Falls back to filename parsing
    if needed.
    """
    # The parent directory is the variant name in the new structure
    return csv_path.parent.name


# ---------------------------------------------------------------------------
# Isolated KEM benchmark instret discovery
# ---------------------------------------------------------------------------


def _parse_kem_total_instret(csv_path: Path, level: int) -> int | None:
    """Parse isolated KEM CSV and return total instret for the given level.

    File format (comments start with '#'):
        level,op,instructions,codesize
        128,total,716983,45570

    Returns None if the file is unreadable or the level/total row is absent.
    """
    try:
        with open(csv_path, "r", encoding="utf-8") as f:
            reader = csv.reader(f)
            for row in reader:
                if not row or row[0].startswith("#"):
                    continue
                if row[0].strip() == "level":
                    continue  # header line
                try:
                    row_level = int(row[0])
                    row_op = row[1].strip()
                    row_instret = int(row[2])
                except (ValueError, IndexError):
                    continue
                if row_level == level and row_op == "total":
                    return row_instret
    except OSError:
        return None
    return None


def discover_kem_instrets(level: int, base_dir: Path) -> dict[str, int]:
    """Load per-variant total KEM instret from isolated benchmark CSVs.

    Searches results/simulation/benchmarks/components/poly_mul/{variant}/
    for isolated_kem_{variant}_latest.csv files.

    Using benchmark instret (keypair+encaps+decaps) rather than histogram
    total_instret avoids the ~1M benchmark-harness overhead that would
    otherwise inflate the denominator for fast variants and underestimate
    their speedup.

    Args:
        level: security level (128, 192, or 256).
        base_dir: project root directory.

    Returns:
        Dict mapping variant name -> total KEM instret at the given level.
        Only variants whose CSV exists and contains the requested level are
        included.
    """
    benchmarks_root = base_dir / "results" / "simulation" / "benchmarks" / "components" / "poly_mul"
    result: dict[str, int] = {}

    if not benchmarks_root.is_dir():
        return result

    for variant_dir in sorted(benchmarks_root.iterdir()):
        if not variant_dir.is_dir():
            continue
        for csv_file in variant_dir.glob("isolated_kem_*_latest.csv"):
            variant = variant_dir.name
            instret = _parse_kem_total_instret(csv_file, level)
            if instret is not None:
                result[variant] = instret

    return result


# ---------------------------------------------------------------------------
# Comparative table generation
# ---------------------------------------------------------------------------


@dataclass
class VariantRow:
    """A single row in the comparative report table."""

    variant: str
    track: str
    total_instret: int
    estimated_cycles: int
    cpi: float
    class_percentages: dict[str, float]  # class_name -> percentage
    speedup_instret: float | None = None  # baseline_instret / variant_instret
    speedup_cycles: float | None = None   # baseline_cycles  / variant_cycles


def _build_variant_row(
    variant: str,
    estimate: CycleEstimate,
    track: str = "",
    speedup_instret: float | None = None,
    speedup_cycles: float | None = None,
) -> VariantRow:
    """Build a VariantRow from a CycleEstimate."""
    class_pcts: dict[str, float] = {}
    for breakdown in estimate.per_class:
        class_pcts[breakdown.class_name] = breakdown.percentage

    return VariantRow(
        variant=variant,
        track=track,
        total_instret=estimate.total_instret,
        estimated_cycles=estimate.total_cycles,
        cpi=estimate.cpi,
        class_percentages=class_pcts,
        speedup_instret=speedup_instret,
        speedup_cycles=speedup_cycles,
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


def compute_speedups(
    estimates: dict[str, CycleEstimate],
    variant_tracks: dict[str, str],
    kem_instrets: dict[str, int] | None = None,
) -> dict[str, tuple[float | None, float | None]]:
    """Compute instret and cycle speedups relative to the per-track baseline.

    speedup_instret: uses actual KEM total instret from isolated benchmarks when
        available (kem_instrets), otherwise falls back to histogram total_instret.
        Benchmark instret avoids the harness-overhead inflation that makes fast
        variants look slower than they are.
    speedup_cycles: always computed from the CPI-model estimated cycles.

    Speedup > 1 means the variant is faster than the baseline.
    Baseline variants themselves receive speedup = 1.000.

    Args:
        estimates: variant -> CycleEstimate.
        variant_tracks: variant -> 'scalar' or 'vector'.
        kem_instrets: optional variant -> total KEM instret from benchmark CSVs.

    Returns:
        Dict mapping variant -> (speedup_instret, speedup_cycles).
        Either value is None when data is unavailable.
    """
    def _instret(variant: str) -> int:
        """Return the best available instret for a variant."""
        if kem_instrets and variant in kem_instrets:
            return kem_instrets[variant]
        return estimates[variant].total_instret if variant in estimates else 0

    # Identify per-track baselines
    baseline_instrets: dict[str, int] = {}
    baseline_cycles: dict[str, int] = {}
    for variant, track in variant_tracks.items():
        baseline_name = TRACK_BASELINES.get(track)
        if baseline_name and variant == baseline_name and variant in estimates:
            baseline_instrets[track] = _instret(variant)
            baseline_cycles[track] = estimates[variant].total_cycles

    result: dict[str, tuple[float | None, float | None]] = {}
    for variant, estimate in estimates.items():
        track = variant_tracks.get(variant, "")
        b_instret = baseline_instrets.get(track)
        b_cycles = baseline_cycles.get(track)
        if b_instret is None or b_cycles is None:
            result[variant] = (None, None)
        else:
            v_instret = _instret(variant)
            sp_instret = (
                round(b_instret / v_instret, 3)
                if v_instret > 0
                else None
            )
            sp_cycles = (
                round(b_cycles / estimate.total_cycles, 3)
                if estimate.total_cycles > 0
                else None
            )
            result[variant] = (sp_instret, sp_cycles)

    return result


def generate_comparative_table(
    estimates: dict[str, CycleEstimate],
    speedups: dict[str, tuple[float | None, float | None]] | None = None,
    variant_tracks: dict[str, str] | None = None,
) -> str:
    """Generate the comparative report CSV content.

    Args:
        estimates: mapping of variant name -> CycleEstimate.
        speedups: optional per-variant (speedup_instret, speedup_cycles) pairs.
        variant_tracks: optional variant -> track mapping (used for row track field).

    Returns:
        CSV string with header and data rows sorted by estimated_cycles ascending.
    """
    # Build rows
    rows: list[VariantRow] = []
    for variant, estimate in estimates.items():
        sp = speedups.get(variant, (None, None)) if speedups else (None, None)
        track = variant_tracks.get(variant, "") if variant_tracks else ""
        rows.append(
            _build_variant_row(
                variant,
                estimate,
                track=track,
                speedup_instret=sp[0],
                speedup_cycles=sp[1],
            )
        )

    # Sort by estimated_cycles ascending
    rows.sort(key=lambda r: r.estimated_cycles)

    # Generate CSV
    output = io.StringIO()
    # Header — speedup columns placed immediately after 'track'
    class_cols = [f"{cls}%" for cls in REPORT_CLASSES]
    has_speedups = speedups is not None
    header = ["variant", "track"]
    if has_speedups:
        header += ["speedup_instret", "speedup_cycles"]
    header += ["total_instret", "estimated_cycles", "CPI"] + class_cols
    output.write(",".join(header) + "\n")

    # Data rows
    for row in rows:
        formatted_pcts = _mark_dominant_class(row.class_percentages)
        fields = [row.variant, row.track]
        if has_speedups:
            fields.append(
                f"{row.speedup_instret:.3f}" if row.speedup_instret is not None else ""
            )
            fields.append(
                f"{row.speedup_cycles:.3f}" if row.speedup_cycles is not None else ""
            )
        fields += [
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
    variant_tracks: dict[str, str] | None = None,
    kem_instrets: dict[str, int] | None = None,
) -> str:
    """Generate comparative table and append delta columns if previous report exists.

    Args:
        estimates: current variant -> CycleEstimate mapping.
        output_dir: directory where reports are stored.
        level: security level.
        variant_tracks: optional variant -> track mapping for speedup computation.
        kem_instrets: optional variant -> total KEM instret from benchmark CSVs;
            used to compute accurate speedup_instret free of harness overhead.

    Returns:
        Complete CSV report content (with or without delta columns).
    """
    # Compute speedups when track information is available
    speedups = (
        compute_speedups(estimates, variant_tracks, kem_instrets=kem_instrets)
        if variant_tracks
        else None
    )

    # Write base comparative table (includes speedup columns when available)
    base_csv = generate_comparative_table(estimates, speedups=speedups, variant_tracks=variant_tracks)

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
        searched_dir = str(base_dir / "results" / "simulation" / "profiling" / "histograms")
        print(
            f"Error: no histogram CSVs found for level {level}. "
            f"Searched directory: {searched_dir}",
            file=sys.stderr,
        )
        return 1

    # Process each histogram
    estimates: dict[str, CycleEstimate] = {}
    variant_tracks: dict[str, str] = {}
    for hist_path in histograms:
        try:
            csv_content = hist_path.read_text(encoding="utf-8")
            instruction_mix = parse_instruction_csv(csv_content, filename=str(hist_path))
            estimate = compute_cycles(instruction_mix, cost_table)
            variant = _extract_variant_from_histogram(hist_path)
            track = _extract_track_from_histogram(hist_path)
            estimates[variant] = estimate
            variant_tracks[variant] = track
        except (OSError, ValueError) as e:
            print(f"Warning: skipping {hist_path}: {e}", file=sys.stderr)
            continue

    if not estimates:
        print(
            f"Error: all histogram CSVs for level {level} failed to process.",
            file=sys.stderr,
        )
        return 1

    # Load actual KEM instret from isolated benchmark CSVs (more accurate than histogram)
    kem_instrets = discover_kem_instrets(level, base_dir)
    if kem_instrets:
        print(
            f"Using benchmark KEM instret for {len(kem_instrets)} variant(s): "
            + ", ".join(sorted(kem_instrets)),
            file=sys.stderr,
        )

    # Generate report
    output_dir = base_dir / "results" / "simulation" / "profiling" / "cpi" / "reports"
    output_dir.mkdir(parents=True, exist_ok=True)

    report_csv = generate_report_with_deltas(
        estimates, output_dir, level, variant_tracks, kem_instrets
    )

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
