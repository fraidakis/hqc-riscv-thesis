"""CPI (Cycles Per Instruction) analysis model for HQC RISC-V.

Provides instruction classification, cycle estimation, static analysis,
and comparative reporting for RISC-V instruction mixes.

Main classes and functions available via this package:
    - CostTable, InstructionClass (from cost_table)
    - CycleEstimate, ClassBreakdown, VectorSummary, compute_cycles, parse_instruction_csv (from cpi_model)
    - discover_histograms, generate_comparative_table, compute_deltas, run_report (from cpi_report)
    - FunctionAnalysis, StaticAnalysis, analyze_elf (from static_analyzer)
"""


def __getattr__(name: str):
    """Lazy imports to avoid circular import issues and RuntimeWarnings with -m invocation."""
    _cost_table_attrs = {"CostTable", "InstructionClass"}
    _cpi_model_attrs = {
        "ClassBreakdown",
        "CycleEstimate",
        "VectorSummary",
        "compute_cycles",
        "parse_instruction_csv",
    }
    _cpi_report_attrs = {
        "discover_histograms",
        "generate_comparative_table",
        "compute_deltas",
        "run_report",
    }
    _static_analyzer_attrs = {
        "FunctionAnalysis",
        "StaticAnalysis",
        "analyze_elf",
    }

    if name in _cost_table_attrs:
        from scripts.report.cpi.cost_table import CostTable, InstructionClass
        return locals()[name]
    elif name in _cpi_model_attrs:
        from scripts.report.cpi.cpi_model import (
            ClassBreakdown,
            CycleEstimate,
            VectorSummary,
            compute_cycles,
            parse_instruction_csv,
        )
        return locals()[name]
    elif name in _cpi_report_attrs:
        from scripts.report.cpi.cpi_report import (
            discover_histograms,
            generate_comparative_table,
            compute_deltas,
            run_report,
        )
        return locals()[name]
    elif name in _static_analyzer_attrs:
        from scripts.report.cpi.static_analyzer import (
            FunctionAnalysis,
            StaticAnalysis,
            analyze_elf,
        )
        return locals()[name]
    raise AttributeError(f"module 'scripts.report.cpi' has no attribute {name!r}")


__all__ = [
    # cost_table
    "CostTable",
    "InstructionClass",
    # cpi_model
    "ClassBreakdown",
    "CycleEstimate",
    "VectorSummary",
    "compute_cycles",
    "parse_instruction_csv",
    # cpi_report
    "discover_histograms",
    "generate_comparative_table",
    "compute_deltas",
    "run_report",
    # static_analyzer
    "FunctionAnalysis",
    "StaticAnalysis",
    "analyze_elf",
]
