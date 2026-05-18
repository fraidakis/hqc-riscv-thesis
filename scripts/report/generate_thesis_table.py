#!/usr/bin/env python3
"""Generate thesis speedup summary table from isolated benchmark CSVs.

Reads the most recent polymul_latest.csv and kem_latest.csv for each variant
and computes exact speedup multipliers:

    Speedup = Baseline_instret / Champion_instret

Three hardware tiers:
  Tier 1 (IoT)     ref_scalar  → karatsuba_zbc
  Tier 2 (Edge)    ref_vector  → karatsuba_rvv   (hybrid: RVV polymul + scalar Keccak)
  Tier 3 (High-End) ref_vector → karatsuba_zvbc

Usage:
  python3 scripts/report/generate_thesis_table.py
  python3 scripts/report/generate_thesis_table.py --level 128
  python3 scripts/report/generate_thesis_table.py --level all   # default
  python3 scripts/report/generate_thesis_table.py --out path/to/output.md
"""

from __future__ import annotations

import argparse
import csv
import io
import sys
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
ISOLATED = REPO_ROOT / "analysis/benchmarks/isolated"

# (subdir, variant_name)
_SCALAR_DIR = ISOLATED / "scalar"
_VECTOR_DIR = ISOLATED / "vector"

TIERS = [
    {
        "label": "Table 1 — Tier 1 (IoT Scalar)",
        "baseline_variant": "ref_scalar",
        "baseline_dir": _SCALAR_DIR,
        "champion_variant": "karatsuba_zbc",
        "champion_dir": _SCALAR_DIR,
        "note": "rv64gc_zbc_zbb_zbkb, no vector",
    },
    {
        "label": "Table 2 — Tier 2 (Edge Hybrid RVV)",
        "baseline_variant": "ref_vector",
        "baseline_dir": _SCALAR_DIR,
        "champion_variant": "karatsuba_rvv",
        "champion_dir": _VECTOR_DIR,
        "note": "rv64gcv + Zbc/Zbb/Zbkb; AP-2 fixed-tree polymul, scalar Keccak",
    },
    {
        "label": "Table 3 — Tier 3 (High-End Zvbc)",
        "baseline_variant": "ref_vector",
        "baseline_dir": _SCALAR_DIR,
        "champion_variant": "karatsuba_zvbc",
        "champion_dir": _VECTOR_DIR,
        "note": "rv64gcv + Zvbc/Zvbb/Zvl256b",
    },
]

LEVELS = [128, 192, 256]


def read_csv(path: Path) -> dict[tuple[int, str], int]:
    """Return {(level, op): instructions} from a benchmark CSV file."""
    result: dict[tuple[int, str], int] = {}
    if not path.exists():
        return result
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


def load_variant(base_dir: Path, variant: str) -> dict[str, dict[tuple[int, str], int]]:
    """Load both polymul and kem CSVs for a variant."""
    variant_dir = base_dir / variant
    return {
        "polymul": read_csv(variant_dir / "polymul_latest.csv"),
        "kem": read_csv(variant_dir / "kem_latest.csv"),
    }


def fmt_instret(n: Optional[int]) -> str:
    if n is None:
        return "—"
    return f"{n:,}"


def fmt_speedup(base: Optional[int], champ: Optional[int]) -> str:
    if base is None or champ is None or champ == 0:
        return "—"
    ratio = base / champ
    return f"x{ratio:.2f}"


def build_table(levels: list[int]) -> str:
    buf = io.StringIO()

    for tier in TIERS:
        buf.write(f"## {tier['label']}\n\n")
        buf.write(f"> {tier['note']}\n\n")

        base_data = load_variant(tier["baseline_dir"], tier["baseline_variant"])
        champ_data = load_variant(tier["champion_dir"], tier["champion_variant"])

        base_name = tier["baseline_variant"]
        champ_name = tier["champion_variant"]

        # --- vect_mul table ---
        buf.write(f"### `vect_mul` (polynomial multiply)\n\n")
        buf.write(
            f"| Level | {base_name} (instret) | {champ_name} (instret) | Speedup |\n"
        )
        buf.write("|-------|----------------------|------------------------|--------|\n")
        for lvl in levels:
            key = (lvl, "vect_mul")
            b = base_data["polymul"].get(key)
            c = champ_data["polymul"].get(key)
            buf.write(
                f"| HQC-{lvl} | {fmt_instret(b)} | {fmt_instret(c)} | {fmt_speedup(b, c)} |\n"
            )
        buf.write("\n")

        # --- KEM table ---
        buf.write(f"### KEM operations\n\n")
        buf.write(
            f"| Level | Op | {base_name} (instret) | {champ_name} (instret) | Speedup |\n"
        )
        buf.write("|-------|-----|----------------------|------------------------|--------|\n")
        for lvl in levels:
            level_base_total = 0
            level_champ_total = 0
            has_all_totals = True
            for op in ("keypair", "encaps", "decaps"):
                key = (lvl, op)
                b = base_data["kem"].get(key)
                c = champ_data["kem"].get(key)
                if b is None or c is None:
                    has_all_totals = False
                else:
                    level_base_total += b
                    level_champ_total += c
                buf.write(
                    f"| HQC-{lvl} | {op} | {fmt_instret(b)} | {fmt_instret(c)} | {fmt_speedup(b, c)} |\n"
                )
            total_b = level_base_total if has_all_totals else None
            total_c = level_champ_total if has_all_totals else None
            buf.write(
                f"| HQC-{lvl} | **Total KEM** | **{fmt_instret(total_b)}** | **{fmt_instret(total_c)}** | **{fmt_speedup(total_b, total_c)}** |\n"
            )
        buf.write("\n")

    return buf.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate thesis speedup table from isolated benchmark CSVs."
    )
    parser.add_argument(
        "--level",
        choices=["128", "192", "256", "all"],
        default="all",
        help="Security level(s) to include (default: all)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Write output to this Markdown file instead of stdout",
    )
    args = parser.parse_args()

    levels = LEVELS if args.level == "all" else [int(args.level)]

    header = (
        "# HQC-RISC-V Thesis Speedup Summary\n\n"
        "Speedup = Baseline instret / Champion instret (higher is better).\n"
        "All measurements are deterministic instruction-retired counts from Spike ISS.\n\n"
    )
    table = header + build_table(levels)

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(table)
        print(f"Written: {args.out}")
    else:
        sys.stdout.write(table)


if __name__ == "__main__":
    main()
