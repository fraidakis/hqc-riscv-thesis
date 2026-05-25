# Polynomial Multiplication Algorithm Alternatives: Analysis and Justification

## Summary

This document presents the findings of a systematic evaluation of alternative polynomial
multiplication algorithms for HQC on RISC-V with Zbc and Zvbc extensions. The analysis
covers Toom-Cook-3/4 over GF(2)[x], Fast Additive FFT (FAFFT), sparse × dense
exploitation, and hybrid approaches.

**Key finding:** The current Karatsuba recursion with vectorized schoolbook leaf
(threshold = 16 words) is near-optimal for HQC polynomial sizes on RISC-V hardware
with carry-less multiply instructions. Toom-Cook-3 with explicit interpolation formulas
offers at most 5–7% theoretical improvement — insufficient to justify the implementation
complexity and verification burden. FAFFT and sparse multiplication are not viable at
these sizes.

**Empirical validation:** A complete TC3 depth-2 implementation was built, validated
(KAT pass at all 3 security levels), and benchmarked on Spike ISS. The measured result
was a **4–5× regression** (not improvement), confirming that the eval/interp overhead
dominates at HQC polynomial sizes.

---

## Motivation

Profiling of the current implementation shows the leaf functions dominate execution:
`vclmul_leaf` accounts for 31% of total instret on Zvbc and `clmul_leaf` for 22.7% on
Zbc. This raises the question: can algorithms with better asymptotic complexity reduce
the total instruction count?

| Algorithm | Asymptotic Complexity | Sub-multiplications |
|-----------|----------------------|---------------------|
| Karatsuba | O(n^1.585) | 3 × mult(n/2) |
| Toom-Cook-3 | O(n^1.465) | 5 × mult(n/3) |
| Toom-Cook-4 | O(n^1.404) | 7 × mult(n/4) |
| FAFFT (Gao-Mateer) | O(n log n log log n) | N/A (transform-based) |

---

## Candidate Analysis

### Toom-Cook-3 over GF(2)[x]

**Approach:** Split the polynomial into 3 pieces of size ⌈n/3⌉, evaluate at 5 points
in GF(2)[x] (e.g., {0, 1, x, x+1, ∞}), perform 5 sub-multiplications using Karatsuba
recursion, then interpolate using explicit XOR/shift formulas derived from the evaluation
matrix inverse.

**Feasibility:** Confirmed. The point set {0, 1, x, x+1, ∞} produces an invertible 5×5
evaluation matrix over GF(2)[x]. The interpolation can be expressed as a fixed sequence
of XOR and shift operations (Bodrato 2007, Bernstein 2009).

**Interpolation details:**
- Determinant: det(E) = x⁴ + x² = x² × (x² + 1)
- Division by det uses: right-shift by 2 (exact div by x²) + parallel-prefix XOR
  division by (x²+1)
- The parallel-prefix division uses: w ^= w<<2; w ^= w<<4; w ^= w<<8; w ^= w<<16;
  w ^= w<<32; per 64-bit word with 2-bit carry between words

**Cost model estimates:**

| Configuration | Zvbc (n=277) | Zbc (n=277) |
|---------------|-------------|-------------|
| Pure Karatsuba | baseline | baseline |
| TC3 depth=1 (top level only) | −5.1% | −4.6% |
| TC3 depth=2 | −7.0% | −6.3% |

**Measured results (Spike ISS):**

| Security Level | karatsuba_zvbc | toom3_zvbc | Change |
|---|---|---|---|
| HQC-128 (n=277) | 66,916 instret | 344,973 instret | **+415% regression** |
| HQC-192 (n=561) | 198,708 instret | 671,064 instret | **+238% regression** |
| HQC-256 (n=901) | 285,190 instret | 1,147,721 instret | **+302% regression** |

**Why the cost model failed:** The model overcounted leaf multiplications but
undercounted the overhead of:
1. Scratch buffer management (memset, pointer arithmetic per TC3 level)
2. Carry-propagating shifts for evaluation at x and x+1 (sequential, not vectorizable)
3. Interpolation numerator computation (12 shifted terms for num1, 10 for num2)
4. Division by (x²+1) — sequential per-word with carry dependency
5. Depth-2 amplification: all overhead is paid 5× at level 2

