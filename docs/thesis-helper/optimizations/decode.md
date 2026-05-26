# 5.4 Vectorized Error Correction and Decode Acceleration

## 1. Baseline Architecture and Starting Point

The HQC key encapsulation mechanism employs a concatenated code consisting of an inner first-order Reed-Muller code RM(1,7) and an outer Reed-Solomon code over $\mathrm{GF}(2^8)$. During decapsulation, the decoder executes these two stages in strict series:

```
code_decode(m, em):
    reed_muller_decode(tmp, em)      ← inner code: soft-decision FHT
    reed_solomon_decode(m, tmp)      ← outer code: algebraic decoding
```

The Reed-Muller decoder processes $N_1$ symbols independently, each requiring three sub-functions: `expand_and_sum` (bit-extraction and accumulation of MULTIPLICITY codeword copies into a 128-element signed array), `hadamard` (seven-pass Fast Hadamard Transform on 128 elements), and `find_peaks` (argmax over the transformed array to recover the decoded byte). The Reed-Solomon decoder operates on the $N_1$-byte output of the RM stage through five sequential sub-functions: `compute_syndromes` (Axis A), `compute_elp` via Berlekamp-Massey (Axis B), `compute_roots` via additive Gao-Mateer FFT (Axis C), `compute_error_values` via the Forney formula (Axis D), and `correct_errors`.

The security-level parameters governing decoder complexity are:

| Level | $N_1$ | $\delta$ (DELTA) | $N_2$ | MULT | $K$ |
|-------|-----:|----------:|-----:|-----:|----:|
| HQC-128 | 46 | 15 | 384 | 3 | 16 |
| HQC-192 | 56 | 16 | 640 | 5 | 24 |
| HQC-256 | 90 | 29 | 640 | 5 | 32 |

The RS sub-functions exhibit quadratic or superlinear scaling with $\delta$ and $N_1$: syndrome computation requires $2\delta \cdot (N_1 - 1)$ GF multiplications, the Berlekamp-Massey inner loops accumulate $O(\delta^2)$ multiplications, and the Forney evaluation requires $O(\delta^2)$ multiplications per error location. This scaling causes the HQC-256 RS cost to exceed the HQC-128 RS cost by approximately a factor of four.

The decode subsystem accounts for a substantial fraction of total KEM instruction retirement across all three implementation variants. In the scalar reference configuration, the decode pipeline consumes between 34% (HQC-128) and 21% (HQC-256) of total decapsulation instructions, with the RM stage dominating at lower security levels and the RS stage dominating at higher levels.

### Cross-Track GF(2^8) Improvement: Zbc `clmul` Injection

A single cross-cutting optimization is shared by all three decoder tracks. The promoted `src/common/gf.c` replaces the software carry-less multiplication loop in `gf_mul()` with a single RISC-V Zbc `clmul` instruction. The original reference implementation (`gf_carryless_mul`) uses a constant-time schoolbook algorithm requiring approximately 15 instructions per 8×8-bit carry-less product. The Zbc path computes the identical 14-bit product in one cycle:

```c
__asm__ volatile ("clmul %0, %1, %2" : "=r"(prod)
    : "r"((uint64_t)(a & 0xFF)), "r"((uint64_t)(b & 0xFF)));
return gf_reduce((uint16_t)prod);
```

The `gf_reduce` function performs a two-pass modular reduction of the 14-bit product modulo the irreducible polynomial $p(x) = x^8 + x^4 + x^3 + x^2 + 1$ (0x11D) using fixed-tap shift-and-XOR feedback. This reduction requires approximately 12 instructions regardless of input, yielding a total `gf_mul` cost of approximately 14 instructions on Zbc targets versus approximately 27 instructions on the software path.

It is important to note that the scalar reference decode path does not benefit meaningfully from Zbb auto-lowering by the compiler. The RM hadamard uses plain `int16` addition and subtraction; `gf_reduce` uses integer shifts and XOR. No rotation, count-leading-zeros, or byte-manipulation patterns amenable to Zbb instructions appear in the decode hot path.

## 2. Optimization Progression (Qualitative)

The decode acceleration proceeded through a sequence of increasingly targeted interventions:

1. **Scalar baseline with Zbc `gf_mul`** (all tracks). The promoted `gf.c` reduced per-multiplication cost from ~27 to ~14 instructions, providing a uniform improvement to every GF(2^8) arithmetic call in both RM and RS decoders.

2. **Phase 3a — Full Reed-Muller vectorization** (Tracks B and C). The three RM sub-functions (`expand_and_sum`, `hadamard`, `find_peaks`) were rewritten using RVV 1.0 intrinsics. The FHT butterfly structure maps naturally to strided vector loads and paired add/subtract operations, yielding the single largest instruction reduction in the decode pipeline. This phase was promoted to `src/decode/reed_muller_rvv.c`.

3. **Phase 3b Axis A — RS syndrome computation** (Tracks B and C). The $2\delta$ independent GF(2^8) dot-products in `compute_syndromes` were vectorized. Track B uses RVV indexed gather loads on the `gf_log[]` and `gf_exp[]` tables (SEW=16, LMUL=4, vl=64 at VLEN=256), requiring only the V extension. Track C uses Zvbc `vclmul.vv` lane-packing with inline Barrett reduction (SEW=64, LMUL=4, vl=16 at VLEN=256). Both approaches replace the per-element scalar `gf_mul` call chain. Promoted to `src/decode/reed_solomon_rvv.c` (Track B) and `src/decode/reed_solomon_zvbc.c` (Track C).

