# Phase 1a Scoreboard — karatsuba_zbc (clmul base-case Karatsuba)

## Summary

Replace schoolbook GF(2)[x] multiply in Karatsuba leaf with RISC-V clmul/clmulh (Zbc).
Each 64x64 carry-less product takes exactly 2 instructions instead of ~64 shift-XOR pairs.

## vect_mul Results (median_instret, N=1 iteration, Spike simulation)

| Level   | baseline_ref (karatsuba_ref) | T=8 instret | T=8 speedup | T=16 instret | T=16 speedup |
|---------|------------------------|-------------|-------------|--------------|--------------|
| HQC-128 |             18,578,007 |     450,644 |       41.2x |      354,187 |       52.5x  |
| HQC-192 |             56,469,320 |   1,389,629 |       40.6x |    1,078,641 |       52.4x  |
| HQC-256 |            139,077,901 |   2,388,025 |       58.2x |    2,257,476 |       61.6x  |

Optimal threshold: T=16 (sweep range T=1..32; T=12-16 all give same instret due to recursion tree shape).
Target: >=2x speedup -> ACHIEVED (52-62x at T=16)

## Full KEM Results (median_instret, N=1 iteration, Spike simulation)

Baseline: baseline_ref karatsuba_ref; karatsuba_zbc at KARATSUBA_THRESHOLD=16.

| Level   | Op      | karatsuba_ref (baseline_ref) | karatsuba_zbc (polymul_zbc) | speedup |
|---------|---------|------------------------|-------------------------|---------|
| HQC-128 | keypair |             18,816,325 |                 592,505 |  31.8x  |
| HQC-128 | encaps  |             37,636,615 |               1,188,975 |  31.7x  |
| HQC-128 | decaps  |             56,962,431 |               2,290,971 |  24.9x  |
| HQC-192 | keypair |             56,986,059 |               1,595,380 |  35.7x  |
| HQC-192 | encaps  |            113,992,185 |               3,210,827 |  35.5x  |
| HQC-192 | decaps  |            171,660,064 |               5,488,027 |  31.3x  |
| HQC-256 | keypair |            140,074,066 |               3,253,640 |  43.0x  |
| HQC-256 | encaps  |            279,984,371 |               6,343,519 |  44.1x  |
| HQC-256 | decaps  |            421,305,306 |              10,844,028 |  38.8x  |

Note: end-to-end KEM speedup is lower than vect_mul speedup because SHAKE/Keccak and
RM/RS decoding are now co-dominant in decaps (~25-40% of total instructions post-optimization,
per Amdahl's law analysis in docs/PLAN.md).

## Instruction Scheduling Investigation

A 2-wide dispatch variant of `clmul_leaf` was benchmarked (two independent clmul pairs
per inner iteration, with the shared r[i+j+1] ^= hi0 ^ lo1 merge). On Spike (instret
metric) it showed +9% regression vs. the sequential loop due to extra loop-bound instructions.
Hardware measurement on BPI-F3 (X60 in-order) confirmed no benefit: −0.67% at L128,
+4.3% regression at L192. The optimization was removed entirely (2026-06-28).
The simple sequential scheduling is the champion.

## Threshold Sweep (HQC-128)

| T  | instret   |
|----|-----------|
| 1  | 1,809,335 |
| 2  | 1,094,119 |
| 4  |   677,836 |
| 8  |   450,644 |
| 12 |   354,187 |
| 14 |   354,187 |
| 16 |   354,187 (optimal) |
| 18 |   360,287 |
| 32 |   360,287 |

## Histogram Top-10 (HQC-128, T=16)

clmul + clmulh = 10.6% of all instructions (dominant compute in leaf loop).

ld=15.8%, addi=11.9%, bnez=9.6%, bne=6.8%, vxor.vv=6.3%, vle64.v=5.8%,
vmv.v.x=5.3%, clmulh=5.3%, clmul=5.3%, add=3.4%

## Artifacts

- src/poly_mul/karatsuba_zbc/gf2x_clmul.c  (KARATSUBA_THRESHOLD=16)
- analysis/benchmarks/polymul_zbc/hqc{128,192,256}_karatsuba_zbc_polymul_N1.csv
- analysis/benchmarks/polymul_zbc/hqc{128,192,256}_karatsuba_zbc_N1.csv
- analysis/benchmarks/polymul_zbc/hqc{128,192,256}_karatsuba_zbc_hist.txt
- analysis/llvm_mca/reports/gf2x_clmul_mca.txt
