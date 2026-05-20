#!/usr/bin/env python3
"""scripts/report/generate_figures.py

Unified CLI for extracting benchmark data and generating all report figures.

Subcommands
-----------
  extract          Print Markdown / LaTeX comparison tables (replaces extract_results.py)
  complexity       Plot theoretical O(n^α) curves vs empirical cycles (replaces plot_complexity.py)
  dashboard        Plot stable-vs-latest cycle-count dashboard (replaces plot_dashboard.py)
  vlen_sweep       Plot throughput vs VLEN sweep (replaces plot_vlen_sweep.py)
  instruction_mix  Parse a Spike trace and emit an instruction-mix CSV (replaces generate_instruction_mix.py)

Examples
--------
  python scripts/report/generate_figures.py extract
  python scripts/report/generate_figures.py extract --latex > analysis/report/table_results.tex
    python scripts/report/generate_figures.py extract --mix --bench-set isolated compiler_compare
  python scripts/report/generate_figures.py extract --write-summary-csv
    python scripts/report/generate_figures.py complexity
  python scripts/report/generate_figures.py complexity analysis/benchmarks/cycle_counts.csv --output docs/figures/complexity.pdf
  python scripts/report/generate_figures.py dashboard
    python scripts/report/generate_figures.py vlen_sweep
    python scripts/report/generate_figures.py instruction_mix --mul karatsuba_rvv --level 128
    python scripts/report/generate_figures.py instruction_mix analysis/profiles/traces/my_trace.log --label karatsuba_rvv_128
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import shutil
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# ── Repository layout ─────────────────────────────────────────────────────────
SCRIPT_DIR = Path(__file__).parent.resolve()
REPO_ROOT   = SCRIPT_DIR.parent.parent
DEFAULT_BENCH_DIR = REPO_ROOT / "analysis" / "benchmarks"
DEFAULT_HIST_DIR  = REPO_ROOT / "analysis" / "profiles" / "histograms"
DEFAULT_TRACE_DIR = REPO_ROOT / "analysis" / "profiles" / "traces"
DEFAULT_FIGURE_DIR = REPO_ROOT / "docs" / "figures"
DEFAULT_REPORT_DIR = REPO_ROOT / "analysis" / "reports"
DEFAULT_VLEN_SWEEP_DIR = DEFAULT_BENCH_DIR / "isolated" / "vector"
DEFAULT_INSTRUCTION_MIX_CSV = DEFAULT_TRACE_DIR / "instruction_mix.csv"

# ── Benchmark domain constants ────────────────────────────────────────────────
ALL_VARIANTS = [
    "ref_scalar",
    "ref_vector",
    "karatsuba_rvv",
    "karatsuba_zbc",
    "karatsuba_zvbc",
    "karatsuba_ref",
    "fafft_zbc",
    "fafft_zvbc",
    "hybrid",
]
ALL_LEVELS = [128, 192, 256]
ALL_OPS    = ["keypair", "encaps", "decaps", "vect_mul"]

VARIANT_ALIASES: dict[str, str] = {
    "hybrid_zbc": "hybrid",
    "ref-scalar": "ref_scalar",
    "ref-vector": "ref_vector",
}

LEVEL_TO_N: dict[int, int] = {
    128: 512,
    192: 768,
    256: 1024,
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

# ── Type aliases ──────────────────────────────────────────────────────────────
# Key: (bench_set, level, variant, op) → median value (int | None)
Results   = dict[tuple[str, int, str, str], int | None]
# Key: (bench_set, level, variant) → codesize_text in bytes (int | None)
CodeSizes = dict[tuple[str, int, str], int | None]

# ═════════════════════════════════════════════════════════════════════════════
# § 1  SHARED DATA EXTRACTION UTILITIES  (ported from extract_results.py)
# ═════════════════════════════════════════════════════════════════════════════

def _safe_int(value: Any) -> int | None:
    try:
        if value is None:
            return None
        text = str(value).strip()
        if not text:
            return None
        return int(float(text))
    except (TypeError, ValueError):
        return None


def _safe_float(value: Any) -> float | None:
    try:
        if value is None:
            return None
        text = str(value).strip()
        if not text:
            return None
        return float(text)
    except (TypeError, ValueError):
        return None


def _normalize_variant_name(name: str | None) -> str:
    if not name:
        return ""
    normalized = name.strip().lower().replace("-", "_")
    return VARIANT_ALIASES.get(normalized, normalized)


def _extract_level_from_name(name: str) -> int | None:
    m = re.search(r"(?:^|[_-])L?(128|192|256)(?:[_-]|$)", name, re.IGNORECASE)
    if not m:
        return None
    return int(m.group(1))


def _infer_bench_set(path: Path) -> str:
    parts = path.parts
    if "benchmarks" in parts:
        idx = parts.index("benchmarks")
        if idx + 1 < len(parts):
            top = parts[idx + 1]
            if top == "suites" and idx + 2 < len(parts):
                suite_name = parts[idx + 2]
                if "curated" in parts:
                    return f"suite:{suite_name}"
            return top
    return path.parent.name


def _extract_variant_from_path(path: Path) -> str:
    candidate_dirs = [path.parent.name, path.parent.parent.name if len(path.parents) > 1 else ""]
    for candidate in candidate_dirs:
        normalized = _normalize_variant_name(candidate)
        if normalized in ALL_VARIANTS:
            return normalized

    stem = _normalize_variant_name(path.stem)
    for variant in sorted(ALL_VARIANTS, key=len, reverse=True):
        if variant and variant in stem:
            return variant
    return stem


def _match_variant_filter(variant: str, selected: list[str] | None) -> bool:
    if not selected:
        return True
    selected_norm = {_normalize_variant_name(v) for v in selected}
    return _normalize_variant_name(variant) in selected_norm


def _resolve_complexity_csv(csv_arg: str | None, bench_dir: Path) -> Path | None:
    if csv_arg:
        path = Path(csv_arg)
        return path if path.is_file() else None

    candidates: list[Path] = [
        bench_dir / "cycle_counts.csv",
        DEFAULT_BENCH_DIR / "cycle_counts.csv",
        bench_dir / "curated" / "cycle_counts.csv",
    ]

    candidates.extend(sorted((bench_dir / "isolated").glob("**/isolated_polymul_*_latest.csv")))
    candidates.extend(sorted((bench_dir / "compiler_compare").glob("**/compiler_compare_*_latest.csv")))

    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def _discover_trace_input(
    trace_arg: str | None,
    mul: str | None,
    level: int | None,
    trace_dir: Path,
    hist_dir: Path,
) -> Path | None:
    if trace_arg:
        path = Path(trace_arg)
        return path if path.is_file() else None

    variant = _normalize_variant_name(mul)
    variant_tokens = {variant}
    if variant:
        variant_tokens.add(variant.replace("_", "-"))
        variant_tokens.add(variant.replace("_", ""))

    def score(path: Path, require_variant: bool) -> int:
        name = path.name.lower()
        norm_name = name.replace("-", "_")
        s = 0
        if level is not None:
            if f"l{level}" in norm_name or str(level) in norm_name:
                s += 6
            else:
                return -1
        if variant:
            if any(token and token in norm_name for token in variant_tokens):
                s += 8
            elif require_variant:
                return -1
        if "latest" in norm_name:
            s += 2
        if "trace" in norm_name:
            s += 1
        if path.suffix.lower() in {".log", ".trace", ".txt"}:
            s += 3
        return s

    trace_candidates = [p for p in trace_dir.rglob("*") if p.is_file()]
    ranked = [(score(p, require_variant=True), p) for p in trace_candidates]
    ranked = [entry for entry in ranked if entry[0] >= 0]
    if ranked:
        ranked.sort(key=lambda item: (item[0], item[1].stat().st_mtime), reverse=True)
        return ranked[0][1]

    # Fallback: instruction histogram CSVs preserve instruction mnemonics and counts.
    hist_candidates = [p for p in hist_dir.rglob("*.csv") if "histogram" in p.name.lower()]
    ranked_hist = [(score(p, require_variant=False), p) for p in hist_candidates]
    ranked_hist = [entry for entry in ranked_hist if entry[0] >= 0]
    if ranked_hist:
        ranked_hist.sort(key=lambda item: (item[0], item[1].stat().st_mtime), reverse=True)
        return ranked_hist[0][1]
    return None


def _parse_markdown_report_file(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError as exc:
        print(f"[extract] WARNING: cannot read {path}: {exc}", file=sys.stderr)
        return rows

    variant = ""
    for line in lines:
        if line.startswith("- Variant:"):
            variant = _normalize_variant_name(line.split(":", 1)[1].strip())
            break
    if not variant:
        variant = _extract_variant_from_path(path)

    i = 0
    while i < len(lines):
        header = lines[i].strip()
        divider = lines[i + 1].strip() if i + 1 < len(lines) else ""
        if not (header.startswith("|") and divider.startswith("|")):
            i += 1
            continue

        headers = [col.strip().lower() for col in header.strip("|").split("|")]
        if "level" not in headers or "op" not in headers or "instructions" not in headers:
            i += 1
            continue

        i += 2
        while i < len(lines):
            line = lines[i].strip()
            if not line.startswith("|"):
                break
            cells = [col.strip() for col in line.strip("|").split("|")]
            if len(cells) < len(headers):
                cells.extend([""] * (len(headers) - len(cells)))
            elif len(cells) > len(headers):
                cells = cells[:len(headers)]
            row_map = {headers[idx]: cells[idx] for idx in range(len(headers))}

            level = _safe_int(row_map.get("level"))
            op = (row_map.get("op") or "").strip().lower()
            instructions = _safe_int(row_map.get("instructions"))
            codesize = _safe_int(row_map.get("codesize"))
            row_variant = _normalize_variant_name(row_map.get("variant") or variant)

            if level is not None and op and instructions is not None:
                rows.append({
                    "bench_set": "reports",
                    "level": level,
                    "variant": row_variant,
                    "op": op,
                    "iterations": 1,
                    "min_cycles": instructions,
                    "median_cycles": instructions,
                    "max_cycles": instructions,
                    "median_instret": instructions,
                    "codesize_text": codesize,
                })
            i += 1
    return rows

def parse_metadata_comment(line: str) -> dict[str, str]:
    """Parse metadata comments in both 'k=v' and 'k: v' styles."""
    meta: dict[str, str] = {}
    line = line.lstrip("#").strip()
    if not line:
        return meta

    if "," in line:
        chunks = [chunk.strip() for chunk in line.split(",")]
    else:
        chunks = [line]

    for chunk in chunks:
        if "=" in chunk:
            k, v = chunk.split("=", 1)
            meta[k.strip().lower()] = v.strip()
        elif ":" in chunk:
            k, v = chunk.split(":", 1)
            meta[k.strip().lower()] = v.strip()
    return meta


def parse_csv_file(path: Path) -> list[dict[str, Any]]:
    """Parse a Spike bench CSV file.  Returns a list of row dicts."""
    rows: list[dict[str, Any]] = []
    try:
        with path.open() as fh:
            lines = fh.readlines()
    except OSError as e:
        print(f"[extract] WARNING: cannot read {path}: {e}", file=sys.stderr)
        return rows

    if not lines:
        return rows

    meta: dict[str, str] = {}
    data_lines: list[str] = []
    for line in lines:
        if line.startswith("#"):
            meta.update(parse_metadata_comment(line))
        else:
            data_lines.append(line)

    if not data_lines:
        return rows

    bench_set = _infer_bench_set(path)
    default_level = _safe_int(meta.get("level")) or _extract_level_from_name(path.name)
    default_variant = _normalize_variant_name(
        meta.get("polymul") or meta.get("variant") or _extract_variant_from_path(path)
    )
    default_codesize = _safe_int(meta.get("codesize_text") or meta.get("codesize"))

    try:
        reader = csv.DictReader(data_lines)
        for row in reader:
            try:
                op = (row.get("op") or "").strip().lower()
                if not op:
                    continue

                level = _safe_int(row.get("level")) or default_level
                if level is None:
                    continue

                variant = _normalize_variant_name(row.get("variant") or row.get("polymul") or default_variant)
                if not variant:
                    continue

                instructions = _safe_int(row.get("instructions"))
                median_cycles = (
                    _safe_int(row.get("median_cycles"))
                    or _safe_int(row.get("cycles"))
                    or instructions
                )
                median_instret = _safe_int(row.get("median_instret")) or instructions or median_cycles
                if median_cycles is None and median_instret is None:
                    continue

                min_cycles = _safe_int(row.get("min_cycles"))
                max_cycles = _safe_int(row.get("max_cycles"))
                if min_cycles is None:
                    min_cycles = median_cycles
                if max_cycles is None:
                    max_cycles = median_cycles

                codesize = (
                    _safe_int(row.get("codesize_text"))
                    or _safe_int(row.get("codesize"))
                    or default_codesize
                )

                rows.append({
                    "bench_set":      bench_set,
                    "level":          level,
                    "variant":        variant,
                    "op":             op,
                    "iterations":     _safe_int(row.get("iterations")) or _safe_int(meta.get("n_iter")) or 1,
                    "min_cycles":     min_cycles,
                    "median_cycles":  median_cycles,
                    "max_cycles":     max_cycles,
                    "median_instret": median_instret,
                    "codesize_text":  codesize,
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
    """Walk bench_dir, parse CSV benchmark artifacts, return (Results, CodeSizes)."""
    results: Results = {}
    code_sizes: CodeSizes = {}

    if not bench_dir.is_dir():
        print(f"[extract] ERROR: bench directory not found: {bench_dir}", file=sys.stderr)
        sys.exit(1)

    csv_files = sorted({path for path in bench_dir.rglob("*.csv") if path.parent.is_dir()})

    if not csv_files:
        print(f"[extract] No CSV files found in subdirectories of {bench_dir}", file=sys.stderr)
        return results, code_sizes

    selected_variants = [_normalize_variant_name(v) for v in variants] if variants else None

    for csv_path in sorted(csv_files):
        if csv_path.name in {"cycle_counts.csv", "instruction_mix.csv"}:
            continue
        for row in parse_csv_file(csv_path):
            if bench_sets and row["bench_set"] not in bench_sets:
                continue
            if levels and row["level"] not in levels:
                continue
            if not _match_variant_filter(row["variant"], selected_variants):
                continue
            if ops and row["op"] not in ops:
                continue

            key = (row["bench_set"], row["level"], row["variant"], row["op"])
            results[key] = row["median_instret"] if metric == "instret" else row["median_cycles"]

            sz_key = (row["bench_set"], row["level"], row["variant"])
            if sz_key not in code_sizes:
                code_sizes[sz_key] = row["codesize_text"]

    if not results:
        report_files = sorted(DEFAULT_REPORT_DIR.glob("*_report.md"))
        for report_path in report_files:
            for row in _parse_markdown_report_file(report_path):
                if levels and row["level"] not in levels:
                    continue
                if not _match_variant_filter(row["variant"], selected_variants):
                    continue
                if ops and row["op"] not in ops:
                    continue
                key = (row["bench_set"], row["level"], row["variant"], row["op"])
                results[key] = row["median_instret"] if metric == "instret" else row["median_cycles"]
                sz_key = (row["bench_set"], row["level"], row["variant"])
                if sz_key not in code_sizes:
                    code_sizes[sz_key] = row["codesize_text"]

    return results, code_sizes


def compute_speedup(baseline_cycles: int | None, variant_cycles: int | None) -> str:
    if baseline_cycles is None or variant_cycles is None:
        return "N/A"
    if variant_cycles == 0:
        return "∞"
    return f"{baseline_cycles / variant_cycles:.1f}×"


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
                bs for bs in bench_sets
                if any(results.get((bs, level, v, op)) is not None for v in variants)
            ]
            if not active_bench_sets:
                continue

            print(f"\n## `{op}` {metric_label} — HQC-{level}\n")
            header  = "| Benchmark Set | " + " | ".join(f"{v:>15}" for v in variants) + " |"
            divider = "|----------|" + "|".join("-" * 17 for _ in variants) + "|"
            print(header)
            print(divider)

            for bench_set in active_bench_sets:
                baseline_val = resolve_baseline_value(results, baseline_variant, bench_sets, level, op)
                cells: list[str] = []
                for v in variants:
                    val = results.get((bench_set, level, v, op))
                    if val is None:
                        cells.append("—")
                    elif v == baseline_variant:
                        cells.append(f"{fmt_cycles(val)} (1.0×)")
                    else:
                        cells.append(f"{fmt_cycles(val)} ({compute_speedup(baseline_val, val)})")
                print("| " + f"{bench_set:<13}" + " | " + " | ".join(f"{c:>15}" for c in cells) + " |")
        print()


def print_latex_table(
    results: Results,
    baseline_variant: str,
    bench_sets: list[str],
    levels: list[int],
    variants: list[str],
    ops: list[str],
    metric: str = "cycles",
) -> None:
    SHORT = {
        "karatsuba_ref":  r"kara\textsubscript{ref}",
        "karatsuba_zbc":  r"kara\textsubscript{zbc}",
        "fafft_zbc":      r"fafft\textsubscript{zbc}",
        "hybrid":         "hybrid",
        "karatsuba_zvbc": r"kara\textsubscript{zvbc}",
        "fafft_zvbc":     r"fafft\textsubscript{zvbc}",
    }
    for op in ops:
        print(f"\n% {op} {metric} (Spike ISS, VLEN=256)")
        print(r"\begin{table}[h]")
        print(r"  \centering")
        metric_label = "retired instructions" if metric == "instret" else "cycle counts"
        print(f"  \\caption{{HQC \\texttt{{{op}}} {metric_label} (Spike ISS, VLEN=256)}}")
        print(f"  \\label{{tab:{op}_counts}}")
        col_spec = "l" + "r" * len(variants)
        print(f"  \\begin{{tabular}}{{{col_spec}}}")
        print(r"    \toprule")
        header_cells = ["Benchmark Set"] + [SHORT.get(v, v) for v in variants]
        print("    " + " & ".join(header_cells) + r" \\")
        print(r"    \midrule")

        for level in levels:
            active_bench_sets = [
                bs for bs in bench_sets
                if any(results.get((bs, level, v, op)) is not None for v in variants)
            ]
            if not active_bench_sets:
                continue
            print(f"    \\multicolumn{{{len(variants) + 1}}}{{l}}{{\\textit{{HQC-{level}}}}}" + r" \\")
            for bench_set in active_bench_sets:
                baseline_val = resolve_baseline_value(results, baseline_variant, bench_sets, level, op)
                cells: list[str] = [bench_set]
                for v in variants:
                    val = results.get((bench_set, level, v, op))
                    if val is None:
                        cells.append("—")
                    elif v == baseline_variant:
                        cells.append(fmt_cycles(val))
                    else:
                        cells.append(f"{fmt_cycles(val)} ({compute_speedup(baseline_val, val)})")
                print("    " + " & ".join(cells) + r" \\")

        print(r"    \bottomrule")
        print(r"  \end{tabular}")
        print(r"\end{table}")


def print_codesize_table(
    code_sizes: CodeSizes,
    bench_sets: list[str],
    levels: list[int],
    variants: list[str],
    as_latex: bool,
) -> None:
    if as_latex:
        print("\n% Code size (.text section, bytes)")
        col_spec = "ll" + "r" * len(variants)
        print(f"\\begin{{tabular}}{{{col_spec}}}")
        print(r"  \toprule")
        print("  Benchmark Set & Level & " + " & ".join(variants) + r" \\")
        print(r"  \midrule")
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
        print(r"  \bottomrule")
        print(r"\end{tabular}")
    else:
        print("\n## Code size (.text section, bytes)\n")
        header  = "| Benchmark Set | Level | " + " | ".join(f"{v:>15}" for v in variants) + " |"
        divider = "|----------|-------|" + "|".join("-" * 17 for _ in variants) + "|"
        print(header)
        print(divider)
        for bench_set in bench_sets:
            for level in levels:
                cells_out: list[str] = []
                any_data = False
                for v in variants:
                    sz = code_sizes.get((bench_set, level, v))
                    cells_out.append(f"{sz:,}" if sz is not None else "—")
                    if sz is not None:
                        any_data = True
                if any_data:
                    row_str = "| " + f"{bench_set:<13}" + " | " + f"{level:<5}" + " | "
                    row_str += " | ".join(f"{c:>15}" for c in cells_out) + " |"
                    print(row_str)


# ── Histogram / instruction-mix parsing ──────────────────────────────────────

def categorize_histogram_instruction(insn: str) -> str:
    for pattern, category in MIX_CATEGORIES:
        if re.match(pattern, insn.strip(), re.IGNORECASE):
            return category
    return "other"


def build_pc_mnemonic_map(elf_path: Path) -> dict[str, str]:
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
    cat_totals: dict[str, int] = defaultdict(int)
    for insn, count in counts.items():
        cat = categorize_histogram_instruction(insn)
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
            row_data: dict[str, dict[str, float]] = {}
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

            categories       = sorted({cat for d in row_data.values() for cat in d})
            active_variants  = [v for v in variants if v in row_data]

            if as_latex:
                print(f"\n% Instruction mix — {bench_set}, HQC-{level}")
                col_spec = "l" + "r" * len(active_variants)
                print(f"\\begin{{tabular}}{{{col_spec}}}")
                print(r"  \toprule")
                print("  Category & " + " & ".join(active_variants) + r" \\")
                print(r"  \midrule")
                for cat in categories:
                    cells = [f"{row_data[v].get(cat, 0.0):.1f}\\%" for v in active_variants]
                    print(f"  {cat} & " + " & ".join(cells) + r" \\")
                print(r"  \bottomrule")
                print(r"\end{tabular}")
            else:
                print(f"\n### {bench_set} — HQC-{level}")
                header  = "| Category            | " + " | ".join(f"{v:>15}" for v in active_variants) + " |"
                divider = "|---------------------|" + "|".join("-" * 17 for _ in active_variants) + "|"
                print(header)
                print(divider)
                for cat in categories:
                    cells = [f"{row_data[v].get(cat, 0.0):>14.1f}%" for v in active_variants]
                    print("| " + f"{cat:<19}" + " | " + " | ".join(cells) + " |")


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
    cycle_csv = bench_dir / "cycle_counts.csv"
    mix_csv   = bench_dir / "instruction_mix.csv"

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


# ═════════════════════════════════════════════════════════════════════════════
# § 2  COMPLEXITY SUBCOMMAND  (ported from plot_complexity.py)
# ═════════════════════════════════════════════════════════════════════════════

def _parse_complexity_csv(csv_path: Path) -> Dict[str, List[Tuple[float, float]]]:
    data: Dict[str, List[Tuple[float, float]]] = {}
    try:
        with csv_path.open("r", encoding="utf-8", errors="ignore") as f:
            raw_lines = f.readlines()

        meta: dict[str, str] = {}
        data_lines: list[str] = []
        for line in raw_lines:
            if line.startswith("#"):
                meta.update(parse_metadata_comment(line))
            else:
                data_lines.append(line)

        reader = csv.DictReader(data_lines)
        if reader.fieldnames is None:
            return data

        field_map = {field.lower().strip(): field for field in reader.fieldnames if field}
        lower_fields = set(field_map)

        # Summary format produced by "extract --write-summary-csv".
        if {"impl", "hqc128", "hqc192", "hqc256"}.issubset(lower_fields):
            for row in reader:
                raw_algo = row.get(field_map["impl"], "") or "unknown"
                algorithm = _normalize_variant_name(raw_algo.split("/")[-1]) or raw_algo
                for level in ALL_LEVELS:
                    col = field_map.get(f"hqc{level}")
                    value = _safe_float(row.get(col)) if col else None
                    if value is not None:
                        data.setdefault(algorithm, []).append((float(LEVEL_TO_N[level]), value))

        # Flat benchmark format (level, op, instructions, ...).
        elif "level" in lower_fields and (
            "instructions" in lower_fields or "median_cycles" in lower_fields or "cycles" in lower_fields
        ):
            rows = list(reader)
            if not rows:
                return data

            op_field = field_map.get("op")
            selected_rows = rows
            if op_field:
                ops = {(row.get(op_field) or "").strip().lower() for row in rows}
                if "vect_mul" in ops:
                    selected_rows = [row for row in rows if (row.get(op_field) or "").strip().lower() == "vect_mul"]
                elif "total" in ops:
                    selected_rows = [row for row in rows if (row.get(op_field) or "").strip().lower() == "total"]

            level_field = field_map["level"]
            inst_field = field_map.get("instructions")
            cyc_field = field_map.get("median_cycles") or field_map.get("cycles")
            variant_field = field_map.get("variant") or field_map.get("polymul")
            default_variant = _normalize_variant_name(meta.get("variant") or meta.get("polymul") or csv_path.stem)

            for row in selected_rows:
                level = _safe_int(row.get(level_field))
                if level not in LEVEL_TO_N:
                    continue
                raw_variant = row.get(variant_field) if variant_field else default_variant
                algorithm = _normalize_variant_name(raw_variant) or default_variant or "unknown"
                value = _safe_float(row.get(inst_field)) if inst_field else None
                if value is None and cyc_field:
                    value = _safe_float(row.get(cyc_field))
                if value is None:
                    continue
                data.setdefault(algorithm, []).append((float(LEVEL_TO_N[level]), value))

        # Legacy complexity format (algorithm + N + cycles columns).
        else:
            for row in reader:
                if not row:
                    continue
                algorithm = None
                for key in ["algorithm", "variant", "polymul", "category"]:
                    field = field_map.get(key)
                    if field:
                        candidate = row.get(field)
                        if candidate:
                            algorithm = candidate
                            break
                if not algorithm:
                    algorithm = "unknown"

                problem_size = None
                for key in ["n", "input_size", "problem_size"]:
                    field = field_map.get(key)
                    if field and row.get(field):
                        problem_size = _safe_float(row.get(field))
                        if problem_size is not None:
                            break
                if problem_size is None:
                    continue

                cycles = None
                for key in ["cycles", "median_cycles", "total_cycles", "cycle_count", "instructions"]:
                    field = field_map.get(key)
                    if field and row.get(field):
                        cycles = _safe_float(row.get(field))
                        if cycles is not None:
                            break
                if cycles is None:
                    continue

                data.setdefault(_normalize_variant_name(algorithm) or algorithm, []).append((problem_size, cycles))
    except Exception as e:
        print(f"[complexity] Error reading {csv_path}: {e}", file=sys.stderr)
        return {}

    for algo in data:
        dedup: dict[float, float] = {}
        for n_value, cycles in data[algo]:
            dedup[n_value] = cycles
        data[algo] = sorted(dedup.items(), key=lambda x: x[0])
    return data


def _theoretical_complexity(n: "np.ndarray", algorithm: str, scale_factor: float = 1.0) -> "np.ndarray":
    import numpy as np
    algo = algorithm.lower()
    if "schoolbook" in algo or "naive" in algo:
        return scale_factor * n ** 2
    elif "karatsuba" in algo:
        return scale_factor * n ** (np.log(3) / np.log(2))
    elif "fafft" in algo or "fft" in algo:
        return scale_factor * n * np.log2(n)
    elif "hybrid" in algo:
        return scale_factor * n ** 1.7
    else:
        return scale_factor * n ** 2


def _complexity_label(algorithm: str) -> str:
    algo = algorithm.lower()
    if "schoolbook" in algo or "naive" in algo:
        return "O(n²)"
    elif "karatsuba" in algo:
        return "O(n¹·⁵⁸⁵)"
    elif "fafft" in algo or "fft" in algo:
        return "O(n log n)"
    elif "hybrid" in algo:
        return "O(n¹·⁷)"
    else:
        return "O(n²)"


def _fit_scale_factor(n_values: "np.ndarray", empirical: "np.ndarray", algorithm: str) -> float:
    import numpy as np
    if len(n_values) == 0 or len(empirical) == 0:
        return 1.0
    theory = _theoretical_complexity(n_values, algorithm, scale_factor=1.0)
    denom  = np.sum(theory ** 2)
    if denom > 0:
        return max(float(np.sum(empirical * theory) / denom), 0.1)
    return 1.0


def cmd_complexity(args: argparse.Namespace) -> int:
    try:
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        sys.exit("[complexity] matplotlib and numpy are required: pip install matplotlib numpy")

    bench_dir = Path(args.bench_dir)
    csv_path = _resolve_complexity_csv(args.csv, bench_dir)
    if csv_path is None:
        print(
            "[complexity] No input CSV found. Provide one explicitly or generate analysis/benchmarks/cycle_counts.csv",
            file=sys.stderr,
        )
        return 1
    print(f"[complexity] Using data source: {csv_path}")

    data = _parse_complexity_csv(csv_path)
    if not data:
        print("[complexity] No benchmark data found in file", file=sys.stderr)
        return 1

    print(f"[complexity] Found {len(data)} algorithms/variants")
    for algo, pts in sorted(data.items()):
        print(f"  {algo}: {len(pts)} data points")

    colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f"]

    plt.figure(figsize=(14, 8))
    for idx, (algorithm, points) in enumerate(sorted(data.items())):
        if len(points) < 2:
            continue
        n_values = np.array([p[0] for p in points])
        cycles   = np.array([p[1] for p in points])
        color    = colors[idx % len(colors)]

        plt.scatter(n_values, cycles, s=100, alpha=0.7, color=color,
                    label=f"{algorithm} (empirical)", zorder=3)
        scale    = _fit_scale_factor(n_values, cycles, algorithm)
        n_theory = np.linspace(n_values.min(), n_values.max(), 200)
        plt.plot(n_theory, _theoretical_complexity(n_theory, algorithm, scale),
                 linestyle="--", alpha=0.6, color=color, linewidth=2,
                 label=f"{algorithm} (theory: {_complexity_label(algorithm)})")

    plt.xlabel("Problem Size (N)", fontsize=12, fontweight="bold")
    plt.ylabel("Cycle Count",      fontsize=12, fontweight="bold")
    plt.title("Poly-Mul Complexity Analysis: Theory vs Empirical", fontsize=14, fontweight="bold")
    plt.legend(loc="best", fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.yscale("log")
    plt.xscale("log")
    plt.tight_layout()

    output_path = Path(args.output) if args.output else None
    if output_path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(output_path, dpi=150)
        print(f"[complexity] Saved: {output_path}")
    else:
        plt.show()
    return 0


# ═════════════════════════════════════════════════════════════════════════════
# § 3  DASHBOARD SUBCOMMAND  (ported from plot_dashboard.py)
# ═════════════════════════════════════════════════════════════════════════════

_DIR_PATTERN          = re.compile(r"^(.+)_(\d{8}_\d{4})$")
_POLYMUL_CSV_PATTERN  = re.compile(r"^hqc(128|192|256)_.+_polymul_N\d+\.csv$")


@dataclass
class _RunSummary:
    path: Path
    label: str
    timestamp: datetime
    cycles_by_level: dict[str, int]


def _parse_timestamped_dir(path: Path) -> tuple[str, datetime] | None:
    m = _DIR_PATTERN.match(path.name)
    if not m:
        return None
    return m.group(1), datetime.strptime(m.group(2), "%Y%m%d_%H%M")


def _extract_vect_mul_median_cycles(csv_path: Path) -> int | None:
    with csv_path.open(newline="", encoding="utf-8") as fh:
        filtered = (line for line in fh if not line.startswith("#"))
        for row in csv.DictReader(filtered):
            if row.get("op") == "vect_mul":
                val = row.get("median_cycles")
                return int(val) if val is not None else None
    return None


def _summarize_run(run_dir: Path) -> _RunSummary | None:
    parsed = _parse_timestamped_dir(run_dir)
    if parsed is None:
        return None
    label, timestamp = parsed
    cycles_by_level: dict[str, int] = {}
    for csv_file in sorted(run_dir.glob("*.csv")):
        m = _POLYMUL_CSV_PATTERN.match(csv_file.name)
        if not m:
            continue
        median = _extract_vect_mul_median_cycles(csv_file)
        if median is not None:
            cycles_by_level[m.group(1)] = median
    if not cycles_by_level:
        return None
    return _RunSummary(path=run_dir, label=label, timestamp=timestamp, cycles_by_level=cycles_by_level)


def _collect_runs(bench_root: Path) -> list[_RunSummary]:
    runs: list[_RunSummary] = []
    for track in ("scalar", "vector"):
        track_dir = bench_root / track
        if not track_dir.exists():
            continue
        for child in sorted(track_dir.iterdir()):
            if not child.is_dir():
                continue
            summary = _summarize_run(child)
            if summary is not None:
                runs.append(summary)
    runs.sort(key=lambda r: r.timestamp)
    return runs


def cmd_dashboard(args: argparse.Namespace) -> int:
    try:
        import matplotlib.pyplot as plt
        from matplotlib.ticker import FuncFormatter
    except ImportError:
        sys.exit("[dashboard] matplotlib is required: pip install matplotlib")

    bench_root = Path(args.bench_dir) if args.bench_dir else DEFAULT_BENCH_DIR
    figure_path = Path(args.output) if args.output else DEFAULT_FIGURE_DIR / "latest_dashboard.png"

    runs = _collect_runs(bench_root)
    if len(runs) < 2:
        print("[dashboard] Need at least two normalized benchmark directories with polymul CSV data",
              file=sys.stderr)
        return 1

    stable, latest = runs[0], runs[-1]
    levels  = ["128", "192", "256"]
    x       = list(range(len(levels)))
    width   = 0.36

    stable_vals = [stable.cycles_by_level.get(lv, 0) for lv in levels]
    latest_vals = [latest.cycles_by_level.get(lv, 0) for lv in levels]

    fig, ax = plt.subplots(figsize=(10, 5.5))
    ax.bar([i - width / 2 for i in x], stable_vals, width,
           label=f"Stable: {stable.path.name}", color="#5B6C8F")
    ax.bar([i + width / 2 for i in x], latest_vals, width,
           label=f"Latest: {latest.path.name}", color="#D46A4F")
    ax.set_title("Cycle Counts Dashboard: Stable vs Latest (vect_mul median)")
    ax.set_xlabel("Security Level")
    ax.set_ylabel("Median Cycles")
    ax.set_xticks(x)
    ax.set_xticklabels([f"HQC-{lv}" for lv in levels])
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{int(v):,}"))
    ax.grid(axis="y", alpha=0.3)
    ax.legend(fontsize=8)
    subtitle = (
        f"Stable: {stable.timestamp.strftime('%Y-%m-%d %H:%M')}   |   "
        f"Latest: {latest.timestamp.strftime('%Y-%m-%d %H:%M')}"
    )
    fig.text(0.5, 0.01, subtitle, ha="center", fontsize=9)
    fig.tight_layout(rect=[0, 0.04, 1, 1])
    figure_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(figure_path, dpi=160)
    print(f"[dashboard] Saved: {figure_path}")
    return 0


# ═════════════════════════════════════════════════════════════════════════════
# § 4  VLEN_SWEEP SUBCOMMAND  (ported from plot_vlen_sweep.py)
# ═════════════════════════════════════════════════════════════════════════════

def _extract_vlen_from_csv(csv_path: Path) -> Optional[int]:
    m = re.search(r"_v(\d+)_", csv_path.name)
    if m:
        return int(m.group(1))
    try:
        with csv_path.open("r", encoding="utf-8") as f:
            for _ in range(10):
                line = f.readline().strip()
                if line.startswith("#") and ("vlen" in line.lower() or "v=" in line.lower()):
                    m2 = re.search(r"(\d+)", line)
                    if m2:
                        return int(m2.group(1))
    except Exception:
        pass
    return None


def _extract_throughput_from_csv(csv_path: Path) -> Optional[float]:
    try:
        with csv_path.open("r", encoding="utf-8") as f:
            lines = [line for line in f if not line.startswith("#")]
        reader = csv.DictReader(lines)
        if reader.fieldnames is None:
            return None
        for row in reader:
            for key in ["throughput", "ops_per_cycle", "throughput_ops_cycle", "tput"]:
                if key in reader.fieldnames and row.get(key):
                    try:
                        return float(row[key])
                    except ValueError:
                        continue
            if "median_instret" in reader.fieldnames and "median_cycles" in reader.fieldnames:
                try:
                    instret = float(row.get("median_instret", 0))
                    cycles  = float(row.get("median_cycles",  0))
                    if cycles > 0:
                        return instret / cycles
                except (ValueError, TypeError):
                    continue
            if row.get("op") == "vect_mul":
                try:
                    instret = float(row.get("median_instret", 0))
                    cycles  = float(row.get("median_cycles",  1))
                    if cycles > 0:
                        return instret / cycles
                except (ValueError, TypeError):
                    continue
    except Exception as e:
        print(f"[vlen_sweep] Warning: could not read {csv_path}: {e}", file=sys.stderr)
    return None


def _collect_vlen_data(benchmark_dir: Path) -> Dict[str, List[Tuple[int, float]]]:
    data: Dict[str, List[Tuple[int, float]]] = defaultdict(list)
    if not benchmark_dir.exists():
        print(f"[vlen_sweep] Benchmark directory not found: {benchmark_dir}", file=sys.stderr)
        return data

    for csv_file in benchmark_dir.rglob("*.csv"):
        if csv_file.name.startswith("final_") or csv_file.name in ["instruction_mix.csv", "cycle_counts.csv"]:
            continue

        try:
            raw_lines = csv_file.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError as exc:
            print(f"[vlen_sweep] Warning: could not read {csv_file}: {exc}", file=sys.stderr)
            continue

        meta: dict[str, str] = {}
        data_lines: list[str] = []
        for line in raw_lines:
            if line.startswith("#"):
                meta.update(parse_metadata_comment(line))
            else:
                data_lines.append(line)

        reader = csv.DictReader(data_lines)
        if reader.fieldnames is None:
            continue

        field_map = {field.lower().strip(): field for field in reader.fieldnames if field}
        rows = list(reader)
        if not rows:
            continue

        variant = _normalize_variant_name(meta.get("variant") or _extract_variant_from_path(csv_file))
        level = _safe_int(meta.get("level")) or _extract_level_from_name(csv_file.name)

        if "vlen" in field_map and "instructions" in field_map:
            op_field = field_map.get("op")
            selected_rows = rows
            if op_field:
                ops = {(row.get(op_field) or "").strip().lower() for row in rows}
                if "vect_mul" in ops:
                    selected_rows = [r for r in rows if (r.get(op_field) or "").strip().lower() == "vect_mul"]
                elif "total" in ops:
                    selected_rows = [r for r in rows if (r.get(op_field) or "").strip().lower() == "total"]

            for row in selected_rows:
                vlen = _safe_int(row.get(field_map["vlen"]))
                inst = _safe_float(row.get(field_map["instructions"]))
                if vlen is None or inst is None or inst <= 0:
                    continue
                lvl = level or _safe_int(row.get(field_map.get("level", "")))
                if lvl not in LEVEL_TO_N:
                    continue
                # Use a monotonic proxy when only instruction counts are available.
                throughput = 1_000_000.0 / inst
                data[f"{variant}_hqc{lvl}"].append((vlen, throughput))
            continue

        m = re.search(r"hqc(\d+)_([a-z0-9_]+)_polymul", csv_file.name)
        if not m:
            continue
        variant_base = _normalize_variant_name(m.group(2))
        dir_name = csv_file.parent.name
        vlen = _extract_vlen_from_csv(csv_file)
        if vlen is None:
            if "zvbc" in variant_base or "zvbc" in dir_name:
                vlen = 256
            elif "zbc" in variant_base:
                vlen = 64
            else:
                vlen = 128

        throughput = _extract_throughput_from_csv(csv_file)
        if throughput is None:
            continue
        data[f"{variant_base}_hqc{m.group(1)}"].append((vlen, throughput))

    if not data:
        csv_count = len(list(benchmark_dir.rglob("*.csv")))
        print(f"[vlen_sweep] No data extracted ({csv_count} CSV files scanned)", file=sys.stderr)

    for variant in data:
        data[variant].sort(key=lambda x: x[0])
    return data


def cmd_vlen_sweep(args: argparse.Namespace) -> int:
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        sys.exit("[vlen_sweep] matplotlib is required: pip install matplotlib")

    bench_dir = Path(args.bench_dir)
    print(f"[vlen_sweep] Scanning: {bench_dir}")
    data = _collect_vlen_data(bench_dir)
    if not data and bench_dir == DEFAULT_VLEN_SWEEP_DIR:
        alt_dir = DEFAULT_BENCH_DIR / "vlen_sweep" / "vector"
        if alt_dir.exists():
            print(f"[vlen_sweep] Falling back to curated sweep directory: {alt_dir}")
            data = _collect_vlen_data(alt_dir)
    if not data:
        print("[vlen_sweep] No benchmark data found", file=sys.stderr)
        return 1

    print(f"[vlen_sweep] Found {len(data)} variants")
    for variant, pts in sorted(data.items()):
        print(f"  {variant}: {len(pts)} data points")

    colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f"]
    plt.figure(figsize=(12, 7))
    for idx, (variant, points) in enumerate(sorted(data.items())):
        if not points:
            continue
        vlens      = [p[0] for p in points]
        throughput = [p[1] for p in points]
        plt.plot(vlens, throughput, marker="o", linestyle="-", linewidth=2, markersize=8,
                 label=variant, color=colors[idx % len(colors)])

    plt.xlabel("VLEN (bits)",             fontsize=12, fontweight="bold")
    plt.ylabel("Throughput (ops/cycle)",  fontsize=12, fontweight="bold")
    plt.title("Throughput vs VLEN Sweep", fontsize=14, fontweight="bold")
    plt.legend(loc="best", fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    output_path = Path(args.output) if args.output else None
    if output_path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(output_path, dpi=150)
        print(f"[vlen_sweep] Saved: {output_path}")
    else:
        plt.show()
    return 0


# ═════════════════════════════════════════════════════════════════════════════
# § 5  INSTRUCTION_MIX SUBCOMMAND  (ported from generate_instruction_mix.py)
# ═════════════════════════════════════════════════════════════════════════════

_CORE_LINE_RE = re.compile(
    r"^core\s+\d+:\s+0x[0-9a-fA-F]+\s+\(0x[0-9a-fA-F]+\)\s+([a-zA-Z0-9_\.]+)"
)
_PC_LINE_RE = re.compile(
    r"^0x[0-9a-fA-F]+\s+\(0x[0-9a-fA-F]+\)\s+([a-zA-Z0-9_\.]+)"
)


def _parse_spike_trace(trace_path: Path) -> "Counter[str]":
    counts: Counter[str] = Counter()
    try:
        with trace_path.open("r", encoding="utf-8", errors="ignore") as fh:
            for raw in fh:
                line = raw.strip()
                if not line:
                    continue
                m = _CORE_LINE_RE.match(line) or _PC_LINE_RE.match(line)
                if m:
                    counts[m.group(1).strip().lower()] += 1
    except OSError as exc:
        print(f"[instruction_mix] Failed to read {trace_path}: {exc}", file=sys.stderr)
        return Counter()
    return counts


def _parse_trace_csv(trace_path: Path) -> "Counter[str]":
    counts: Counter[str] = Counter()
    try:
        with trace_path.open("r", encoding="utf-8", errors="ignore") as fh:
            lines = [line for line in fh if not line.startswith("#")]
        reader = csv.DictReader(lines)
        if reader.fieldnames is None:
            return counts

        field_map = {field.lower().strip(): field for field in reader.fieldnames if field}
        key_field = field_map.get("instruction") or field_map.get("mnemonic") or field_map.get("function")
        count_field = field_map.get("count") or field_map.get("instructions")
        if not key_field or not count_field:
            return counts

        for row in reader:
            key = (row.get(key_field) or "").strip().lower()
            value = _safe_int(row.get(count_field))
            if key and value is not None:
                counts[key] += value
    except OSError as exc:
        print(f"[instruction_mix] Failed to read {trace_path}: {exc}", file=sys.stderr)
        return Counter()
    return counts


def _parse_instruction_input(trace_path: Path) -> "Counter[str]":
    if trace_path.suffix.lower() == ".csv":
        counts = _parse_trace_csv(trace_path)
        if counts:
            return counts
    return _parse_spike_trace(trace_path)


def _classify_instruction(mnemonic: str) -> str:
    op = mnemonic.strip().lower()
    if op.startswith("v"):
        return "vector"
    if op.startswith(("c.l", "c.s")):
        return "memory"
    if op in {"lb","lbu","lh","lhu","lw","lwu","ld","sb","sh","sw","sd","flw","fld","fsw","fsd"}:
        return "memory"
    if op in {"beq","bne","blt","bge","bltu","bgeu","jal","jalr","c.j","c.jal","c.jr","c.jalr"}:
        return "control"
    if op.startswith("csr") or op in {"ecall","ebreak","mret","sret","fence","fence.i"}:
        return "system"
    if op.startswith("c."):
        return "alu"
    return "alu"


def _build_distribution(counts: "Counter[str]") -> dict[str, int]:
    buckets = {"alu": 0, "memory": 0, "vector": 0, "control": 0, "system": 0, "other": 0}
    for mnemonic, count in counts.items():
        cat = _classify_instruction(mnemonic)
        if cat not in buckets:
            cat = "other"
        buckets[cat] += count
    return buckets


def _write_distribution_csv(
    output_path: Path,
    label: str,
    trace_path: Path,
    total: int,
    unique_instr: int,
    dist: dict[str, int],
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "label", "trace", "total_instructions", "unique_instructions",
        "alu", "memory", "vector", "control", "system", "other",
        "alu_pct", "memory_pct", "vector_pct", "control_pct", "system_pct", "other_pct",
    ]

    def pct(key: str) -> str:
        return f"{(dist[key] * 100.0 / total):.2f}" if total else "0.00"

    row = {
        "label": label, "trace": str(trace_path),
        "total_instructions": total, "unique_instructions": unique_instr,
        "alu": dist["alu"], "memory": dist["memory"], "vector": dist["vector"],
        "control": dist["control"], "system": dist["system"], "other": dist["other"],
        "alu_pct": pct("alu"), "memory_pct": pct("memory"), "vector_pct": pct("vector"),
        "control_pct": pct("control"), "system_pct": pct("system"), "other_pct": pct("other"),
    }

    write_mode  = "a"
    write_header = not output_path.exists()
    if output_path.exists():
        with output_path.open("r", encoding="utf-8", errors="ignore") as ef:
            first = ef.readline().strip()
        if first and first != ",".join(fieldnames):
            write_mode   = "w"
            write_header = True

    with output_path.open(write_mode, newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=fieldnames)
        if write_header:
            writer.writeheader()
        writer.writerow(row)


def cmd_instruction_mix(args: argparse.Namespace) -> int:
    trace_path = _discover_trace_input(
        args.trace,
        args.mul,
        args.level,
        args.trace_dir,
        args.hist_dir,
    )
    if trace_path is None:
        print(
            "[instruction_mix] Could not resolve input trace/histogram. Use --trace or provide --mul/--level.",
            file=sys.stderr,
        )
        return 1
    if not trace_path.is_file():
        print(f"[instruction_mix] Trace not found: {trace_path}", file=sys.stderr)
        return 1

    counts = _parse_instruction_input(trace_path)
    if not counts:
        print(f"[instruction_mix] No instructions parsed from {trace_path}", file=sys.stderr)
        return 1

    if args.label:
        label = args.label
    elif args.mul and args.level:
        label = f"{_normalize_variant_name(args.mul)}_L{args.level}"
    else:
        label = trace_path.stem
    total  = sum(counts.values())
    dist   = _build_distribution(counts)
    output = args.output
    _write_distribution_csv(output, label, trace_path, total, len(counts), dist)
    print(f"[instruction_mix] Wrote {output}")
    return 0


# ═════════════════════════════════════════════════════════════════════════════
# § 6  EXTRACT SUBCOMMAND  (ported from extract_results.py CLI)
# ═════════════════════════════════════════════════════════════════════════════

def cmd_extract(args: argparse.Namespace) -> int:
    bench_dir: Path = args.bench_dir
    hist_dir:  Path = args.hist_dir
    build_dir: Path | None = args.build_dir if args.build_dir and args.build_dir.is_dir() else None
    if args.build_dir and not args.build_dir.is_dir():
        print(f"[extract] NOTE: --build-dir not found: {args.build_dir} — PC resolution disabled",
              file=sys.stderr)

    if args.bench_set:
        bench_sets = args.bench_set
    elif bench_dir.is_dir():
        bench_sets = sorted(d.name for d in bench_dir.iterdir() if d.is_dir())
    else:
        bench_sets = []

    levels   = args.level   or ALL_LEVELS
    variants = args.variant or ALL_VARIANTS
    ops      = args.op      or ALL_OPS

    if not bench_sets:
        print("[extract] No benchmark-set directories found. Run Spike benchmarks first.",
              file=sys.stderr)
        return 0

    results, code_sizes = collect_results(bench_dir, bench_sets, levels, variants, ops,
                                          metric=args.metric)
    if not results:
        print("[extract] No benchmark data found. Check that Spike runs have completed.",
              file=sys.stderr)
        return 0

    active_bench_sets = sorted({key[0] for key in results.keys()})
    if active_bench_sets:
        bench_sets = active_bench_sets

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
    return 0


# ═════════════════════════════════════════════════════════════════════════════
# § 7  ARGUMENT PARSER & ENTRY POINT
# ═════════════════════════════════════════════════════════════════════════════

def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="generate_figures.py",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="subcommand", required=True)

    # ── extract ───────────────────────────────────────────────────────────────
    p_ext = sub.add_parser("extract", help="Print Markdown / LaTeX comparison tables")
    p_ext.add_argument("--bench-dir",  type=Path, default=DEFAULT_BENCH_DIR)
    p_ext.add_argument("--hist-dir",   type=Path, default=DEFAULT_HIST_DIR)
    p_ext.add_argument("--build-dir",  type=Path, default=REPO_ROOT / "build/spike")
    p_ext.add_argument("--bench-set",  nargs="+", metavar="NAME")
    p_ext.add_argument("--level",      nargs="+", type=int, choices=[128, 192, 256], metavar="{128,192,256}")
    p_ext.add_argument("--variant", "--mul", dest="variant", nargs="+", metavar="VARIANT")
    p_ext.add_argument("--op",         nargs="+", choices=ALL_OPS, metavar="OP")
    p_ext.add_argument("--baseline",   default="karatsuba_ref")
    p_ext.add_argument("--metric",     choices=["cycles", "instret"], default="cycles")
    p_ext.add_argument("--codesize",   action="store_true")
    p_ext.add_argument("--latex",      action="store_true")
    p_ext.add_argument("--mix",        action="store_true")
    p_ext.add_argument("--write-summary-csv", action="store_true")

    # ── complexity ────────────────────────────────────────────────────────────
    p_cpx = sub.add_parser("complexity", help="Plot O(n^α) theory vs empirical cycles")
    p_cpx.add_argument("csv", nargs="?", default=None,
                       help="Optional input CSV path. If omitted, auto-resolves from benchmark outputs.")
    p_cpx.add_argument("--bench-dir", type=Path, default=DEFAULT_BENCH_DIR,
                       help=f"Benchmark root used for auto-discovery (default: {DEFAULT_BENCH_DIR})")
    p_cpx.add_argument("--output", "-o", default=None,
                       help="Output path (PDF/PNG). Omit to show interactively.")

    # ── dashboard ─────────────────────────────────────────────────────────────
    p_dash = sub.add_parser("dashboard", help="Plot stable vs latest cycle-count dashboard")
    p_dash.add_argument("--bench-dir", default=None,
                        help=f"Root of benchmark directories (default: {DEFAULT_BENCH_DIR})")
    p_dash.add_argument("--output", "-o", default=None,
                        help="Output path (default: docs/figures/latest_dashboard.png)")

    # ── vlen_sweep ────────────────────────────────────────────────────────────
    p_vlen = sub.add_parser("vlen_sweep", help="Plot throughput vs VLEN sweep")
    p_vlen.add_argument("bench_dir", nargs="?", default=DEFAULT_VLEN_SWEEP_DIR,
                        help=f"Benchmark directory to scan (default: {DEFAULT_VLEN_SWEEP_DIR})")
    p_vlen.add_argument("--output", "-o", default=None,
                        help="Output path (PDF/PNG). Omit to show interactively.")

    # ── instruction_mix ───────────────────────────────────────────────────────
    p_mix = sub.add_parser("instruction_mix", help="Parse a Spike trace → instruction-mix CSV")
    p_mix.add_argument("trace", nargs="?", default=None,
                       help="Optional explicit trace path. If omitted, use --mul/--level auto-discovery.")
    p_mix.add_argument("--mul", default=None,
                       help="Multiplier variant used to auto-resolve trace/histogram input (e.g. karatsuba_rvv)")
    p_mix.add_argument("--level", type=int, choices=ALL_LEVELS, default=None,
                       help="Security level used for auto-resolution (128|192|256)")
    p_mix.add_argument("--trace-dir", type=Path, default=DEFAULT_TRACE_DIR,
                       help=f"Trace root for auto-discovery (default: {DEFAULT_TRACE_DIR})")
    p_mix.add_argument("--hist-dir", type=Path, default=DEFAULT_HIST_DIR,
                       help=f"Histogram fallback root (default: {DEFAULT_HIST_DIR})")
    p_mix.add_argument("--output", "-o",
                       type=Path,
                       default=DEFAULT_INSTRUCTION_MIX_CSV,
                       help=f"Output CSV path (default: {DEFAULT_INSTRUCTION_MIX_CSV})")
    p_mix.add_argument("--label", default="",
                       help="Row label (default: trace filename stem)")

    return parser


def main() -> int:
    parser = _build_parser()
    args   = parser.parse_args()

    dispatch = {
        "extract":         cmd_extract,
        "complexity":      cmd_complexity,
        "dashboard":       cmd_dashboard,
        "vlen_sweep":      cmd_vlen_sweep,
        "instruction_mix": cmd_instruction_mix,
    }
    return dispatch[args.subcommand](args)


if __name__ == "__main__":
    raise SystemExit(main())