4. **Phase 3b Axes B+D — BM inner loops and Forney** (Track C only). The three inner loops of Berlekamp-Massey (`compute_elp`) and the two inner structures of the Forney formula (`compute_error_values`) were vectorized using the same Zvbc lane-packing strategy. The outer BM loop remains scalar due to sequential data dependencies. Promoted to `src/decode/reed_solomon_zvbc.c`.

5. **Phase 3b Axis C — FFT root finding** (Track C, implemented). The recursive Gao-Mateer additive FFT was rewritten as an iterative breadth-first traversal (`src/decode/fft_zvbc.c`), with three vectorized helpers: `beta_scale_zvbc()` (vclmul.vv lane-packing for element-wise GF multiply), `xor_broadcast_zvbc()` (contiguous vle16/vxor.vx/vse16), and `gammas_sums_combine_zvbc()` (vclmul.vv + Barrett reduce + vxor.vv butterfly). The recursive-to-iterative transformation eliminated O(2^M) function calls and enabled vectorization of the beta-scaling and XOR-broadcast passes. Linked into the build when `HQC_DECODE_IMPL=zvbc`, replacing vendor `fft.c`. KAT validated: 300/300 vectors pass. Instret reduction: −31% to −36% on the Axis C component.

## 3. Variant I — Track A: Scalar Reference Decode (`karatsuba_zbc`)

Track A pairs the scalar `karatsuba_zbc` polynomial multiplier with the unmodified reference decoders from the upstream HQC specification. The sole decode-path improvement present in this track is the Zbc `clmul` injection in `gf_mul()` described in Section 1.

### Reed-Muller Scalar Path

The reference `reed_muller.c` implements RM(1,7) decoding through three functions executed per symbol:

- **`expand_and_sum`**: Extracts individual bits from MULTIPLICITY copies of a 128-bit codeword using scalar shift-and-mask operations (4 words × 32 bits × MULT iterations), accumulating into a 128-element `int16` array.
- **`hadamard`**: Performs seven passes of the butterfly Fast Hadamard Transform. Each pass iterates over 64 pairs with explicit indexing (`p1[2*i]`, `p1[2*i+1]`), producing 128 add/subtract/store operations per pass — 896 scalar arithmetic operations total per symbol.
- **`find_peaks`**: Scans the 128-element transformed array to locate the position of maximum absolute value, using arithmetic masking (`pos_mask = -(t > 0)`) to maintain constant-time execution.

### Reed-Solomon Scalar Path

The reference `reed_solomon.c` implements the five-axis pipeline:

- **Axis A** (`compute_syndromes`): Evaluates $2\delta$ syndromes, each as a degree-$(N_1-1)$ polynomial over GF(2^8). The double-nested loop executes $2\delta \cdot (N_1-1)$ scalar `gf_mul` calls.
- **Axis B** (`compute_elp`): Berlekamp-Massey algorithm with $2\delta$ outer iterations. Each iteration contains a SAXPY update (`sigma[i] ^= gf_mul(dd, X_sigma_p[i])`) and a discrepancy dot-product, both growing from length 1 to $\delta$. The `volatile uint16_t mask12__` declaration prevents compiler optimization of the constant-time mask update.
- **Axis C** (`compute_roots`): Delegates to the additive Gao-Mateer FFT (`fft.c`) which evaluates the error-locator polynomial $\sigma(x)$ at all $2^8$ field elements via recursive radix decomposition.
- **Axis D** (`compute_error_values`): Forney formula evaluation with an $O(N_1 \cdot \delta)$ beta-scatter phase and an $O(\delta^2)$ error-value computation phase, both using arithmetic masking for constant-time behaviour.
- **`correct_errors`**: Trivial XOR loop over $N_1$ bytes.

## 4. Variant II — Track B: RVV Decode (`karatsuba_rvv`)

Track B introduces full RVV vectorization of the Reed-Muller decoder (Phase 3a) and Zvbc-accelerated syndrome computation for the Reed-Solomon decoder (Phase 3b Axis A). The remaining RS axes (B, C, D) fall back to the scalar reference implementations.

### Reed-Muller Vectorization (Phase 3a)

#### RM-1: `expand_and_sum_rvv` — Bit Extraction via `vid` + `vsrl` + `vncvt`

The scalar bit-extraction loop (4 words × 32 bits × MULT shifts per symbol) was replaced with a vectorized pattern exploiting the RVV `vid` instruction to generate shift amounts:

```c
vuint32m4_t shift_amounts = __riscv_vid_v_u32m4(32);  // [0,1,...,31]
vuint32m4_t bits32 = vand_vx(vsrl_vv(vword, shift_amounts), 1);
vuint16m2_t bits16 = vncvt_x_x_w(bits32);
```

At SEW=32, LMUL=4, the vector length reaches 32 elements — sufficient to extract all 32 bits of one `uint32` word in a single vector pass. The `vncvt` narrowing conversion truncates the 32-bit results to 16-bit accumulators without loss (values are 0 or 1). For copies beyond the first, a vector `vadd` accumulates into the destination array. This replaces $4 \times 32 \times \text{MULT}$ scalar shift-and-mask operations per symbol.

