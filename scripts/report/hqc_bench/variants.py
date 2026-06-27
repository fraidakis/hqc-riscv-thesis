"""Variant naming utilities for HQC-RISC-V benchmark infrastructure.

Provides canonical variant-to-slug mapping and track detection.
"""

from __future__ import annotations

# Canonical variant names mapped to their hyphenated slugs (used in CSV filenames).
# The slug is the canonical name with underscores replaced by hyphens.
VARIANT_SLUG_MAP: dict[str, str] = {
    "scalar_ref": "scalar-ref",
    "scalar_zb": "scalar-zb",
    "vector_rvv": "vector-rvv",
    "vector_zvk": "vector-zvk",
}

# Maps legacy/CSV variant names to canonical names used in reports and tables.
LEGACY_VARIANT_MAP: dict[str, str] = {
    "ref_scalar": "scalar_ref",
    "karatsuba_zbc": "scalar_zb",
    "karatsuba_rvv": "vector_rvv",
    "karatsuba_zvbc": "vector_zvk",
}


def resolve_variant(raw: str) -> str:
    """Resolve a raw variant name (possibly legacy) to its canonical form.

    Args:
        raw: A variant name as found in CSV files (e.g., "karatsuba_zbc").

    Returns:
        The canonical variant name (e.g., "scalar_zb").
    """
    return LEGACY_VARIANT_MAP.get(raw, raw)


def variant_slug(variant: str) -> str:
    """Return the hyphenated slug for a canonical variant name.

    If the variant is in VARIANT_SLUG_MAP, returns the mapped slug.
    Otherwise, falls back to replacing underscores with hyphens.

    Args:
        variant: A canonical variant name (e.g., "scalar_zb").

    Returns:
        The corresponding slug (e.g., "scalar-zb").
    """
    return VARIANT_SLUG_MAP.get(variant, variant.replace("_", "-"))


def variant_track(variant: str) -> str:
    """Return the track ("scalar" or "vector") for a given variant.

    Detection is based on the variant's prefix: names starting with
    "vector" are on the vector track; everything else is scalar.

    Args:
        variant: A canonical variant name (e.g., "vector_rvv").

    Returns:
        "vector" if the variant starts with "vector", otherwise "scalar".
    """
    if variant.lower().startswith("vector"):
        return "vector"
    return "scalar"
