# EXP-fafft-rvv-blueprint

Archived RVV-only FAFFT helper blueprint moved from the stable source tree.

## Scope
- Original location: `src/poly_mul/vector/rvv_only/`
- Archived location: `experiments/EXP-fafft-rvv-blueprint/`
- Reason: ZVBC-focused cleanup; keep RVV-only code as historical/reference material, not as part of stable default build inputs.

## Notes
- This archive is not linked into default CMake targets.
- Stable `fafft_zbc` now benchmarks and builds without this RVV-only helper path.