#### RM-2: `hadamard_rvv` — Seven-Pass Butterfly FHT with Strided Loads

The centrepiece of the RM vectorization is the Fast Hadamard Transform rewrite. The scalar implementation processes 64 butterfly pairs per pass using explicit `p1[2*i]` / `p1[2*i+1]` indexing — a stride-2 access pattern that maps directly to the RVV `vlse16` strided load instruction.

At VLEN=256, SEW=16, LMUL=4, the hardware vector length reaches 64 elements, covering all 64 butterfly pairs in a single vector operation:

```c
size_t vl = __riscv_vsetvl_e16m4(64);
vint16m4_t even = vlse16_v(in,     stride=4);  // p1[0,2,4,...,126]
vint16m4_t odd  = vlse16_v(in + 1, stride=4);  // p1[1,3,5,...,127]
vse16_v(out,      vadd_vv(even, odd, vl));      // sums  → out[0..63]
vse16_v(out + 64, vsub_vv(even, odd, vl));      // diffs → out[64..127]
```

The stride of 4 bytes (2 elements × 2 bytes/element) gathers every even-indexed `int16` into one vector register group and every odd-indexed element into another. Each pass thus requires exactly 2 strided loads, 2 arithmetic operations, and 2 contiguous stores — 6 vector instructions per pass, 42 vector instructions total for all 7 passes. This replaces the 896 scalar add/subtract/store sequences of the reference implementation.

The pointer-swap technique (`p1`/`p2` alternation) is preserved unchanged, ensuring the final result resides in the correct buffer after an odd number of passes.

#### RM-3: `find_peaks_rvv` — VLEN-Wide Argmax

The peak-finding function was vectorized to process the entire 128-element array in a single pass using LMUL=8 (at VLEN=256, SEW=16: vl=128):

1. **Absolute value**: Computed as `vmax(v, vrsub(v, 0))`, exploiting the fact that RM values are bounded by MULTIPLICITY (≤46) and thus `INT16_MIN` never appears.
2. **Global maximum**: A `vredmaxu` unsigned reduction across all 128 lanes yields the scalar maximum absolute value.
3. **Peak location**: A `vmseq` comparison generates a mask of lanes matching the maximum, and `vfirst_m` extracts the index of the first set bit.
4. **Sign recovery**: The sign of the peak value at the located index determines bit 7 of the decoded byte.

This approach eliminates the 128-iteration scalar scan and its associated branch-prediction pressure. However, the use of `vfirst_m` introduces a constant-time concern discussed in Section 6.

### Reed-Solomon Axis A: Vectorized Syndrome Computation

#### Algorithm-Level Structure

Each of the $2\delta$ syndromes is an independent GF(2^8) dot-product of length $N_1 - 1$:

$$S_i = \text{cdw}[0] \oplus \bigoplus_{j=1}^{N_1-1} \text{gf\_mul}(\text{cdw}[j],\; \alpha^{i \cdot j})$$

The outer loop over $i$ remains scalar (each syndrome is reduced to a single accumulator), while the inner loop over $j$ is fully vectorized.

#### ISA-Level: RVV Log/Exp Table Gather

The vectorization strategy computes GF(2^8) products using the logarithm/antilogarithm identity:

$$\text{gf\_mul}(a, b) = \begin{cases} 0 & \text{if } a = 0 \text{ or } b = 0 \\ \text{gf\_exp}[(\text{gf\_log}[a] + \text{gf\_log}[b]) \bmod 255] & \text{otherwise} \end{cases}$$

This approach uses only standard RVV instructions (no Zvbc vector carry-less multiply dependency), keeping the `rvv` decode tier compatible with hardware that implements the V extension without vector-crypto extensions.

The implementation proceeds in five vectorized steps per inner-loop iteration:

1. **Indexed gather on `gf_log[]`**: Load codeword bytes via `vle8` and zero-extend to `u16` for index computation. Compute byte offsets (`vsll_vx(..., 1)` since `gf_log` is `uint16_t[256]`) and gather via `vluxei16` to obtain `log_a[j]` and `log_b[j]` in parallel.

2. **Modular log-sum**: Add the two log vectors (`vadd_vv`). Since $\max(\log_a + \log_b) = 254 + 254 = 508 > 255$, a branchless conditional subtract reduces mod 255: compare against 255 via `vmsgeu_vx`, then `vsub_vx` under mask-undisturbed policy (`_mu` suffix) to subtract 255 only from lanes where the sum exceeds the modulus.

3. **Indexed gather on `gf_exp[]`**: Compute byte offsets from the reduced log-sum and gather `gf_exp[log_sum]` via `vluxei16`. The `gf_exp` table has 258 entries (indices 0–257), accommodating all possible reduced sums without bounds checking.

4. **Zero-masking**: Generate a combined non-zero mask via `vmsne_vx(..., 0)` on both operands and `vmand_mm`. Apply `vmerge_vvm` to force the product to zero in lanes where either operand is zero, preserving the GF multiplication identity $0 \cdot x = 0$.

5. **XOR-reduction**: Accumulate the masked products into the scalar syndrome accumulator via `vredxor_vs`.

