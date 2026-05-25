"""CPI Model: cycle estimation from instruction mix histograms.

Computes estimated clock cycles, CPI ratios, and per-class breakdowns
from Spike histogram CSVs and a configurable cost table.
"""

from __future__ import annotations

import argparse
import csv
import io
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

# Ensure project root is on sys.path for direct script invocation
_PROJECT_ROOT = str(Path(__file__).resolve().parent.parent.parent.parent)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from scripts.report.cpi.cost_table import CostTable


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------


@dataclass
class ClassBreakdown:
    """Per-instruction-class contribution to total cycles."""

    class_name: str
    total_instructions: int
    total_cycles: int
    percentage: float  # of total cycles, rounded to 1 dp


@dataclass
class VectorSummary:
    """Aggregated vector instruction statistics."""

    total_vector_instructions: int
    total_vector_cycles: int
    vector_cpi: float  # rounded to 2 decimal places


@dataclass
class CycleEstimate:
    """Result of cycle estimation from an instruction mix."""

    total_cycles: int
    total_instret: int
    cpi: float  # rounded to 2 decimal places
    per_class: list[ClassBreakdown]  # sorted desc by cycle contribution
    vector_summary: VectorSummary | None


# ---------------------------------------------------------------------------
# Core computation
# ---------------------------------------------------------------------------


def compute_cycles(
    instruction_mix: dict[str, int],
    cost_table: CostTable,
) -> CycleEstimate:
    """Pure function: compute total cycles, CPI, per-class breakdown.

    Args:
        instruction_mix: mapping of mnemonic -> execution count
        cost_table: the CostTable used for classification

    Returns:
        CycleEstimate with all fields populated.
    """
    # Accumulate per-class totals
    class_instructions: dict[str, int] = {}
    class_cycles: dict[str, int] = {}

    for mnemonic, count in instruction_mix.items():
        class_name, cost = cost_table.classify(mnemonic)
        class_instructions[class_name] = class_instructions.get(class_name, 0) + count
        class_cycles[class_name] = class_cycles.get(class_name, 0) + (count * cost)

    total_cycles = sum(class_cycles.values())
    total_instret = sum(class_instructions.values())

    # CPI ratio
    if total_instret == 0:
        cpi = 0.0
    else:
        cpi = round(total_cycles / total_instret, 2)

    # Per-class breakdown sorted descending by cycle contribution
    per_class: list[ClassBreakdown] = []
    for class_name in class_instructions:
        cycles = class_cycles[class_name]
        percentage = round((cycles / total_cycles) * 100, 1) if total_cycles > 0 else 0.0
        per_class.append(
            ClassBreakdown(
                class_name=class_name,
                total_instructions=class_instructions[class_name],
                total_cycles=cycles,
                percentage=percentage,
            )
        )
    per_class.sort(key=lambda b: b.total_cycles, reverse=True)

    # Vector summary
    vector_instructions = 0
    vector_cycles = 0
    for class_name in class_instructions:
        if cost_table.is_vector(class_name):
            vector_instructions += class_instructions[class_name]
            vector_cycles += class_cycles[class_name]

    vector_summary: VectorSummary | None = None
    if vector_instructions > 0:
        vector_cpi = round(vector_cycles / vector_instructions, 2)
        vector_summary = VectorSummary(
            total_vector_instructions=vector_instructions,
            total_vector_cycles=vector_cycles,
            vector_cpi=vector_cpi,
        )

    return CycleEstimate(
        total_cycles=total_cycles,
        total_instret=total_instret,
        cpi=cpi,
        per_class=per_class,
        vector_summary=vector_summary,
    )


# ---------------------------------------------------------------------------
# CSV parsing
# ---------------------------------------------------------------------------


