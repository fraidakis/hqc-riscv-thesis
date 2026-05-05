# Polynomial Variant Index

This project uses five canonical variant labels in scripts, reports, and target names:

- karatsuba_zbc
- karatsuba_zvbc
- fafft_zbc
- fafft_zvbc
- hybrid

Notes:
- `hybrid` is the public label used in build targets and benchmark CSV metadata.
- Source files for `hybrid` currently live under `src/poly_mul/hybrid_zbc/` for continuity.
- Legacy labels such as `hybrid_zbc`, `hybrid_zbc_v2`, and `hybrid_2way_zbc` are treated as aliases in some scripts.

## Versioning Convention

Use versioned subfolders inside each variant directory:

- `v0-baseline/` -> first working baseline for that variant
- `v1/`, `v2/`, ... -> successive implementations
- highest numbered version is the current best candidate

Recommended policy:
- Keep one stable entry file used by CMake (for example `hybrid.c`).
- That entry file dispatches to the selected version implementation.
- Store experiment notes/scoreboards in `analysis/report/` and run IDs in `analysis/benchmarks/runs/`.

## Current status

- `fafft_zbc` already has versioned folders (`v3`, `v4`, `v5`).
- `hybrid` currently has versioned folder `v3`.
- `karatsuba_zbc`, `karatsuba_zvbc`, and `fafft_zvbc` currently have single active source files and can adopt `v0-baseline/` as needed.