#### Micro-Level: LMUL/SEW Parameters and Register Layout

The configuration uses SEW=16, LMUL=4, yielding vl=64 lanes at VLEN=256. Each vector pass processes 64 GF multiplications simultaneously — a 4× improvement in elements-per-pass compared to the Zvbc lane-packing approach used in Track C (which requires SEW=64 for `vclmul.vv`, limiting vl to 16). The pass counts per syndrome are:

- HQC-128: $\lceil 45/64 \rceil = 1$ pass × 30 syndromes ≈ ~30 vector passes total
- HQC-192: $\lceil 55/64 \rceil = 1$ pass × 32 syndromes ≈ ~32 vector passes total
- HQC-256: $\lceil 89/64 \rceil = 2$ passes × 58 syndromes ≈ ~116 vector passes total

The implementation is vector-length agnostic (VLA): it uses `vsetvl_e16m4` to query the hardware vector length and processes the remainder in a tail pass, ensuring correctness at any VLEN ≥ 64.

#### Trade-offs vs. Zvbc Lane-Packing (Track C)

| Aspect | RVV Log/Exp Gather (Track B) | Zvbc `vclmul.vv` (Track C) |
|--------|------------------------------|---------------------------|
| ISA requirement | RVV (V ext only) | RVV + Zvbc |
| Elements per pass (VLEN=256) | 64 (SEW=16, LMUL=4) | 16 (SEW=64, LMUL=4) |
| Memory traffic | 2 indexed gathers per iteration | None (compute-only) |
| Cache pressure | Requires gf_log (512B) + gf_exp (516B) in L1 | No table dependency |
| Instruction count per element | ~12 (gather + add + gather + mask + xor) | ~8 (clmul + reduce) |
| Hardware compatibility | Any RVV implementation | Requires vector-crypto |

The RVV gather approach trades higher per-element instruction count for wider parallelism (4× more elements per pass) and broader hardware compatibility. On implementations with efficient indexed-load units, the wider parallelism dominates; on implementations where `vluxei` is expensive (e.g., serialized gather), the Zvbc approach in Track C would be preferred.

## 5. Variant III — Track C: Zvbc Decode (`karatsuba_zvbc`)

Track C extends Track B by vectorizing the remaining RS inner loops (Axes B, C, and D) using the same Zvbc lane-packing strategy. The RM decoder and Axis A syndrome computation are identical to Track B and are not repeated here.

### RS Axis B: Berlekamp-Massey ELP Vectorization (`compute_elp_zvbc`)

#### Algorithm-Level: Outer/Inner Split

The Berlekamp-Massey algorithm executes $2\delta$ outer iterations, each sequentially dependent on the previous iteration's discrepancy value $d$ and degree tracking variables. This outer loop cannot be parallelized. However, each iteration contains three inner loops whose lengths grow from 1 to $\delta$, and these inner loops are independent within each iteration:

1. **Sigma SAXPY**: $\sigma[i] \mathrel{\oplus}= dd \cdot X_{\sigma_p}[i]$ for $i = 1, \ldots, \min(\mu+1, \delta)$
2. **X_sigma_p blend**: conditional copy based on the CT mask `mask12`
3. **Discrepancy dot-product**: $d = S_{\mu+1} \oplus \bigoplus_{i=1}^{n} \text{gf\_mul}(\sigma[i], S_{\mu+1-i})$

#### ISA-Level: Three Vectorized Inner Loops

**Loop 1 — Sigma SAXPY** uses a scalar broadcast of `dd` into all 64-bit lanes via `vmv_v_x`, followed by `vclmul.vv` against the zero-extended `X_sigma_p` elements, inline Barrett reduction, and XOR with the current `sigma` values. The result is narrowed from 64-bit lanes back to 16-bit storage via two `vnsrl` operations (u64→u32→u16).

**Loop 2 — X_sigma_p blend** operates at SEW=16, LMUL=4 (no GF multiplication required). The constant-time blend is implemented as:
```c
result = (mask12 & sigma_copy) ^ (~mask12 & X_sigma_p)
```
using `vmv_v_x` to broadcast the scalar mask and its complement, followed by `vand` and `vxor` — a branchless bitwise select pattern.

**Loop 3 — Discrepancy dot-product** loads `sigma[1..n]` contiguously and `syndromes[mu..mu+1-n]` in reverse order using `vlse16` with a negative stride of $-2$ bytes. The reversed syndrome vector and sigma vector are zero-extended to 64-bit lanes, multiplied via `vclmul.vv`, reduced, and XOR-accumulated with `vredxor.vs`.

#### Micro-Level: Pass Counts and Register Pressure

At LMUL=4, SEW=64, vl=16 (VLEN=256), the inner loop dimensions determine pass counts:

- HQC-128 ($\delta=15$): all three inner loops fit in 1 vector pass at maximum length.
- HQC-192 ($\delta=16$): 1 pass (exactly at vl boundary).
- HQC-256 ($\delta=29$): 2 passes per inner loop at maximum length.

The SAXPY and dot-product loops each consume one `u64m4` register group for operands and one for products (8 architectural vector registers per group at LMUL=4). The blend loop uses `u16m4` (4 registers per group), allowing all three loops to execute without register spills on a 32-register vector file.

### RS Axis D: Forney Error-Value Vectorization (`compute_error_values_zvbc`)

