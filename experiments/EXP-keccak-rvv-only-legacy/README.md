# EXP-keccak-rvv-only-legacy

Archived RVV-only Keccak backend moved out of stable src/keccak.

## Scope
- Original location: src/keccak/fips202_rvv_only.c
- Archived location: experiments/EXP-keccak-rvv-only-legacy/src/fips202_rvv_only.c
- Stable replacement for Track B: HQC_KECCAK_IMPL=ref

## Why Archived
Isolated Keccak benchmarking (2026-05-21, Spike) showed rvv_only is slower than both stable alternatives:

- L128: rvv_only 7100 vs ref 4680 vs zvbb_x2_soa 2114
- L192: rvv_only 7100 vs ref 4680 vs zvbb_x2_soa 2114
- L256: rvv_only 7100 vs ref 4680 vs zvbb_x2_soa 2114

Given consistently higher instruction count and no offsetting benefit in the default Track B pipeline, rvv_only was removed from stable selectable options.

## Notes
- This archive is not linked into default CMake targets.
- The archived rvv_x2 experiment depends on this file for standalone reproducibility.
