"""Static Analyzer for RISC-V ELF binaries.

Disassembles ELF binaries via objdump and classifies instructions by their
cycle cost, producing per-function multi-cycle instruction reports.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

# Ensure project root is on sys.path for direct script invocation
_PROJECT_ROOT = str(Path(__file__).resolve().parent.parent.parent.parent)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from scripts.report.cpi.cost_table import CostTable


@dataclass
class ClassCount:
    """Count and cost for a single instruction class."""

    count: int
    cost: int


@dataclass
class FunctionAnalysis:
    """Per-function analysis of multi-cycle instructions."""

    name: str                           # function name or "unknown"
    multi_cycle_counts: dict[str, int] = field(default_factory=dict)  # class_name -> count (cost > 1 only)
    estimated_contribution: int = 0     # sum(count * cost) for multi-cycle


@dataclass
class StaticAnalysis:
    """Complete static analysis result for an ELF binary."""

    per_class_totals: dict[str, ClassCount] = field(default_factory=dict)  # all classes with cost > 1
    per_function: list[FunctionAnalysis] = field(default_factory=list)
    total_multi_cycle_contribution: int = 0


def _detect_objdump() -> str:
    """Auto-detect objdump binary from toolchain.env or PATH.

    Searches for toolchain.env in the project hierarchy, parses the OBJDUMP
    export line, and falls back to searching PATH for riscv64-unknown-elf-objdump.
    """
    # Try to find toolchain.env by walking up from this script's location
    search_dirs = [
        Path(__file__).resolve().parent.parent.parent.parent,  # repo root
        Path.cwd(),
    ]

    for base in search_dirs:
        toolchain_env = base / "toolchain" / "toolchain.env"
        if toolchain_env.exists():
            # Parse the file for OBJDUMP export
            content = toolchain_env.read_text(encoding="utf-8")
            # Look for: export OBJDUMP="..."
            match = re.search(
                r'export\s+OBJDUMP="([^"]+)"', content
            )
            if match:
                objdump_path = match.group(1)

                # Resolve $RISCV — check environment first, then parse
                # the RISCV assignment from toolchain.env
                riscv_val = os.environ.get("RISCV", "")
                if not riscv_val:
                    # Parse: export RISCV="${RISCV:-$HOME/riscv-tools}"
                    # or:    RISCV="..."
                    riscv_match = re.search(
                        r'(?:export\s+)?RISCV="\$\{RISCV:-([^}]*)\}"', content
                    )
                    if riscv_match:
                        riscv_val = riscv_match.group(1)
                        riscv_val = riscv_val.replace(
                            "$HOME", str(Path.home())
                        )
                    else:
                        # Try simple assignment: export RISCV="value"
                        riscv_match = re.search(
                            r'(?:export\s+)?RISCV="([^$][^"]*)"', content
                        )
                        if riscv_match:
                            riscv_val = riscv_match.group(1)

                if riscv_val:
                    objdump_path = objdump_path.replace("$RISCV", riscv_val)

                # Resolve any remaining $HOME
                objdump_path = objdump_path.replace("$HOME", str(Path.home()))

                return objdump_path

    # Fallback: try riscv64-unknown-elf-objdump in PATH
    return "riscv64-unknown-elf-objdump"


def _validate_elf(elf_path: Path) -> None:
    """Validate that the file exists and looks like an ELF binary.

    Raises:
        SystemExit: if file doesn't exist or isn't a valid ELF
    """
    if not elf_path.exists():
        print(
            f"Error: ELF file not found: {elf_path}",
            file=sys.stderr,
        )
        sys.exit(1)

    # Check ELF magic bytes
    try:
        with open(elf_path, "rb") as f:
            magic = f.read(4)
        if magic != b"\x7fELF":
            print(
                f"Error: not a valid ELF binary: {elf_path}",
                file=sys.stderr,
            )
            sys.exit(1)
    except (OSError, IOError) as e:
        print(
            f"Error: cannot read file '{elf_path}': {e}",
            file=sys.stderr,
        )
        sys.exit(1)


def _run_objdump(elf_path: Path, objdump_bin: str) -> str:
    """Run objdump and return its stdout output.

    Raises:
        SystemExit: if objdump is not found or fails
    """
    cmd = [objdump_bin, "-d", "-t", str(elf_path)]
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,
        )
    except FileNotFoundError:
        print(
            f"Error: objdump binary not found: '{objdump_bin}'. "
            f"Ensure toolchain.env is configured or provide --objdump path.",
            file=sys.stderr,
        )
        sys.exit(1)
    except subprocess.TimeoutExpired:
        print(
            f"Error: objdump timed out on '{elf_path}'.",
            file=sys.stderr,
        )
        sys.exit(1)

    if result.returncode != 0:
        print(
            f"Error: objdump failed (exit {result.returncode}): {result.stderr.strip()}",
            file=sys.stderr,
        )
        sys.exit(1)

    return result.stdout


def _parse_objdump_output(
    output: str,
    cost_table: CostTable,
) -> tuple[dict[str, dict[str, int]], bool]:
    """Parse objdump disassembly output to extract per-function instruction counts.

    Returns:
        A tuple of:
        - dict mapping function_name -> {class_name -> count} (multi-cycle only)
        - bool indicating whether symbol table was available
    """
    # Pattern for function labels in objdump output:
    # e.g., "0000000080000000 <_start>:"
    func_pattern = re.compile(r"^[0-9a-f]+\s+<([^>]+)>:\s*$")

    # Pattern for instruction lines in objdump output:
    # e.g., "   80000004:  00000097  auipc  ra,0x0"
    # or    "   80000004:\t00000097\tauipc\tra,0x0"
    instr_pattern = re.compile(
        r"^\s*[0-9a-f]+:\s+[0-9a-f]+\s+(\w[\w.]*)"
    )

    has_symbols = False
    functions: dict[str, dict[str, int]] = {}
    current_func = "unknown"

    for line in output.splitlines():
        # Check for function header
        func_match = func_pattern.match(line)
        if func_match:
            current_func = func_match.group(1)
            has_symbols = True
            if current_func not in functions:
                functions[current_func] = {}
            continue

        # Check for instruction line
        instr_match = instr_pattern.match(line)
        if instr_match:
            mnemonic = instr_match.group(1)
            class_name, cost = cost_table.classify(mnemonic)

            # Only track multi-cycle instructions (cost > 1)
            if cost > 1:
                if current_func not in functions:
                    functions[current_func] = {}
                func_classes = functions[current_func]
                func_classes[class_name] = func_classes.get(class_name, 0) + 1

    return functions, has_symbols


def analyze_elf(
    elf_path: Path,
    objdump_bin: str,
    cost_table: CostTable,
) -> StaticAnalysis:
    """Disassemble ELF and classify instructions by function.

    Args:
        elf_path: Path to the ELF binary to analyze
        objdump_bin: Path or name of the objdump binary
        cost_table: CostTable instance for instruction classification

    Returns:
        StaticAnalysis with per-class totals and per-function breakdowns
    """
    _validate_elf(elf_path)
    output = _run_objdump(elf_path, objdump_bin)

    func_data, has_symbols = _parse_objdump_output(output, cost_table)

    if not has_symbols:
        print(
            "Warning: ELF file has no symbol table; all functions labeled as 'unknown'.",
            file=sys.stderr,
        )

    # Build per-class totals and per-function analysis
    per_class_totals: dict[str, ClassCount] = {}
    per_function: list[FunctionAnalysis] = []
    total_multi_cycle_contribution = 0

    # Build a cost lookup from the cost table classes
    cost_lookup: dict[str, int] = {}
    for ic in cost_table.classes:
        cost_lookup[ic.name] = ic.cost
    # UNKNOWN class gets default_cost (but won't appear if default_cost == 1)

    for func_name, class_counts in func_data.items():
        func_analysis = FunctionAnalysis(name=func_name)

        for class_name, count in class_counts.items():
            # Determine cost for this class
            cost = cost_lookup.get(class_name, cost_table.default_cost)

            # Aggregate per-class totals
            if class_name in per_class_totals:
                per_class_totals[class_name].count += count
            else:
                per_class_totals[class_name] = ClassCount(count=count, cost=cost)

            # Per-function tracking
            func_analysis.multi_cycle_counts[class_name] = count
            func_analysis.estimated_contribution += count * cost

        total_multi_cycle_contribution += func_analysis.estimated_contribution

        # Only include functions that actually have multi-cycle instructions
        if func_analysis.multi_cycle_counts:
            per_function.append(func_analysis)

    # Sort per-function by estimated_contribution descending
    per_function.sort(key=lambda f: f.estimated_contribution, reverse=True)

    # Emit unmatched mnemonic warnings
    cost_table.emit_unmatched_warnings()

    return StaticAnalysis(
        per_class_totals=per_class_totals,
        per_function=per_function,
        total_multi_cycle_contribution=total_multi_cycle_contribution,
    )


def _format_output(analysis: StaticAnalysis) -> str:
    """Format analysis results for terminal output."""
    lines: list[str] = []

    lines.append("=" * 70)
    lines.append("Static Analysis: Multi-Cycle Instruction Report")
    lines.append("=" * 70)
    lines.append("")

    # Per-class totals
    lines.append("Per-Class Totals (cost > 1 only):")
    lines.append(f"  {'Class':<20} {'Count':>10} {'Cost':>6} {'Contribution':>14}")
    lines.append(f"  {'-'*20} {'-'*10} {'-'*6} {'-'*14}")

    for class_name, cc in sorted(
        analysis.per_class_totals.items(),
        key=lambda x: x[1].count * x[1].cost,
        reverse=True,
    ):
        contribution = cc.count * cc.cost
        lines.append(
            f"  {class_name:<20} {cc.count:>10} {cc.cost:>6} {contribution:>14}"
        )

    lines.append("")
    lines.append(
        f"  Total multi-cycle contribution: {analysis.total_multi_cycle_contribution} cycles"
    )
    lines.append("")

    # Per-function breakdown
    lines.append("Per-Function Breakdown (top contributors):")
    lines.append(
        f"  {'Function':<40} {'Contribution':>14} {'Classes':>20}"
    )
    lines.append(f"  {'-'*40} {'-'*14} {'-'*20}")

    for func in analysis.per_function[:20]:  # Show top 20
        classes_str = ", ".join(
            f"{k}:{v}" for k, v in sorted(
                func.multi_cycle_counts.items(),
                key=lambda x: x[1],
                reverse=True,
            )
        )
        lines.append(
            f"  {func.name:<40} {func.estimated_contribution:>14} {classes_str}"
        )

    if len(analysis.per_function) > 20:
        lines.append(f"  ... and {len(analysis.per_function) - 20} more functions")

    lines.append("")
    return "\n".join(lines)


def main() -> None:
    """CLI entry point for the static analyzer."""
    parser = argparse.ArgumentParser(
        description="Static analysis of RISC-V ELF binaries for multi-cycle instructions",
    )
    parser.add_argument(
        "elf_file",
        type=Path,
        help="Path to the ELF binary to analyze",
    )
    parser.add_argument(
        "--cost-table",
        type=Path,
        default=None,
        help="Path to JSON cost table (default: built-in Rocket-like)",
    )
    parser.add_argument(
        "--objdump",
        type=str,
        default=None,
        help="Path to objdump binary (default: auto-detect from toolchain.env)",
    )

    args = parser.parse_args()

    # Load cost table
    if args.cost_table:
        try:
            cost_table = CostTable.from_json(args.cost_table)
        except (FileNotFoundError, ValueError) as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        cost_table = CostTable.default()

    # Determine objdump binary
    objdump_bin = args.objdump if args.objdump else _detect_objdump()

    # Run analysis
    analysis = analyze_elf(args.elf_file, objdump_bin, cost_table)

    # Output results
    print(_format_output(analysis))


if __name__ == "__main__":
    main()