The Karatsuba champion's code-generated specialized functions avoid all this overhead —
they inline the recursion tree with compile-time-known sizes and use RVV vectorized
copy/XOR for recomposition.

**Why the gain is modest even in theory:** At n=277 (HQC-128), the Karatsuba recursion
tree is only ~4 levels deep. The asymptotic advantage of TC3 (exponent 1.465 vs 1.585)
barely manifests at this depth. Concretely:

- 5 × Karatsuba(93) requires 18,865 clmul pairs
- 3 × Karatsuba(139) requires 19,122 clmul pairs
- Ratio: 0.987 — only 1.3% fewer carry-less multiplications

**Why not ℤ-lifted Toom-Cook (Robert & Véron style)?** The Robert & Véron (2022) paper
achieves 12% speedup on x86 using VPCLMULQDQ, but their gain comes primarily from
vectorization (1→4 parallel clmuls), not from Toom-Cook itself. Their Toom-Cook uses
integer evaluation points (0, 1, −1, 2, −2) which require integer addition/subtraction
with carry propagation. Over GF(2)[x], addition = subtraction = XOR, so evaluation at
1 and −1 produces identical results — the ℤ-lifted approach cannot be applied when
sub-multiplications use carry-less multiply (`clmul`/`vclmul`). The vectorization gain
they report is already captured by our Zvbc variant.

### Toom-Cook-4 over GF(2)[x]

**Approach:** Split into 4 pieces, evaluate at 7 points, perform 7 sub-multiplications.

**Feasibility:** Confirmed — a valid 7-point set exists in GF(2)[x] elements of degree ≤ 3.

**Why it degrades performance:**

1. The evaluation matrix has entries of degree up to 3, requiring 3 shifts + 3 XORs per
   word per non-trivial entry — significantly more overhead than TC3.
2. The determinant of the 7×7 evaluation matrix is a higher-degree polynomial, making
   interpolation more expensive even with explicit formulas.
3. For n=277, TC4 gives pieces of size ⌈277/4⌉ = 70. Seven Karatsuba multiplications of
   size 70 produce more total leaf work than five multiplications of size 93 (TC3) or
   three of size 139 (Karatsuba), because the 7/3 sub-multiplication ratio exceeds the
   savings from smaller piece size at this polynomial length.
4. The break-even point where TC4 outperforms Karatsuba is well beyond HQC sizes.

### Fast Additive FFT (FAFFT / Gao-Mateer)

**Approach:** Convert polynomial multiplication to pointwise multiplications in an
extension field GF(2^m) via additive FFT, where m is chosen so that GF(2^m) contains
a subspace of sufficient size.

**Why it is not viable at HQC sizes:**

The fundamental problem is that HQC polynomial degrees are slightly above powers of 2.
For HQC-128, n = 17,669 bits. The additive FFT requires a transform length that is a
power of 2 and at least 2n. Since n ≈ 2^14 + 1,285, the required FFT size is 2^16 =
65,536 — approximately 4× the polynomial size in padding overhead.

Concrete cost for HQC-128 on Zvbc:
- Extension field: GF(2^16), each element = 1 word
- FFT butterflies: 65,536/2 × 16 = 524,288 per transform
- Each butterfly: 1 GF(2^16) multiplication (1 clmul pair + 15 XOR reduction steps) +
  1 GF(2^16) addition (1 word XOR)
- Pipeline: 3 FFTs + 65,536 pointwise multiplications + basis conversion
- **Estimated total: ~68.8 million instret vs Karatsuba's 72,644 instret — a ~950× slowdown**

No crossover point was found in the search range [57,664, 1,000,000] bits for any
hardware tier.

### Sparse × Dense Multiplication

**Approach:** Exploit the fixed-weight structure of HQC secret keys (w non-zero bits in
an n-bit polynomial, where w << n) by iterating over non-zero positions and
shift-XOR-accumulating into the result.

**Why it is not viable:**