#### Algorithm-Level: Forney Formula Decomposition

The Forney formula computes error values $e_j$ for each of the (at most) $\delta$ error locations. For each location $i$, two quantities are evaluated:

$$\text{tmp1} = \bigoplus_{j=1}^{\delta} \text{inverse}^j \cdot z[j] \qquad \text{(numerator)}$$
$$\text{tmp2} = \prod_{k=1}^{\delta-1} \left(1 \oplus \text{gf\_mul}(\text{inverse}, \beta_{(i+k) \bmod \delta})\right) \qquad \text{(denominator)}$$

The sequential chain `inverse_power_j = gf_mul(inverse_power_j, inverse)` in the numerator is unavoidable (each power depends on the previous). However, once the $\delta$ powers are precomputed into a scratch array `inv_pow[1..\delta]`, the dot-product with $z[1..\delta]$ is fully vectorizable.

#### ISA-Level: Two Vectorized Structures

**Step 2 — Forney numerator dot-product**: After scalar precomputation of `inv_pow[1..DELTA]`, the implementation loads both `inv_pow` and `z[1..DELTA]` as 16-bit values, zero-extends to 64-bit lanes, applies `vclmul.vv` with inline Barrett reduction, and XOR-reduces via `vredxor.vs`. This is structurally identical to the Axis A syndrome dot-product.

**Step 3 — Inner-k inverse-beta batch**: The modular index `beta_j[(i+k) % DELTA]` for $k = 1, \ldots, \delta-1$ is decomposed into two contiguous memory segments to avoid gather operations:

- Segment A: `beta_j[i+1 .. DELTA-1]` (length $\delta - 1 - i$)
- Segment B: `beta_j[0 .. i-1]` (length $i$)

Each segment is processed with a broadcast of `inverse` via `vmv_v_x`, `vclmul.vv` against the contiguous beta values, inline reduction, and XOR with the constant 1 (`vxor_vx(..., 1)`). Results are narrowed and stored into a scratch array `inner_k[0..DELTA-2]`. The final sequential product `tmp2 = gf_mul(tmp2, inner_k[k])` over $\delta - 1$ elements remains scalar — this $O(\delta)$ chain cannot be parallelized without a prefix-product scan, which would not amortize at the small $\delta$ values encountered in HQC.

#### Micro-Level: Stack Layout and Segment Sizing

The scratch arrays `inv_pow[DELTA+1]` and `inner_k[DELTA]` reside on the stack. At $\delta = 29$ (HQC-256), these consume at most 60 bytes each — well within L1 cache residency. The two-segment decomposition ensures all memory accesses are contiguous, avoiding the performance penalty of indexed gather loads (`vluxei`) on in-order RISC-V cores.

### RS Axis C: FFT Root Finding — Iterative Zvbc-Vectorized Gao-Mateer FFT

The additive Gao-Mateer FFT in `compute_roots` evaluates the error-locator polynomial $\sigma(x)$ at all $2^M = 256$ points of GF(2^8) via recursive radix decomposition. This axis was previously deferred (experiment slot `EXP-decode-rs-fft-zvbc`) due to the engineering complexity of the recursive-to-iterative transformation. It has now been implemented as `src/decode/fft_zvbc.c`, completing the full Zvbc decode pipeline (Axes A+B+C+D).

#### Algorithm-Level: Recursive-to-Iterative Transformation

The reference `fft_rec` performs a depth-first recursive traversal of a binary tree of depth `PARAM_M` (= 8 for GF(2^8)). At each node, it: (1) beta-scales the polynomial coefficients, (2) radix-decomposes into two half-size polynomials, (3) recurses on both halves, and (4) combines results via XOR-broadcast and gammas-sums multiplication.

The iterative transformation converts this into a two-pass breadth-first traversal:

- **Pass 1 (top-down):** For each tree level from root to leaf, iterate over all segments at that level. For each segment: apply vectorized beta-scaling, then radix-decompose into two children at the next level.
- **Leaf base case:** At the deepest level (segments of size 2), apply vectorized XOR-broadcast to generate the leaf evaluation outputs.
- **Pass 2 (bottom-up):** For each tree level from leaf to root, combine children outputs using the vectorized gammas-sums butterfly.

This eliminates all recursive function calls (O(2^M) calls in the reference) and replaces them with explicit level-loops over compile-time-bounded arrays. All working memory is stack-allocated with constant sizes determined by `PARAM_M` and `PARAM_FFT`.

#### ISA-Level: Three Vectorized Helpers

**1. `beta_scale_zvbc(f, n, beta)` — Vectorized GF(2^8) Element-Wise Multiply**

Computes `f[i] = gf_mul(beta^i, f[i])` for `i = 1..n-1`. Beta powers are precomputed into a bounded stack array (`uint16_t beta_pows[1 << (PARAM_FFT-1)]`, max 16 elements at HQC-256). The vectorized multiply uses the same Zvbc lane-packing pipeline as Axes A, B, D:

