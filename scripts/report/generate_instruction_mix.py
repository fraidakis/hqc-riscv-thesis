#!/usr/bin/env python3
"""Generate ALU/Memory/Vector instruction mix from Spike traces.

Supports Spike traces captured with both:
  - spike -l  (instruction log)
  - spike -d  (debug/disassembly stream)

Writes one CSV row per trace to analysis/benchmarks/instruction_mix.csv by default.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import Counter
from pathlib import Path


CORE_LINE_RE = re.compile(
    r"^core\s+\d+:\s+0x[0-9a-fA-F]+\s+\(0x[0-9a-fA-F]+\)\s+([a-zA-Z0-9_\.]+)"
)
PC_LINE_RE = re.compile(
    r"^0x[0-9a-fA-F]+\s+\(0x[0-9a-fA-F]+\)\s+([a-zA-Z0-9_\.]+)"
)


def normalize_mnemonic(mnemonic: str) -> str:
    return mnemonic.strip().lower()


def parse_spike_trace(trace_path: Path) -> Counter[str]:
    """Extract instruction mnemonics from Spike -l/-d output."""
    counts: Counter[str] = Counter()

    try:
        with trace_path.open("r", encoding="utf-8", errors="ignore") as handle:
            for raw in handle:
                line = raw.strip()
                if not line:
                    continue

                match = CORE_LINE_RE.match(line)
                if not match:
                    match = PC_LINE_RE.match(line)

                if not match:
                    continue

                mnemonic = normalize_mnemonic(match.group(1))
                counts[mnemonic] += 1
    except OSError as exc:
        print(f"[generate_instruction_mix] Failed to read {trace_path}: {exc}", file=sys.stderr)
        return Counter()

    return counts


def classify_instruction(mnemonic: str) -> str:
    """Map RISC-V mnemonic to high-level mix bucket."""
    op = normalize_mnemonic(mnemonic)

    if op.startswith("v"):
        return "vector"

    if op.startswith(("c.l", "c.s")):
        return "memory"
    if op in {
        "lb", "lbu", "lh", "lhu", "lw", "lwu", "ld",
        "sb", "sh", "sw", "sd",
        "flw", "fld", "fsw", "fsd",
    }:
        return "memory"

    if op in {
        "beq", "bne", "blt", "bge", "bltu", "bgeu",
        "jal", "jalr", "c.j", "c.jal", "c.jr", "c.jalr",
    }:
        return "control"

    if op.startswith("csr") or op in {"ecall", "ebreak", "mret", "sret", "fence", "fence.i"}:
        return "system"

    if op.startswith("c."):
        return "alu"

    return "alu"


def build_distribution(counts: Counter[str]) -> dict[str, int]:
    buckets = {"alu": 0, "memory": 0, "vector": 0, "control": 0, "system": 0, "other": 0}
    for mnemonic, count in counts.items():
        category = classify_instruction(mnemonic)
        if category not in buckets:
            category = "other"
        buckets[category] += count
    return buckets


def write_distribution_csv(
    output_path: Path,
    label: str,
    trace_path: Path,
    total: int,
    unique_instr: int,
    dist: dict[str, int],
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "label",
        "trace",
        "total_instructions",
        "unique_instructions",
        "alu",
        "memory",
        "vector",
        "control",
        "system",
        "other",
        "alu_pct",
        "memory_pct",
        "vector_pct",
        "control_pct",
        "system_pct",
        "other_pct",
    ]

    row = {
        "label": label,
        "trace": str(trace_path),
        "total_instructions": total,
        "unique_instructions": unique_instr,
        "alu": dist["alu"],
        "memory": dist["memory"],
        "vector": dist["vector"],
        "control": dist["control"],
        "system": dist["system"],
        "other": dist["other"],
        "alu_pct": f"{(dist['alu'] * 100.0 / total):.2f}" if total else "0.00",
        "memory_pct": f"{(dist['memory'] * 100.0 / total):.2f}" if total else "0.00",
        "vector_pct": f"{(dist['vector'] * 100.0 / total):.2f}" if total else "0.00",
        "control_pct": f"{(dist['control'] * 100.0 / total):.2f}" if total else "0.00",
        "system_pct": f"{(dist['system'] * 100.0 / total):.2f}" if total else "0.00",
        "other_pct": f"{(dist['other'] * 100.0 / total):.2f}" if total else "0.00",
    }

    write_mode = "a"
    write_header = not output_path.exists()

    if output_path.exists():
        with output_path.open("r", encoding="utf-8", errors="ignore") as existing:
            first_line = existing.readline().strip()
        expected_header = ",".join(fieldnames)
        if first_line and first_line != expected_header:
            write_mode = "w"
            write_header = True

    with output_path.open(write_mode, newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        if write_header:
            writer.writeheader()
        writer.writerow(row)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="Spike trace generated with -l or -d")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("analysis/benchmarks/instruction_mix.csv"),
        help="Output CSV path (default: analysis/benchmarks/instruction_mix.csv)",
    )
    parser.add_argument(
        "--label",
        default="",
        help="Row label (default: trace filename stem)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    trace_path: Path = args.trace

    if not trace_path.is_file():
        print(f"[generate_instruction_mix] Trace not found: {trace_path}", file=sys.stderr)
        sys.exit(1)

    counts = parse_spike_trace(trace_path)
    if not counts:
        print(f"[generate_instruction_mix] No instructions parsed from {trace_path}", file=sys.stderr)
        sys.exit(1)

    label = args.label or trace_path.stem
    total = sum(counts.values())
    dist = build_distribution(counts)
    write_distribution_csv(
        output_path=args.output,
        label=label,
        trace_path=trace_path,
        total=total,
        unique_instr=len(counts),
        dist=dist,
    )

    print(f"[generate_instruction_mix] label={label}")
    print(f"[generate_instruction_mix] total={total} unique={len(counts)}")
    print(
        "[generate_instruction_mix] mix "
        f"alu={dist['alu']} memory={dist['memory']} vector={dist['vector']} "
        f"control={dist['control']} system={dist['system']} other={dist['other']}"
    )
    print(f"[generate_instruction_mix] wrote: {args.output}")


if __name__ == "__main__":
    main()
