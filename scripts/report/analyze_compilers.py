#!/usr/bin/env python3
"""Generate a compiler showdown Markdown report from compare_compilers CSV files.

The script scans analysis/benchmarks/compiler_compare for CSV outputs and, for
each (variant, level), picks the best Clang and best GCC run by minimum
instruction count among rows where status == OK. It then computes the winner and
the percentage margin:

    margin = ((loser_instret / winner_instret) - 1) * 100
"""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
COMPARE_ROOT = REPO_ROOT / "analysis" / "benchmarks" / "compiler_compare"
OUT_PATH = REPO_ROOT / "analysis" / "reports" / "compiler_analysis.md"
TARGET_COMPILERS = {"clang", "gcc"}


@dataclass(frozen=True)
class BestRun:
    compiler: str
    opt: str
    level: int
    variant: str
    instructions: int


def read_rows(csv_path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(line for line in handle if not line.startswith("#"))
        for row in reader:
            rows.append(row)
    return rows


def scan_best_runs() -> tuple[dict[tuple[str, int, str], BestRun], int]:
    best: dict[tuple[str, int, str], BestRun] = {}
    csv_files = sorted(COMPARE_ROOT.rglob("*.csv"))

    for csv_path in csv_files:
        for row in read_rows(csv_path):
            try:
                compiler = row["compiler"].strip().lower()
                opt = row["opt"].strip()
                level = int(row["level"])
                variant = row["variant"].strip()
                status = row["status"].strip().upper()
                instructions = int(row["instructions"])
            except (KeyError, TypeError, ValueError):
                continue

            if compiler not in TARGET_COMPILERS:
                continue
            if status != "OK":
                continue

            key = (variant, level, compiler)
            candidate = BestRun(
                compiler=compiler,
                opt=opt,
                level=level,
                variant=variant,
                instructions=instructions,
            )

            current = best.get(key)
            if current is None:
                best[key] = candidate
                continue

            if candidate.instructions < current.instructions:
                best[key] = candidate
                continue

            if candidate.instructions == current.instructions and candidate.opt < current.opt:
                best[key] = candidate

    return best, len(csv_files)


def fmt_int(value: int | None) -> str:
    if value is None:
        return "N/A"
    return f"{value:,}"


def fmt_best(run: BestRun | None) -> str:
    if run is None:
        return "N/A"
    return f"{run.opt} ({fmt_int(run.instructions)})"


def pick_winner(clang: BestRun | None, gcc: BestRun | None) -> tuple[str, str]:
    if clang is None and gcc is None:
        return "N/A", "N/A"
    if clang is None:
        return "GCC", "N/A"
    if gcc is None:
        return "Clang", "N/A"

    if clang.instructions < gcc.instructions:
        margin = ((gcc.instructions / clang.instructions) - 1.0) * 100.0
        return "Clang", f"{margin:.1f}%"

    if gcc.instructions < clang.instructions:
        margin = ((clang.instructions / gcc.instructions) - 1.0) * 100.0
        return "GCC", f"{margin:.1f}%"

    return "Tie", "0.0%"


def build_markdown(best: dict[tuple[str, int, str], BestRun], scanned_files: int) -> str:
    keys = sorted({(variant, level) for (variant, level, _compiler) in best.keys()})

    lines: list[str] = []
    lines.append("# Compiler Showdown Analysis")
    lines.append("")
    lines.append(
        "Derived from compare_compilers CSVs under analysis/benchmarks/compiler_compare."
    )
    lines.append(f"Scanned CSV files: {scanned_files}")
    lines.append("")
    lines.append("| Variant | Level | Best Clang | Best GCC | Winner | Margin |")
    lines.append("|---|---:|---|---|---|---:|")

    for variant, level in keys:
        clang = best.get((variant, level, "clang"))
        gcc = best.get((variant, level, "gcc"))
        winner, margin = pick_winner(clang, gcc)
        lines.append(
            f"| {variant} | {level} | {fmt_best(clang)} | {fmt_best(gcc)} | {winner} | {margin} |"
        )

    if not keys:
        lines.append("| N/A | N/A | N/A | N/A | N/A | N/A |")

    lines.append("")
    return "\n".join(lines)


def main() -> None:
    best, scanned_files = scan_best_runs()
    markdown = build_markdown(best, scanned_files)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)

    previous = OUT_PATH.read_text(encoding="utf-8") if OUT_PATH.exists() else None
    OUT_PATH.write_text(markdown, encoding="utf-8")

    if previous == markdown:
        print(f"Unchanged: {OUT_PATH}")
    else:
        print(f"Updated: {OUT_PATH}")


if __name__ == "__main__":
    main()
