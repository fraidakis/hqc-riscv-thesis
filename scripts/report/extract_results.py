#!/usr/bin/env python3
"""scripts/report/extract_results.py

Parse Spike benchmark CSV files across all phase directories, compute speedups
vs. a baseline variant, and print comparison tables in Markdown or LaTeX format.
Optionally parse Spike --histogram files to produce instruction-mix breakdowns.

Usage:
    python3 scripts/report/extract_results.py [options]

Options:
    --bench-dir DIR      Root of benchmark directories (default: analysis/benchmarks)
    --phase PHASE [...]  Restrict to named phase(s), e.g. phase0b phase1a
    --level {128,192,256} [...]  Restrict to security level(s)
    --variant V [...]    Restrict to variant name(s)
    --op OP [...]        Restrict to operation(s): keypair encaps decaps vect_mul
    --baseline V         Variant used as speedup baseline (default: karatsuba_ref)
    --metric {cycles,instret}  Primary metric to display (default: cycles)
    --codesize           Print a code-size table (bytes of .text)
    --latex              Output LaTeX tabular instead of Markdown
    --mix                Also parse *_hist.txt files and print instruction-mix table
    --write-summary-csv  Write cycle_counts.csv + instruction_mix.csv for plot.py

Examples:
    python3 scripts/report/extract_results.py
    python3 scripts/report/extract_results.py --phase phase1a --level 128
    python3 scripts/report/extract_results.py --metric instret
    python3 scripts/report/extract_results.py --codesize
    python3 scripts/report/extract_results.py --latex > analysis/report/table_results.tex
    python3 scripts/report/extract_results.py --mix --phase phase0b phase1a
    python3 scripts/report/extract_results.py --write-summary-csv
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any

# ── Constants ─────────────────────────────────────────────────────────────────
SCRIPT_DIR = Path(__file__).parent.resolve()
REPO_ROOT   = SCRIPT_DIR.parent.parent
DEFAULT_BENCH_DIR = REPO_ROOT / "analysis" / "benchmarks"

ALL_VARIANTS = [
    "karatsuba_ref",
    "karatsuba_zbc",
    "fafft_zbc",
    "hybrid_zbc",
    "fafft_zvbc",
]
ALL_LEVELS  = [128, 192, 256]
ALL_OPS     = ["keypair", "encaps", "decaps", "vect_mul"]

# Instruction-mix histogram categories (grep patterns → friendly name)
MIX_CATEGORIES: list[tuple[str, str]] = [
    (r"^(xor|xori|slli|srli|srai|and|andi|or|ori)$", "scalar_alu"),
    (r"^(clmul|clmulh|clmulr)$",                      "clmul/clmulh"),
    (r"^(vclmul\.vv|vclmulh\.vv)$",                   "vclmul/vclmulh"),
    (r"^(vxor\.vv|vxor\.vi|vxor\.vx)$",               "vxor"),
    (r"^(ld|lw|lh|lb|sd|sw|sh|sb|lbu|lhu|lwu)$",      "load_store"),
    (r"^(beq|bne|blt|bge|bltu|bgeu|jal|jalr)$",       "branch/jump"),
]

# ── Data structures ───────────────────────────────────────────────────────────
# Key: (phase, level, variant, op) → median_cycles or median_instret (int or None)
Results = dict[tuple[str, int, str, str], int | None]
# Key: (phase, level, variant) → codesize_text in bytes (int or None)
CodeSizes = dict[tuple[str, int, str], int | None]

# ── CSV parsing ───────────────────────────────────────────────────────────────

def parse_metadata_comment(line: str) -> dict[str, str]:
    """Parse '# level=128,polymul=karatsuba_ref,n_iter=1' → {'level': '128', ...}"""
    meta: dict[str, str] = {}
    line = line.lstrip("#").strip()
    for kv in line.split(","):
        if "=" in kv:
            k, v = kv.split("=", 1)
            meta[k.strip()] = v.strip()
    return meta


def parse_csv_file(path: Path) -> list[dict[str, Any]]:
    """
    Parse a Spike bench CSV file.  Returns a list of row dicts with keys:
      phase, level, variant, op, iterations, min_cycles, median_cycles, max_cycles
    Returns [] on error (logs a warning).
    """
    rows: list[dict[str, Any]] = []
    try:
        with path.open() as fh:
            lines = fh.readlines()
    except OSError as e:
        print(f"[extract] WARNING: cannot read {path}: {e}", file=sys.stderr)
        return rows

    if not lines:
        return rows

    # Extract metadata from the first non-empty comment line
    meta: dict[str, str] = {}
    data_lines: list[str] = []
    for line in lines:
        if line.startswith("#"):
            if not meta:
                meta = parse_metadata_comment(line)
        else:
            data_lines.append(line)

    if not meta:
        print(f"[extract] WARNING: no metadata header in {path}", file=sys.stderr)
        return rows

    try:
        level   = int(meta.get("level", "0"))
        variant = meta.get("polymul", "")
    except ValueError:
        print(f"[extract] WARNING: bad metadata in {path}: {meta}", file=sys.stderr)
        return rows

    # Derive phase from parent directory name (e.g. "phase0b")
    phase = path.parent.name

    # Parse CSV body
    try:
        reader = csv.DictReader(data_lines)
        for row in reader:
            try:
                rows.append({
                    "phase":           phase,
                    "level":           level,
                    "variant":         variant,
                    "op":              row["op"].strip(),
                    "iterations":      int(row.get("iterations", 1)),
                    "min_cycles":      int(row["min_cycles"]),
                    "median_cycles":   int(row["median_cycles"]),
                    "max_cycles":      int(row["max_cycles"]),
                    "median_instret":  int(row["median_instret"]) if "median_instret" in row else None,
                    "codesize_text":   int(meta["codesize_text"]) if "codesize_text" in meta else None,
                })
            except (KeyError, ValueError) as e:
                print(f"[extract] WARNING: skipping row in {path}: {e}", file=sys.stderr)
    except Exception as e:
        print(f"[extract] WARNING: CSV parse error in {path}: {e}", file=sys.stderr)

    return rows


def collect_results(
    bench_dir: Path,
    phases: list[str] | None,
    levels: list[int] | None,
    variants: list[str] | None,
    ops: list[str] | None,
    metric: str = "cycles",
) -> tuple[Results, CodeSizes]:
    """Walk bench_dir, parse all *_N1.csv files, return (Results, CodeSizes).

    metric: "cycles" (default) or "instret" — selects the median value stored
    in Results.  Old CSV files without instret columns return None for instret.
    """
    results: Results = {}
    code_sizes: CodeSizes = {}

    if not bench_dir.is_dir():
        print(f"[extract] ERROR: bench directory not found: {bench_dir}", file=sys.stderr)
        sys.exit(1)

    csv_files = list(bench_dir.glob("phase*/*_N1.csv"))
    if not csv_files:
        print(f"[extract] No CSV files found in {bench_dir}/phase*/", file=sys.stderr)
        return results, code_sizes

    for csv_path in sorted(csv_files):
        for row in parse_csv_file(csv_path):
            if phases   and row["phase"]   not in phases:   continue
            if levels   and row["level"]   not in levels:   continue
            if variants and row["variant"] not in variants: continue
            if ops      and row["op"]      not in ops:      continue

            key = (row["phase"], row["level"], row["variant"], row["op"])
            if metric == "instret":
                results[key] = row["median_instret"]
            else:
                results[key] = row["median_cycles"]

            # Store codesize keyed by (phase, level, variant) — same for all ops
            sz_key = (row["phase"], row["level"], row["variant"])
            if sz_key not in code_sizes:
                code_sizes[sz_key] = row["codesize_text"]

    return results, code_sizes


# ── Speedup computation ───────────────────────────────────────────────────────

def compute_speedup(baseline_cycles: int | None, variant_cycles: int | None) -> str:
    if baseline_cycles is None or variant_cycles is None:
        return "N/A"
    if variant_cycles == 0:
        return "∞"
    ratio = baseline_cycles / variant_cycles
    return f"{ratio:.1f}×"


def fmt_cycles(n: int | None) -> str:
    if n is None:
        return "—"
    if n >= 1_000_000_000:
        return f"{n / 1_000_000_000:.2f} G"
    if n >= 1_000_000:
        return f"{n / 1_000_000:.1f} M"
    if n >= 1_000:
        return f"{n / 1_000:.1f} K"
    return str(n)


# ── Markdown table output ─────────────────────────────────────────────────────

def print_markdown_table(
    results: Results,
    baseline_variant: str,
    phases: list[str],
    levels: list[int],
    variants: list[str],
    ops: list[str],
    metric: str = "cycles",
) -> None:
    metric_label = "retired instructions" if metric == "instret" else "cycle counts"
    for op in ops:
        for level in levels:
            # Collect phases that have any data for this level/op
            active_phases = [
                ph for ph in phases
                if any(results.get((ph, level, v, op)) is not None for v in variants)
            ]
            if not active_phases:
                continue

            print(f"\n## `{op}` {metric_label} — HQC-{level}\n")
            header  = "| Phase    | " + " | ".join(f"{v:>15}" for v in variants) + " |"
            divider = "|----------|" + "|".join("-" * 17 for _ in variants) + "|"
            print(header)
            print(divider)

            for phase in active_phases:
                baseline_val = results.get((phase, level, baseline_variant, op))
                cells: list[str] = []
                for v in variants:
                    val = results.get((phase, level, v, op))
                    if val is None:
                        cells.append("—")
                    elif v == baseline_variant:
                        cells.append(f"{fmt_cycles(val)} (1.0×)")
                    else:
                        speedup = compute_speedup(baseline_val, val)
                        cells.append(f"{fmt_cycles(val)} ({speedup})")
                row_str = "| " + f"{phase:<8}" + " | " + " | ".join(f"{c:>15}" for c in cells) + " |"
                print(row_str)

        print()


# ── LaTeX table output ────────────────────────────────────────────────────────

def print_latex_table(
    results: Results,
    baseline_variant: str,
    phases: list[str],
    levels: list[int],
    variants: list[str],
    ops: list[str],
    metric: str = "cycles",
) -> None:
    # Short variant names for LaTeX column headers (to fit in table)
    SHORT = {
        "karatsuba_ref": "kara\\textsubscript{ref}",
        "karatsuba_zbc": "kara\\textsubscript{zbc}",
        "fafft_zbc":     "fafft\\textsubscript{zbc}",
        "hybrid_zbc":    "hybrid\\textsubscript{zbc}",
        "fafft_zvbc":    "fafft\\textsubscript{zvbc}",
    }

    for op in ops:
        print(f"\n% {op} {metric} (Spike ISS, VLEN=256)")
        print(f"\\begin{{table}}[h]")
        print(f"  \\centering")
        metric_label = "retired instructions" if metric == "instret" else "cycle counts"
        print(f"  \\caption{{HQC \\texttt{{{op}}} {metric_label} (Spike ISS, VLEN=256)}}")
        print(f"  \\label{{tab:{op}_counts}}")
        col_spec = "l" + "r" * len(variants)
        print(f"  \\begin{{tabular}}{{{col_spec}}}")
        print(f"    \\toprule")

        # Header row
        header_cells = ["Phase"] + [SHORT.get(v, v) for v in variants]
        print(f"    " + " & ".join(header_cells) + r" \\")
        print(f"    \\midrule")

        for level in levels:
            active_phases = [
                ph for ph in phases
                if any(results.get((ph, level, v, op)) is not None for v in variants)
            ]
            if not active_phases:
                continue

            print(f"    \\multicolumn{{{len(variants) + 1}}}{{l}}{{\\textit{{HQC-{level}}}}}" + r" \\")

            for phase in active_phases:
                baseline_val = results.get((phase, level, baseline_variant, op))
                cells: list[str] = [phase]
                for v in variants:
                    val = results.get((phase, level, v, op))
                    if val is None:
                        cells.append("—")
                    elif v == baseline_variant:
                        cells.append(f"{fmt_cycles(val)}")
                    else:
                        speedup = compute_speedup(baseline_val, val)
                        cells.append(f"{fmt_cycles(val)} ({speedup})")
                print(f"    " + " & ".join(cells) + r" \\")

        print(f"    \\bottomrule")
        print(f"  \\end{{tabular}}")
        print(f"\\end{{table}}")


# ── Code-size table ───────────────────────────────────────────────────────────

def print_codesize_table(
    code_sizes: CodeSizes,
    phases: list[str],
    levels: list[int],
    variants: list[str],
    as_latex: bool,
) -> None:
    """Print a table of .text section sizes (bytes) per (phase, level, variant)."""
    if as_latex:
        print("\n% Code size (.text section, bytes)")
        col_spec = "ll" + "r" * len(variants)
        print(f"\\begin{{tabular}}{{{col_spec}}}")
        print("  \\toprule")
        print("  Phase & Level & " + " & ".join(variants) + r" \\")
        print("  \\midrule")
        for phase in phases:
            for level in levels:
                cells = [phase, str(level)]
                any_data = False
                for v in variants:
                    sz = code_sizes.get((phase, level, v))
                    cells.append(f"{sz:,}" if sz is not None else "—")
                    if sz is not None:
                        any_data = True
                if any_data:
                    print("  " + " & ".join(cells) + r" \\")
        print("  \\bottomrule")
        print("\\end{tabular}")
    else:
        print("\n## Code size (.text section, bytes)\n")
        header  = "| Phase    | Level | " + " | ".join(f"{v:>15}" for v in variants) + " |"
        divider = "|----------|-------|" + "|".join("-" * 17 for _ in variants) + "|"
        print(header)
        print(divider)
        for phase in phases:
            for level in levels:
                cells: list[str] = []
                any_data = False
                for v in variants:
                    sz = code_sizes.get((phase, level, v))
                    cells.append(f"{sz:,}" if sz is not None else "—")
                    if sz is not None:
                        any_data = True
                if any_data:
                    row_str = "| " + f"{phase:<8}" + " | " + f"{level:<5}" + " | "
                    row_str += " | ".join(f"{c:>15}" for c in cells) + " |"
                    print(row_str)


# ── Histogram / instruction-mix parsing ──────────────────────────────────────

def categorize_instruction(insn: str) -> str:
    for pattern, category in MIX_CATEGORIES:
        if re.match(pattern, insn.strip(), re.IGNORECASE):
            return category
    return "other"


def parse_histogram_file(path: Path) -> dict[str, int]:
    """Parse a Spike --histogram output file.  Returns {insn: count}."""
    counts: dict[str, int] = {}
    try:
        with path.open() as fh:
            for line in fh:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) >= 2:
                    insn  = parts[0].strip()
                    try:
                        count = int(parts[1].strip())
                        counts[insn] = counts.get(insn, 0) + count
                    except ValueError:
                        pass
    except OSError as e:
        print(f"[extract] WARNING: cannot read {path}: {e}", file=sys.stderr)
    return counts


def aggregate_mix(counts: dict[str, int]) -> dict[str, int]:
    """Aggregate per-instruction counts into broad categories."""
    cat_totals: dict[str, int] = defaultdict(int)
    for insn, count in counts.items():
        cat = categorize_instruction(insn)
        cat_totals[cat] += count
    return dict(cat_totals)


def print_mix_table(
    bench_dir: Path,
    phases: list[str],
    levels: list[int],
    variants: list[str],
    as_latex: bool,
) -> None:
    print("\n## Instruction mix breakdown\n")

    for phase in phases:
        for level in levels:
            row_data: dict[str, dict[str, float]] = {}  # variant → {category: pct}

            for variant in variants:
                hist_path = bench_dir / phase / f"hqc{level}_{variant}_hist.txt"
                if not hist_path.exists():
                    continue
                counts = parse_histogram_file(hist_path)
                if not counts:
                    continue
                agg   = aggregate_mix(counts)
                total = sum(agg.values()) or 1
                row_data[variant] = {cat: 100.0 * n / total for cat, n in agg.items()}

            if not row_data:
                continue

            categories = sorted({cat for d in row_data.values() for cat in d})
            active_variants = [v for v in variants if v in row_data]

            if as_latex:
                print(f"\n% Instruction mix — {phase}, HQC-{level}")
                col_spec = "l" + "r" * len(active_variants)
                print(f"\\begin{{tabular}}{{{col_spec}}}")
                print("  \\toprule")
                print("  Category & " + " & ".join(active_variants) + r" \\")
                print("  \\midrule")
                for cat in categories:
                    cells = [f"{row_data[v].get(cat, 0.0):.1f}\\%" for v in active_variants]
                    print(f"  {cat} & " + " & ".join(cells) + r" \\")
                print("  \\bottomrule")
                print("\\end{tabular}")
            else:
                print(f"\n### {phase} — HQC-{level}")
                header  = "| Category            | " + " | ".join(f"{v:>15}" for v in active_variants) + " |"
                divider = "|---------------------|" + "|".join("-" * 17 for _ in active_variants) + "|"
                print(header)
                print(divider)
                for cat in categories:
                    cells = [f"{row_data[v].get(cat, 0.0):>14.1f}%" for v in active_variants]
                    print("| " + f"{cat:<19}" + " | " + " | ".join(cells) + " |")


# ── Write summary CSVs for plot.py ────────────────────────────────────────────

def write_summary_csvs(
    results: Results,
    bench_dir: Path,
    baseline_variant: str,
    phases: list[str],
    levels: list[int],
    variants: list[str],
    ops: list[str],
) -> None:
    """
    Write cycle_counts.csv and instruction_mix.csv in the format expected by
    analysis/benchmarks/plot.py.

    cycle_counts.csv columns: impl, hqc128, hqc192, hqc256
    Each row represents one (phase, variant, op) combination.
    We use the encaps op as the representative KEM operation for the cycle plot.
    """
    cycle_csv = bench_dir / "cycle_counts.csv"
    mix_csv   = bench_dir / "instruction_mix.csv"

    # ── cycle_counts.csv ─────────────────────────────────────────────────────
    rep_op = "encaps" if "encaps" in ops else (ops[0] if ops else "encaps")

    rows_out: list[dict[str, str]] = []
    for phase in phases:
        for variant in variants:
            row: dict[str, str] = {"impl": f"{phase}/{variant}"}
            any_data = False
            for level in levels:
                val = results.get((phase, level, variant, rep_op))
                row[f"hqc{level}"] = str(val) if val is not None else ""
                if val is not None:
                    any_data = True
            if any_data:
                rows_out.append(row)

    if rows_out:
        with cycle_csv.open("w", newline="") as fh:
            writer = csv.DictWriter(fh, fieldnames=["impl", "hqc128", "hqc192", "hqc256"])
            writer.writeheader()
            writer.writerows(rows_out)
        print(f"[extract] Wrote {cycle_csv}", file=sys.stderr)
    else:
        print(f"[extract] No data for cycle_counts.csv (run Spike benches first)", file=sys.stderr)

    # ── instruction_mix.csv ───────────────────────────────────────────────────
    # Collect mix data across all phase/level/variant combinations
    mix_rows: list[dict[str, str]] = []
    category_names: list[str] = []
    seen_mix: dict[str, dict[str, float]] = {}

    for phase in phases:
        for level in levels:
            for variant in variants:
                hist_path = bench_dir / phase / f"hqc{level}_{variant}_hist.txt"
                if not hist_path.exists():
                    continue
                counts = parse_histogram_file(hist_path)
                if not counts:
                    continue
                agg   = aggregate_mix(counts)
                total = sum(agg.values()) or 1
                key   = f"{phase}/{variant}/{level}"
                seen_mix[key] = {cat: 100.0 * n / total for cat, n in agg.items()}
                for cat in agg:
                    if cat not in category_names:
                        category_names.append(cat)

    if seen_mix:
        # Build rows: category → {impl_key: pct}
        col_names = sorted(seen_mix.keys())
        with mix_csv.open("w", newline="") as fh:
            writer = csv.DictWriter(fh, fieldnames=["category"] + col_names)
            writer.writeheader()
            for cat in sorted(category_names):
                row_out: dict[str, str] = {"category": cat}
                for key in col_names:
                    row_out[key] = f"{seen_mix[key].get(cat, 0.0):.1f}"
                writer.writerow(row_out)
        print(f"[extract] Wrote {mix_csv}", file=sys.stderr)
    else:
        print(f"[extract] No histogram data for instruction_mix.csv", file=sys.stderr)


# ── CLI ───────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--bench-dir", type=Path, default=DEFAULT_BENCH_DIR,
                        help="Root of benchmark directories")
    parser.add_argument("--phase", nargs="+", metavar="PHASE",
                        help="Phase(s) to include, e.g. phase0b phase1a")
    parser.add_argument("--level", nargs="+", type=int, choices=[128, 192, 256],
                        metavar="{128,192,256}",
                        help="Security level(s) to include")
    parser.add_argument("--variant", nargs="+", metavar="VARIANT",
                        help="Variant name(s) to include")
    parser.add_argument("--op", nargs="+",
                        choices=["keypair", "encaps", "decaps", "vect_mul"],
                        metavar="OP",
                        help="Operation(s) to include")
    parser.add_argument("--baseline", default="karatsuba_ref",
                        help="Variant used as speedup baseline (default: karatsuba_ref)")
    parser.add_argument("--metric", choices=["cycles", "instret"], default="cycles",
                        help="Primary metric: cycles (default) or instret (retired instructions)")
    parser.add_argument("--codesize", action="store_true",
                        help="Print a code-size (.text bytes) table")
    parser.add_argument("--latex", action="store_true",
                        help="Output LaTeX tabular instead of Markdown")
    parser.add_argument("--mix", action="store_true",
                        help="Also parse *_hist.txt files and print instruction-mix table")
    parser.add_argument("--write-summary-csv", action="store_true",
                        help="Write cycle_counts.csv + instruction_mix.csv for plot.py")
    args = parser.parse_args()

    bench_dir: Path = args.bench_dir

    # Discover available phases from directory names
    if args.phase:
        phases = args.phase
    else:
        if bench_dir.is_dir():
            phases = sorted(
                d.name for d in bench_dir.iterdir()
                if d.is_dir() and d.name.startswith("phase")
            )
        else:
            phases = []

    levels   = args.level   or ALL_LEVELS
    variants = args.variant or ALL_VARIANTS
    ops      = args.op      or ALL_OPS

    if not phases:
        print("[extract] No phase directories found. Run Spike benchmarks first.", file=sys.stderr)
        sys.exit(0)

    results, code_sizes = collect_results(bench_dir, phases, levels, variants, ops,
                                           metric=args.metric)

    if not results:
        print("[extract] No benchmark data found. Check that Spike runs have completed.", file=sys.stderr)
        sys.exit(0)

    # ── Output tables ──────────────────────────────────────────────────────────
    if args.latex:
        print_latex_table(results, args.baseline, phases, levels, variants, ops,
                          metric=args.metric)
    else:
        print_markdown_table(results, args.baseline, phases, levels, variants, ops,
                             metric=args.metric)

    if args.codesize:
        print_codesize_table(code_sizes, phases, levels, variants, as_latex=args.latex)

    if args.mix:
        print_mix_table(bench_dir, phases, levels, variants, as_latex=args.latex)

    if args.write_summary_csv:
        write_summary_csvs(results, bench_dir, args.baseline, phases, levels, variants, ops)

if __name__ == "__main__":
    main()
