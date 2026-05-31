"""CSV I/O helpers for HQC-RISC-V benchmark results.

Provides functions for reading simulation and hardware benchmark CSVs,
including metadata parsing for both old single-line and new multi-line
comment header formats.
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path


def read_csv(path: Path) -> dict[tuple[int, str], int]:
    """Read a simulation benchmark CSV into {(level, op): instructions}.

    Reads CSV files produced by the Spike ISS benchmark runner. Comment lines
    (starting with ``#``) are skipped. Each data row must contain ``level``,
    ``op``, and ``instructions`` columns.

    Args:
        path: Path to the benchmark CSV file.

    Returns:
        Mapping from (security_level, operation_name) to instruction count.

    Raises:
        SystemExit: If *path* does not exist.
    """
    if not path.exists():
        print(f"Error: expected file not found: {path}", file=sys.stderr)
        sys.exit(1)
    result: dict[tuple[int, str], int] = {}
    with path.open() as f:
        reader = csv.DictReader(row for row in f if not row.startswith("#"))
        for row in reader:
            try:
                level = int(row["level"])
                op = row["op"].strip()
                instret = int(row["instructions"])
                result[(level, op)] = instret
            except (KeyError, ValueError):
                continue
    return result


def parse_metadata(lines: list[str]) -> dict[str, str]:
    """Parse metadata from CSV comment header lines.

    Handles two formats:
      - Old single-line: ``# key=val,key=val,...``
      - New multi-line: ``# key: value`` (one per line)

    Only lines starting with ``#`` are processed; iteration stops at the
    first non-comment line.

    Args:
        lines: Raw lines from the beginning of a CSV file (including the
            leading ``#`` characters).

    Returns:
        Dictionary of parsed metadata key-value pairs.
    """
    metadata: dict[str, str] = {}
    for line in lines:
        if not line.startswith("#"):
            break
        content = line.lstrip("#").strip()
        if not content:
            continue
        if "," in content and ":" not in content:
            # Old format: key=value,key=value,...
            for pair in content.split(","):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    metadata[k.strip()] = v.strip()
        else:
            # New format: key: value
            if ":" in content:
                k, v = content.split(":", 1)
                metadata[k.strip()] = v.strip()
    return metadata


def read_hw_csv(path: Path) -> dict[tuple[int, str], float]:
    """Read a hardware benchmark CSV into {(level, op): median_cycles}.

    Handles both the old format (level stored in metadata, no ``level``
    column) and the new consolidated format (``level`` column in data rows).

    Args:
        path: Path to the hardware benchmark CSV file.

    Returns:
        Mapping from (security_level, operation_name) to median cycle count.
        Returns an empty dict if *path* does not exist.
    """
    result: dict[tuple[int, str], float] = {}
    if not path.exists():
        return result

    with path.open() as f:
        all_lines = f.readlines()

    # Extract comment lines for metadata
    comment_lines = [line for line in all_lines if line.startswith("#")]
    metadata = parse_metadata(comment_lines)

    # Extract data lines (non-comment)
    data_lines = [line for line in all_lines if not line.startswith("#")]

    if not data_lines:
        return result

    reader = csv.DictReader(data_lines)
    fieldnames = reader.fieldnames or []
    has_level_column = "level" in fieldnames

    for row in reader:
        try:
            op = row["op"].strip()
            median_cycles = float(row["median_cycles"])

            if has_level_column:
                level = int(row["level"])
            else:
                # Level from metadata
                level_str = metadata.get("level", "")
                if not level_str:
                    continue
                level = int(level_str)

            result[(level, op)] = median_cycles
        except (KeyError, ValueError, TypeError):
            continue

    return result