```c
while (rem > 0) {
    size_t vl = __riscv_vsetvl_e64m4(rem);
    vuint16m1_t f16 = __riscv_vle16_v_u16m1(pf, vl);
    vuint64m4_t f64 = __riscv_vzext_vf4_u64m4(f16, vl);
    vuint16m1_t b16 = __riscv_vle16_v_u16m1(pb, vl);
    vuint64m4_t b64 = __riscv_vzext_vf4_u64m4(b16, vl);
    vuint64m4_t prod = __riscv_vclmul_vv_u64m4(f64, b64, vl);
    prod = gf_reduce_vec_u64m4(prod, vl);  // shared 2-pass Barrett
    // narrow u64→u32→u16 and store
    ...
}
```

Constant-time: the `beta == 1` identity case (which the reference guards with a branch) is processed unconditionally — `gf_mul(1, x) = x` is a fixed-latency vector no-op.

**2. `xor_broadcast_zvbc(dst, src, len, scalar_val)` — Contiguous XOR-Broadcast**

Computes `dst[k] = src[k] ^ scalar_val` for `k = 0..len-1` using a VLA loop:

```c
while (rem > 0) {
    size_t vl = __riscv_vsetvl_e16m4(rem);
    vuint16m4_t v = __riscv_vle16_v_u16m4(src, vl);
    vuint16m4_t r = __riscv_vxor_vx_u16m4(v, scalar_val, vl);
    __riscv_vse16_v_u16m4(dst, r, vl);
    ...
}
```

This replaces the scalar `for (k = 0; k < x; ++k) out[x+k] = out[k] ^ tmp_betas[j]` loop at each tree level. At SEW=16, LMUL=4, VLEN=256: vl=64 elements per pass.

**3. `gammas_sums_combine_zvbc(out, u, gs, v, count)` — Butterfly Combination**

Computes `out[i] = u[i] ^ gf_mul(gs[i], v[i])` for `i = 0..count-1` using the full vclmul.vv + Barrett-reduce + vxor.vv pipeline. This is the core FFT butterfly combining step, structurally identical to the Axis A/B/D GF multiply pipeline but with three independent input arrays.

A scalar-broadcast variant `gammas_sums_combine_scalar_zvbc` handles the `sc <= 3` case where `v` is a constant, using `vmv_v_x` to broadcast the scalar to all lanes.

#### Micro-Level: Memory Budget and Pass Counts

| Security Level | PARAM_FFT | Max segment size | Work array | Stack total |
|---|---|---|---|---|
| HQC-128 | 4 | 16 coefficients | 32 B | ~512 B |
| HQC-192 | 4 | 16 coefficients | 32 B | ~512 B |
| HQC-256 | 5 | 32 coefficients | 64 B | ~1024 B |

All memory is stack-allocated with compile-time-constant sizes. No heap allocation, no VLA.

At VLEN=256, SEW=64, LMUL=4 (vl=16): beta-scaling passes process 16 GF elements per vector iteration. At SEW=16, LMUL=4 (vl=64): XOR-broadcast passes process 64 elements per iteration. The small segment sizes (≤32 elements) mean most passes complete in 1–2 vector iterations.

#### Constant-Time Analysis

The iterative FFT preserves constant-time execution:
- All loop bounds depend only on compile-time constants (`PARAM_M`, `PARAM_FFT`) and the public `f_coeffs` parameter
- Beta-scaling uses element-wise `vclmul.vv` (fixed latency regardless of operand values)
- XOR-broadcast uses `vxor.vx` (fixed latency)
- Radix decomposition uses only XOR and fixed-index array copies
- No early-exit conditions or data-dependent branches
- The `betas[m-1] != 1` guard from the reference is eliminated — unconditional execution

#### Build Integration

When `HQC_DECODE_IMPL=zvbc`, the CMake build system links `src/decode/fft_zvbc.c` instead of `vendor/hqc-ref/src/common/fft.c`. The file is compiled with `-march=rv64gcv_zbc_zvbc` matching `reed_solomon_zvbc.c`. The public API (`fft()` and `fft_retrieve_error_poly()`) has identical signatures to the vendor implementation — no changes to `reed_solomon_zvbc.c` were required.

#### Benchmark Results

Isolated decode pipeline instret on Spike (full RM + RS including compute_roots/FFT):

| Level | Scalar ref | Zvbc (with FFT) | Speedup |
|-------|-----------|-----------------|---------|
| HQC-128 | 384,986 | 60,353 | 6.38× |
| HQC-192 | 562,125 | 77,114 | 7.29× |
| HQC-256 | 1,059,605 | 159,206 | 6.66× |

Isolating the Axis C contribution by differencing against the previous Zvbc baseline (same Axes A/B/D, scalar `fft_rec` for Axis C):

| Level | Zvbc (scalar FFT) | Zvbc (vectorized FFT) | Axis C Δ |
|-------|-------------------|----------------------|----------|
| HQC-128 | 88,011 | 60,353 | −31.4% |
| HQC-192 | 119,982 | 77,114 | −35.7% |
| HQC-256 | 204,354 | 159,206 | −22.1% |

KAT validation: 300/300 vectors pass (100 × HQC-128, 100 × HQC-192, 100 × HQC-256).

### Lane-Packing Applicability Across Axes

