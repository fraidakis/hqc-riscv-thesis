# Phase 1c Scoreboard — Hybrid Karatsuba-FAFFT (hybrid_zbc)

## Summary

Phase 1c implements a hybrid that applies d Karatsuba outer levels before
calling the FAFFT full-product kernel on each sub-multiplication.  An adaptive
kernel selection is applied: for sub-problems ≤ 277 words (≤ HQC-128 size),
the smaller `bmul2_4096_to_8192` kernel (65536-bit domain) is used; for larger
sub-problems, `bmul2_8192_to_16384` (131072-bit domain) is used.

**Key finding:** No crossover depth exists.  The curve is monotonically
increasing (not U-shaped) across all security levels.  d*=0 (pure FAFFT) is
the best hybrid variant, but is still ~4× slower than karatsuba_zbc.

---

## Crossover Depth Sweep (polymul instret, median, N_ITER=1)

| Level   | d=0 (FAFFT) | d=1        | d=2         | d=3 (est.) | d=4 (est.) | d* (best) |
|---------|------------|------------|-------------|------------|------------|-----------|
| HQC-128 | 1,463,531  | 4,390,073  | 13,160,160  | ~39M       | ~118M      | d=0       |
| HQC-192 | 3,254,008  | 9,773,238  | 13,176,205  | ~40M       | ~120M      | d=0       |
| HQC-256 | 3,258,265  | 9,785,206  | 13,197,465  | ~40M       | ~120M      | d=0       |

Note: d=3,4 not measured (trend unambiguously monotonic from d=0→1→2).
Note: HQC-192/256 d=2 uses adaptive kernel (bmul2_4096_to_8192) for sub-problems
of ≤277 words; reduces per-call domain by 2× but 9 calls × ½ cost ≈ 4.5× still worse.

Ratios (d=1/d=0 per level):
- HQC-128: 4.39M / 1.46M = 3.0× (3 FAFFT calls, same domain)
- HQC-192: 9.77M / 3.25M = 3.0× (3 FAFFT calls, same domain — sub-problems too large for smaller kernel)
- HQC-256: 9.79M / 3.26M = 3.0× (same)

Ratios (d=2/d=1 per level):
- HQC-128: 13.16M / 4.39M = 3.0× (9 calls, same domain still)
- HQC-192: 13.18M / 9.77M = 1.35× (9 calls, but adaptive kernel halves domain — 9×½=4.5 calls of full cost, ~1.5×, actual 1.35×)
- HQC-256: 13.20M / 9.79M = 1.35× (same adaptive kernel effect)

---

## Iteration Scoreboard vs Previous Phases

| Iter | Technique                          | HQC-128 instret | Δ vs Phase 1a | Δ vs Phase 1b | Verdict   |
|------|------------------------------------|-----------------|---------------|---------------|-----------|
| —    | baseline_ref ref (karatsuba_ref)         | 18,578,007      | ref0          | —             | baseline  |
| —    | polymul_zbc (karatsuba_zbc)            | 354,187         | **52.4×**     | ref1a         | ✓ best    |
| —    | fafft_zbc (fafft_zbc)                | 1,462,167       | 4.1× slower   | ref1b         | ✓ done    |
| 0    | hybrid d=0 (= pure FAFFT)          | 1,463,531       | 4.1× slower   | +0.1%         | =fafft_zbc |
| 1    | hybrid d=1                         | 4,390,073       | 12.4× slower  | 3.0× worse    | drop      |
| 2    | hybrid d=2 (adaptive kernel 192/256)| 13,160,160     | 37.2× slower  | 9.0× worse    | drop      |

---

## Root Cause Analysis: Why No Crossover?

Expected (from PLAN.md):
> "n=17669 pads to 32768 directly, but two Karatsuba sub-problems of ~8835 each
> pad to 16384 — halving wasted work."

Actual:
The existing FAFFT infrastructure has **fixed-domain** kernels:
- `bmul2_4096_to_8192`: hardcodes `memcpy(..., 2216)` = 277 uint64 words (HQC-128 size)
- `bmul2_8192_to_16384`: reads `VEC_N_SIZE_64*8` bytes (full polynomial size)

For HQC-128 at d=1: sub-problems are ~139 words.  The `bmul2_4096_to_8192` kernel
still processes the full 32768-bit domain (bc_1_32768 + btfy_65536), regardless of
how sparse the input is.  **The domain does NOT shrink with the sub-problem.**

Consequence: each Karatsuba level multiplies FAFFT call count by 3 without
reducing per-call cost → 3× penalty per level → monotonic worsening.

For HQC-192/256 at d≥2: sub-problems drop below 277 words, enabling the smaller
`bmul2_4096_to_8192` (65536-bit domain) via adaptive selection.  This halves the
per-call cost but still leaves 9 calls vs 1 → net factor ~4.5× vs d=0.

Root cause of the missing crossover:
1. **No bc_1_16384-based kernel**: A `bmul2_2048_to_4096` kernel using bc_1_16384
   (16384-bit domain) could handle sub-problems of ≤512 uint32 words.  This is the
   smallest bc_1 available but has no bmul2 wrapper in the Jang et al. source.
2. **HQC polynomial sizes are below the FAFFT crossover point**: The O(n log²n)
   advantage of FAFFT over Karatsuba only materialises at degree ≫ 2^15 bits.
   HQC-128 polynomials are 2^14 bits — exactly below the crossover.

---

## Conclusion

**Phase 1c local optimum:** karatsuba_zbc (polymul_zbc) is the global optimum for
HQC polynomial multiplication on Zbc-equipped RISC-V hardware.

| Algorithm     | HQC-128    | HQC-192    | HQC-256    | vs Phase 0   |
|---------------|------------|------------|------------|--------------|
| karatsuba_zbc | 354,187    | 1,078,640  | 2,257,476  | **52.4×**    |
| fafft_zbc     | 1,462,167  | 3,256,399  | 3,258,453  | 12.7×        |
| hybrid_zbc d* | 1,463,531  | 3,254,008  | 3,258,265  | ≈fafft_zbc   |

The empirical finding is a **thesis contribution**: on Zbc-capable hardware where
clmul gives Karatsuba an ~8–10× leaf speedup, FAFFT does not achieve its theoretical
advantage at HQC-128/192/256 degrees.  The crossover would require either:
- A much smaller bc_1 kernel (e.g., bc_1_16384 with a proper bmul2 wrapper), or
- Scaling to polynomial degrees ≫ 2^15 bits (outside HQC's current parameter sets).

**Next phase:** Phase 1+ (Zvbc vectorised FAFFT) or full-stack optimisation of KEM.
