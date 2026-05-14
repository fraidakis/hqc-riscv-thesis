# Phase 1b Scoreboard — `fafft_zbc` v5 (Paper-Optimised CRT Bypass)

**Date**: 2025-07  
**Baseline**: v4 RVV Vectorised Schoolbook = **178,048 instret** (HQC-128, N_ITER=1)  
**Old v4 CRT Bypass (Task 2)**: 1,652,853 instret

---

## Results

| Iter | Technique | HQC-128 instret | Δ vs v4 (178,048) | Δ vs v4 CRT (1,652,853) | KAT | Verdict |
|------|-----------|-----------------|-------------------|-------------------------|-----|---------|
| 5.1  | v5 3-Product CRT Split | **179,959** | +1,911 (+1.1%) | −1,472,894 (−89.1%) | PASS 100/100 | keep (parity) |

## Multi-Level Results

| Level | v5 instret | v4 instret | Δ |
|-------|-----------|-----------|---|
| HQC-128 | 179,959 | 178,048 | +1,911 (+1.1%) |
| HQC-192 | 723,841 | 723,841 | 0 (0.0%) — fallback path |
| HQC-256 | 1,843,597 | 1,843,597 | 0 (0.0%) — fallback path |

---

## Algorithm Description

v5 implements the **Paper-Optimised CRT Bypass** via a 3-product polynomial split
at the power-of-two boundary x^{16384} (512 32-bit / 256 64-bit words):

```
A = A_L + A_H · x^{16384}    A_L: 256 words, A_H: 21 words (HQC-128)
B = B_L + B_H · x^{16384}    B_L: 256 words, B_H: 21 words

A×B = C0 + cross·x^{16384} + C2·x^{32768}
      C0    = A_L×B_L           (511 64-bit words, r[0..510])
      cross = A_L×B_H + A_H×B_L (≤277 words, r[256..532])
      C2    = A_H×B_H           ( ≤41 words, r[512..552])
```

All sub-products fit within the 553-word (64-bit) double-width ring buffer
`r[0..2n]`, avoiding any overflow.  Final `reduce()` maps to `GF(2)[x]/(x^17669+1)`.

Each sub-product uses the same RVV `vclmul.vv`/`vclmulh.vv` vectorised schoolbook
inner loop from v4.  For HQC-192/256 (no split defined), v5 falls back to the
identical 277×277 / 561×561 / 901×901 schoolbook with a 64-bit ring reduction.

---

## Diagnostics & Root-Cause Notes

### Why the reference `CRT_fft_HQC1` is broken for HQC-128

`ref/CRT_fft_hqc1.c::radix16_81()` reads **exactly 81 words** from each input
(via `poly_swap(sa, a, 81)`), hardcoded in `radix16_81.c`.  For HQC-128,
polynomials have 553 32-bit words; the remaining 472 words are silently ignored,
producing a wrong CRT remainder.  This is documented in `ref/gfmul_fft.c` under
`bmul2_4096_to_4096`: *"bc_1_16384 accepts at most 512-word inputs, and HQC-128
polynomials have 553 words."*  The reference CRT function is therefore inactive
for HQC-128 and fails KAT 0/100.

### The `bmul2_4096_to_4096` stub workaround

The stub calls `bmul2_4096_to_8192` (full bc_1_32768 domain) and truncates the
result.  This correctly computes A×B mod x^{32768}, but the CRT correction step
(`radix16_81(rx, A, B)`) still only sees 81 words of A and B, so the algorithm
remains broken.  KAT confirmed: even with the stub, `CRT_fft_HQC1` gives
0/100 PASS.

### Why the 3-product split works (and why it can't use P_15)

P_15 (the 501-term CRT modulus polynomial from the paper) is designed to be used
with `radix16_81` evaluated at 81 CRT points.  Since `radix16_81` only covers
81 words of input (2592 bits < PARAM_N=17669), the P_15 correction pathway
cannot provide a valid CRT recovery for HQC-128 polynomials.

The direct 3-product split avoids this constraint entirely: it decomposes A×B
exactly without any CRT evaluation step, placing all sub-products within the
`2n`-word ring buffer and delegating ring reduction to the existing `reduce.c`.

---

## Why v5 ≈ v4 Performance

The v5 split introduces marginal overhead vs v4:
- v4: 277 rows × ceil(277/VL) inner iterations + `v4_reduce` (277 iterations)
- v5: 256 rows × ceil(256/VL) [C0] + 256×ceil(21/VL) + 21×ceil(256/VL) [cross] +
      21×ceil(21/VL) [C2] + `reduce()` (553 iterations, 32-bit)

Total v5 rows ≈ C0 (65536) + cross (11264+10496) + C2 (441) ≈ 87737 iterations vs
v4 ≈ 19390 iterations × VL=4 = 77561 row-steps.  The split decomposition computes
MORE total schoolbook work than the monolithic 277×277.

**The 3-product split is not a performance optimisation over v4.**  The paper's
speedup relies on replacing the dominant C0 (512×512) multiplication with a fast
FAFFT using `bc_1_16384`, which itself requires inputs of exactly ≤512 words—a
constraint satisfied by A_L/B_L but not by the full 553-word HQC-128 polynomials.
Substituting the RVV schoolbook for FAFFT in C0 eliminates the speedup.

**Finding:** The Paper-Optimised CRT Bypass architecture does NOT improve
instruction count over the v4 RVV schoolbook when the FAFFT is replaced by the
schoolbook for the dominant 512×512 sub-product.  v5 performs at **parity** with
v4 for HQC-128 (+1.1%) and identically for HQC-192/256.

---

## Files Changed

| File | Change |
|------|--------|
| `src/poly_mul/fafft_zbc/v5/fafft_v5.h` | New header |
| `src/poly_mul/fafft_zbc/v5/fafft_v5.c` | New implementation (3-product CRT split) |
| `src/poly_mul/fafft_zbc/fafft.c` | Dispatch updated to `vect_mul_v5` |
| `CMakeLists.txt` | v5 source added to `fafft_zbc` target |