The Zvbc lane-packing scheme — one GF(2^8) element per 64-bit lane, `vclmul.vv` for carry-less multiplication, inline two-pass Barrett reduction — is applied uniformly across all three implemented RS axes (A, B, D). This uniformity simplifies the implementation: a single `gf_reduce_vec_u64m4` helper function serves all three axes, and the load/extend/multiply/reduce/accumulate pipeline is structurally identical regardless of whether the operation is a syndrome dot-product (Axis A), a SAXPY update (Axis B Loop 1), a discrepancy accumulation (Axis B Loop 3), or a Forney evaluation (Axis D). The only variation is the source of operands (contiguous vs. strided vs. broadcast) and the disposition of results (reduction vs. store-back).

## 6. Constant-Time Safety Analysis

### Scalar CT Principles in the Reference

The upstream HQC Reed-Solomon decoder achieves constant-time execution through two mechanisms:

1. **Arithmetic masking**: All conditional updates use the pattern `mask = -((uint16_t)condition >> 15)` to produce an all-ones or all-zeros mask, followed by `value ^= mask & (new_value ^ value)` or `value += mask & increment`. No data-dependent branches appear in the decode path.

2. **Volatile mask discipline**: The critical BM mask `mask12` (which gates whether the ELP degree increased) is computed through a `volatile uint16_t mask12__` intermediate. This prevents the compiler from reordering or eliminating the mask computation, ensuring the subsequent conditional updates execute in constant time regardless of optimization level.

### RVV CT-Safe Substitution Patterns

The vectorized implementations preserve constant-time semantics by using only fixed-latency vector operations:

| Scalar Pattern | RVV Equivalent | CT Safety |
|---|---|---|
| `mask & val` (arithmetic select) | `vand.vv` / `vmerge.vvm` | Fixed execution regardless of mask value |
| `a > b ? x : y` (conditional select) | `vmsgtu` + `vmerge.vxm` | Both paths always execute |
| $\sum \text{gf\_mul}(a_i, b_i)$ (dot product) | `vclmul.vv` + `vredxor.vs` | No early-exit reduction |
| `a[i] ^= dd * b[i]` (SAXPY) | `vclmul.vv` + `vxor.vv` | Element-wise, no branching |
| `gf_reduce` feedback XOR | `vsrl` + `vxor` in fixed pattern | Fixed instruction count |

All vector arithmetic operations (`vadd`, `vsub`, `vxor`, `vand`, `vclmul`, `vredxor`) execute in constant time on conforming RISC-V implementations: their latency depends only on LMUL and SEW, not on operand values.

### The `vfirst_m` Concern in `find_peaks_rvv`

The `find_peaks_rvv` function uses `vfirst_m_b2` to locate the first lane in a mask register where the absolute value equals the global maximum. On the Spike instruction-accurate simulator, `vfirst_m` retires in one cycle regardless of the position of the first set bit. However, on physical RISC-V hardware, the RISC-V Vector specification does not mandate constant-time execution for `vfirst_m` — implementations may use priority-encoder circuits whose latency varies with bit position.

**Mitigation options for physical deployment**:

1. Replace `vfirst_m` with a `viota_m` + `vredminu` pattern that computes the argmax index through a fixed-latency population-count scan, eliminating early-exit semantics entirely.
2. Use a scalar fallback loop over the 128-element result. Since $N = 128$ is a compile-time constant, the loop count is data-independent and CT-safe on out-of-order cores.
3. Accept `vfirst_m` with a hardware-specific timing validation gate, documenting the assumption that the target microarchitecture implements `vfirst_m` in constant time.

The current implementation accepts option (3) for Spike-only development, with a `TODO-CT` marker for hardware validation.

### The `volatile uint16_t mask12__` BM Discipline

In the vectorized `compute_elp_zvbc`, the outer Berlekamp-Massey loop retains the `volatile uint16_t mask12__` declaration from the reference. The scalar-resident control variables (`mask12`, `pp`, `d_p`, `deg_sigma`, `deg_sigma_p`) remain on the scalar register file and are never hoisted into vector registers across iterations. Only the three inner loops (SAXPY, blend, dot-product) are replaced with RVV paths; the outer loop's conditional mask updates execute identically to the scalar reference.

This discipline ensures that the compiler cannot speculate across the volatile barrier, and that the vector inner loops observe a consistent, already-committed mask value on each iteration. The blend loop (Loop 2) broadcasts `mask12` and `~mask12` as scalar immediates into vector registers via `vmv_v_x`, applying the bitwise select without introducing any new data-dependent control flow.

## 7. Intuition-Based Impact Ranking

The following ranking orders all decode optimizations by expected contribution to total instruction reduction, based on hotspot proximity and loop iteration counts:

### High Expected Impact

- **RM FHT vectorization (Phase 3a, RM-2)**: The hadamard transform dominates the RM decoder at all security levels. Replacing 896 scalar operations per symbol with 42 vector instructions yields the largest single-function improvement in the decode pipeline.
- **RS Axis A syndrome vectorization (Phase 3b)**: The syndrome computation is the single hottest RS loop ($2\delta \times (N_1-1)$ GF multiplications), and its fully independent structure allows near-ideal SIMD utilization.

### Medium Expected Impact

