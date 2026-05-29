#!/usr/bin/env python3
"""Generate cross-platform comparison report: Spike ISS vs real hardware.

Reads Spike instret CSVs from results/simulation/benchmarks/components/poly_mul/
and hardware cycle CSVs from results/hardware/benchmarks/ to compute and compare
speedups across both platforms.

For each champion variant (scalar_Zb, vector_rvv):
    Spike speedup   = scalar_ref_instret / champion_instret
    HW speedup      = scalar_ref_median_cycles / champion_median_cycles
    HW/Spike ratio  = HW_speedup / Spike_speedup

A ratio > 1 means hardware amplifies the advantage predicted by Spike.
A ratio < 1 means hardware attenuates it (microarchitectural overhead).

Usage:
    python3 scripts/report/generate_comparison.py
    python3 scripts/report/generate_comparison.py --out path/to/output.md
"""

from __future__ import annotations

import argparse
import csv
import io
import re
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
POLY_MUL_DIR = REPO_ROOT / "results" / "simulation" / "benchmarks" / "components" / "poly_mul"
HW_KEM_DIR = REPO_ROOT / "results" / "hardware" / "benchmarks" / "kem"
HW_MUL_DIR = REPO_ROOT / "results" / "hardware" / "benchmarks" / "components" / "poly_mul"

LEVELS = [128, 192, 256]

# Champion variants and their Spike/HW tier mappings.
# Each entry maps a champion to its Spike baseline directory and variant name.
CHAMPIONS = [
    {
        "variant": "scalar_Zb",
        "label": "Scalar Zb (Scalar Tier)",
        "spike_baseline_dir": POLY_MUL_DIR / "scalar_ref",
        "spike_baseline_variant": "scalar_ref",
        "spike_champion_dir": POLY_MUL_DIR / "scalar_Zb",
        "spike_champion_variant": "scalar_Zb",
    },
    {
        "variant": "vector_rvv",
        "label": "Vector RVV (Vector Tier)",
        "spike_baseline_dir": POLY_MUL_DIR / "vector_ref",
        "spike_baseline_variant": "vector_ref",
        "spike_champion_dir": POLY_MUL_DIR / "vector_rvv",
        "spike_champion_variant": "vector_rvv",
    },
]


# ── CSV Readers ──────────────────────────────────────────────────────────────


def read_spike_csv(path: Path) -> dict[tuple[int, str], int]:
    """Read a Spike isolated CSV → {(level, op): instructions}."""
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


def _parse_hw_header_level(path: Path) -> Optional[int]:
    """Extract level from hardware CSV header comment (e.g. '# level=128,...')."""
    if not path.exists():
        return None
    with path.open() as f:
        for line in f:
            if not line.startswith("#"):
                break
            m = re.search(r"level=(\d+)", line)
            if m:
                return int(m.group(1))
    return None


def read_hw_csv(path: Path) -> dict[tuple[int, str], float]:
    """Read a hardware CSV → {(level, op): median_cycles}.

    Handles two formats:
      1. Consolidated (has 'level' column): level is per-row.
      2. Single-level (level in header comment): all rows share that level.
    """
    result: dict[tuple[int, str], float] = {}
    if not path.exists():
        return result

    header_level = _parse_hw_header_level(path)

    with path.open() as f:
        reader = csv.DictReader(row for row in f if not row.startswith("#"))
        for row in reader:
            try:
                op = row["op"].strip()
                median_cycles = float(row["median_cycles"])
                # Use row-level 'level' column if present, else header level.
                if "level" in row and row["level"].strip():
                    level = int(row["level"])
                elif header_level is not None:
                    level = header_level
                else:
                    continue
                result[(level, op)] = median_cycles
            except (KeyError, ValueError):
                continue
    return result


# ── Data Loading ─────────────────────────────────────────────────────────────


def load_spike_variant(base_dir: Path, variant: str) -> dict[tuple[int, str], int]:
    """Load the polymul Spike CSV for a variant."""
    slug = variant.replace("_", "-")
    # Try isolated_mul_ first (newer naming), then isolated_polymul_ (legacy).
    mul_path = base_dir / f"isolated_mul_{slug}_latest.csv"
    if not mul_path.exists():
        mul_path = base_dir / f"isolated_polymul_{slug}_latest.csv"
    return read_spike_csv(mul_path)


def load_hw_variant(variant: str) -> dict[tuple[int, str], float]:
    """Load the polymul hardware CSV for a variant."""
    slug = variant.replace("_", "-")
    hw_path = HW_MUL_DIR / variant / f"hw_mul_{slug}_latest.csv"
    return read_hw_csv(hw_path)


# ── Formatting ───────────────────────────────────────────────────────────────


def fmt_int(n: Optional[int]) -> str:
    if n is None:
        return "—"
    return f"{n:,}"


def fmt_cycles(n: Optional[float]) -> str:
    if n is None:
        return "—"
    return f"{int(n):,}"


def fmt_speedup(val: Optional[float]) -> str:
    if val is None:
        return "—"
    return f"×{val:.2f}"


