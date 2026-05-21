# EXP-keccak-rvv-x2-legacy

Archived RVV x2 SoA Keccak backend moved out of stable src/keccak.

## Scope
- Original location: src/keccak/rvv/fips202_rvv_x2.c
- Archived location: experiments/EXP-keccak-rvv-x2-legacy/src/fips202_rvv_x2.c
- Stable replacement: src/keccak/fips202_rvv_only.c (Track B)

## Why Archived
Isolated Keccak benchmarking (2026-05-21, Spike) showed no throughput gain over rvv_only:

- L128: rvv_x2_soa 7100 vs rvv_only 7100
- L192: rvv_x2_soa 7100 vs rvv_only 7100
- L256: rvv_x2_soa 7100 vs rvv_only 7100

Given equivalent instructions and higher maintenance complexity, rvv_x2_soa was removed from stable selectable options.

## Notes
- This archive is not linked into default CMake targets.
- To re-evaluate, add an experiment-only CMake option and wire this file explicitly in an experiment build directory.
