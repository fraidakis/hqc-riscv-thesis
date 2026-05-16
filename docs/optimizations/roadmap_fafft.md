# Roadmap v3: Empirical FAFFT Exploration with Strict Architectural Track Mappings

> **Status:** Active  
> **Date:** 2026-05-16  
> **Supersedes:** `roadmap_karatsuba_zvbc_v2.md` (Karatsuba track)  
> **Motivation:** `karatsuba_zvbc` `vect_mul` at HQC-128 = **90,302 instret** (isolated) but accounts
> for ~49% of the full decaps trace at L256. Its $O(n^{1.585})$ scaling produces
> **399,347** instret at L256 and will reach ~1.9 M at the polynomial sizes required
> by full HQC-256. An $O(n \log n)$ algorithm is required to remain competitive at
> L192 and L256.  
> **Simulator:** Spike (`instret` = retired instruction count; ISA `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b`)

---

## Measured Baselines (Spike, isolated `vect_mul`)

| Variant | L128 | L192 | L256 |
|---------|-----:|-----:|-----:|
| `karatsuba_ref` (schoolbook leaf) | 18,527,412 | 56,315,140 | 139,115,660 |
| `karatsuba_zbc` (Track A) | 283,686 | 863,483 | 1,904,461 |
| `karatsuba_rvv` (Track B, no Zvbc) | 270,021 | 823,102 | 1,857,509 |
| `karatsuba_zvbc` (Track C) | **90,302** | **273,547** | **399,347** |

**The Karatsuba ceiling:** L256 / L128 ratio for `karatsuba_zvbc` = 4.42×, consistent
with $O(n^{1.585})$ scaling ($2^{1.585} \approx 3.0$ per doubling). An FAFFT with
$O(n \log n)$ scaling would produce a ratio near $2 \times \frac{\log(35k)}{\log(17.5k)} \approx 2.07\times$.

**Primary target:** beat `karatsuba_zbc` L128 = **283,686** and `karatsuba_zvbc` L128 = **90,302**.  
**Stretch target:** demonstrate sub-linear scaling — L256/L128 ratio < 2.5.

---

## Part 1 — The Mathematical Landscape

This section analyses the three most practically relevant FAFFT bases for GF(2)
polynomial multiplication at HQC's polynomial sizes (n ≈ 17k–35k bits, i.e.,
552–1101 words of 64 bits).

### 1.1 Cantor Basis (Lin-Chung-Han / Jang et al.)

**Core idea.** Work in GF(2^m) using Cantor's basis $\{\beta_0, \beta_1, \ldots, \beta_{m-1}\}$
where $\beta_i = \prod_{j < i}(\beta_j + \alpha_j)$ for primitive elements $\alpha_j$.
The defining property is that the evaluation map (DFT over GF(2^m)) is a *recursive
additive butterfly* — each butterfly computes

$$f_{\rm lo}(x) = f(x),\quad f_{\rm hi}(x) = f(x + \beta_i)$$

using only one GF(2^m) field multiplication and one GF(2^m) addition (= XOR).
This gives the classic Jang et al. / Gao-Mateer FAFFT with $O(n \log^2 n)$ field operations.

**Algebraic complexity.** For HQC-128 (n = 17,669), the FFT size rounds up to
$N = 2^{15} = 32768$ evaluations. The number of butterfly stages is $\log_2 N = 15$.
Each stage touches $N/2$ butterfly pairs, each requiring one GF(2^m) multiply and one XOR.
Total field multiplies: $\frac{N}{2} \cdot \log_2 N = 245,760$.

**Memory access pattern.** The Cantor butterfly at stage $s$ accesses pairs
$\{j, j + 2^s\}$ for $j = 0, 1, \ldots, N/2 - 1$ with $j \bmod 2^{s+1} < 2^s$.
This is a *stride-$2^s$* pattern — sequential at large $s$, strided at small $s$.
The working set is $N \times \lceil m/64 \rceil$ words; for m = 32 this is
$32768 \times 1$ 32-bit words = 128 KB, fitting in a 256 KB L1.