def parse_instruction_csv(csv_content: str, filename: str = "<input>") -> dict[str, int]:
    """Parse an instruction mix CSV (instruction,count,percentage format).

    Args:
        csv_content: the CSV file content as a string.
        filename: filename used in error messages.

    Returns:
        dict mapping mnemonic -> count.

    Raises:
        ValueError: if required columns are missing, data is empty,
                    or rows contain non-numeric/negative counts.
    """
    # Strip comment lines (starting with #) before parsing
    filtered_lines = [
        line for line in csv_content.splitlines()
        if not line.strip().startswith("#")
    ]
    filtered_content = "\n".join(filtered_lines)

    reader = csv.reader(io.StringIO(filtered_content))

    # Read header
    try:
        header = next(reader)
    except StopIteration:
        raise ValueError(f"{filename}: CSV file is empty (no header row)")

    # Normalize header column names
    header_lower = [col.strip().lower() for col in header]

    if "instruction" not in header_lower or "count" not in header_lower:
        raise ValueError(
            f"{filename}: missing required columns. "
            f"Expected 'instruction' and 'count', got: {header}"
        )

    instr_idx = header_lower.index("instruction")
    count_idx = header_lower.index("count")

    # Parse data rows
    instruction_mix: dict[str, int] = {}
    row_num = 1  # 1-indexed (header is row 0)

    for row in reader:
        row_num += 1
        if not row or all(cell.strip() == "" for cell in row):
            continue  # skip blank lines

        if len(row) <= max(instr_idx, count_idx):
            raise ValueError(
                f"{filename}: row {row_num} has insufficient columns: {row}"
            )

        mnemonic = row[instr_idx].strip()
        count_str = row[count_idx].strip()

        if not mnemonic:
            continue  # skip rows with empty mnemonic

        # Validate count
        try:
            count = int(count_str)
        except ValueError:
            raise ValueError(
                f"{filename}: row {row_num} has non-numeric count value '{count_str}' "
                f"for instruction '{mnemonic}'"
            )

        if count < 0:
            raise ValueError(
                f"{filename}: row {row_num} has negative count value '{count}' "
                f"for instruction '{mnemonic}'"
            )

        instruction_mix[mnemonic] = instruction_mix.get(mnemonic, 0) + count

    if not instruction_mix:
        raise ValueError(
            f"{filename}: CSV contains no instruction data (zero valid data rows)"
        )

    return instruction_mix


# ---------------------------------------------------------------------------
# Terminal output formatting
# ---------------------------------------------------------------------------


def format_terminal_output(estimate: CycleEstimate, cost_table: CostTable, instruction_mix: dict[str, int]) -> str:
    """Format a human-readable table for terminal display.

    Shows mnemonic, count, CPI (cost), and cycles for each instruction,
    followed by a per-class summary and totals.
    """
    lines: list[str] = []

    # Header
    lines.append(f"{'Mnemonic':<20} {'Count':>12} {'CPI':>6} {'Cycles':>14}")
    lines.append("-" * 56)

    # Per-instruction rows sorted by cycles descending
    instr_rows: list[tuple[str, int, int, int]] = []
    for mnemonic, count in instruction_mix.items():
        _, cost = cost_table.classify(mnemonic)
        cycles = count * cost
        instr_rows.append((mnemonic, count, cost, cycles))

    instr_rows.sort(key=lambda r: r[3], reverse=True)

    for mnemonic, count, cost, cycles in instr_rows:
        lines.append(f"{mnemonic:<20} {count:>12,} {cost:>6} {cycles:>14,}")

    # Separator and class summary
    lines.append("")
    lines.append("Per-Class Breakdown:")
    lines.append(f"{'Class':<20} {'Instructions':>12} {'Cycles':>14} {'%':>7}")
    lines.append("-" * 56)

    for breakdown in estimate.per_class:
        lines.append(
            f"{breakdown.class_name:<20} "
            f"{breakdown.total_instructions:>12,} "
            f"{breakdown.total_cycles:>14,} "
            f"{breakdown.percentage:>6.1f}%"
        )

    # Totals
    lines.append("-" * 56)
    lines.append(
        f"{'TOTAL':<20} {estimate.total_instret:>12,} {estimate.total_cycles:>14,} {'100.0':>6}%"
    )
    lines.append(f"CPI: {estimate.cpi:.2f}")

    # Vector summary
    if estimate.vector_summary:
        vs = estimate.vector_summary
        lines.append("")
        lines.append("Vector Summary:")
        lines.append(f"  Vector Instructions: {vs.total_vector_instructions:,}")
        lines.append(f"  Vector Cycles:       {vs.total_vector_cycles:,}")
        lines.append(f"  Vector CPI:          {vs.vector_cpi:.2f}")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CSV output formatting
