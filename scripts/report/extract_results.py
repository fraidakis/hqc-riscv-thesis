#!/usr/bin/env python3
"""scripts/report/extract_results.py

Parse Spike benchmark CSV files across all benchmark-set directories, compute
speedups vs. a baseline variant, and print comparison tables in Markdown or
LaTeX format.
Optionally parse Spike --histogram files to produce instruction-mix breakdowns.

Usage:
    python3 scripts/report/extract_results.py [options]

Options:
    --bench-dir DIR      Root of benchmark directories (default: analysis/benchmarks)
    --bench-set NAME [...]  Restrict to named benchmark set(s)
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
    python3 scripts/report/extract_results.py --bench-set polymul_zbc --level 128
    python3 scripts/report/extract_results.py --metric instret
    python3 scripts/report/extract_results.py --codesize
    python3 scripts/report/extract_results.py --latex > analysis/report/table_results.tex
    python3 scripts/report/extract_results.py --mix --bench-set baseline_ref polymul_zbc
    python3 scripts/report/extract_results.py --write-summary-csv
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any

# ── Constants ─────────────────────────────────────────────────────────────────
SCRIPT_DIR = Path(__file__).parent.resolve()
REPO_ROOT   = SCRIPT_DIR.parent.parent
DEFAULT_BENCH_DIR = REPO_ROOT / "analysis" / "benchmarks"
DEFAULT_HIST_DIR = REPO_ROOT / "analysis" / "profiles" / "histograms"

ALL_VARIANTS = [
    "karatsuba_ref",
    "karatsuba_zbc",
    "fafft_zbc",
    "hybrid",
    "karatsuba_zvbc",
    "fafft_zvbc",
]
ALL_LEVELS  = [128, 192, 256]
ALL_OPS     = ["keypair", "encaps", "decaps", "vect_mul"]

VARIANT_ALIASES = {
    "hybrid_zbc": "hybrid",
}

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
# Key: (bench_set, level, variant, op) → median_cycles or median_instret (int or None)
Results = dict[tuple[str, int, str, str], int | None]
# Key: (bench_set, level, variant) → codesize_text in bytes (int or None)
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
    bench_set, level, variant, op, iterations, min_cycles, median_cycles, max_cycles
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
        variant = VARIANT_ALIASES.get(variant, variant)
    except ValueError:
        print(f"[extract] WARNING: bad metadata in {path}: {meta}", file=sys.stderr)
        return rows

    bench_set = path.parent.name
    run_label_re = re.compile(r"^[A-Za-z0-9_]+_[A-Za-z0-9_]+_[0-9]{8}_[0-9]{4}$")
    if run_label_re.fullmatch(bench_set):
        bench_set = path.parent.parent.name

    if path.parent.name == "curated" and len(path.parents) >= 3 and path.parents[2].name == "suites":
        bench_set = f"suite:{path.parent.parent.name}"

    # Parse CSV body
    try:
        reader = csv.DictReader(data_lines)
        for row in reader:
            try:
                rows.append({
                    "bench_set":       bench_set,
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
    bench_sets: list[str] | None,
    levels: list[int] | None,
    variants: list[str] | None,
    ops: list[str] | None,
    metric: str = "cycles",
) -> tuple[Results, CodeSizes]:
    """Walk bench_dir, parse all *_N*.csv files, return (Results, CodeSizes).

    metric: "cycles" (default) or "instret" — selects the median value stored
    in Results.  Old CSV files without instret columns return None for instret.
    """
    results: Results = {}
    code_sizes: CodeSizes = {}

    if not bench_dir.is_dir():
        print(f"[extract] ERROR: bench directory not found: {bench_dir}", file=sys.stderr)
        sys.exit(1)

    csv_patterns = [
        "isolated/*/*/*_N*.csv",
        "isolated/*/*/*/*_N*.csv",
        "isolated/*/*_N*.csv",
        "suites/*/curated/*_N*.csv",
        "canonical/*/*_N*.csv",
        "canonical/*/*/*_N*.csv",
    ]

    csv_files: list[Path] = []
    for pattern in csv_patterns:
        csv_files.extend(path for path in bench_dir.glob(pattern) if path.parent.is_dir())

    # Keep deterministic order and avoid duplicate paths when patterns overlap.
    csv_files = sorted(set(csv_files))
    if not csv_files:
        print(f"[extract] No CSV files found in subdirectories of {bench_dir}", file=sys.stderr)
        return results, code_sizes

    for csv_path in sorted(csv_files):
        for row in parse_csv_file(csv_path):
            if bench_sets and row["bench_set"] not in bench_sets: continue
            if levels   and row["level"]   not in levels:   continue
            if variants and row["variant"] not in variants: continue
            if ops      and row["op"]      not in ops:      continue

            key = (row["bench_set"], row["level"], row["variant"], row["op"])
            if metric == "instret":
                results[key] = row["median_instret"]
            else:
                results[key] = row["median_cycles"]

            sz_key = (row["bench_set"], row["level"], row["variant"])
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


def resolve_baseline_value(
    results: Results,
    baseline_variant: str,
    bench_sets: list[str],
    level: int,
    op: str,
) -> int | None:
    for bench_set in bench_sets:
        value = results.get((bench_set, level, baseline_variant, op))
        if value is not None:
            return value
    return None


# ── Markdown table output ─────────────────────────────────────────────────────

def print_markdown_table(
    results: Results,
    baseline_variant: str,
    bench_sets: list[str],
    levels: list[int],
    variants: list[str],
    ops: list[str],
    metric: str = "cycles",
) -> None:
    metric_label = "retired instructions" if metric == "instret" else "cycle counts"
    for op in ops:
        for level in levels:
            active_bench_sets = [
                bench_set for bench_set in bench_sets
                if any(results.get((bench_set, level, v, op)) is not None for v in variants)
            ]
            if not active_bench_sets:
                continue

            print(f"\n## `{op}` {metric_label} — HQC-{level}\n")
            header  = "| Benchmark Set | " + " | ".join(f"{v:>15}" for v in variants) + " |"
            divider = "|----------|" + "|".join("-" * 17 for _ in variants) + "|"
            print(header)
            print(divider)

            for bench_set in active_bench_sets:
                baseline_val = resolve_baseline_value(
                    results, baseline_variant, bench_sets, level, op
                )
                cells: list[str] = []
                for v in variants:
                    val = results.get((bench_set, level, v, op))
                    if val is None:
                        cells.append("—")
                    elif v == baseline_variant:
                        cells.append(f"{fmt_cycles(val)} (1.0×)")
                    else:
                        speedup = compute_speedup(baseline_val, val)
                        cells.append(f"{fmt_cycles(val)} ({speedup})")
                row_str = "| " + f"{bench_set:<13}" + " | " + " | ".join(f"{c:>15}" for c in cells) + " |"
                print(row_str)

        print()


# ── LaTeX table output ────────────────────────────────────────────────────────

def print_latex_table(
    results: Results,
    baseline_variant: str,
    bench_sets: list[str],
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
        "hybrid":        "hybrid",
        "karatsuba_zvbc": "kara\\textsubscript{zvbc}",
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
        header_cells = ["Benchmark Set"] + [SHORT.get(v, v) for v in variants]
        print(f"    " + " & ".join(header_cells) + r" \\")
        print(f"    \\midrule")

        for level in levels:
            active_bench_sets = [
                bench_set for bench_set in bench_sets
                if any(results.get((bench_set, level, v, op)) is not None for v in variants)
            ]
            if not active_bench_sets:
                continue

            print(f"    \\multicolumn{{{len(variants) + 1}}}{{l}}{{\\textit{{HQC-{level}}}}}" + r" \\")

            for bench_set in active_bench_sets:
                baseline_val = resolve_baseline_value(
                    results, baseline_variant, bench_sets, level, op
                )
                cells: list[str] = [bench_set]
                for v in variants:
                    val = results.get((bench_set, level, v, op))
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
    bench_sets: list[str],
    levels: list[int],
    variants: list[str],
    as_latex: bool,
) -> None:
    """Print a table of .text section sizes (bytes) per (benchmark set, level, variant)."""
    if as_latex:
        print("\n% Code size (.text section, bytes)")
        col_spec = "ll" + "r" * len(variants)
        print(f"\\begin{{tabular}}{{{col_spec}}}")
        print("  \\toprule")
        print("  Benchmark Set & Level & " + " & ".join(variants) + r" \\")
        print("  \\midrule")
        for bench_set in bench_sets:
            for level in levels:
                cells = [bench_set, str(level)]
                any_data = False
                for v in variants:
                    sz = code_sizes.get((bench_set, level, v))
                    cells.append(f"{sz:,}" if sz is not None else "—")
                    if sz is not None:
                        any_data = True
                if any_data:
                    print("  " + " & ".join(cells) + r" \\")
        print("  \\bottomrule")
        print("\\end{tabular}")
    else:
        print("\n## Code size (.text section, bytes)\n")
        header  = "| Benchmark Set | Level | " + " | ".join(f"{v:>15}" for v in variants) + " |"
        divider = "|----------|-------|" + "|".join("-" * 17 for _ in variants) + "|"
        print(header)
        print(divider)
        for bench_set in bench_sets:
            for level in levels:
                cells: list[str] = []
                any_data = False
                for v in variants:
                    sz = code_sizes.get((bench_set, level, v))
                    cells.append(f"{sz:,}" if sz is not None else "—")
                    if sz is not None:
                        any_data = True
                if any_data:
                    row_str = "| " + f"{bench_set:<13}" + " | " + f"{level:<5}" + " | "
                    row_str += " | ".join(f"{c:>15}" for c in cells) + " |"
                    print(row_str)


# ── Histogram / instruction-mix parsing ──────────────────────────────────────

def categorize_instruction(insn: str) -> str:
    for pattern, category in MIX_CATEGORIES:
        if re.match(pattern, insn.strip(), re.IGNORECASE):
            return category
    return "other"


def build_pc_mnemonic_map(elf_path: Path) -> dict[str, str]:
    """Run objdump on elf_path and return {hex_pc: mnemonic}.

    Tries, in order: $RISCV/bin/riscv64-unknown-elf-objdump, the default
    installed-tools location, then whatever is on PATH.  Returns {} on failure.
    """
    candidates = [
        os.path.join(os.environ.get("RISCV", ""), "bin", "riscv64-unknown-elf-objdump"),
        str(Path.home() / "Thesis" / "installed-tools" / "bin" / "riscv64-unknown-elf-objdump"),
    ]
    found = shutil.which("riscv64-unknown-elf-objdump")
    if found:
        candidates.append(found)

    objdump = next((c for c in candidates if c and Path(c).is_file()), None)
    if objdump is None or not elf_path.is_file():
        return {}

    try:
        result = subprocess.run(
            [objdump, "-d", str(elf_path)],
            capture_output=True, text=True, timeout=60,
        )
        pc_map: dict[str, str] = {}
        for line in result.stdout.splitlines():
            m = re.match(r'^\s+([0-9a-f]+):\s+[0-9a-f]+\s+(\S+)', line)
            if m:
                pc_map[m.group(1)] = m.group(2)
        return pc_map
    except Exception:
        return {}


def parse_histogram_file(path: Path, pc_map: dict[str, str] | None = None) -> dict[str, int]:
    """Parse a Spike -g histogram file.  Returns {mnemonic_or_pc: count}.

    Spike -g writes space-separated "pc count" lines prefixed by a
    "PC Histogram size:N" header.  If pc_map is provided, each PC is resolved
    to its instruction mnemonic so the returned keys match MIX_CATEGORIES.
    """
    counts: dict[str, int] = {}
    try:
        with path.open() as fh:
            for line in fh:
                line = line.strip()
                if not line or line.startswith("PC Histogram"):
                    continue
                parts = line.split()
                if len(parts) >= 2:
                    pc = parts[0]
                    try:
                        count = int(parts[1])
                        key = pc_map.get(pc, pc) if pc_map else pc
                        counts[key] = counts.get(key, 0) + count
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
    hist_dir: Path,
    bench_sets: list[str],
    levels: list[int],
    variants: list[str],
    as_latex: bool,
    build_dir: Path | None = None,
) -> None:
    print("\n## Instruction mix breakdown\n")

    for bench_set in bench_sets:
        for level in levels:
            row_data: dict[str, dict[str, float]] = {}  # variant → {category: pct}

            for variant in variants:
                hist_path = hist_dir / bench_set / f"hqc{level}_{variant}_hist.txt"
                if not hist_path.exists():
                    continue

                pc_map: dict[str, str] | None = None
                if build_dir is not None:
                    elf = build_dir / f"bench_polymul_hqc{level}_{variant}.elf"
                    pc_map = build_pc_mnemonic_map(elf) or None

                counts = parse_histogram_file(hist_path, pc_map)
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
                print(f"\n% Instruction mix — {bench_set}, HQC-{level}")
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
                print(f"\n### {bench_set} — HQC-{level}")
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
    hist_dir: Path,
    baseline_variant: str,
    bench_sets: list[str],
    levels: list[int],
    variants: list[str],
    ops: list[str],
    build_dir: Path | None = None,
) -> None:
    """
    Write cycle_counts.csv and instruction_mix.csv in the format expected by
    scripts/report/plot.py.

    cycle_counts.csv columns: impl, hqc128, hqc192, hqc256
    Each row represents one (benchmark set, variant, op) combination.
    We use the encaps op as the representative KEM operation for the cycle plot.
    """
    cycle_csv = bench_dir / "cycle_counts.csv"
    mix_csv   = bench_dir / "instruction_mix.csv"

    # ── cycle_counts.csv ─────────────────────────────────────────────────────
    rep_op = "encaps" if "encaps" in ops else (ops[0] if ops else "encaps")

    rows_out: list[dict[str, str]] = []
    for bench_set in bench_sets:
        for variant in variants:
            row: dict[str, str] = {"impl": f"{bench_set}/{variant}"}
            any_data = False
            for level in levels:
                val = results.get((bench_set, level, variant, rep_op))
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
    # Collect mix data across all benchmark-set/level/variant combinations
    mix_rows: list[dict[str, str]] = []
    category_names: list[str] = []
    seen_mix: dict[str, dict[str, float]] = {}

    for bench_set in bench_sets:
        for level in levels:
            for variant in variants:
                hist_path = hist_dir / bench_set / f"hqc{level}_{variant}_hist.txt"
                if not hist_path.exists():
                    continue

                pc_map: dict[str, str] | None = None
                if build_dir is not None:
                    elf = build_dir / f"bench_polymul_hqc{level}_{variant}.elf"
                    pc_map = build_pc_mnemonic_map(elf) or None

                counts = parse_histogram_file(hist_path, pc_map)
                if not counts:
                    continue
                agg   = aggregate_mix(counts)
                total = sum(agg.values()) or 1
                key   = f"{bench_set}/{variant}/{level}"
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
    parser.add_argument("--hist-dir", type=Path, default=DEFAULT_HIST_DIR,
                        help="Root of histogram directories")
    parser.add_argument("--build-dir", type=Path,
                        default=REPO_ROOT / "build/spike",
                        help="Directory containing bench_polymul*.elf files for PC→mnemonic "
                            "resolution (default: build/spike)")
    parser.add_argument("--bench-set", nargs="+", metavar="NAME",
                        help="Benchmark set(s) to include, e.g. baseline_ref polymul_zbc")
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
    hist_dir: Path = args.hist_dir
    build_dir: Path | None = args.build_dir if args.build_dir.is_dir() else None
    if args.build_dir and not args.build_dir.is_dir():
        print(f"[extract] NOTE: --build-dir not found: {args.build_dir} — PC resolution disabled",
              file=sys.stderr)

    if args.bench_set:
        bench_sets = args.bench_set
    else:
        if bench_dir.is_dir():
            bench_sets = sorted(
                d.name for d in bench_dir.iterdir()
                if d.is_dir()
            )
        else:
            bench_sets = []

    levels   = args.level   or ALL_LEVELS
    variants = args.variant or ALL_VARIANTS
    ops      = args.op      or ALL_OPS

    if not bench_sets:
        print("[extract] No benchmark-set directories found. Run Spike benchmarks first.", file=sys.stderr)
        sys.exit(0)

    results, code_sizes = collect_results(bench_dir, bench_sets, levels, variants, ops,
                                           metric=args.metric)

    if not results:
        print("[extract] No benchmark data found. Check that Spike runs have completed.", file=sys.stderr)
        sys.exit(0)

    # ── Output tables ──────────────────────────────────────────────────────────
    if args.latex:
        print_latex_table(results, args.baseline, bench_sets, levels, variants, ops,
                          metric=args.metric)
    else:
        print_markdown_table(results, args.baseline, bench_sets, levels, variants, ops,
                             metric=args.metric)

    if args.codesize:
        print_codesize_table(code_sizes, bench_sets, levels, variants, as_latex=args.latex)

    if args.mix:
        print_mix_table(hist_dir, bench_sets, levels, variants, as_latex=args.latex,
                        build_dir=build_dir)

    if args.write_summary_csv:
        write_summary_csvs(results, bench_dir, hist_dir, args.baseline, bench_sets, levels,
                           variants, ops, build_dir=build_dir)

if __name__ == "__main__":
    main()