**Key trade-off.** The Cantor basis is the most memory-efficient FAFFT but
its GF(2^m) multiplier inside the butterfly is not `clmul`-friendly.
Jang et al. encode GF(2^m) elements in a *bit-sliced* tower-field
representation — each `uint32_t` carries one bit from each of 32 parallel
GF(2^m) elements. The multiply is then pure AND+XOR on 32-bit words, with
no carry-less polynomial product. This is confirmed by `EXP-fafft-zvbc-towerfield`
which achieved only 1,731,112 instret and was retired — `vclmul.vv` is
semantically incompatible with bit-sliced tower arithmetic (see archived
comment in `experiments/EXP-fafft-zvbc-towerfield/fafft_vclmul_towerfield.c`).

**HW compatibility summary:**
- Track A: ✅ Pure C / Zbc scalable (AND+XOR inner loop)
- Track B: ✅ Vectorizable XOR stages with `vxor.vv`; mul stages vectorizable as SIMD AND+XOR
- Track C: ⚠️ `Zvbc` (`vclmul.vv`) gives **no benefit** here; all gains come from base `V`

---

### 1.2 Tower Fields — Gao-Mateer (GF(2^{2^k}) Recursive Construction)

**Core idea.** Rather than Cantor's additive basis, build the extension field as
a recursive tower: $\text{GF}(2^1) \subset \text{GF}(2^2) \subset \text{GF}(2^4) \subset \cdots \subset \text{GF}(2^{2^k})$.
Each level uses the irreducible polynomial $p_k(x) = x^2 + x + c_k$ for a
pre-computed constant $c_k$. The Gao-Mateer algorithm exploits this tower
structure to express the DFT as two DFTs of half-size over the sub-field,
connected by a *linear map* (basis conversion) at each level.

**Algebraic complexity.** The recursion $T(n) = 2T(n/2) + O(n)$ resolves to
$O(n \log n)$ — strictly better asymptotically than the Cantor basis.
The constant is favourable because each basis-conversion butterfly computes
$f_{\rm hi}' = f_{\rm hi} \cdot \gamma_k$ where $\gamma_k$ is a constant element
of GF(2^{2^k}) and the multiplication in GF(2^{2^k}) factors into two GF(2^{2^{k-1}})
multiplications plus one addition via the Karatsuba/Montgomery-style trick.

**Memory access pattern.** The tower structure produces a *recursively bit-reversed*
index pattern, equivalent to a standard radix-2 DIT butterfly. At the leaf level
($k=0$), the operation reduces to XOR. At level $k=1$, GF(4) multiply is
$3 \times$ AND + $2 \times$ XOR (Mastrovito). At level $k \le 5$ (GF(2^{32})),
the multiply decomposes into a carry-less polynomial product modulo a degree-32
irreducible, which is **directly expressible as `clmul` + `clmulh` + XOR-shift reduction**.

**The key insight for hardware mapping:** For extension degrees $m \le 64$, the
GF(2^m) multiply at each tower level is a binary polynomial product of two 
$m/2$-bit operands modulo an irreducible polynomial — and this is exactly
what `clmul`/`clmulh` / `vclmul.vv` / `vclmulh.vv` compute natively.
This is the mathematical basis that makes Track C (`Zvbc`) directly applicable.

**HW compatibility summary:**
- Track A: ✅ `clmul`/`clmulh` directly replaces GF(2^m) multiply at each tower level
- Track B: ✅ XOR-only combine stages vectorize with `vxor.vv`; mul stages with SIMD `and.vv + xor.vv`
- Track C: ✅✅ `vclmul.vv` / `vclmulh.vv` directly accelerates the GF(2^m) butterfly core

**Practical constraint:** Gao-Mateer requires the transform length $N$ to be a
power of 2. HQC's polynomial degree is not a power of two (n = 17,669 for L128),
requiring either zero-padding (N = 32768 → ~86% overhead) or a CRT split
(recommended; see §1.3).

---

### 1.3 CRT-Based Approach (Chinese Remainder Theorem Decomposition)

**Core idea.** Decompose the product ring modulo HQC's reduction polynomial
$x^n + x^k + 1$ (or the weight-2 equivalent) using CRT: find coprime polynomials
$m_1(x), m_2(x)$ such that $m_1 m_2 \equiv x^n + x^k + 1 \pmod{2}$ and
compute the two residue products separately, then reconstruct via CRT lifting.

