"""Number formatting utilities for benchmark reports.

Provides thousand-separator formatting for integers, float cycle counts,
and speedup ratio display.
"""

from __future__ import annotations

from typing import Optional, Union


def fmt_int(n: Optional[int]) -> str:
    """Format an integer with thousand separators.

    Args:
        n: The integer to format, or None for missing data.

    Returns:
        Formatted string with commas as thousand separators,
        or "—" (em-dash) if n is None.

    Examples:
        >>> fmt_int(1234567)
        '1,234,567'
        >>> fmt_int(42)
        '42'
        >>> fmt_int(None)
        '—'
    """
    if n is None:
        return "\u2014"
    return f"{n:,}"


def fmt_speedup(
    base: Optional[Union[int, float]],
    champ: Optional[Union[int, float]],
) -> str:
    """Format speedup ratio as ×N.N.

    Computes `base / champ` and returns the ratio formatted with a
    multiplication sign prefix and one decimal place.

    Args:
        base: Baseline metric value (numerator).
        champ: Champion/optimized metric value (denominator).

    Returns:
        Formatted speedup string like "×2.5", or "—" if either value
        is None or champ is zero.

    Examples:
        >>> fmt_speedup(1000, 500)
        '×2.0'
        >>> fmt_speedup(7992, 100)
        '×79.9'
        >>> fmt_speedup(None, 500)
        '—'
        >>> fmt_speedup(1000, 0)
        '—'
    """
    if base is None or champ is None or champ == 0:
        return "\u2014"
    ratio = base / champ
    return f"\u00d7{ratio:.1f}"


def fmt_cycles(n: Optional[float]) -> str:
    """Format a float cycle count with thousand separators.

    Rounds the value to the nearest integer and applies thousand-separator
    formatting.

    Args:
        n: The cycle count as a float, or None for missing data.

    Returns:
        Formatted string with commas as thousand separators,
        or "—" (em-dash) if n is None.

    Examples:
        >>> fmt_cycles(1234567.8)
        '1,234,568'
        >>> fmt_cycles(42.0)
        '42'
        >>> fmt_cycles(None)
        '—'
    """
    if n is None:
        return "\u2014"
    return f"{int(round(n)):,}"