def fmt_ratio(val: Optional[float]) -> str:
    if val is None:
        return "—"
    return f"{val:.3f}"


# ── Report Generation ────────────────────────────────────────────────────────


def compute_speedup_int(base: Optional[int], champ: Optional[int]) -> Optional[float]:
    """Compute speedup from integer instret values."""
    if base is None or champ is None or champ == 0:
        return None
    return base / champ


def compute_speedup_float(base: Optional[float], champ: Optional[float]) -> Optional[float]:
    """Compute speedup from float cycle values."""
    if base is None or champ is None or champ == 0.0:
        return None
    return base / champ


def build_report() -> str:
    buf = io.StringIO()

    buf.write("# HQC-RISC-V Cross-Platform Comparison: Spike ISS vs BPI-F3 Hardware\n\n")
    buf.write(
        "Compares Spike instruction-retired speedups against real hardware cycle speedups\n"
        "for polynomial multiplication (`vect_mul`).\n\n"
        "- **Spike speedup** = ref_instret / champion_instret (deterministic)\n"
        "- **HW speedup** = ref_median_cycles / champion_median_cycles (statistical)\n"
        "- **HW/Spike ratio** = HW_speedup / Spike_speedup\n"
        "  - ratio > 1 → hardware **amplifies** the Spike-predicted advantage\n"
        "  - ratio < 1 → hardware **attenuates** it (microarchitectural overhead)\n\n"
    )

    for champ_cfg in CHAMPIONS:
        variant = champ_cfg["variant"]
        label = champ_cfg["label"]

        buf.write(f"## {label}\n\n")

        # Load Spike data.
        spike_base = load_spike_variant(
            champ_cfg["spike_baseline_dir"], champ_cfg["spike_baseline_variant"]
        )
        spike_champ = load_spike_variant(
            champ_cfg["spike_champion_dir"], champ_cfg["spike_champion_variant"]
        )

        # Load hardware data.
        hw_base = load_hw_variant("scalar_ref")
        hw_champ = load_hw_variant(variant)

        # Table header.
        buf.write(
            "| Level | Spike Instret (ref) | Spike Instret (champion) | Spike Speedup "
            "| HW Cycles (ref) | HW Cycles (champion) | HW Speedup | HW/Spike Ratio |\n"
        )
        buf.write(
            "|-------|--------------------:|-------------------------:|--------------:"
            "|----------------:|---------------------:|-----------:|---------------:|\n"
        )

        for lvl in LEVELS:
            key = (lvl, "vect_mul")

            s_base = spike_base.get(key)
            s_champ = spike_champ.get(key)
            spike_spd = compute_speedup_int(s_base, s_champ)

            h_base = hw_base.get(key)
            h_champ = hw_champ.get(key)
            hw_spd = compute_speedup_float(h_base, h_champ)

            # HW/Spike ratio.
            if hw_spd is not None and spike_spd is not None and spike_spd != 0:
                ratio = hw_spd / spike_spd
            else:
                ratio = None

            buf.write(
                f"| HQC-{lvl} "
                f"| {fmt_int(s_base)} "
                f"| {fmt_int(s_champ)} "
                f"| {fmt_speedup(spike_spd)} "
                f"| {fmt_cycles(h_base)} "
                f"| {fmt_cycles(h_champ)} "
                f"| {fmt_speedup(hw_spd)} "
                f"| {fmt_ratio(ratio)} |\n"
            )

        buf.write("\n")

    # Interpretation section.
    buf.write("## Interpretation\n\n")
    buf.write(
        "- A **HW/Spike ratio > 1** indicates that the real hardware amplifies the\n"
        "  speedup predicted by Spike's instruction-retired model. This can happen when\n"
        "  the champion variant benefits from microarchitectural features (caches, OoO\n"
        "  execution, prefetching) more than the baseline.\n\n"
        "- A **HW/Spike ratio < 1** indicates that hardware attenuates the predicted\n"
        "  speedup. This typically occurs when the champion variant incurs\n"
        "  microarchitectural penalties (cache misses, pipeline stalls, memory bandwidth\n"
        "  limits) not captured by the instruction-count model.\n\n"
        "- A **ratio ≈ 1** suggests the Spike instret model is a good predictor of\n"
        "  real-world performance for that variant/level combination.\n"
    )

    return buf.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate cross-platform comparison report: Spike ISS vs hardware."
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Write output to this Markdown file (default: results/reports/summaries/speedups_cross_platform.md)",
    )
    args = parser.parse_args()

    report = build_report()

    out_path = args.out if args.out else REPO_ROOT / "results" / "reports" / "summaries" / "speedups_cross_platform.md"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    existing = out_path.read_text() if out_path.exists() else None
    out_path.write_text(report)

    if existing == report:
        print(f"⏭ Unchanged: {out_path.resolve()}")
    else:
        print(f"🔄 Updated: {out_path.resolve()}")


if __name__ == "__main__":
    main()