**The Jang et al. CRT split (confirmed effective in this codebase):**
- Split at $X = x^{2^{14}} = x^{16384}$ (exactly 512 × 32-bit words).
- $C_0$ (low × low, 512-word) and $C_2$ (tail × tail, 41-word) computed by
  the inner FAFFT/Karatsuba kernel.
- The middle product $C_1$ exploits cancellation at the split boundary, saving
  the upper $2^{14} - 2r$ bits of XOR work.
- $\mathcal{P}_{15}(x)$ sparse modulus: 501 non-zero terms → sparse-dense multiply.

**Algebraic complexity.** The primary benefit is eliminating padding overhead.
Without CRT, an FAFFT for n = 17,669 must pad to N = 32768 (ratio 1.85×).
With CRT split at 16384, both sub-problems are exactly power-of-two sized.
The tail problem (41 words) is small enough for a Karatsuba/schoolbook kernel.

**Memory access pattern.** CRT introduces two sequential passes (split → transform
→ ICRT), each with streaming access. No stride-$2^s$ scatter; the critical path
is two forward FFTs of half-size plus one inverse FFT of the same size.

**HW compatibility summary:**
- Track A: ✅ CRT split is pure scalar arithmetic (XOR + masking)
- Track B: ✅ Split and combine stages are vectorizable as bulk XOR with `vxor.vv`
- Track C: ✅ Same as Track B; `Zvbc` benefits apply in the inner kernel

**Note:** CRT is an *orthogonal* optimization that can be layered on top of any
of the three FFT variants above. The recommended strategy is:
1. Use CRT to eliminate padding (mandatory for all variants at HQC sizes).
2. Use Tower-Field construction for the inner kernel (enables `clmul` / `vclmul`).
3. Use Cantor-basis bit-slicing only as a fallback Track B path.

---

## Part 2 — Hardware Track Mappings

### Track Definitions (Strict — No Exceptions)

| Track | Allowed ISA Extensions |
|-------|----------------------|
| **A (Scalar/Zbc)** | Standard C, `Zbb`, `Zbkb`, `Zbc` (`clmul`, `clmulh`). No vector instructions. |
| **B (Base RVV)** | Base `V` (RVV 1.0). **`Zvbb` and `Zvbc` are strictly forbidden.** |
| **C (Vector Crypto)** | `V` + `Zvbb` + `Zvbc`. All Track B operations allowed. |

---

### 2.1 Cantor Basis Mapping

#### Track A (Scalar/Zbc)
The bit-sliced Cantor multiply (AND+XOR over 32-bit lanes) maps cleanly to
scalar C without `clmul`. The critical loop is:

```c
// GF(2^32) Cantor-basis multiply (32-way bit-slice over uint32_t words)
// a[0..31], b[0..31] — 32 × uint32_t, each bit-position is one GF(2^32) element
// Uses only: AND, XOR — fully scalar, no clmul
for (int i = 0; i < 32; i++)
    for (int j = 0; j < 32; j++)
        tmp[i+j] ^= (a[i] & b[j]);  // schoolbook over GF(2) slices
// Reduce tmp[0..62] mod irreducible poly of degree 32 — XOR-only
```

`Zbb` (`andn`) can replace `~a & b` in the twiddle-factor negation step.
`Zbkb` provides no benefit here.
`clmul` / `clmulh`: **not applicable** (operands are bit-sliced, not binary poly coefficients).

**Estimated instruction density:** Dominated by the 32×32 schoolbook over bit
lanes. A fully unrolled inner loop produces 1024 AND + 1024 XOR per butterfly
step = 2048 instructions per GF(2^32) multiply. With 245,760 butterflies for
N = 32768, this is ~503 M operations — substantially worse than Karatsuba.
**Verdict: Cantor + Track A is not competitive at HQC sizes.**

#### Track B (Base RVV)
The bit-sliced representation is a natural fit for SIMD: pack each of the 32
GF(2^32) elements' bits into a vector register. With VLEN=256 and e32:

```asm
# vect_and_xor butterfly stage: 256 GF(2^32) elements per vl iteration
vsetvli t0, count, e32, m8, ta, ma
vle32.v   v0, (a_ptr)          # load 8*8=64 u32 words = 256 sliced elements
vle32.v   v8, (b_ptr)
vand.vv   v16, v0, v8          # AND step
vxor.vv   v24, v0, v8          # XOR step (addition in GF(2^32))
```

