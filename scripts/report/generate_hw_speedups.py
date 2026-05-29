#!/usr/bin/env python3
"""Generate hardware speedup report from BPI-F3 benchmark CSVs.

Reads the most recent hw_{op}_{variant_slug}_latest.csv from each
per-variant subdirectory and computes cycle-based speedups:

    Speedup = baseline_median_cycles / champion_median_cycles

Comparisons:
  scalar_ref → scalar_Zb    (Tier 1: scalar Zbc)
  scalar_ref → vector_rvv   (Tier 2: RVV vector)

Usage:
    python3 scripts/report/generate_hw_speedups.py
    python3 scripts/report/generate_hw_speedups.py --out path/to/output.md
"""

from __future__ import annotations

import argparse
import csv
import io
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
HW_KEM_DIR = REPO_ROOT / "results" / "hardware" / "benchmarks" / "kem"
HW_MUL_DIR = REPO_ROOT / "results" / "hardware" / "benchmarks" / "components" / "poly_mul"

BASELINE_VARIANT = "scalar_ref"
CHAMPION_VARIANTS = ["scalar_Zb", "vector_rvv"]
LEVELS = [128, 192, 256]
OPS_MUL = ["vect_mul"]
OPS_KEM = ["keypair", "encaps", "decaps"]


def parse_metadata(path: Path) -> dict[str, str]:
    """Parse CSV metadata from comment lines.

    Handles two formats:
      - Old: single line `# level=128,polymul=ref_scalar,n_iter=10000,...`
      - New: multi-line `# key: value` format
    """
    metadata: dict[str, str] = {}
    if not path.exists():
        return metadata
    with path.open() as f:
        for line in f:
            if not line.startswith("#"):
                break
            content = line.lstrip("#").strip()
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
    """Return {(level, op): median_cycles} from a hardware benchmark CSV.

    Handles both old format (level in metadata, no level column) and
    new consolidated format (level column in data rows).
    """
    result: dict[tuple[int, str], float] = {}
    if not path.exists():
        return result

    metadata = parse_metadata(path)

    with path.open() as f:
        # Skip comment lines
        data_lines = [line for line in f if not line.startswith("#")]

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


def load_variant_data(variant: str) -> dict[str, dict[tuple[int, str], float]]:
    """Load both mul and kem CSV data for a variant."""
    slug = variant.replace("_", "-")
    return {
        "mul": read_hw_csv(HW_MUL_DIR / variant / f"hw_mul_{slug}_latest.csv"),
        "kem": read_hw_csv(HW_KEM_DIR / variant / f"hw_kem_{slug}_latest.csv"),
    }


def get_iteration_count(variant: str) -> Optional[str]:
    """Extract n_iter from the baseline variant's metadata."""
    slug = variant.replace("_", "-")
    # Try mul first, then kem
    for hw_dir, op in ((HW_MUL_DIR, "mul"), (HW_KEM_DIR, "kem")):
        path = hw_dir / variant / f"hw_{op}_{slug}_latest.csv"
        if path.exists():
            meta = parse_metadata(path)
            n_iter = meta.get("n_iter")
            if n_iter:
                return n_iter
    return None


def fmt_cycles(n: Optional[float]) -> str:
    """Format cycle count with thousand-separators (e.g., 1,234,567)."""
    if n is None:
        return "—"
    return f"{int(n):,}"


def fmt_speedup(baseline: Optional[float], champion: Optional[float]) -> str:
    """Format speedup ratio as ×N.N (e.g., ×79.9)."""
    if baseline is None or champion is None or champion == 0:
        return "—"
    ratio = baseline / champion
    return f"×{ratio:.1f}"


