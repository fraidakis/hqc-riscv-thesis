#!/usr/bin/env python3
"""Generate speedup reports from benchmark CSVs.

Unified speedup generator supporting simulation, hardware, and cross-platform
comparison via a single --platform flag.

Modes:
  --platform sim   : Spike ISS instret-based speedups (simulation)
  --platform hw    : BPI-F3 cycle-based speedups (hardware)
  --platform cross : Sim vs hw correlation report

Usage:
    python3 scripts/report/generate_speedups.py --platform sim
    python3 scripts/report/generate_speedups.py --platform hw --out path/to/output.md
    python3 scripts/report/generate_speedups.py --platform cross
"""

from __future__ import annotations

import argparse
import io
from pathlib import Path
from typing import Optional

from hqc_bench import (
    fmt_cycles,
    fmt_int,
    fmt_speedup,
    parse_metadata,
    read_csv,
    read_hw_csv,
    variant_slug,
)

REPO_ROOT = Path(__file__).resolve().parents[2]

# Flat category directories (no per-variant subdirectories)
POLY_MUL_DIR = REPO_ROOT / "results/simulation/benchmarks/components/poly_mul"
KEM_DIR = REPO_ROOT / "results/simulation/benchmarks/kem"
KECCAK_DIR = REPO_ROOT / "results/simulation/benchmarks/components/keccak"
DECODE_DIR = REPO_ROOT / "results/simulation/benchmarks/components/decode"

# Hardware result directories
HW_KEM_DIR = REPO_ROOT / "results" / "hardware" / "benchmarks" / "kem"
HW_MUL_DIR = REPO_ROOT / "results" / "hardware" / "benchmarks" / "components" / "poly_mul"

# Hardware report configuration
HW_BASELINE_VARIANT = "scalar_ref"
HW_CHAMPION_VARIANTS = ["scalar_zb", "vector_rvv"]
HW_OPS_KEM = ["keypair", "encaps", "decaps"]

TIERS = [
    {
        "label": "Table 1 — Tier 1 (IoT Scalar)",
        "baseline_variant": "scalar_ref",
        "champion_variant": "scalar_zb",
        "note": "rv64gc_zbc_zbb, no vector",
    },
    {
        "label": "Table 2 — Tier 2 (Edge RVV)",
        "baseline_variant": "vector_ref",
        "champion_variant": "vector_rvv",
        "note": "rv64gcv + Zbc/Zbb; AP-2 fixed-tree polymul, scalar Keccak",
    },
    {
        "label": "Table 3 — Tier 3 (High-End Zvbc)",
        "baseline_variant": "vector_ref",
        "champion_variant": "vector_zvk",
        "note": "rv64gcv + Zvbc/Zvbb/Zvl256b",
    },
]

LEVELS = [128, 192, 256]

SINGLE_BENCH_TABLES = [
    {
        "label": "Table 4 — Keccak Speedups",
        "title": "`keccak`",
        "subdir": KECCAK_DIR,
        "baseline_file": "sim_keccak_ref_latest.csv",
        "champion_file": "sim_keccak_opt_latest.csv",
        "op": "keccak",
    },
    {
        "label": "Table 5 — Decode Speedups",
        "title": "`decode`",
        "subdir": DECODE_DIR,
        "baseline_file": "sim_decode_ref_latest.csv",
        "champion_file": "sim_decode_zvbc_latest.csv",
        "op": "decode",
    },
]


def load_variant(variant: str) -> dict[str, dict[tuple[int, str], int]]:
    """Load both polymul and KEM CSVs for a variant.

    Files are read from flat category directories using the new naming convention:
      - polymul: POLY_MUL_DIR / sim_mul_{slug}_latest.csv
      - kem:     KEM_DIR / sim_kem_{slug}_latest.csv
    """
    slug = variant_slug(variant)
    return {
        "polymul": read_csv(POLY_MUL_DIR / f"sim_mul_{slug}_latest.csv"),
        "kem": read_csv(KEM_DIR / f"sim_kem_{slug}_latest.csv"),
    }