LMUL=8 allows processing 256 GF(2^32) elements per instruction. The 245,760
butterfly operations decompose into ~960 vector AND+XOR pairs — a theoretical
256× improvement over scalar.
**`Zvbb` and `Zvbc` are NOT used here; Track B constraint holds.**

#### Track C (Vector Crypto)
`vclmul.vv` is **semantically incompatible** with Cantor bit-sliced arithmetic
(confirmed by `EXP-fafft-zvbc-towerfield`). Track C gains over Track B here are
limited to `Zvbb` bit manipulation (`vandn.vv` replacing `vnot.vv + vand.vv`).
**Verdict: Cantor basis does not benefit from Track C. Use Tower-Field instead.**

---

### 2.2 Tower-Field (Gao-Mateer) Mapping

#### Track A (Scalar/Zbc)
Each butterfly at tower level $k$ computes a multiplication in GF(2^{2^k}).
For $k=5$ (GF(2^{32})), the multiply is a degree-31 × degree-31 binary
polynomial product modulo an irreducible polynomial of degree 32:

```c
// GF(2^32) multiply via clmul: a, b are uint32_t encoded as binary polynomials
static inline uint64_t gf32_mul_zbc(uint32_t a, uint32_t b) {
    uint64_t lo = __riscv_clmul((uint64_t)a, (uint64_t)b);   // degree ≤ 62 product
    // Barrett reduction mod irreducible(x) of degree 32 (fixed XOR pattern):
    uint64_t hi = (lo >> 32);
    // e.g., for irred = x^32 + x^7 + x^3 + x^2 + 1 (if chosen):
    lo ^= (hi << 7) ^ (hi << 3) ^ (hi << 2) ^ hi;
    return (uint32_t)lo;
}
```

`clmul`/`clmulh`: **directly computes the core butterfly operation**.
This is the primary performance lever for Track A Gao-Mateer FAFFT.

#### Track B (Base RVV)
The XOR-only combine stages (no field multiply) vectorize trivially:

```asm
# Forward FFT combine: dst[i] ^= src[i] (XOR-only stages)
vsetvli t0, n, e64, m4, ta, ma
vle64.v v0, (src)
vle64.v v4, (dst)
vxor.vv v8, v0, v4
vse64.v v8, (dst)
```

The butterfly multiplication stages require SIMD GF(2^32) multiply without `vclmul.vv`.
The best Track B approach packs multiple 32-bit GF elements into a vector:

```asm
# GF(2^32) school-book in vector registers (Mastrovito, Track B)
# a, b in e32 registers; result by XOR-accumulation of AND-shifted sub-products
# No vclmul — uses vand.vv + vxor.vv only
```

**Throughput estimate:** LMUL=4, VLEN=256 processes 32 GF(2^32) elements
per iteration of the butterfly. Multiply stages: ~7,680 vector AND+XOR pairs.
XOR-only stages: ~7,680 vector XOR pairs. Total vector instructions: ~23,040.

#### Track C (Vector Crypto)
`vclmul.vv` / `vclmulh.vv` map **directly** to the GF(2^{2^k}) butterfly multiply.
Each `vclmul.vv` with LMUL=4 and VLEN=256 processes 16 simultaneous GF(2^32)
multiplications in a single instruction:

```asm
# GF(2^32) butterfly multiply: a[i] * twiddle[i] in GF(2^32), 16 elements/inst
vsetvli t0, n, e64, m4, ta, ma
vle64.v  v0,  (a_ptr)          # load 16 × (u32,u32) pairs as e64
vle64.v  v4,  (twiddle_ptr)    # twiddle factors (pre-computed)
vclmul.vv  v8,  v0, v4         # 16 × clmul (lower half of product)
vclmulh.vv v12, v0, v4         # 16 × clmulh (upper half of product)
# Barrett reduction: vxor + vsrl + vand to reduce mod irreducible
```

This directly eliminates the scalar `clmul`/`clmulh` bottleneck identified in
`karatsuba_zvbc` profiling (4.86% of all instret from scalar leaf).
**Track C Gao-Mateer is the primary FAFFT candidate.**