def build_report() -> str:
    """Build the full hardware speedup Markdown report."""
    buf = io.StringIO()
    missing_data: list[str] = []

    # Load all variant data
    baseline_data = load_variant_data(BASELINE_VARIANT)
    champion_data = {v: load_variant_data(v) for v in CHAMPION_VARIANTS}

    # Check for missing data and record notes
    for op_type, op_label in [("mul", "polymul"), ("kem", "KEM")]:
        if not baseline_data[op_type]:
            missing_data.append(f"{BASELINE_VARIANT} ({op_label}): no data found")
        else:
            missing_levels = [lvl for lvl in LEVELS if not any(k[0] == lvl for k in baseline_data[op_type])]
            if missing_levels:
                missing_data.append(f"{BASELINE_VARIANT} ({op_label}): missing levels {missing_levels}")
        for v in CHAMPION_VARIANTS:
            if not champion_data[v][op_type]:
                missing_data.append(f"{v} ({op_label}): no data found")
            else:
                missing_levels = [lvl for lvl in LEVELS if not any(k[0] == lvl for k in champion_data[v][op_type])]
                if missing_levels:
                    missing_data.append(f"{v} ({op_label}): missing levels {missing_levels}")

    # Platform metadata header
    n_iter = get_iteration_count(BASELINE_VARIANT) or "—"
    buf.write("# Hardware Speedup Report — BPI-F3\n\n")
    buf.write("| Parameter | Value |\n")
    buf.write("|-----------|-------|\n")
    buf.write("| Board | Banana Pi BPI-F3 (SpacemiT K1) |\n")
    buf.write("| Core | SpacemiT X60 |\n")
    buf.write("| VLEN | 256 |\n")
    buf.write("| Compiler | GCC (native) |\n")
    buf.write("| Isolation | taskset (single-core pinning) |\n")
    buf.write(f"| Iterations | {n_iter} |\n")
    buf.write("\n")
    buf.write("Speedup = baseline_median_cycles / champion_median_cycles (higher is better).\n\n")

    # ── Isolated polymul table ────────────────────────────────────────────────
    buf.write("## Isolated Polynomial Multiplication (`vect_mul`)\n\n")
    buf.write("| Level | scalar_ref (cycles) |")
    for v in CHAMPION_VARIANTS:
        buf.write(f" {v} (cycles) | Speedup |")
    buf.write("\n")
    buf.write("|-------|---------------------|")
    for _ in CHAMPION_VARIANTS:
        buf.write("--------------------|---------|")
    buf.write("\n")

    for lvl in LEVELS:
        key = (lvl, "vect_mul")
        base_val = baseline_data["mul"].get(key)
        row = f"| HQC-{lvl} | {fmt_cycles(base_val)} |"
        for v in CHAMPION_VARIANTS:
            champ_val = champion_data[v]["mul"].get(key)
            row += f" {fmt_cycles(champ_val)} | {fmt_speedup(base_val, champ_val)} |"
        buf.write(row + "\n")

    buf.write("\n")

    # ── Full KEM table ────────────────────────────────────────────────────────
    buf.write("## Full KEM Operations\n\n")
    buf.write("| Level | Op | scalar_ref (cycles) |")
    for v in CHAMPION_VARIANTS:
        buf.write(f" {v} (cycles) | Speedup |")
    buf.write("\n")
    buf.write("|-------|-----|---------------------|")
    for _ in CHAMPION_VARIANTS:
        buf.write("--------------------|---------|")
    buf.write("\n")

    for lvl in LEVELS:
        for op in OPS_KEM:
            key = (lvl, op)
            base_val = baseline_data["kem"].get(key)
            row = f"| HQC-{lvl} | {op} | {fmt_cycles(base_val)} |"
            for v in CHAMPION_VARIANTS:
                champ_val = champion_data[v]["kem"].get(key)
                row += f" {fmt_cycles(champ_val)} | {fmt_speedup(base_val, champ_val)} |"
            buf.write(row + "\n")

        # Total KEM row for this level
        base_total: Optional[float] = None
        champ_totals: dict[str, Optional[float]] = {v: None for v in CHAMPION_VARIANTS}
        all_base = True
        all_champ = {v: True for v in CHAMPION_VARIANTS}

        for op in OPS_KEM:
            key = (lvl, op)
            b = baseline_data["kem"].get(key)
            if b is None:
                all_base = False
            else:
                base_total = (base_total or 0) + b

            for v in CHAMPION_VARIANTS:
                c = champion_data[v]["kem"].get(key)
                if c is None:
                    all_champ[v] = False
                else:
                    champ_totals[v] = (champ_totals[v] or 0) + c

        if not all_base:
            base_total = None
        for v in CHAMPION_VARIANTS:
            if not all_champ[v]:
                champ_totals[v] = None

        row = f"| HQC-{lvl} | **Total** | **{fmt_cycles(base_total)}** |"
        for v in CHAMPION_VARIANTS:
            ct = champ_totals[v]
            row += f" **{fmt_cycles(ct)}** | **{fmt_speedup(base_total, ct)}** |"
        buf.write(row + "\n")

    buf.write("\n")

    # ── Missing data note ─────────────────────────────────────────────────────
    if missing_data:
        buf.write("## Notes\n\n")
        buf.write("⚠ **Missing data** — the following `_latest.csv` files were not found; ")
        buf.write("corresponding comparisons are skipped (shown as —):\n\n")
        for item in missing_data:
            buf.write(f"- {item}\n")
        buf.write("\n")

    return buf.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate hardware speedup report from BPI-F3 benchmark CSVs."
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Write output to this Markdown file (default: results/reports/summaries/speedups_hardware.md)",
    )
    args = parser.parse_args()

    report = build_report()

    out_path: Path = args.out if args.out else REPO_ROOT / "results" / "reports" / "summaries" / "speedups_hardware.md"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    existing = out_path.read_text() if out_path.exists() else None
    out_path.write_text(report)

    if existing == report:
        print(f"⏭ Unchanged: {out_path.resolve()}")
    else:
        print(f"🔄 Updated: {out_path.resolve()}")


if __name__ == "__main__":
    main()