def load_single_benchmark(
    base_dir: Path, baseline_file: str, champion_file: str
) -> tuple[dict[tuple[int, str], int], dict[tuple[int, str], int]]:
    """Load a reference/optimized CSV pair for an isolated benchmark.

    Files are read directly from the flat category directory.
    """
    return (
        read_csv(base_dir / baseline_file),
        read_csv(base_dir / champion_file),
    )


def build_table() -> str:
    buf = io.StringIO()

    for tier in TIERS:
        buf.write(f"## {tier['label']}\n\n")
        buf.write(f"> {tier['note']}\n\n")

        base_data = load_variant(tier["baseline_variant"])
        champ_data = load_variant(tier["champion_variant"])

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
                f"| HQC-{lvl} | {fmt_int(b)} | {fmt_int(c)} | {fmt_speedup(b, c)} |\n"
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
                    f"| HQC-{lvl} | {op} | {fmt_int(b)} | {fmt_int(c)} | {fmt_speedup(b, c)} |\n"
                )
            total_b = level_base_total if has_all_totals else None
            total_c = level_champ_total if has_all_totals else None
            buf.write(
                f"| HQC-{lvl} | **Total KEM** | **{fmt_int(total_b)}** | **{fmt_int(total_c)}** | **{fmt_speedup(total_b, total_c)}** |\n"
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
                f"| HQC-{lvl} | {fmt_int(b)} | {fmt_int(c)} | {fmt_speedup(b, c)} |\n"
            )
        buf.write("\n")

    return buf.getvalue()


def generate_sim_report(out_path: Path | None) -> None:
    """Generate simulation (Spike ISS) instret-based speedup report."""
    header = (
        "# HQC-RISC-V Thesis Speedup Summary\n\n"
        "Speedup = Baseline instret / Champion instret (higher is better).\n"
        "All measurements are deterministic instruction-retired counts from Spike ISS.\n\n"
    )
    table = header + build_table()

    resolved = out_path if out_path else REPO_ROOT / "results" / "reports" / "summaries" / "speedups_simulation.md"
    resolved = resolved.resolve()

    resolved.parent.mkdir(parents=True, exist_ok=True)

    existing = resolved.read_text() if resolved.exists() else None
    resolved.write_text(table)

    if existing == table:
        print(f" ⏭ Unchanged: {resolved}")
    else:
        print(f" 🔄 Updated: {resolved}")


def generate_hw_report(out_path: Path | None) -> None:
    """Generate hardware (BPI-F3) cycle-based speedup report."""
    report = _build_hw_report()

    resolved = out_path if out_path else REPO_ROOT / "results" / "reports" / "summaries" / "speedups_hardware.md"
    resolved = resolved.resolve()

    resolved.parent.mkdir(parents=True, exist_ok=True)

    existing = resolved.read_text() if resolved.exists() else None
    resolved.write_text(report)

    if existing == report:
        print(f" ⏭ Unchanged: {resolved}")
    else:
        print(f" 🔄 Updated: {resolved}")


def _load_hw_variant_data(variant: str) -> dict[str, dict[tuple[int, str], float]]:
    """Load both mul and kem CSV data for a hardware variant."""
    slug = variant_slug(variant)
    return {
        "mul": read_hw_csv(HW_MUL_DIR / f"hw_mul_{slug}_latest.csv"),
        "kem": read_hw_csv(HW_KEM_DIR / f"hw_kem_{slug}_latest.csv"),
    }


