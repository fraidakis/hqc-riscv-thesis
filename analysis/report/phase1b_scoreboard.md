# Phase 1b Scoreboard — fafft_zbc

## Status

Phase 1b variant is now fully integrated and correctness-stable:

- `hqc{128,192,256}_fafft_zbc.elf` builds
- KAT PASS for HQC-128/192/256
- phase1b polymul/full-KEM CSVs generated
- phase1b histograms generated
- llvm-mca report generated for `src/poly_mul/fafft_zbc/fafft.c`

Current `fafft_zbc` kernel is a bootstrap implementation (clmul-Karatsuba scalar backbone + RVV XOR helper API), used to remove stubs/traps and establish a measurable baseline for subsequent true-FAFFT iterations (Cantor basis + CRT decomposition + additive butterfly scheduling).

## Baselines

- Phase 0 HQC-128 `karatsuba_ref` vect_mul median_instret: **18,578,007**
- Phase 1a HQC-128 `karatsuba_zbc` vect_mul median_instret: **354,187**

## Mandatory Optimization Loop (HQC-128, median_instret)

| Iter | Technique | HQC-128 instret | Delta vs prev kept | Verdict |
|------|-----------|-----------------|--------------------|---------|
| 0 | phase0 baseline (karatsuba_ref) | 18,578,007 | - | ref |
| 0a | phase1a reference (karatsuba_zbc, T=16) | 354,187 | - | ref |
| 1 | fafft_zbc bootstrap kernel (T=16) | 353,946 | -0.07% | drop (<1% gain rule) |
| 2 | probe T=12 | 353,946 | +0.00% vs iter1 | drop |
| 3 | probe T=20 | 360,208 | +1.77% slower | drop |
| 4 | probe T=24 | 360,208 | +0.00% vs iter3 | drop |

Local optimum for this bootstrap kernel: **T=12..16 plateau**, with T=16 kept in source.

## Phase1b Current Results (from `analysis/benchmarks/phase1b`)

### Polymul micro-bench (vect_mul)

| Level | median_instret |
|-------|----------------|
| HQC-128 | 353,946 |
| HQC-192 | 1,077,913 |
| HQC-256 | 2,256,748 |

### Full KEM bench (N=1)

| Level | keypair | encaps | decaps |
|-------|---------|--------|--------|
| HQC-128 | 592,263 | 1,188,491 | 2,290,245 |
| HQC-192 | 1,594,652 | 3,209,371 | 5,485,843 |
| HQC-256 | 3,252,912 | 6,342,063 | 10,841,844 |

## Histogram Signal

HQC-256 top mix includes:

- `clmulh`: 5.3%
- `clmul`: 5.3%

Combined `clmul`+`clmulh` = **10.6%** of dynamic instructions.

## Gap vs Phase1b Target

Target from prompt: `fafft_zbc` should be **>= 3x faster than phase1a karatsuba_zbc** on HQC-128 vect_mul.

Measured now:

- phase1a: 354,187
- phase1b current: 353,946
- relative: **1.00068x** (0.07% faster)

So the **>=3x target is not reached yet**. Remaining work is the actual FAFFT algorithmic port (Cantor/CRT/butterfly), not further tuning of this bootstrap kernel.

## Produced Artifacts

- `analysis/benchmarks/phase1b/hqc128_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/phase1b/hqc192_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/phase1b/hqc256_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/phase1b/hqc128_fafft_zbc_hist.txt`
- `analysis/benchmarks/phase1b/hqc192_fafft_zbc_hist.txt`
- `analysis/benchmarks/phase1b/hqc256_fafft_zbc_hist.txt`
- `analysis/benchmarks/phase1b/hqc128_fafft_zbc_N1.csv`
- `analysis/benchmarks/phase1b/hqc192_fafft_zbc_N1.csv`
- `analysis/benchmarks/phase1b/hqc256_fafft_zbc_N1.csv`
- `analysis/llvm_mca/reports/fafft_mca.txt`
- `analysis/report/phase1b_scoreboard.md`
