#!/usr/bin/env python3
"""Parse a Spike trace log and generate instruction frequency CSV.

Processes a Spike trace log (from 'spike -l') and outputs a CSV file
containing instruction frequency statistics.

Usage:
    python generate_instruction_mix.py <spike_trace.log> [output.csv]
    Example: python generate_instruction_mix.py /path/to/spike_trace.log instruction_mix.csv
"""

import re
import sys
import csv
from collections import Counter
from pathlib import Path
from typing import Optional


def parse_spike_trace(trace_path: Path) -> dict[str, int]:
    """Parse spike trace log and count instruction frequencies.

    Args:
        trace_path: Path to spike trace log file (format: "core   0: <PC> (<hex>) <instr>")

    Returns:
        Dictionary mapping instruction mnemonic to frequency count
    """
    instruction_counts: Counter[str] = Counter()

    # Regex to match spike trace lines
    # Format: core   0: 0x0000000000001000 (0x00000297) auipc   t0, 0x0
    trace_pattern = re.compile(
        r"^core\s+\d+:\s+0x[0-9a-fA-F]+\s+\(0x[0-9a-fA-F]+\)\s+(\S+)"
    )

    try:
        with open(trace_path, "r", encoding="utf-8") as f:
            for line in f:
                match = trace_pattern.match(line.strip())
                if match:
                    instr = match.group(1)
                    instruction_counts[instr] += 1
    except Exception as e:
        print(f"Error reading trace file: {e}", file=sys.stderr)
        return {}

    return dict(instruction_counts)


def categorize_instruction(instr: str) -> str:
    """Categorize instruction into a type group.

    Args:
        instr: Instruction mnemonic (e.g., 'add', 'vxor.vv', 'ld')

    Returns:
        Category string
    """
    instr_lower = instr.lower()

    # Vector instructions
    if instr_lower.startswith("v"):
        if "xor" in instr_lower:
            return "vxor"
        elif "mul" in instr_lower or "mul" in instr_lower:
            return "vmul"
        elif "and" in instr_lower:
            return "vand"
        elif "or" in instr_lower:
            return "vor"
        elif "add" in instr_lower:
            return "vadd"
        else:
            return "vector_other"

    # Branch/jump instructions
    if instr_lower in ["beq", "bne", "blt", "bge", "bltu", "bgeu", "jal", "jalr"]:
        return "branch/jump"

    # Load/store instructions
    if instr_lower in ["ld", "lw", "lh", "lb", "sd", "sw", "sh", "sb"]:
        return "load_store"

    # Scalar ALU
    if instr_lower in [
        "add",
        "addi",
        "sub",
        "xor",
        "xori",
        "or",
        "ori",
        "and",
        "andi",
        "sll",
        "slli",
        "srl",
        "srli",
        "sra",
        "srai",
    ]:
        return "scalar_alu"

    # Compressed instructions
    if instr_lower.startswith("c."):
        return "compressed"

    # System/CSR
    if instr_lower in ["csrr", "csrs", "csrw", "csrc", "ecall", "ebreak"]:
        return "system"

    # Everything else
    return "other"


def generate_csv_report(instruction_counts: dict[str, int], output_path: Path) -> None:
    """Generate CSV report with instruction frequencies and categories.

    Args:
        instruction_counts: Dictionary mapping instruction to frequency
        output_path: Path where CSV should be written
    """
    categorized: dict[str, list[tuple[str, int]]] = {}

    for instr, count in sorted(instruction_counts.items(), key=lambda x: -x[1]):
        category = categorize_instruction(instr)
        if category not in categorized:
            categorized[category] = []
        categorized[category].append((instr, count))

    total_instructions = sum(instruction_counts.values())

    try:
        with open(output_path, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)

            # Header
            writer.writerow(
                ["category", "instruction", "count", "percentage", "frequency (per 1000 instr)"]
            )

            # Data rows
            for category in sorted(categorized.keys()):
                for instr, count in categorized[category]:
                    percentage = (count / total_instructions * 100) if total_instructions > 0 else 0
                    per_1k = (count / total_instructions * 1000) if total_instructions > 0 else 0
                    writer.writerow([category, instr, count, f"{percentage:.2f}", f"{per_1k:.1f}"])

            # Summary section
            writer.writerow([])
            writer.writerow(["Summary Statistics"])
            writer.writerow(["total_instructions", total_instructions])
            writer.writerow(["unique_instructions", len(instruction_counts)])

            # Category summary
            writer.writerow([])
            writer.writerow(["Category Summary"])
            category_totals = {}
            for category, instrs in categorized.items():
                category_totals[category] = sum(count for _, count in instrs)

            for category in sorted(category_totals.keys()):
                count = category_totals[category]
                percentage = (count / total_instructions * 100) if total_instructions > 0 else 0
                writer.writerow([category, count, f"{percentage:.2f}%"])

        print(f"[generate_instruction_mix] Report written to: {output_path}")
        print(f"[generate_instruction_mix] Total instructions: {total_instructions}")
        print(f"[generate_instruction_mix] Unique instruction types: {len(instruction_counts)}")

    except Exception as e:
        print(f"Error writing CSV report: {e}", file=sys.stderr)
        sys.exit(1)


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python generate_instruction_mix.py <spike_trace.log> [output.csv]", file=sys.stderr)
        sys.exit(1)

    trace_path = Path(sys.argv[1])
    if not trace_path.exists():
        print(f"Error: Trace file not found: {trace_path}", file=sys.stderr)
        sys.exit(1)

    # Default output: same directory as input, with _instruction_mix.csv suffix
    if len(sys.argv) > 2:
        output_path = Path(sys.argv[2])
    else:
        output_path = trace_path.parent / f"{trace_path.stem}_instruction_mix.csv"

    print(f"[generate_instruction_mix] Parsing trace: {trace_path}")
    instruction_counts = parse_spike_trace(trace_path)

    if not instruction_counts:
        print("Error: No instructions parsed from trace file", file=sys.stderr)
        sys.exit(1)

    print(f"[generate_instruction_mix] Found {len(instruction_counts)} unique instructions")
    generate_csv_report(instruction_counts, output_path)


if __name__ == "__main__":
    main()