- **RS Axes B+D vectorization (Phase 3b, Track C)**: The BM inner loops and Forney evaluation collectively account for the majority of remaining RS instructions after Axis A vectorization. The improvement scales with $\delta^2$, making it most significant at HQC-256.
- **RS Axis C FFT vectorization (Phase 3b, Track C — implemented)**: The iterative Zvbc-vectorized Gao-Mateer FFT delivered −31.4% (HQC-128) to −35.7% (HQC-192) instruction reduction on the Axis C component. While the absolute contribution is smaller than Axes B+D, the recursive-to-iterative transformation plus vectorized beta-scaling and XOR-broadcast yielded significant gains by eliminating function-call overhead and parallelizing element-wise GF operations.
- **RM `expand_and_sum` vectorization (RM-1)**: Eliminates $4 \times 32 \times \text{MULT}$ scalar shifts per symbol, though the absolute instruction count is smaller than the FHT.

### Low Expected Impact

- **Zbc `gf_mul` injection (cross-track)**: Reduces per-multiplication cost by approximately 48%, but this improvement is subsumed by the vector paths in Tracks B and C where `vclmul.vv` replaces scalar `gf_mul` entirely.
- **RM `find_peaks` vectorization (RM-3)**: The 128-element scan is a small fraction of per-symbol cost; the vectorized argmax saves instructions but is not on the critical path.

## 8. Discarded Alternatives and Why They Were Archived

### (a) Hand-Optimized Scalar Assembly Keccak Backend (`HQC_KECCAK_IMPL=asm`)

A hand-optimized RISC-V assembly implementation of KeccakF1600 using Zbb instructions (`rori` for Rho rotations, `andn` for Chi step) was implemented and validated (300/300 KAT vectors pass). The design rationale targeted cycle savings via dual-issue scheduling on in-order cores. However, benchmarking on Spike ISS showed 6,159 instret per permutation — 31.6% *worse* than the reference scalar (4,680 instret). The explanation: Spike measures retired instructions, not cycles. The compiler with `-march=rv64gc_zbc_zbb` already auto-lowers `rori` and `andn`, producing a tighter instruction count than the hand-scheduled assembly (which includes extra register moves and stack spills for the 25-lane state). The assembly backend's value would only manifest on a cycle-accurate model of an in-order dual-issue core, which Spike doesn't model. The implementation remains in the codebase (`src/keccak/fips202_asm.c` + `src/keccak/keccak_f1600.S`, selectable via `HQC_KECCAK_IMPL=asm`) but is not documented in the thesis as a performance improvement.

### (b) Zvbc `vclmul.vv` Lane-Packing for Track B Syndromes

An alternative syndrome vectorization strategy was considered that would use Zvbc `vclmul.vv` carry-less multiply instructions with inline Barrett reduction, placing one GF(2^8) element per 64-bit lane. This approach eliminates table-lookup latency entirely but requires the Zvbc vector-crypto extension, which would violate the architectural separation between the `rvv` tier (V extension only) and the `zvbc` tier (V + vector-crypto). The Zvbc lane-packing approach is used in Track C where the extension is already required for Axes B, C, and D; for Track B, the RVV log/exp gather approach was preferred to maintain hardware compatibility with RVV-only implementations. The trade-off is: Zvbc processes fewer elements per pass (16 at SEW=64 vs. 64 at SEW=16) but avoids indexed-load latency.

### (c) Batched Outer-Loop BM Parallelization

A speculative approach considered parallelizing multiple BM outer iterations simultaneously by exploiting the observation that early iterations (where the inner loop length is small) have minimal inter-iteration dependencies. This was rejected because the sequential dependency on the discrepancy value $d$ and the degree-update mask `mask12` is fundamental to the algorithm's correctness — any reordering would require speculative execution with rollback, incompatible with constant-time requirements.

## 9. Benchmarking Plan for Next Phase

The following measurements are planned for the upcoming analytic pass. This section defines what should be measured per optimization without reporting numeric results.

### Per-Optimization Measurements

1. **RS Axis C FFT scaling study**: Isolate `compute_roots` instruction count across all three security levels under both scalar and a hypothetical vectorized implementation. Determine the break-even VLEN at which vectorized beta-scaling passes outperform the scalar recursive path, accounting for function-call overhead at leaf levels.

2. **Forney validation (Axis D isolation)**: Measure `compute_error_values` in isolation with controlled error patterns (1 error, $\delta/2$ errors, $\delta$ errors) to characterize the vectorization benefit as a function of actual error count versus the worst-case $\delta$ iterations.

3. **Full-pipeline projected instret targets**: Measure the complete `reed_solomon_decode` function with all currently-promoted axes (A+B+D) active, and project the residual instruction count attributable to Axis C and scalar overhead (`compute_z_poly`, `correct_errors`, function prologues). Compare against the roadmap targets of 5.7–9.0× total speedup relative to the scalar baseline.

4. **VLEN sensitivity analysis**: Repeat the full decode benchmark at VLEN=128, 256, 512, and 1024 to characterize how the lane-packing strategy scales with wider vector implementations. At VLEN=128 (vl=8 at SEW=64, LMUL=4), the HQC-256 inner loops require 4 passes instead of 2 — quantify the resulting throughput degradation.

5. **Code size audit**: Verify that the vectorized decode implementations do not exceed 2× the scalar code size, avoiding instruction-cache pressure that could offset the instruction-count reduction on area-constrained embedded cores.

6. **CT timing validation**: On physical hardware (when available), measure execution-time variance of `find_peaks_rvv` as a function of peak position to determine whether `vfirst_m` exhibits data-dependent timing on the target microarchitecture.
