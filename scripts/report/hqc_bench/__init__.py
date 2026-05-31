"""hqc_bench — shared utilities for HQC-RISC-V benchmark report generation.

This package provides common data structures and I/O functions used across
the benchmark report scripts, eliminating duplicated boilerplate for variant
mapping, CSV reading, and number formatting.

Submodules:
    variants   — canonical variant names, slug mapping, legacy resolution
    csv_io     — CSV reading helpers for simulation and hardware results
    formatting — thousand-separator and speedup-ratio formatting
"""

from hqc_bench.variants import (
    LEGACY_VARIANT_MAP,
    VARIANT_SLUG_MAP,
    resolve_variant,
    variant_slug,
    variant_track,
)
from hqc_bench.csv_io import (
    parse_metadata,
    read_csv,
    read_hw_csv,
)
from hqc_bench.formatting import (
    fmt_cycles,
    fmt_int,
    fmt_speedup,
)

__all__ = [
    # variants
    "VARIANT_SLUG_MAP",
    "variant_slug",
    "variant_track",
    "LEGACY_VARIANT_MAP",
    "resolve_variant",
    # csv_io
    "read_csv",
    "parse_metadata",
    "read_hw_csv",
    # formatting
    "fmt_int",
    "fmt_speedup",
    "fmt_cycles",
]