---

### 2.3 CRT Decomposition Mapping (Orthogonal Layer)

CRT is applied as a *wrapper* around whichever inner FFT kernel is chosen.

#### Track A
The CRT split at $x^{16384}$ requires:
- `memcpy` / word-copy for low/high halves (standard scalar)
- Sparse-dense multiply for $\mathcal{P}_{15}(x)$ (501 non-zero terms): use `clmul` for
  aligned block shifts — one `clmul` per 64-bit word of the dense operand per
  shift-group, with XOR accumulation into the result.

#### Track B
- `vle64.v` + `vxor.vv` for the combine/split XOR layers
- The sparse-dense multiply is amenable to gather-based acceleration (`vloxei64.v`)
  for non-contiguous index access

#### Track C
- Same as Track B; `vclmul.vv` can accelerate the sparse-dense inner loop if
  operands are padded to 64-bit granularity.

---

## Part 3 — The Goal

### Primary Benchmark Targets

| Target | Metric | Goal | Rationale |
|--------|--------|------|-----------|
| **F-primary** | vect_mul L128 instret | < 90,302 (beat `karatsuba_zvbc`) | Direct improvement over current champion |
| **F-stretch** | vect_mul L128 instret | < 50,000 | ~2× improvement over `karatsuba_zvbc` |
| **F-scale** | L256 / L128 ratio | < 2.5× | Demonstrate sub-Karatsuba scaling |
| **F-l256** | vect_mul L256 instret | < 200,000 | Transform the L256 bottleneck |
| **F-decaps** | decaps L128 instret | < 700,000 | System-level impact (currently 953,889) |

### KAT Requirement
All targets must produce 100/100 NIST KAT passes across HQC-128, HQC-192, and HQC-256
before any benchmark numbers are reported.

---

## Part 4 — Target F-1: The Scalar Prototypes

**Goal:** Establish correctness baselines and empirically race all promising FAFFT
variants under Track A constraints. All implementations live in separate `experiments/`
directories per the workflow protocol.

### F-1a: Tower-Field Gao-Mateer + CRT (Primary Candidate)

**Experiment:** `EXP-fafft-tower-gm-zbc`

**Implementation steps:**
1. Create `experiments/EXP-fafft-tower-gm-zbc/` and register in `experiments/manifest.yaml`.
2. Choose the irreducible polynomial for GF(2^{32}): recommend $x^{32} + x^7 + x^3 + x^2 + 1$
   (pentanomial, low XOR-shift reduction cost).
3. Implement `gf32_mul_zbc(uint32_t a, uint32_t b)` using `clmul`/`clmulh` + Barrett
   reduction (4 XOR + 2 SRL scalar instructions post-multiply).
4. Implement the Gao-Mateer forward and inverse FFT over GF(2^{32})^N:
   - Forward: bit-reversal permutation + $\log_2 N$ butterfly stages.
   - Inverse: conjugate butterfly + normalization (division by N over GF(2^{32})).
   - CRT split at x^{16384}: low half (512 words) + tail (41 words, Karatsuba fallback).
5. Implement `vect_mul_gm_zbc`: CRT split → forward FFT × 2 → pointwise GF multiply
   → inverse FFT → CRT reconstruct → reduce mod HQC irreducible.
6. Run KAT: `./scripts/test/run_kat.sh --mul fafft_gm_zbc --level 128`.
7. Benchmark: `./scripts/report/run_full_analysis.sh --mul fafft_gm_zbc --level 128`.
8. Record `vect_mul` instret in `analysis/benchmarks/isolated/`.

**Acceptance criterion:** KAT 100/100 + instret < 283,686 (beat Track A Karatsuba baseline).

---

### F-1b: Cantor-Basis Bit-Sliced FAFFT (Reference Comparison)

**Experiment:** `EXP-fafft-cantor-bitslice-zbc`

**Implementation steps:**
1. Create `experiments/EXP-fafft-cantor-bitslice-zbc/` and register in `experiments/manifest.yaml`.
2. Implement GF(2^{32}) bit-sliced multiply using 32-lane SIMD over `uint32_t` arrays:
   - 32-word input arrays (one bit per lane per coefficient).
   - Inner loop: scalar AND+XOR schoolbook, 32×32 terms.
   - Reduce modulo degree-32 irreducible via XOR-shift pattern.