def _get_hw_iteration_count(variant: str) -> Optional[str]:
    """Extract n_iter from a hardware variant's CSV metadata."""
    slug = variant_slug(variant)
    for hw_dir, op in ((HW_MUL_DIR, "mul"), (HW_KEM_DIR, "kem")):
        path = hw_dir / f"hw_{op}_{slug}_latest.csv"
        if path.exists():
            with path.open() as f:
                comment_lines = [line for line in f if line.startswith("#")]
            meta = parse_metadata(comment_lines)
            n_iter = meta.get("n_iter")
            if n_iter:
                return n_iter
    return None


def _build_hw_report() -> str:
    """Build the full hardware speedup Markdown report."""
    buf = io.StringIO()
    missing_data: list[str] = []

    # Load all variant data
    baseline_data = _load_hw_variant_data(HW_BASELINE_VARIANT)
    champion_data = {v: _load_hw_variant_data(v) for v in HW_CHAMPION_VARIANTS}

    # Check for missing data and record notes
    for op_type, op_label in [("mul", "polymul"), ("kem", "KEM")]:
        if not baseline_data[op_type]:
            missing_data.append(f"{HW_BASELINE_VARIANT} ({op_label}): no data found")
        else:
            missing_levels = [lvl for lvl in LEVELS if not any(k[0] == lvl for k in baseline_data[op_type])]
            if missing_levels:
                missing_data.append(f"{HW_BASELINE_VARIANT} ({op_label}): missing levels {missing_levels}")
        for v in HW_CHAMPION_VARIANTS:
            if not champion_data[v][op_type]:
                missing_data.append(f"{v} ({op_label}): no data found")
            else:
                missing_levels = [lvl for lvl in LEVELS if not any(k[0] == lvl for k in champion_data[v][op_type])]
                if missing_levels:
                    missing_data.append(f"{v} ({op_label}): missing levels {missing_levels}")

    # Platform metadata header
    n_iter = _get_hw_iteration_count(HW_BASELINE_VARIANT) or "\u2014"
    buf.write("# Hardware Speedup Report \u2014 BPI-F3\n\n")
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
    for v in HW_CHAMPION_VARIANTS:
        buf.write(f" {v} (cycles) | Speedup |")
    buf.write("\n")
    buf.write("|-------|---------------------|")
    for _ in HW_CHAMPION_VARIANTS:
        buf.write("--------------------|---------|")
    buf.write("\n")

    for lvl in LEVELS:
        key = (lvl, "vect_mul")
        base_val = baseline_data["mul"].get(key)
        row = f"| HQC-{lvl} | {fmt_cycles(base_val)} |"
        for v in HW_CHAMPION_VARIANTS:
            champ_val = champion_data[v]["mul"].get(key)
            row += f" {fmt_cycles(champ_val)} | {fmt_speedup(base_val, champ_val)} |"
        buf.write(row + "\n")

    buf.write("\n")

    # ── Full KEM table ────────────────────────────────────────────────────────
    buf.write("## Full KEM Operations\n\n")
    buf.write("| Level | Op | scalar_ref (cycles) |")
    for v in HW_CHAMPION_VARIANTS:
        buf.write(f" {v} (cycles) | Speedup |")
    buf.write("\n")
    buf.write("|-------|-----|---------------------|")
    for _ in HW_CHAMPION_VARIANTS:
        buf.write("--------------------|---------|")
    buf.write("\n")

    for lvl in LEVELS:
        for op in HW_OPS_KEM:
            key = (lvl, op)
            base_val = baseline_data["kem"].get(key)
            row = f"| HQC-{lvl} | {op} | {fmt_cycles(base_val)} |"
            for v in HW_CHAMPION_VARIANTS:
                champ_val = champion_data[v]["kem"].get(key)
                row += f" {fmt_cycles(champ_val)} | {fmt_speedup(base_val, champ_val)} |"
            buf.write(row + "\n")

        # Total KEM row for this level
        base_total: Optional[float] = None
        champ_totals: dict[str, Optional[float]] = {v: None for v in HW_CHAMPION_VARIANTS}
        all_base = True
        all_champ = {v: True for v in HW_CHAMPION_VARIANTS}

        for op in HW_OPS_KEM:
            key = (lvl, op)
            b = baseline_data["kem"].get(key)
            if b is None:
                all_base = False
            else:
                base_total = (base_total or 0) + b

            for v in HW_CHAMPION_VARIANTS:
                c = champion_data[v]["kem"].get(key)
                if c is None:
                    all_champ[v] = False
                else:
                    champ_totals[v] = (champ_totals[v] or 0) + c

        if not all_base:
            base_total = None
        for v in HW_CHAMPION_VARIANTS:
            if not all_champ[v]:
                champ_totals[v] = None

        row = f"| HQC-{lvl} | **Total** | **{fmt_cycles(base_total)}** |"
        for v in HW_CHAMPION_VARIANTS:
            ct = champ_totals[v]
            row += f" **{fmt_cycles(ct)}** | **{fmt_speedup(base_total, ct)}** |"
        buf.write(row + "\n")

    buf.write("\n")

    # ── Missing data note ─────────────────────────────────────────────────────
    if missing_data:
        buf.write("## Notes\n\n")
        buf.write("\u26a0 **Missing data** \u2014 the following `_latest.csv` files were not found; ")
        buf.write("corresponding comparisons are skipped (shown as \u2014):\n\n")
        for item in missing_data:
            buf.write(f"- {item}\n")
        buf.write("\n")

    return buf.getvalue()


