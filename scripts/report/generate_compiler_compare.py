#!/usr/bin/env python3
"""Generate a compiler showdown Markdown report from hardware compare_compilers CSV files.

The script scans results/hardware/benchmarks/compiler_compare for CSV outputs
matching the pattern hw_compiler_compare_*.csv and, for each (variant, level),
picks the best Clang and best GCC run by minimum cycle count among rows
where status == OK. It then computes the winner and the percentage margin:

    margin = ((loser_cycles / winner_cycles) - 1) * 100
"""

from __future__ import annotations

import csv
import sys
from dataclasses import dataclass
from pathlib import Path

from hqc_bench import resolve_variant


REPO_ROOT = Path(__file__).resolve().parents[2]
COMPARE_ROOT = REPO_ROOT / "results" / "hardware" / "benchmarks" / "compiler_compare"
OUT_PATH = REPO_ROOT / "results" / "reports" / "summaries" / "compiler_analysis.md"
TARGET_COMPILERS = {"clang", "gcc"}
VARIANT_ORDER = [
    "scalar_ref",
    "scalar_zb",
    "vector_rvv",
    "vector_zvk",
]
VARIANT_RANK = {name: idx for idx, name in enumerate(VARIANT_ORDER)}


@dataclass(frozen=True)
class BestRun:
    compiler: str
    opt: str
    level: int
    variant: str
    cycles: int


def read_rows(csv_path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(line for line in handle if not line.startswith("#"))
        for row in reader:
            rows.append(row)
    return rows


def scan_best_runs() -> tuple[dict[tuple[str, int, str], BestRun], set[int], int]:
    best: dict[tuple[str, int, str], BestRun] = {}
    levels_seen: set[int] = set()
    expected_pattern = COMPARE_ROOT / "hw_compiler_compare_*.csv"
    csv_files = sorted(COMPARE_ROOT.glob("hw_compiler_compare_*.csv"))

    if not csv_files:
        print(
            f"Error: no compiler compare CSV files found at {expected_pattern}",
            file=sys.stderr,
        )
        sys.exit(1)

    for csv_path in csv_files:
        for row in read_rows(csv_path):
            try:
                compiler = row["compiler"].strip().lower()
                opt = row["opt"].strip()
                level = int(row["level"])
                variant = resolve_variant(row["variant"].strip())
                status = row["status"].strip().upper()
                cycles = int(row["cycles"])
            except (KeyError, TypeError, ValueError):
                continue

            if compiler not in TARGET_COMPILERS:
                continue
            if status != "OK":
                continue

            levels_seen.add(level)

            key = (variant, level, compiler)
            candidate = BestRun(
                compiler=compiler,
                opt=opt,
                level=level,
                variant=variant,
                cycles=cycles,
            )

            current = best.get(key)
            if current is None:
                best[key] = candidate
                continue

            if candidate.cycles < current.cycles:
                best[key] = candidate
                continue

            if candidate.cycles == current.cycles and candidate.opt < current.opt:
                best[key] = candidate

    return best, levels_seen, len(csv_files)


def fmt_int(value: int | None) -> str:
    if value is None:
        return "N/A"
    return f"{value:,}"


def fmt_best(run: BestRun | None) -> str:
    if run is None:
        return "N/A"
    return f"{run.opt} ({fmt_int(run.cycles)})"


def pick_winner(clang: BestRun | None, gcc: BestRun | None) -> tuple[str, str]:
    if clang is None and gcc is None:
        return "N/A", "N/A"
    if clang is None:
        return "GCC", "N/A"
    if gcc is None:
        return "Clang", "N/A"

    if clang.cycles < gcc.cycles:
        margin = ((gcc.cycles / clang.cycles) - 1.0) * 100.0
        return "Clang", f"{margin:.1f}%"

    if gcc.cycles < clang.cycles:
        margin = ((clang.cycles / gcc.cycles) - 1.0) * 100.0
        return "GCC", f"{margin:.1f}%"

    return "Tie", "0.0%"


def build_markdown(
    best: dict[tuple[str, int, str], BestRun], levels_seen: set[int], scanned_files: int
) -> str:
    levels = sorted(levels_seen) if levels_seen else [128, 192, 256]

    keys = [(variant, level) for variant in VARIANT_ORDER for level in levels]
    known_keys = {(variant, level) for (variant, level, _compiler) in best.keys()}
    extra_keys = sorted(
        known_keys - set(keys),
        key=lambda item: (VARIANT_RANK.get(item[0], len(VARIANT_RANK)), item[0], item[1]),
    )
    keys.extend(extra_keys)

    lines: list[str] = []
    lines.append("# Compiler Showdown Analysis")
    lines.append("")
    lines.append(
        "Derived from hardware compiler compare CSVs under results/hardware/benchmarks/compiler_compare."
    )
    lines.append(f"Scanned CSV files: {scanned_files}")
    lines.append("")
    lines.append("| Variant | Level | Best Clang | Best GCC | Winner | Margin |")
    lines.append("|---|---:|---|---|---|---:|")

    for variant, level in keys:
        clang = best.get((variant, level, "clang"))
        gcc = best.get((variant, level, "gcc"))
        if clang is None and gcc is None:
            continue
        winner, margin = pick_winner(clang, gcc)
        lines.append(
            f"| {variant} | {level} | {fmt_best(clang)} | {fmt_best(gcc)} | {winner} | {margin} |"
        )

    lines.append("")
    return "\n".join(lines)


def main() -> None:
    best, levels_seen, scanned_files = scan_best_runs()
    markdown = build_markdown(best, levels_seen, scanned_files)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)

    previous = OUT_PATH.read_text(encoding="utf-8") if OUT_PATH.exists() else None
    OUT_PATH.write_text(markdown, encoding="utf-8")

    if previous == markdown:
        print(f"Unchanged: {OUT_PATH}")
    else:
        print(f"Updated: {OUT_PATH}")


if __name__ == "__main__":
    main()