3. Implement Jang et al. Cantor-basis additive FFT:
   - Twiddle factors pre-computed into a flat table indexed by (stage, position).
   - Butterfly: `f_hi ^= gf32_mul(f_lo, twiddle)`.
4. Wire into `vect_mul` with CRT split as in F-1a.
5. Run KAT. Benchmark. Record instret.

**Acceptance criterion:** KAT 100/100. Benchmark serves as reference point; this
variant is *not* expected to beat F-1a on Track A because `clmul` cannot accelerate
bit-sliced AND+XOR. If F-1b > F-1a, retire Cantor and invest Track B/C effort
only in Gao-Mateer.

---

### F-1c: CRT-Only Schoolbook (Upper Bound Baseline)

**Experiment:** `EXP-fafft-crt-schoolbook-zbc`

**Implementation steps:**
1. Implement the minimal CRT split + dense schoolbook with `clmul`:
   - Low 512-word product: `karatsuba_zbc` (already exists in `src/poly_mul/scalar/karatsuba_zbc/`).
   - Tail 41-word product: direct `clmul`-based schoolbook (41×41 = 1,681 multiplies).
   - CRT reconstruct: sparse-dense multiply for $\mathcal{P}_{15}(x)$.
2. This variant has $O(n^2)$ scaling but eliminates the padding overhead.
3. Run KAT. Benchmark. The result establishes whether CRT alone (without FFT) is
   sufficient to beat the current `karatsuba_zvbc` L128 = 90,302 baseline.
4. If this beats 90,302 — document the finding and evaluate CRT schoolbook as
   the Track A champion before investing in full FAFFT.

---

### F-1 Race Decision

After running all three F-1 experiments:

| Winner criterion | Action |
|-----------------|--------|
| F-1a (Tower+CRT) wins AND beats `karatsuba_zvbc` | Proceed to F-2/F-3 with Gao-Mateer kernel |
| F-1b (Cantor) wins | Proceed to F-2 with Cantor bitslice (Track B only) |
| F-1c (CRT schoolbook) wins | Record as new Track A champion; F-2/F-3 still pursue FFT for scale |
| None beats `karatsuba_zvbc` on Track A | Accept Track A as saturated; focus F-2/F-3 on Track C gains |

---

## Part 5 — Target F-2: Base RVV Acceleration (Track B)

**Goal:** Vectorize the winning FAFFT variant from F-1 using strictly base `V`.
All `Zvbb` and `Zvbc` instructions are forbidden.

**Experiment prefix:** `EXP-fafft-<variant>-rvv`

### F-2a: Vector XOR-Combine Stages

The XOR-only combine layers of any FAFFT are the easiest and most impactful
vectorization target:

```c
// Track B: vectorized XOR butterfly combine (no field multiply)
// Uses: vsetvli, vle64.v, vxor.vv, vse64.v — all base V
void fafft_xor_combine_rvv(uint64_t *a, const uint64_t *b, size_t words) {
    size_t vl;
    for (size_t i = 0; i < words; i += vl) {
        vl = __riscv_vsetvl_e64m4(words - i);
        vuint64m4_t va = __riscv_vle64_v_u64m4(a + i, vl);
        vuint64m4_t vb = __riscv_vle64_v_u64m4(b + i, vl);
        va = __riscv_vxor_vv_u64m4(va, vb, vl);
        __riscv_vse64_v_u64m4(a + i, va, vl);
    }
}
```

Apply this to all XOR-only butterfly stages (approximately half the total stages
in a Gao-Mateer FFT have no multiplication at the lowest extension levels).

### F-2b: Vectorized GF(2^{2^k}) Multiply (Track B, No vclmul)

For extension degrees $k \le 4$ (GF(2^{16}) and below), the Mastrovito schoolbook
is feasible with base vector AND+XOR:

```asm
# GF(2^16) Mastrovito multiply: 3 AND + 2 XOR per element, LMUL=4 → 64 elems/inst
vsetvli t0, n, e16, m4, ta, ma
vand.vv v8, v0, v4          # sub-product A0 × B0
vand.vv v12, v0, v5         # sub-product A0 × B1  (shifted into v5)
vxor.vv v8, v8, v12         # accumulate
# ... Karatsuba-over-GF(2) unrolled for fixed degree
```

