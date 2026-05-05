# Polynomial Variant Index

This project uses five canonical variant labels in scripts, reports, and target names:

- karatsuba_zbc
- karatsuba_zvbc
- fafft_zbc
- fafft_zvbc
- hybrid

Notes:
- `hybrid` is the public label used in build targets and benchmark CSV metadata.
- Scalar implementations are grouped under `src/poly_mul/scalar/`.
- Vector implementations are grouped under `src/poly_mul/vector/`.
- Legacy labels such as `hybrid_zbc`, `hybrid_zbc_v2`, and `hybrid_2way_zbc` are treated as aliases in some scripts.

## Versioning Convention

Stable promoted implementations live directly in each variant directory.
Historical versioned experiments are moved to top-level `experiments/` and tracked in `experiments/manifest.yaml`.

## Current status

- Scalar:
	- `fafft_zbc` -> `src/poly_mul/scalar/fafft_zbc/`
	- `hybrid` -> `src/poly_mul/scalar/hybrid_zbc/`
	- `karatsuba_zbc` -> `src/poly_mul/scalar/karatsuba_zbc/`
- Vector:
	- `fafft_zvbc` -> `src/poly_mul/vector/fafft_zvbc/`
	- `karatsuba_zvbc` -> `src/poly_mul/vector/karatsuba_zvbc/`