def generate_cross_report(out_path: Path | None) -> None:
    """Generate cross-platform correlation report (sim vs hw).

    Compares Spike instruction-retired speedups against real hardware cycle
    speedups for polynomial multiplication (vect_mul).

    For each champion variant:
        Spike speedup   = ref_instret / champion_instret
        HW speedup      = ref_median_cycles / champion_median_cycles
        HW/Spike ratio  = HW_speedup / Spike_speedup

    A ratio > 1 means hardware amplifies the advantage predicted by Spike.
    A ratio < 1 means hardware attenuates it (microarchitectural overhead).
    """
    from hqc_bench import read_hw_csv, fmt_cycles

    HW_MUL_DIR = REPO_ROOT / "results" / "hardware" / "benchmarks" / "components" / "poly_mul"

    # Champion configurations: maps a champion to its sim baseline/champion paths
    # and hardware baseline/champion variants.
    CHAMPIONS = [
        {
            "variant": "scalar_zb",
            "label": "Scalar Zb (Scalar Tier)",
            "sim_baseline_variant": "scalar_ref",
            "sim_champion_variant": "scalar_zb",
        },
        {
            "variant": "vector_rvv",
            "label": "Vector RVV (Vector Tier)",
            "sim_baseline_variant": "vector_ref",
            "sim_champion_variant": "vector_rvv",
        },
    ]

    def _load_sim_variant(variant: str) -> dict[tuple[int, str], int]:
        """Load simulation polymul CSV for a variant (returns empty dict if missing)."""
        slug = variant_slug(variant)
        path = POLY_MUL_DIR / f"sim_mul_{slug}_latest.csv"
        if not path.exists():
            return {}
        return read_csv(path)

    def _load_hw_variant(variant: str) -> dict[tuple[int, str], float]:
        """Load hardware polymul CSV for a variant."""
        slug = variant_slug(variant)
        hw_path = HW_MUL_DIR / f"hw_mul_{slug}_latest.csv"
        return read_hw_csv(hw_path)

    def _compute_speedup_int(base: int | None, champ: int | None) -> float | None:
        """Compute speedup from integer instret values."""
        if base is None or champ is None or champ == 0:
            return None
        return base / champ

    def _compute_speedup_float(base: float | None, champ: float | None) -> float | None:
        """Compute speedup from float cycle values."""
        if base is None or champ is None or champ == 0.0:
            return None
        return base / champ

    def _fmt_ratio(val: float | None) -> str:
        """Format a HW/Spike ratio value."""
        if val is None:
            return "\u2014"
        return f"{val:.3f}"

    def _fmt_speedup_val(val: float | None) -> str:
        """Format a pre-computed speedup value as ×N.NN."""
        if val is None:
            return "\u2014"
        return f"\u00d7{val:.2f}"

    buf = io.StringIO()

    buf.write("# HQC-RISC-V Cross-Platform Comparison: Spike ISS vs BPI-F3 Hardware\n\n")
    buf.write(
        "Compares Spike instruction-retired speedups against real hardware cycle speedups\n"
        "for polynomial multiplication (`vect_mul`).\n\n"
        "- **Spike speedup** = ref_instret / champion_instret (deterministic)\n"
        "- **HW speedup** = ref_median_cycles / champion_median_cycles (statistical)\n"
        "- **HW/Spike ratio** = HW_speedup / Spike_speedup\n"
        "  - ratio > 1 \u2192 hardware **amplifies** the Spike-predicted advantage\n"
        "  - ratio < 1 \u2192 hardware **attenuates** it (microarchitectural overhead)\n\n"
    )

    for champ_cfg in CHAMPIONS:
        variant = champ_cfg["variant"]
        label = champ_cfg["label"]

        buf.write(f"## {label}\n\n")

        # Load Spike data
        spike_base = _load_sim_variant(champ_cfg["sim_baseline_variant"])
        spike_champ = _load_sim_variant(champ_cfg["sim_champion_variant"])

        # Load hardware data
        hw_base = _load_hw_variant("scalar_ref")
        hw_champ = _load_hw_variant(variant)

        # Table header
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
            spike_spd = _compute_speedup_int(s_base, s_champ)

            h_base = hw_base.get(key)
            h_champ = hw_champ.get(key)
            hw_spd = _compute_speedup_float(h_base, h_champ)

            # HW/Spike ratio
            if hw_spd is not None and spike_spd is not None and spike_spd != 0:
                ratio = hw_spd / spike_spd
            else:
                ratio = None

            buf.write(
                f"| HQC-{lvl} "
                f"| {fmt_int(s_base)} "
                f"| {fmt_int(s_champ)} "
                f"| {_fmt_speedup_val(spike_spd)} "
                f"| {fmt_cycles(h_base)} "
                f"| {fmt_cycles(h_champ)} "
                f"| {_fmt_speedup_val(hw_spd)} "
                f"| {_fmt_ratio(ratio)} |\n"
            )

        buf.write("\n")

    # Interpretation section
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
        "- A **ratio \u2248 1** suggests the Spike instret model is a good predictor of\n"
        "  real-world performance for that variant/level combination.\n"
    )

    report = buf.getvalue()

    resolved = out_path if out_path else REPO_ROOT / "results" / "reports" / "summaries" / "speedups_cross.md"
    resolved = resolved.resolve()

    resolved.parent.mkdir(parents=True, exist_ok=True)

    existing = resolved.read_text() if resolved.exists() else None
    resolved.write_text(report)

    if existing == report:
        print(f" \u23ed Unchanged: {resolved}")
    else:
        print(f" \U0001f504 Updated: {resolved}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate speedup reports from benchmark CSVs.",
        usage="%(prog)s --platform {sim,hw,cross} [--out PATH]",
    )
    parser.add_argument(
        "--platform",
        choices=["sim", "hw", "cross"],
        required=True,
        help="Target platform: sim (Spike ISS instret), hw (BPI-F3 cycles), cross (sim vs hw comparison)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Write output to this Markdown file (default: auto-determined by platform)",
    )
    args = parser.parse_args()

    if args.platform == "sim":
        generate_sim_report(args.out)
    elif args.platform == "hw":
        generate_hw_report(args.out)
    elif args.platform == "cross":
        generate_cross_report(args.out)


if __name__ == "__main__":
    main()
