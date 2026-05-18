#!/usr/bin/env python3
"""Generate thesis master metrics tables from latest isolated benchmark CSVs.

This script computes exact speedups using:
    Speedup = Baseline_instret / Champion_instret

Outputs docs/THESIS_METRICS_MASTER.md with:
- Table 1: Scalar optimization path (IoT)
- Table 4: Vector optimization path (Edge)
- Table 5: Vector crypto path (High-End)
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
ISOLATED_ROOT = REPO_ROOT / "analysis" / "benchmarks" / "isolated"
OUT_PATH = REPO_ROOT / "docs" / "THESIS_METRICS_MASTER.md"


def read_latest_csv(path: Path) -> dict[tuple[int, str], int]:
    data: dict[tuple[int, str], int] = {}
    if not path.exists():
        return data
    with path.open("r", encoding="utf-8") as f:
        reader = csv.DictReader(row for row in f if not row.startswith("#"))
        for row in reader:
            try:
                lvl = int(row["level"])
                op = row["op"].strip()
                inst = int(row["instructions"])
            except (KeyError, ValueError):
                continue
            data[(lvl, op)] = inst
    return data


def get_instret(variant: str, op: str, level: int) -> Optional[int]:
    track = "vector" if ("rvv" in variant or "zvbc" in variant) else "scalar"
    csv_name = "kem_latest.csv" if op == "decaps" else "polymul_latest.csv"
    csv_path = ISOLATED_ROOT / track / variant / csv_name
    return read_latest_csv(csv_path).get((level, op))


def fmt_int(x: Optional[int]) -> str:
    if x is None:
        return "—"
    return f"{x:,}"


def speedup(base: Optional[int], champ: Optional[int]) -> Optional[float]:
    if base is None or champ is None or champ == 0:
        return None
    return base / champ


def fmt_speed(ratio: Optional[float]) -> str:
    if ratio is None:
        return "—"
    return f"x{ratio:.2f}"


def row_for(label: str, variant: str, level: int) -> tuple[str, str, str]:
    vm = get_instret(variant, "vect_mul", level)
    dec = get_instret(variant, "decaps", level)
    return (label, fmt_int(vm), fmt_int(dec))


def build_table_1(level: int) -> str:
    l1_vm = get_instret("ref_pure", "vect_mul", level)
    l1_dec = get_instret("ref_pure", "decaps", level)
    l2_vm = get_instret("ref_scalar", "vect_mul", level)
    l2_dec = get_instret("ref_scalar", "decaps", level)
    l3_vm = get_instret("karatsuba_zbc", "vect_mul", level)
    l3_dec = get_instret("karatsuba_zbc", "decaps", level)

    lines = []
    lines.append("## Table 1: The Scalar Optimization Path (IoT Tier)")
    lines.append("")
    lines.append("| Stage | Variant | `vect_mul` instret | Speedup vs previous (`vect_mul`) | `decaps` instret | Speedup vs previous (`decaps`) |")
    lines.append("|---|---|---:|---:|---:|---:|")
    lines.append(
        f"| Level 1 (Pure C) | `ref_pure` | {fmt_int(l1_vm)} | — | {fmt_int(l1_dec)} | — |"
    )
    lines.append(
        f"| Level 2 (Hardware Assisted) | `ref_scalar` | {fmt_int(l2_vm)} | {fmt_speed(speedup(l1_vm, l2_vm))} | {fmt_int(l2_dec)} | {fmt_speed(speedup(l1_dec, l2_dec))} |"
    )
    lines.append(
        f"| Level 3 (Algorithmic Champion) | `karatsuba_zbc` | {fmt_int(l3_vm)} | {fmt_speed(speedup(l2_vm, l3_vm))} | {fmt_int(l3_dec)} | {fmt_speed(speedup(l2_dec, l3_dec))} |"
    )
    lines.append("")
    return "\n".join(lines)


def build_table_4(level: int) -> str:
    l1_vm = get_instret("ref_vector", "vect_mul", level)
    l1_dec = get_instret("ref_vector", "decaps", level)
    l2_vm = get_instret("karatsuba_rvv", "vect_mul", level)
    l2_dec = get_instret("karatsuba_rvv", "decaps", level)

    lines = []
    lines.append("## Table 4: The Vector Optimization Path (Edge/Banana Pi Tier)")
    lines.append("")
    lines.append("| Stage | Variant | `vect_mul` instret | Speedup vs previous (`vect_mul`) | `decaps` instret | Speedup vs previous (`decaps`) |")
    lines.append("|---|---|---:|---:|---:|---:|")
    lines.append(
        f"| Level 1 (Compiler Auto-Vectorized) | `ref_vector` | {fmt_int(l1_vm)} | — | {fmt_int(l1_dec)} | — |"
    )
    lines.append(
        f"| Level 2 (Hybrid Champion) | `karatsuba_rvv` | {fmt_int(l2_vm)} | {fmt_speed(speedup(l1_vm, l2_vm))} | {fmt_int(l2_dec)} | {fmt_speed(speedup(l1_dec, l2_dec))} |"
    )
    lines.append("")
    return "\n".join(lines)


def build_table_5(level: int) -> str:
    l1_vm = get_instret("ref_vector", "vect_mul", level)
    l1_dec = get_instret("ref_vector", "decaps", level)
    l2_vm = get_instret("karatsuba_zvbc", "vect_mul", level)
    l2_dec = get_instret("karatsuba_zvbc", "decaps", level)

    lines = []
    lines.append("## Table 5: The Vector Crypto Path (High-End Tier)")
    lines.append("")
    lines.append("| Stage | Variant | `vect_mul` instret | Speedup vs previous (`vect_mul`) | `decaps` instret | Speedup vs previous (`decaps`) |")
    lines.append("|---|---|---:|---:|---:|---:|")
    lines.append(
        f"| Level 1 (Compiler Barrier) | `ref_vector` | {fmt_int(l1_vm)} | — | {fmt_int(l1_dec)} | — |"
    )
    lines.append(
        f"| Level 2 (Full Hardware Champion) | `karatsuba_zvbc` | {fmt_int(l2_vm)} | {fmt_speed(speedup(l1_vm, l2_vm))} | {fmt_int(l2_dec)} | {fmt_speed(speedup(l1_dec, l2_dec))} |"
    )
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate THESIS_METRICS_MASTER.md from latest benchmark CSVs")
    parser.add_argument("--level", type=int, default=128, choices=[128, 192, 256], help="Security level to render (default: 128)")
    args = parser.parse_args()

    doc_lines = []
    doc_lines.append("# Thesis Metrics Master")
    doc_lines.append("")
    doc_lines.append(f"Security level: HQC-{args.level}")
    doc_lines.append("")
    doc_lines.append("Speedup definition: `Baseline / Champion`.")
    doc_lines.append("")
    doc_lines.append(build_table_1(args.level))
    doc_lines.append(build_table_4(args.level))
    doc_lines.append(build_table_5(args.level))

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text("\n".join(doc_lines), encoding="utf-8")
    print(f"Written: {OUT_PATH}")


if __name__ == "__main__":
    main()
