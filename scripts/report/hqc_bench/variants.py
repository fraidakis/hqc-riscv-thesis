"""Variant naming utilities for HQC-RISC-V benchmark infrastructure.

Provides canonical variant-to-slug mapping, track detection, and legacy
variant name resolution for backward compatibility during migration.
"""

from __future__ import annotations

# Canonical variant names mapped to their hyphenated slugs (used in CSV filenames).
# The slug is the canonical name with underscores replaced by hyphens.
VARIANT_SLUG_MAP: dict[str, str] = {
    "scalar_ref": "scalar-ref",
    "scalar_zb": "scalar-zb",
    "vector_ref": "vector-ref",
    "vector_rvv": "vector-rvv",
    "vector_zvk": "vector-zvk",
}

# Legacy variant names mapped to their canonical equivalents.
# Used for backward compatibility with older scripts and CSV filenames.
LEGACY_VARIANT_MAP: dict[str, str] = {
    "ref_scalar": "scalar_ref",
    "karatsuba_zbc": "scalar_zb",
    "ref_vector": "vector_ref",
    "karatsuba_rvv": "vector_rvv",
    "karatsuba_zvbc": "vector_zvk",
    # Hyphenated legacy aliases occasionally appear in filenames or CLI args.
    "ref-scalar": "scalar_ref",
    "karatsuba-zbc": "scalar_zb",
    "ref-vector": "vector_ref",
    "karatsuba-rvv": "vector_rvv",
    "karatsuba-zvbc": "vector_zvk",
    # Slug aliases map back to canonical variant names.
    "scalar-ref": "scalar_ref",
    "scalar-zb": "scalar_zb",
    "vector-ref": "vector_ref",
    "vector-rvv": "vector_rvv",
    "vector-zvk": "vector_zvk",
}


def variant_slug(variant: str) -> str:
    """Return the hyphenated slug for a canonical variant name.

    If the variant is in VARIANT_SLUG_MAP, returns the mapped slug.
    Otherwise, falls back to replacing underscores with hyphens.

    Args:
        variant: A canonical variant name (e.g., "scalar_zb").

    Returns:
        The corresponding slug (e.g., "scalar-zb").
    """
    canonical = resolve_variant(variant)
    return VARIANT_SLUG_MAP.get(canonical, canonical.replace("_", "-"))


def variant_track(variant: str) -> str:
    """Return the track ("scalar" or "vector") for a given variant.

    Detection is based on the variant's prefix: names starting with
    "vector" are on the vector track; everything else is scalar.

    Args:
        variant: A canonical variant name (e.g., "vector_rvv").

    Returns:
        "vector" if the variant starts with "vector", otherwise "scalar".
    """
    normalized = resolve_variant(variant).lower()
    if normalized.startswith("vector"):
        return "vector"
    return "scalar"


def resolve_variant(name: str) -> str:
    """Resolve a possibly-legacy variant name to its canonical form.

    If the name is already canonical (present in VARIANT_SLUG_MAP), it is
    returned unchanged. If the name is a legacy alias (present in
    LEGACY_VARIANT_MAP), the corresponding canonical name is returned.
    Otherwise the name is returned as-is.

    Args:
        name: A variant name, possibly using legacy naming conventions.

    Returns:
        The canonical variant name.
    """
    if name in VARIANT_SLUG_MAP:
        return name
    if name in LEGACY_VARIANT_MAP:
        return LEGACY_VARIANT_MAP[name]
    return name
