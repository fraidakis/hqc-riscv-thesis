# EXP-keccak-zvbb-gather-legacy

Archived single-state Zvbb gather Keccak variant moved out of stable selectable options.

## Scope
- Original location: src/keccak/fips202_zvbb_gather.c
- Archived location: experiments/EXP-keccak-zvbb-gather-legacy/src/fips202_zvbb_gather.c
- Stable replacement: src/keccak/fips202_zvbb_x2.c (Track C2)

## Why Archived
Isolated Keccak benchmarking (2026-05-21, Spike) showed zvbb_x2_soa is consistently faster:

- L128: zvbb_gather 2838 vs zvbb_x2_soa 2114
- L192: zvbb_gather 2838 vs zvbb_x2_soa 2114
- L256: zvbb_gather 2838 vs zvbb_x2_soa 2114

This is a +34.25% instruction overhead for zvbb_gather relative to the stable x2 backend.

## Notes
- This archive is not linked into default CMake targets.
- The stable x2 implementation reuses an internal core copy at src/keccak/fips202_zvbb_core.c.
- To rerun A/B comparisons, wire this source as an experiment-only Keccak option.