# ---------------------------------------------------------------------------


def _get_git_commit() -> str:
    """Get short git commit hash, or 'unknown' if not in a git repo."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except (subprocess.TimeoutExpired, FileNotFoundError):
        pass
    return "unknown"


def format_csv_output(
    estimate: CycleEstimate,
    variant: str,
    level: int,
    cost_table_name: str,
    track: str,
    base_dir: Path | None = None,
) -> str:
    """Format cycle estimate as CSV with metadata header comments.

    Args:
        estimate: the CycleEstimate result.
        variant: variant name (e.g., "karatsuba_zbc").
        level: security level (128, 192, or 256).
        cost_table_name: name of the cost table used.
        track: "scalar" or "vector".
        base_dir: base directory for output (defaults to project root analysis/).

    Returns:
        The formatted CSV content as a string.
    """
    lines: list[str] = []

    # Metadata header comments
    now = datetime.now()
    commit = _get_git_commit()
    lines.append(f"# date: {now.strftime('%Y-%m-%d %H:%M')}")
    lines.append(f"# commit: {commit}")
    lines.append(f"# variant: {variant}")
    lines.append(f"# level: {level}")
    lines.append(f"# cost_table: {cost_table_name}")

    # CSV header
    lines.append("class,instructions,cycles,percentage")

    # Per-class rows
    for breakdown in estimate.per_class:
        lines.append(
            f"{breakdown.class_name},{breakdown.total_instructions},"
            f"{breakdown.total_cycles},{breakdown.percentage}"
        )

    # TOTAL row
    lines.append(f"TOTAL,{estimate.total_instret},{estimate.total_cycles},100.0")

    # CPI row
    lines.append(f"CPI,,,{estimate.cpi:.2f}")

    # Vector summary (if present)
    if estimate.vector_summary:
        vs = estimate.vector_summary
        lines.append(f"VECTOR_TOTAL,{vs.total_vector_instructions},{vs.total_vector_cycles},")
        lines.append(f"VECTOR_CPI,,,{vs.vector_cpi:.2f}")

    return "\n".join(lines) + "\n"


def write_csv_output(
    csv_content: str,
    variant: str,
    track: str,
    base_dir: Path | None = None,
) -> Path:
    """Write CSV output to the standard directory structure.

    Writes to: {base_dir}/analysis/profiles/cpi/{track}/{variant}/
    Creates directories if they don't exist.

    Args:
        csv_content: formatted CSV string.
        variant: variant name.
        track: "scalar" or "vector".
        base_dir: project root directory (defaults to cwd).

    Returns:
        Path to the written file.
    """
    if base_dir is None:
        base_dir = Path.cwd()

    output_dir = base_dir / "analysis" / "profiles" / "cpi" / track / variant
    output_dir.mkdir(parents=True, exist_ok=True)

    # Filename with timestamp
    now = datetime.now()
    filename = f"cpi_{variant}_{now.strftime('%Y-%m-%d_%H-%M')}.csv"
    output_path = output_dir / filename

    output_path.write_text(csv_content, encoding="utf-8")
    return output_path


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _extract_variant_from_path(csv_path: Path) -> str:
    """Extract variant name from the CSV filename.

    Expects filenames like: histogram_kem_L128_karatsuba_zbc_latest.csv
    Falls back to the stem (filename without extension) if pattern doesn't match.
    """
    stem = csv_path.stem
    # Try to extract variant from histogram naming convention
    # Pattern: histogram_kem_L{level}_{variant}_latest
    parts = stem.split("_")
    # Find the level marker (e.g., "L128") and take everything after it
    # until "latest" (if present)
    for i, part in enumerate(parts):
        if part.startswith("L") and part[1:].isdigit():
            # Everything after the level and before "latest" is the variant
            remaining = parts[i + 1 :]
            if remaining and remaining[-1] == "latest":
                remaining = remaining[:-1]
            if remaining:
                return "_".join(remaining)
    # Fallback: use the full stem
    return stem


def _determine_track_from_path(csv_path: Path) -> str:
    """Determine track (scalar/vector) from the CSV file path.

    Looks for 'vector' or 'scalar' in the path components.
    Defaults to 'scalar' if neither is found.
    """
    path_str = str(csv_path.resolve()).lower()
    if "/vector/" in path_str or "\\vector\\" in path_str:
        return "vector"
    return "scalar"


def _build_parser() -> argparse.ArgumentParser:
    """Build the argparse parser for cpi_model CLI."""
    parser = argparse.ArgumentParser(
        prog="cpi_model.py",
        description="CPI Model: estimate clock cycles from instruction mix histograms.",
    )
    parser.add_argument(
        "input_csv",
        type=Path,
        help="Path to instruction mix CSV (instruction,count,percentage format)",
    )
    parser.add_argument(
        "--cost-table",
        type=Path,
        default=None,
        help="Path to JSON cost table (default: built-in Rocket-like)",
    )
    parser.add_argument(
        "--output",
        choices=["terminal", "csv"],
        default="terminal",
        help="Output format (default: terminal)",
    )
    parser.add_argument(
        "--level",
        type=int,
        choices=[128, 192, 256],
        default=128,
        help="Security level for report mode (default: 128)",
    )
    parser.add_argument(
        "--report",
        action="store_true",
        help="Run comparative report across all variants",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    """CLI entry point for cpi_model.

    Args:
        argv: command-line arguments (defaults to sys.argv[1:] if None).

    Returns:
        Exit code (0 for success, 1 for errors).
    """
    parser = _build_parser()
    args = parser.parse_args(argv)

    # Report mode: comparative analysis across all variants
    if args.report:
        from scripts.report.cpi.cpi_report import run_report

        # Load cost table for report mode
        try:
            if args.cost_table:
                report_cost_table = CostTable.from_json(args.cost_table)
            else:
                report_cost_table = CostTable.default()
        except (FileNotFoundError, ValueError) as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1

        base_dir = Path.cwd()
        return run_report(args.level, base_dir, report_cost_table)

    # Load cost table
    try:
        if args.cost_table:
            cost_table = CostTable.from_json(args.cost_table)
        else:
            cost_table = CostTable.default()
    except (FileNotFoundError, ValueError) as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    # Read and parse input CSV
    input_path: Path = args.input_csv
    if not input_path.exists():
        print(f"Error: input CSV file not found: {input_path}", file=sys.stderr)
        return 1

    try:
        csv_content = input_path.read_text(encoding="utf-8")
    except OSError as e:
        print(f"Error: cannot read input CSV: {e}", file=sys.stderr)
        return 1

    try:
        instruction_mix = parse_instruction_csv(csv_content, filename=str(input_path))
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    # Compute cycle estimate
    estimate = compute_cycles(instruction_mix, cost_table)

    # Emit unmatched mnemonic warnings
    cost_table.emit_unmatched_warnings()

    # Output results
    if args.output == "terminal":
        output = format_terminal_output(estimate, cost_table, instruction_mix)
        print(output)
    elif args.output == "csv":
        variant = _extract_variant_from_path(input_path)
        track = _determine_track_from_path(input_path)
        level = args.level

        csv_output = format_csv_output(
            estimate=estimate,
            variant=variant,
            level=level,
            cost_table_name=cost_table.name or "custom",
            track=track,
        )

        output_path = write_csv_output(
            csv_content=csv_output,
            variant=variant,
            track=track,
        )
        print(f"Output written to: {output_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