For GF(2^{32}), Track B falls back to scalar `clmul` (Track A code inlined into
the vector loop) while using vector instructions for the XOR-combine outer structure.

### F-2c: Bit-Reversal Permutation Vectorization

The DIT bit-reversal permutation required by Gao-Mateer can be partially
vectorized using indexed loads:

```asm
# Bit-reversed scatter/gather using vloxei32.v / vsoxei32.v
vsetvli t0, n, e32, m4, ta, ma
vloxei32.v v8, (base), v_br_idx   # gather from bit-reversed index table
```

Pre-compute the bit-reversal index table once at startup (or as a compile-time
constant for fixed N). This converts the strided permutation into a sequential
gather, improving prefetcher utilization.

### F-2 Benchmarking Protocol

For each F-2 sub-experiment:
1. Run KAT: `./scripts/test/run_kat.sh --mul <variant> --level 128`.
2. Run KAT at all three levels: `--level 192`, `--level 256`.
3. Run isolated benchmark: `./scripts/report/run_full_analysis.sh --mul <variant> --level 128`.
4. Record L128/L192/L256 isolated `vect_mul` instret in `analysis/benchmarks/isolated/`.
5. Record L128 full decaps instret to verify system-level impact.

**Track B acceptance target:** beat `karatsuba_rvv` L128 = **270,021** instret.

---

## Part 6 — Target F-3: Vector Crypto Acceleration (Track C)

**Goal:** Apply `Zvbc` (`vclmul.vv`, `vclmulh.vv`) and `Zvbb` to the winning
FAFFT variant, achieving maximum throughput on Track C hardware.

**Experiment prefix:** `EXP-fafft-<variant>-zvbc`

### F-3a: `vclmul.vv` Butterfly Core

Replace the scalar GF(2^{32}) multiply in the Gao-Mateer butterfly with
`vclmul.vv` + `vclmulh.vv`:

```c
// Track C: vectorized GF(2^32) butterfly multiply using vclmul.vv
// a[i], twiddle[i] are uint32_t encoded as binary polynomials over GF(2)
// NOTE: operands must be zero-extended to 64-bit before vclmul to prevent
//       aliasing between hi/lo halves (confirmed lesson from karatsuba_zvbc)
void gf32_butterfly_zvbc(uint32_t *a, const uint32_t *twiddle, size_t n) {
    size_t vl;
    for (size_t i = 0; i < n; i += vl) {
        vl = __riscv_vsetvl_e64m4(n - i);
        // Zero-extend u32 operands to e64 before clmul
        vuint64m4_t va = __riscv_vzext_vf2_u64m4(
            __riscv_vle32_v_u32m2(a + i, vl), vl);
        vuint64m4_t vt = __riscv_vzext_vf2_u64m4(
            __riscv_vle32_v_u32m2(twiddle + i, vl), vl);
        vuint64m4_t vlo = __riscv_vclmul_vv_u64m4(va, vt, vl);
        vuint64m4_t vhi = __riscv_vclmulh_vv_u64m4(va, vt, vl);
        // Barrett reduction mod degree-32 irreducible (fixed bit pattern):
        // result = lower 32 bits of (vlo ^ (vhi reduction))
        vuint64m4_t vred = __riscv_vsrl_vx_u64m4(vlo, 32, vl);  // upper word of lo
        vred = __riscv_vxor_vv_u64m4(vred, vhi, vl);             // XOR with hi
        // Apply polynomial reduction: fixed XOR-shift for chosen irreducible
        // ... (2-3 more vsrl + vxor instructions depending on irreducible)
        vuint64m4_t vresult = __riscv_vxor_vv_u64m4(
            __riscv_vand_vx_u64m4(vlo, 0xFFFFFFFF, vl), /* ... reduction */ vred, vl);
        __riscv_vse32_v_u32m2(a + i, __riscv_vncvt_x_x_w_u32m2(vresult, vl), vl);
    }
}
```

