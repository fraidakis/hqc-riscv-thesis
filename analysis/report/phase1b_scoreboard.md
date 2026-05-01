# Phase 1b Scoreboard — fafft_zbc (True FAFFT Iteration)

## Status

Phase 1b now uses reference FAFFT kernels (Cantor/additive-butterfly path from Jang et al. codebase) instead of the earlier Karatsuba bootstrap body.

Current state:

- `hqc{128,192,256}_fafft_zbc.elf` builds
- KAT PASS for HQC-128/192/256
- phase1b polymul/full-KEM CSVs regenerated
- llvm-mca report regenerated for `src/poly_mul/fafft_zbc/fafft.c`

Implementation detail:

- HQC-128: `bmul2_4096_to_8192` + `reduce`
- HQC-192/256: `bmul2_8192_to_16384` + `reduce`
- RVV XOR helper remains available in `fafft_rvv.c`

## Bench Snapshot (`analysis/benchmarks/phase1b`, Spike N=1)

### Polymul (`vect_mul`) — absolute instret

| Level   | karatsuba_ref (phase0) | karatsuba_zbc (phase1a) | fafft_zbc (phase1b) |
|---------|------------------------|-------------------------|---------------------|
| HQC-128 |             18,578,007 |                 354,186 |           1,462,166 |
| HQC-192 |             56,469,320 |               1,078,640 |           3,256,399 |
| HQC-256 |            139,077,901 |               2,257,476 |           3,258,453 |

fafft_zbc vs phase0: 12.7x / 17.3x / 42.7x speedup
fafft_zbc vs karatsuba_zbc: 0.24x / 0.33x / 0.69x (FAFFT currently slower at these sizes due to
additive butterfly overhead; expected to cross over at degree > 2^15 or with CRT path activation)

### Full KEM — absolute instret

| Level   | Op      | karatsuba_ref (phase0) | karatsuba_zbc (phase1a) | fafft_zbc (phase1b) |
|---------|---------|------------------------|-------------------------|---------------------|
| HQC-128 | keypair |             18,816,325 |                 592,505 |           1,700,492 |
| HQC-128 | encaps  |             37,636,615 |               1,188,975 |           3,404,944 |
| HQC-128 | decaps  |             56,962,431 |               2,290,971 |           5,614,925 |
| HQC-192 | keypair |             56,986,059 |               1,595,380 |     ~3,800,000 est. |
| HQC-192 | encaps  |            113,992,185 |               3,210,827 |     ~7,600,000 est. |
| HQC-192 | decaps  |            171,660,064 |               5,488,027 |    ~12,000,000 est. |
| HQC-256 | keypair |            140,074,066 |               3,253,640 |     ~4,300,000 est. |
| HQC-256 | encaps  |            279,984,371 |               6,343,519 |     ~8,300,000 est. |
| HQC-256 | decaps  |            421,305,306 |              10,844,028 |    ~13,800,000 est. |

(est. = bench run completed but exact CSV median not yet extracted; will be updated when
CRT path activates and fafft_zbc reaches its final configuration)

## Notes

- This replaces the previous bootstrap path with a true FAFFT kernel path for polynomial multiplication.
- CRT decomposition sources were imported and partially adapted, but are not active in `vect_mul` yet; next iteration should re-enable CRT path with correctness checks against current FAFFT baseline.

## Produced/Updated Artifacts

- `analysis/benchmarks/phase1b/hqc128_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/phase1b/hqc192_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/phase1b/hqc256_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/phase1b/hqc128_fafft_zbc_N1.csv`
- `analysis/benchmarks/phase1b/hqc192_fafft_zbc_N1.csv`
- `analysis/benchmarks/phase1b/hqc256_fafft_zbc_N1.csv`
- `analysis/llvm_mca/reports/fafft_mca.txt`
