# Phase 1a Scoreboard — karatsuba_zbc (clmul base-case Karatsuba)

## Summary

Replace schoolbook GF(2)[x] multiply in Karatsuba leaf with RISC-V clmul/clmulh (Zbc).
Each 64x64 carry-less product takes exactly 2 instructions instead of ~64 shift-XOR pairs.

## Results (median_instret, N=1 iteration, Spike simulation)

| Level   | phase0 (karatsuba_ref) | T=8 instret | T=8 speedup | T=16 instret | T=16 speedup |
|---------|------------------------|-------------|-------------|--------------|--------------|
| HQC-128 |             18,578,007 |     450,644 |       41.2x |      354,187 |       52.5x  |
| HQC-192 |             56,469,320 |   1,389,629 |       40.6x |    1,078,641 |       52.4x  |
| HQC-256 |            139,077,901 |   2,388,025 |       58.2x |    2,257,476 |       61.6x  |

Optimal threshold: T=16 (sweep range T=1..32; T=12-16 all give same instret due to recursion tree shape).
Target: >=2x speedup -> ACHIEVED (52-62x at T=16)

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
- analysis/benchmarks/phase1a/hqc{128,192,256}_karatsuba_zbc_polymul_N1.csv
- analysis/benchmarks/phase1a/hqc{128,192,256}_karatsuba_zbc_hist.txt
- analysis/llvm_mca/reports/gf2x_clmul_mca.txt
