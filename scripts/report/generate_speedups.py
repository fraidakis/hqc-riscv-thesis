#!/usr/bin/env python3
"""Generate thesis speedup summary table from isolated benchmark CSVs.

Reads the most recent isolated_polymul_*_latest.csv and
isolated_kem_*_latest.csv for each variant
and computes exact speedup multipliers:

    Speedup = Baseline_instret / Champion_instret

Three hardware tiers:
  Tier 1 (IoT)     ref_scalar  → karatsuba_zbc
  Tier 2 (Edge)    ref_vector  → karatsuba_rvv   (hybrid: RVV polymul + scalar Keccak)
  Tier 3 (High-End) ref_vector → karatsuba_zvbc

Usage:
    python3 scripts/report/generate_speedups.py
    python3 scripts/report/generate_speedups.py --out path/to/output.md
"""

from __future__ import annotations

import argparse
import csv
import io
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
        "note": "rv64gc_zbc_zbb, no vector",
    },
    {
        "label": "Table 2 — Tier 2 (Edge RVV)",
        "baseline_variant": "ref_vector",
        "baseline_dir": _VECTOR_DIR,
        "champion_variant": "karatsuba_rvv",
        "champion_dir": _VECTOR_DIR,
        "note": "rv64gcv + Zbc/Zbb; AP-2 fixed-tree polymul, scalar Keccak",
    },
    {
        "label": "Table 3 — Tier 3 (High-End Zvbc)",
        "baseline_variant": "ref_vector",
        "baseline_dir": _VECTOR_DIR,
        "champion_variant": "karatsuba_zvbc",
        "champion_dir": _VECTOR_DIR,
        "note": "rv64gcv + Zvbc/Zvbb/Zvl256b",
    },
]

LEVELS = [128, 192, 256]

SINGLE_BENCH_TABLES = [
    {
        "label": "Table 4 — Keccak Speedups",
        "title": "`keccak`",
        "subdir": ISOLATED / "keccak",
        "baseline_file": "isolated_keccak_ref_latest.csv",
        "champion_file": "isolated_keccak_opt_latest.csv",
        "op": "keccak",
    },
    {
        "label": "Table 5 — Decode Speedups",
        "title": "`decode`",
        "subdir": ISOLATED / "decode",
        "baseline_file": "isolated_decode_ref_latest.csv",
        "champion_file": "isolated_decode_opt_latest.csv",
        "op": "decode",
    },
]


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
    variant_slug = variant.replace("_", "-")
    return {
        "polymul": read_csv(variant_dir / f"isolated_polymul_{variant_slug}_latest.csv"),
        "kem": read_csv(variant_dir / f"isolated_kem_{variant_slug}_latest.csv"),
    }


def load_single_benchmark(
    base_dir: Path, baseline_file: str, champion_file: str
) -> tuple[dict[tuple[int, str], int], dict[tuple[int, str], int]]:
    """Load a reference/optimized CSV pair for an isolated benchmark."""
    return (
        read_csv(base_dir / baseline_file),
        read_csv(base_dir / champion_file),
    )


def fmt_instret(n: Optional[int]) -> str:
    if n is None:
        return "—"
    return f"{n:,}"


def fmt_speedup(base: Optional[int], champ: Optional[int]) -> str:
    if base is None or champ is None or champ == 0:
        return "—"
    ratio = base / champ
    return f"x{ratio:.2f}"


def build_table() -> str:
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
        for lvl in LEVELS:
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
        for lvl in LEVELS:
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

    for table in SINGLE_BENCH_TABLES:
        buf.write(f"## {table['label']}\n\n")
        buf.write(f"### {table['title']}\n\n")

        base_data, champ_data = load_single_benchmark(
            table["subdir"],
            table["baseline_file"],
            table["champion_file"],
        )

        buf.write("| Level | reference (instret) | optimized (instret) | Speedup |\n")
        buf.write("|-------|----------------------|---------------------|--------|\n")
        for lvl in LEVELS:
            key = (lvl, table["op"])
            b = base_data.get(key)
            c = champ_data.get(key)
            buf.write(
                f"| HQC-{lvl} | {fmt_instret(b)} | {fmt_instret(c)} | {fmt_speedup(b, c)} |\n"
            )
        buf.write("\n")

    return buf.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate thesis speedup table from isolated benchmark CSVs."
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Write output to this Markdown file",
    )
    args = parser.parse_args()

    header = (
        "# HQC-RISC-V Thesis Speedup Summary\n\n"
        "Speedup = Baseline instret / Champion instret (higher is better).\n"
        "All measurements are deterministic instruction-retired counts from Spike ISS.\n\n"
    )
    table = header + build_table()

    out_path = args.out if args.out else REPO_ROOT / "analysis" / "reports" / "speedups.md"
    resolved_out_path = out_path.resolve()

    out_path.parent.mkdir(parents=True, exist_ok=True)

    existing = out_path.read_text() if out_path.exists() else None
    out_path.write_text(table)

    if existing == table:
        print(f" ⏭ Unchanged: {resolved_out_path}")
    else:
        print(f" 🔄 Updated: {resolved_out_path}")


if __name__ == "__main__":
    main()
