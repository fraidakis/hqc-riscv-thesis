#!/usr/bin/env python3
"""Generate a benchmark dashboard from normalized benchmark directories.

The script scans analysis/benchmarks/{scalar,vector}/ for directories following
the naming convention: [variant]_[bench_type]_[YYYYMMDD_HHMM].

It then compares polymul median cycle counts (vect_mul) between:
- Stable baseline: oldest normalized directory with CSV measurements
- Latest experiment: newest normalized directory with CSV measurements

Output:
    docs/figures/latest_dashboard.png
"""

from __future__ import annotations

import csv
import re
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

try:
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FuncFormatter
except ImportError:
    sys.exit("matplotlib is not installed. Install it with: pip install matplotlib")


ROOT = Path(__file__).resolve().parents[2]
BENCH_ROOT = ROOT / "analysis" / "benchmarks"
FIGURE_PATH = ROOT / "docs" / "figures" / "latest_dashboard.png"

DIR_PATTERN = re.compile(r"^(.+)_(\d{8}_\d{4})$")
POLYMUL_CSV_PATTERN = re.compile(r"^hqc(128|192|256)_.+_polymul_N\d+\.csv$")


@dataclass
class RunSummary:
    path: Path
    label: str
    timestamp: datetime
    cycles_by_level: dict[str, int]


def parse_timestamped_dir(path: Path) -> tuple[str, datetime] | None:
    match = DIR_PATTERN.match(path.name)
    if not match:
        return None
    label = match.group(1)
    ts = datetime.strptime(match.group(2), "%Y%m%d_%H%M")
    return label, ts


def extract_vect_mul_median_cycles(csv_path: Path) -> int | None:
    with csv_path.open(newline="", encoding="utf-8") as handle:
        filtered = (line for line in handle if not line.startswith("#"))
        reader = csv.DictReader(filtered)
        for row in reader:
            if row.get("op") == "vect_mul":
                value = row.get("median_cycles")
                if value is None:
                    return None
                return int(value)
    return None


def summarize_run(run_dir: Path) -> RunSummary | None:
    parsed = parse_timestamped_dir(run_dir)
    if parsed is None:
        return None
    label, timestamp = parsed

    cycles_by_level: dict[str, int] = {}
    for csv_file in sorted(run_dir.glob("*.csv")):
        name_match = POLYMUL_CSV_PATTERN.match(csv_file.name)
        if not name_match:
            continue
        level = name_match.group(1)
        median_cycles = extract_vect_mul_median_cycles(csv_file)
        if median_cycles is not None:
            cycles_by_level[level] = median_cycles

    if not cycles_by_level:
        return None
    return RunSummary(path=run_dir, label=label, timestamp=timestamp, cycles_by_level=cycles_by_level)


def collect_runs() -> list[RunSummary]:
    run_summaries: list[RunSummary] = []
    for track in ("scalar", "vector"):
        track_dir = BENCH_ROOT / track
        if not track_dir.exists():
            continue
        for child in sorted(track_dir.iterdir()):
            if not child.is_dir():
                continue
            summary = summarize_run(child)
            if summary is not None:
                run_summaries.append(summary)
    run_summaries.sort(key=lambda item: item.timestamp)
    return run_summaries


def make_plot(stable: RunSummary, latest: RunSummary) -> None:
    levels = ["128", "192", "256"]
    x = list(range(len(levels)))
    width = 0.36

    stable_vals = [stable.cycles_by_level.get(level, 0) for level in levels]
    latest_vals = [latest.cycles_by_level.get(level, 0) for level in levels]

    fig, ax = plt.subplots(figsize=(10, 5.5))
    ax.bar([i - width / 2 for i in x], stable_vals, width, label=f"Stable: {stable.path.name}", color="#5B6C8F")
    ax.bar([i + width / 2 for i in x], latest_vals, width, label=f"Latest: {latest.path.name}", color="#D46A4F")

    ax.set_title("Cycle Counts Dashboard: Stable vs Latest (vect_mul median)")
    ax.set_xlabel("Security Level")
    ax.set_ylabel("Median Cycles")
    ax.set_xticks(x)
    ax.set_xticklabels([f"HQC-{level}" for level in levels])
    ax.yaxis.set_major_formatter(FuncFormatter(lambda value, _pos: f"{int(value):,}"))
    ax.grid(axis="y", alpha=0.3)
    ax.legend(fontsize=8)

    subtitle = (
        f"Stable timestamp: {stable.timestamp.strftime('%Y-%m-%d %H:%M')}   |   "
        f"Latest timestamp: {latest.timestamp.strftime('%Y-%m-%d %H:%M')}"
    )
    fig.text(0.5, 0.01, subtitle, ha="center", fontsize=9)
    fig.tight_layout(rect=[0, 0.04, 1, 1])

    FIGURE_PATH.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIGURE_PATH, dpi=160)
    print(f"[dashboard] saved: {FIGURE_PATH}")


def main() -> int:
    runs = collect_runs()
    if len(runs) < 2:
        print("[dashboard] need at least two normalized benchmark directories with polymul CSV data")
        return 1

    stable = runs[0]
    latest = runs[-1]
    make_plot(stable, latest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())