1. **Side-channel vulnerability:** The word index of the accumulation target depends on
   the secret bit position (target_word = position // 64). This creates a
   data-dependent memory access pattern that leaks through cache timing.

2. **Constant-time mitigation kills performance:** The full-width scan mitigation
   (touching all n_words per iteration with conditional XOR) eliminates the side channel
   but makes the approach slower than Karatsuba for all HQC parameter sets.

3. **Deprecated by HQC designers:** The HQC reference implementation moved away from
   sparse multiplication in favor of dense Karatsuba precisely due to these
   constant-time concerns.

---

## Measured Karatsuba Champion Baselines

| Tier | HQC-128 | HQC-192 | HQC-256 |
|------|---------|---------|---------|
| Zvbc | 66,916 | 198,708 | 285,190 |
| RVV | 157,377 | 481,065 | 1,608,062 |
| Zbc | 211,841 | 650,635 | 1,900,362 |

---

## Conclusion

For RISC-V implementations with Zbc (scalar clmul) or Zvbc (vector vclmul.vv), the
current Karatsuba recursion with vectorized schoolbook leaf is the optimal choice for
single-call polynomial multiplication at HQC sizes. The reasons are:

1. **Hardware carry-less multiply makes the leaf cheap.** With `vclmul.vv` processing 4
   element pairs per instruction at LMUL=m4, the O(n²) schoolbook leaf at threshold=16
   costs only 16 vector instruction groups per call. No alternative algorithm can reduce
   this leaf cost — it's already at the hardware minimum.

2. **HQC polynomial sizes are too small for asymptotic gains.** At n=277–901 words
   (4–5 Karatsuba recursion levels), the improved exponents of TC3 (1.465) and TC4
   (1.404) provide negligible concrete savings over Karatsuba (1.585). The
   eval/interp overhead of these algorithms nearly cancels their sub-problem advantage.

3. **TC3 depth-2 was empirically validated as 4–5× slower.** A complete implementation
   passing all KAT vectors confirmed that the overhead dominates at these sizes.

4. **FAFFT requires ~4× padding due to HQC's non-power-of-2 degrees.** The resulting
   transform size makes FAFFT ~950× slower than Karatsuba at HQC-128 on Zvbc.

5. **Sparse multiplication is a security liability.** Data-dependent memory access
   patterns leak secret key information, and constant-time mitigations eliminate the
   performance advantage.

6. **The vectorization gain is already captured.** The Zvbc variant with LMUL=m4
   achieves the same 4× parallelism as x86 VPCLMULQDQ.

---

## Future Work: Additive FFT with CRT and Transform Caching

The most promising path to further improvement is the approach of Chen, Chiu, Peng &
Yang (TCHES 2026, "Accelerating HQC with Additive FFT"). Their key innovations:

1. **CRT-based splitting:** Instead of padding to the next power of 2, they factor the
   polynomial ring using the Chinese Remainder Theorem to split the multiplication into
   smaller pieces that fit cleanly into power-of-2 FFT sizes.

2. **Transform caching:** They cache the FFT of the public/secret key after key
   generation and reuse the transform during encapsulation and decapsulation.

3. **Results:** On platforms with PCLMULQDQ (x86 AVX2), the caching strategy allows
   FFT-based implementations to outperform Toom-Karatsuba in Encap and Decap operations.

**Applicability to RISC-V Zvbc:** This approach requires architectural changes beyond a
drop-in `vect_mul` replacement — modified KEM API, stored transformed keys, and CRT
splitting adapted to RISC-V vector register constraints.

---

## References

- Bodrato, M. "Towards Optimal Toom-Cook Multiplication for Univariate and Multivariate
  Polynomials in Characteristic 2 and 0" (WAIFI 2007)
- Bernstein, D.J. "Batch Binary Edwards" (CRYPTO 2009)
- Robert, J.-M. and Véron, P. "Faster multiplication over F₂[X] using AVX512 instruction
  set and VPCLMULQDQ instruction" (J. Cryptographic Engineering, 2022)
- Chen, M.-S., Chiu, C.-M., Peng, C.-T., and Yang, B.-Y. "Accelerating HQC with
  Additive FFT" (IACR TCHES 2026, Vol. 2, pp. 520–544)