**Expected gain:** `vclmul.vv` with LMUL=4, VLEN=256 processes 16 GF(2^32)
multiplications per pair of instructions vs. 1 per scalar `clmul`+`clmulh`.
Theoretical throughput: 16× over Track A scalar, ~8× over Track B SIMD AND+XOR.

### F-3b: `Zvbb` Bit-Manipulation Acceleration

`Zvbb` additions:
- `vandn.vv`: replaces `vnot.vv + vand.vv` in GF complement operations (2→1 instruction)
- `vbrev8.v` / `vrev8.v`: bit/byte reversal for the butterfly permutation stage
  (replaces the scalar bit-reversal precomputation if implemented in-register)
- `vclz.v` / `vctz.v`: counting leading/trailing zeros for degree detection in
  Barrett reduction (minor optimization; reduction is typically hardcoded)

The most impactful Zvbb contribution is `vbrev8.v` for in-register bit-reversal,
which eliminates the index table used in F-2c's gather approach.

### F-3c: Twiddle Factor Pre-computation and Caching

All FAFFT twiddle factors are **public constants** (they depend only on the field
structure, not on the secret polynomial). Pre-compute them once at build time
and store as `static const uint32_t twiddle_table[N_STAGES][N/2]`.

For HQC-128 with N=32768, the table is:
- 15 stages × 16384 twiddle factors × 4 bytes = 960 KB (too large for L1)
- Stage-by-stage: only the current stage's 16384 entries = 64 KB per stage.

**Recommendation:** Store twiddle factors in transposed (stage-first) layout to
maximize cache-line utilization. Pre-group twiddle factors for stages 0-5
(small half-sizes) into a compact 2 KB table that fits in L1 permanently.

### F-3 Benchmarking Protocol

Same as F-2 with addition:
- Profile instruction mix histogram to verify `vclmul.vv` dominates inner loop.
- Compare L128/L192/L256 scaling ratios to confirm $O(n \log n)$ behaviour.
- Run constant-time verification: `tests/ct/dudect/` if FAFFT twiddle-factor
  lookups are data-dependent (they are not — twiddles are public — but verify).

**Track C acceptance target:** beat `karatsuba_zvbc` L128 = **90,302** instret.  
**Track C stretch target:** < 50,000 instret L128, with L256/L128 < 2.5×.

---

## Appendix A — Experiment Registration Template

When creating any F-1/F-2/F-3 experiment, add to `experiments/manifest.yaml`:

```yaml
- id: EXP-fafft-<variant>-<track>
  status: in-progress
  description: "<description>"
  author: fraidakisg
  started: 2026-05-16
  target_levels: [HQC-128, HQC-192, HQC-256]
```

---

## Appendix B — Promotion Criteria

An FAFFT variant is eligible for promotion to `src/poly_mul/` when:
1. KAT 100/100 passes at HQC-128, HQC-192, HQC-256.
2. `vect_mul` instret beats the current track champion at all three levels.
3. L256/L128 instret ratio < 3.0 (demonstrating better-than-Karatsuba scaling).
4. No constant-time violations detected via `dudect` / `ctgrind`.
5. Code reviewed and CMakeLists.txt integration tested.

Upon promotion:
- Move to `src/poly_mul/scalar/<variant>/` or `src/poly_mul/vector/<variant>/`.
- Mark experiment as `promoted_to_stable` in `manifest.yaml`.
- Update `VARIANT_INDEX.md`.
- Commit with `feat(poly_mul): promote <variant> as new champion`.

---

## Appendix C — Projected Scaling Table

Estimated `vect_mul` instret under $O(n \log n)$ FAFFT if the constant matches
the Tower-Field Gao-Mateer + Zvbc analysis (normalized to L128 target of 60,000):

| Level | n (bits) | $n \log_2 n$ | Projected (normalized) |
|-------|----------|-------------|------------------------|
| L128 | 17,669 | 246,601 | ~60,000 |
| L192 | 35,851 | 527,218 | ~128,000 |
| L256 | 57,637 | 886,948 | ~215,000 |

Compare against `karatsuba_zvbc` measured: 90,302 / 273,547 / 399,347.
The $O(n \log n)$ projection beats Karatsuba at L192 (~53% reduction) and
L256 (~46% reduction), confirming the strategic pivot is justified.
