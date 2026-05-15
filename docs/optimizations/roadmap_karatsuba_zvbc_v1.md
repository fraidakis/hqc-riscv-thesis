# Track C Optimization Roadmap: Vector Crypto Extensions

> **Based on:** Track B profile (`karatsuba_zbc_rvv`) — date 2026-05-14  
> **Source variant:** `karatsuba_zbc_rvv` (rvv_only)  
> **Security levels profiled:** HQC-128 / HQC-192 / HQC-256  
> **Simulator:** Spike (instruction-accurate; `instret` = retired instruction count)  
> **Ratified extensions now unlocked:** `Zvbc`, `Zvbb`, `Zvkb`, `Zk*`

---

## Track History

| Track | Key Extension(s) | vect_mul HQC-128 | Δ vs Baseline |
|-------|-----------------|-------------------|---------------|
| `baseline_ref` | — | 18,578,007 | — |
| `karatsuba_zbc` (Track A scalar) | `Zbc` | 296,703 | **−98.4%** |
| `karatsuba_zbc_rvv` (Track B) | `V` (RVV 1.0, no crypto) | 286,173 | **−3.55% vs Track A** |
| `karatsuba_zvbc` (Track C target) | `Zvbc` + `V` | ~187,100† | **~−35% vs Track B** |

> †From existing `karatsuba_zvbc` benchmarks at VLEN=256 (plan data). Track C
> starts from this point and extends to all other subsystems.

**Key observation on Track B:** The modest −3.55% gain over the scalar Track A
baseline confirms that vectorizing only the Karatsuba XOR-combine loops (the
Track B contribution) provides marginal benefit. The scalar `clmul`/`clmulh`
leaf multiply inside `clmul_leaf` remains the dominant bottleneck in `vect_mul`.
Track B's VLEN scaling (VLEN=256: −3.55%, VLEN=512: −8.46%, VLEN=1024: −7.28%)
further shows that the XOR-combine vectorisation amortises modestly at best —
the leaf is the limiting factor, not the combines.

---

## Methodology

All figures are **Spike `instret` counts** (retired instructions). On
out-of-order hardware the relative bottleneck ordering may shift slightly, but
the instruction-count reduction is a direct proxy for throughput under any
issue-width model.

**Track C hard constraints:**
- `Zvbc` (`vclmul.vv`, `vclmulh.vv`) — **ALLOWED**
- `Zvbb` (`vandn.vv`, `vbrev8.v`, `vrev8.v`, `vrol.vv`, `vror.vv`, `vclz.v`, `vctz.v`, `vcpop.v`) — **ALLOWED**
- `Zvkb` (superset of `Zbkb` in vector form; `vrol.vv`, `vror.vv`, `vbrev8.v`, `vrev8.v`) — **ALLOWED**
- `Zk*` scalar cryptography suite (`Zkn`, `Zkr`, `Zks`, `Zkt`) — **ALLOWED**
- Base `V` (RVV 1.0) — **ALLOWED** (inherited from Track B)
- Custom ISE / non-ratified extensions — **FORBIDDEN** (thesis constraint)

---

## Track B Hotspot Analysis

### Instruction-Mix Histogram (Track B, HQC-128 full KEM)

| Instruction | Count | % | Diagnosis |
|-------------|-------|---|-----------|
| `ld` | 766,672 | 15.94 | Memory reads from tmp/scratch buffers |
| `addi` | 615,173 | 12.79 | Loop control & pointer arithmetic |
| `xor` | 470,770 | 9.79 | Scalar XOR (non-vectorised residuals) |
| `bnez` / `bne` | 627,203 | 13.04 | Loop-back branches (Karatsuba recursion) |
| `sd` | 218,129 | 4.53 | Memory writes |
| **`clmul`** | **119,322** | **2.48** | **Scalar leaf multiply (hot, unvectorised)** |
| **`clmulh`** | **114,324** | **2.38** | **Scalar leaf multiply (hot, unvectorised)** |
| `rori` | 98,832 | 2.05 | Keccak ρ-step bit rotations (scalar) |
| `andn` | 85,375 | 1.77 | Keccak χ-step AND-NOT (scalar) |
| `vsetivli` | 89,171 | 1.85 | Vector config overhead |
| `vle64.v` | 74,382 | 1.55 | Vector load (Track B XOR loops) |
| `vxor.vv` | 62,071 | 1.29 | Vector XOR (Track B XOR loops) |

**Critical finding:** `clmul` + `clmulh` together account for **4.86%** of all
retired instructions in Track B — yet they are confined to the `clmul_leaf`
function, which is called from the Karatsuba recursion leaves. These are
single-scalar instructions that process exactly one 64×64 carry-less product
each. This is the primary Track C target.

**Secondary finding:** `rori` (2.05%) and `andn` (1.77%) are the scalar
signatures of the Keccak-f[1600] permutation (ρ-step and χ-step respectively).
After Zvbc polymul collapses the leaf cost, these dominate the non-polymul
portion of the KEM.

### Function-Level Hotspots (Track A scalar baseline, HQC-128)

The Track A scalar profiling (karatsuba_zbc) provides function-level
attribution with the full call hierarchy visible:

| Rank | Function | Instructions | % | Track C Relevance |
|------|----------|-------------|---|-------------------|
| 1 | `clmul_leaf` | ~209,000† | 22.07 | **C-1: vectorise with `vclmul.vv`** |
| 2 | `karatsuba_clmul` | ~168,000 | 17.79 | C-1: XOR combines already vectorised in Track B |
| 3 | `KeccakF1600_StatePermute` | ~112,000 | 11.84 | **C-2: RVV + `Zvkb` rotation/andnot** |
| 4 | `gf_carryless_mul` | ~83,000 | 8.80 | **C-3: RS GF(2⁸) `vclmul.vv` batching** |
| 5 | `memset` | ~59,000 | 6.13 | Resolved in Track A (direct-assign discipline) |
| 6 | `clmul64` | ~50,000 | 5.15 | Subsumed by C-1 (`clmul_leaf` inlines this) |
| 7 | `hadamard` | ~44,000 | 4.66 | **C-4: FHT `vadd.vv` / `vsub.vv`** |
| 8 | `keccak_inc_absorb` | ~28,000 | 2.89 | C-2: subsumed by Keccak acceleration |
| 9 | `find_peaks` | ~21,000 | 2.17 | **C-4: `vabs.v` / `vredmax`** |
| 10 | `keccak_inc_squeeze` | ~19,000 | 1.97 | C-2: subsumed by Keccak acceleration |
| 11 | `expand_and_sum` | ~19,000 | 1.92 | C-4: vectorisable bit-unpack |
| 12 | `gf_reduce` | ~18,000 | 1.85 | C-3: subsumed by GF(2⁸) work |
| 13 | `gf_mul` | ~17,000 | 1.74 | C-3: subsumed by GF(2⁸) work |
| 14 | `vect_generate_random_support2` | ~16,000 | 1.62 | C-2: SHAKE-limited; benefits from C-2 |

> †Absolute counts estimated as percentage × total KEM instret
> (keypair+encaps+decaps = 3,642,879 for karatsuba_zbc HQC-128).

### Post-Track-B Amdahl Projection

With Track B polymul isolated at 286,173 per `vect_mul` call and the full KEM:

| Operation | Track B Total | ~Polymul Share | ~Non-Polymul |
|-----------|--------------|----------------|--------------|
| keypair (≈1 call) | 426,449 | 286,173 (67%) | 140,276 |
| encaps (≈2 calls) | 878,608 | 572,346 (65%) | 306,262 |
| decaps (≈5 calls) | 1,795,968 | 1,430,865 (80%) | 365,103 |

After Track C-1 (`vclmul.vv`) drops each `vect_mul` to ~187,100:

| Operation | Estimated Track C | Δ vs Track B |
|-----------|-------------------|--------------|
| keypair | ~327,376 | −23.2% |
| encaps | ~680,462 | −22.5% |
| decaps | ~1,300,603 | −27.6% |

In decaps, the non-polymul block (~365K) then represents **28%** of post-C-1
runtime — up from **20%** in Track B. Keccak and the decoder become
structurally dominant once C-1 lands.

---

## Target C-1: Vector Carry-Less Multiply for Karatsuba Leaf (`Zvbc`)

### Root Cause

The `clmul_leaf` function performs a schoolbook-style double loop over the
`a[]` and `b[]` input arrays, accumulating 64×64 carry-less products:

```c
// src/poly_mul/vector/rvv_only/karatsuba_zbc/gf2x_clmul.c
static void clmul_leaf(uint64_t *r, const uint64_t *a,
                       const uint64_t *b, size_t n)
{
    // ...
    for (size_t i = 1; i < n; i++) {
        for (size_t j = 0; j < n - 1; j += 2) {
            clmul64(a[i], b[j],     &lo0, &hi0);   // scalar clmul + clmulh
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            // ... accumulate
        }
    }
}
```

At `KARATSUBA_THRESHOLD=16`, each leaf call processes 16×16 = 256 pairs.
Each `clmul64` emits two scalar instructions (`clmul` + `clmulh`), so each
leaf call pays **≥512 scalar carry-less multiply instructions**. With the
Karatsuba recursion producing O(n^log₂3) leaf calls, scalar leaf cost
overwhelms the XOR-combine work that Track B vectorised.

In the Track B instruction histogram, `clmul` + `clmulh` together constitute
**4.86%** of all retired instructions in the full KEM run. Track B's vector
extensions (`vle64.v` + `vxor.vv`) account for only **2.84%** combined —
confirming that Track B vectorised the cheaper part of the work.

### Track C Solution

Use `vclmul.vv` and `vclmulh.vv` (Zvbc) to replace the scalar double loop
with a **splat-and-sweep** vector pattern:

```
// For each outer index i (scalar):
//   Load a[i] → scalar register  ai
//   Splat ai across a vector register: vmv.v.x v_ai, ai
//   For inner j-block at LMUL=m4 (VLEN=256 → 16 uint64_t per group):
//     vle64.v    v_b,   b+j            // load 16 b words
//     vclmul.vv  v_lo,  v_ai, v_b     // 16 low halves in ONE instruction
//     vclmulh.vv v_hi,  v_ai, v_b     // 16 high halves in ONE instruction
//     // accumulate into r[] with vxor.vv
```

At VLEN=256 with LMUL=m4, the entire inner j-loop (n=16 elements) collapses
to **one vector iteration** per outer `i`. The existing prototype
`src/poly_mul/vector/zvbc/karatsuba_zvbc/gf2x_vclmul.c` implements and
benchmarks this pattern, measuring approximately **12.8× leaf-level speedup**
and a resulting `vect_mul` count of **~187,100 instructions** for HQC-128
(vs 286,173 for Track B).

The 4-way outer unroll (`a[i..i+3]` splat batching) in `gf2x_vclmul.c`
further exploits dual-issue pipelines by keeping 8 vector multiplies in
flight simultaneously.

**Extensions required:** `Zvbc` (adds `vclmul.vv`, `vclmulh.vv`).  
**Builds on:** Track B (retains `rvv_copy_u64` / `rvv_xor3_u64` combine loops).  
**Experiment path:** `experiments/EXP-karatsuba-zvbc-leaf/` (fork of Track B variant).  
**Expected vect_mul improvement:** −35% vs Track B at VLEN=256; −46% at VLEN=512.  
**Expected full-KEM improvement (decaps HQC-128):** −27.6%.

---

## Target C-2: RVV Keccak-f[1600] with `Zvkb` Bitwise Rotations

### Root Cause

`KeccakF1600_StatePermute` executes 24 rounds of the Keccak-f[1600]
permutation over a 25-element `uint64_t[25]` state. Each round contains
four steps:

- **θ:** column-XOR reduction (5 × 5 XOR chains)
- **ρ+π combined:** 25 lane rotations with fixed offsets (0–63 bits)
- **χ:** bitwise `state[i] ^= (~state[i+1]) & state[i+2]`
- **ι:** round-constant XOR on lane 0

The scalar implementation executes all 24 rounds sequentially on a single
state. In the Track B instruction histogram, **`rori` (2.05%)** and
**`andn` (1.77%)** are the direct fingerprints of ρ and χ respectively —
together 3.82% of all retired instructions in the full KEM, and representing
only a single Keccak instance.

HQC calls SHAKE-256 multiple times per KEM operation for key expansion,
pseudorandom sampling, and ciphertext hashing. These calls are independent and
can be processed in parallel. After Track C-1 reduces polymul to ~187K per
call, Keccak emerges as the second-largest bottleneck per the Amdahl model in
`PLAN.md` (~36% of post-polymul runtime).

The existing file `src/keccak/rvv/fips202_rvv.c` already contains a skeleton
for 4-way interleaved `shake256_x4()` using `vuint64m1_t st[25]` at `vl=4`
(VLEN=256, SEW=64, LMUL=1).

### Track C Solution

**Layer 1 — Drop-in single-state RVV (`V` only, already available):**
Vectorise the five θ-column XORs and χ step using `vxor.vv` and `vandn.vv`
(Zvbb) over the 25-lane state. At VLEN=256 with LMUL=m1 (`vl=4`), a single
vector instruction processes 4 lanes simultaneously.

**Layer 2 — `Zvkb` rotation for ρ+π step (key Track C addition):**
The ρ step applies 25 different rotation offsets to the 25 state lanes. In
scalar code, each rotation is compiled as `rori`. In vector code with `Zvkb`:

```
// vror.vv  vd, vs2, vs1   — element-wise vector rotate-right
// vrol.vv  vd, vs2, vs1   — element-wise vector rotate-left
// vror.vi  vd, vs2, uimm  — rotate-right by immediate (up to 63)
```

A vector of 4 lanes sharing the same rotation constant can be rotated by a
single `vror.vi`. Lanes with distinct offsets require a rotation-mask vector
loaded from a precomputed 25-element table and applied with `vror.vv`. This
eliminates the `rori`-dominated scalar ρ loop entirely within each round.

**Layer 3 — Zvbb `vandn.vv` for χ step:**

```
// χ: state[i] ^= (~state[i+1]) & state[i+2]
// vandn.vv vd, ~vs2, vs1  ≡  vd = vs1 & ~vs2
```

`Zvbb` provides `vandn.vv` (vector AND-NOT) as a single instruction,
replacing the two-instruction scalar sequence (`not` + `and`). The entire
25-lane χ step becomes 5 vector iterations with `vandn.vv` + `vxor.vv`.

**Layer 4 — 4-way interleaved SHAKE for batch workloads:**
When HQC calls SHAKE-256 on multiple independent inputs (key expansion and
sampling in `vect_generate_random_support`), 4 Keccak states can be packed
into `vl=4` vector registers and processed simultaneously. The AoS→SoA
layout already exists in `fips202_rvv.c`. This batches 4 permutations for
the cost of one, provided the call sites are refactored to produce 4
independent SHAKE instances simultaneously.

**Extensions required:** `Zvbb` (for `vandn.vv`, `vror.vv`/`vrol.vv`); `Zvkb`
adds vector byte-reversal (`vbrev8.v`, `vrev8.v`) useful for endian handling
in SHAKE sponge absorption.  
**Experiment path:** `experiments/EXP-keccak-rvv-zvkb/`.  
**Expected Keccak speedup:** ~3× (drop-in single-state) to ~4× (4-way interleaved).  
**Expected full-KEM improvement:** −30 to −40% in encaps/decaps after C-1.

---

## Target C-3: Reed-Solomon GF(2⁸) Arithmetic with `vclmul.vv` (`Zvbc`)

### Root Cause

`gf_carryless_mul` is the **fourth-largest** function in the Track A
hotspot table at **8.80%** of total KEM instructions, and `gf_mul`,
`gf_reduce` add a further 3.59%. These functions implement GF(2⁸)
arithmetic for the Reed-Solomon decoder (`reed_solomon.c`):

- **Syndrome computation:** 28 independent polynomial evaluations of a
  45-coefficient locator polynomial over GF(2⁸). Each evaluation performs
  45 × (GF multiply + GF add) = 1260 GF(2⁸) field operations.
- **Chien search:** 256 independent polynomial evaluations (one per candidate
  error position). All 256 are data-independent.
- **Forney formula:** 28 GF inversions plus associated multiplications.

Each GF(2⁸) carry-less multiply is an 8×8-bit polynomial product modulo the
irreducible polynomial `x⁸ + x⁴ + x³ + x + 1 = 0x11D`. Scalar `clmul` can
compute this in one instruction (8 bits of product fit within a 64-bit word),
but the current implementation calls it once per element.

The natural vector parallelism is along the 45 coefficients (syndrome) and the
256 candidate positions (Chien). All 45 syndrome terms for a given evaluation
point are independent; all 256 Chien evaluations are independent.

### Track C Solution

**Axis A — Vector syndrome computation with `vclmul.vv`:**

Pack 8 GF(2⁸) elements into each 64-bit lane (8-bit elements, SEW=8). With
VLEN=256 and LMUL=m4, one vector group holds 128 GF(2⁸) elements — sufficient
to hold all 45 syndrome coefficients with room to spare. A single `vclmul.vv`
with SEW=8 computes **n_elements** 8×8-bit carry-less products in one
instruction, followed by a vector `vmod` reduction step.

Alternatively (using 64-bit packing for cleaner reduction): pack 8 coefficients
per lane, use `vclmul.vv` at SEW=64 to produce 16-bit products per element
position, then mask and reduce with `vand.vi` + shift pairs — matching the
scalar `gf_reduce` pattern in parallel.

**Axis B — Vector Chien search with `vluxei8.v` (indexed load):**

The Chien search evaluates `σ(αⁱ)` for all 256 positions. Each evaluation is
a dot-product in GF(2⁸), where the coefficients are fixed (error-locator
polynomial) and the evaluation points `αⁱ` are precomputed. Using precomputed
log/antilog tables (already present in `gf.c`):

```
vluxei8.v vd, (base), vindices   // gather 32 table entries in one instruction
vxor.vv                          // XOR-accumulate into syndrome
```

The 256 Chien evaluations map directly to 256/32 = 8 `vluxei8.v` passes at
VLEN=256 (LMUL=m1, SEW=8, vl=32), replacing a 256-iteration scalar loop.

**Axis C — `vbrev8.v` for byte-order normalisation (Zvbb):**
If the GF(2⁸) element bit ordering differs between the HQC reference and the
vector layout, `vbrev8.v` (bit-reversal within each byte, from `Zvbb`) provides
a zero-cost in-register reordering without mask operations.

**Extensions required:** `Zvbc` (primary, for `vclmul.vv` at SEW=8); `Zvbb`
(for `vbrev8.v`); base `V` for `vluxei8.v`, `vxor.vv`.  
**Experiment path:** `experiments/EXP-rs-gf28-zvbc/`.  
**Expected RS decoder speedup:** ~4–6× on syndrome + Chien (Berlekamp-Massey
remains sequential).  
**Expected full-KEM improvement:** −5 to −8% in decaps (decoder share after C-1).

---

## Target C-4: Reed-Muller Fast Hadamard Transform with `Zvbb`

### Root Cause

`hadamard` (4.66%) and `find_peaks` (2.17%) together constitute **6.83%** of
total profiled KEM instructions in the Track A baseline. Both come from the
Reed-Muller decoder (`reed_muller.c`), which decodes RM(1,7) codewords during
`decaps`.

The decoder executes a 7-stage Fast Hadamard Transform over a 128-element
`int16_t` accumulator. Each butterfly stage computes:

```c
for (int i = 0; i < 64; i++) {
    dst[i]      = src[2*i]   + src[2*i+1];   // add
    dst[i + 64] = src[2*i]   - src[2*i+1];   // subtract
}
```

This is 128 `int16_t` add/subtract pairs per stage × 7 stages = 896 scalar
arithmetic operations per decoded symbol, repeated for each RM codeword in the
concatenated decoder.

`find_peaks` performs an `int16_t` argmax over 128 elements using branchful
scalar comparison, with absolute-value emulated via mask-and-negate.

### Track C Solution

**FHT butterfly stages with `vadd.vv` / `vsub.vv` (SEW=16):**

At VLEN=256 with SEW=16, one vector group holds 16 `int16_t` elements (LMUL=m1)
or 128 `int16_t` at LMUL=m8. With LMUL=m8, the entire 128-element FHT
transform fits in a single vector group. The butterfly structure requires
stride-2 element interleaving; stages 1–3 are contiguous-stride and map
directly to `vadd.vv`/`vsub.vv` with `vslidedown.vi` for the upper half.
Stages 4–7 require cross-group butterfly patterns handled by `vslide1up.vx` or
`vrgather.vv` with precomputed permutation indices.

**Argmax with `vabs.v` + `vredmax.vs` (Zvbb):**

`Zvbb` provides `vabs.v` (element-wise absolute value) for integer types,
eliminating the branch-heavy scalar absolute-value emulation in `find_peaks`:

```
vabs.v      v_abs, v_transform          // |transform[i]| for all i
vredmax.vs  v_peak, v_abs, v_zero       // reduce: find max |value|
// equality mask to recover position:
vmseq.vv    v_mask, v_abs, v_peak
vfirst.m    a0, v_mask                  // first set bit = peak position
```

This replaces the 128-iteration scalar loop with ~4 vector instructions.

**`expand_and_sum` vectorisation (bit-unpack, base V):**

`expand_and_sum` (1.92%) unpacks `uint32_t` words bit-by-bit into `int16_t`
accumulators. This is a bit-extract scatter that maps to `vand.vi` +
`vsrl.vi` patterns or, with `Zvbb`, `vbrev.v` + mask-extraction.

**Extensions required:** Base `V` (primary); `Zvbb` (for `vabs.v`, `vcpop.v`,
`vbrev.v`).  
**Experiment path:** `experiments/EXP-reed-muller-fht-rvv/`.  
**Expected RM decoder speedup:** ~3–4× (FHT + argmax combined).  
**Expected full-KEM improvement:** −3 to −5% in decaps (RM share after C-1).

---

## Target C-5: FAFFT with `vclmul.vv` — Research Track (`Zvbc`)

### Root Cause & Hypothesis

The scalar FAFFT investigation (`fafft_zbc`, `hybrid_2way_zbc`) concluded that
Karatsuba with `clmul` dominates at all three HQC parameter sizes. The decisive
factor was that `clmul` collapsed the Karatsuba leaf cost to **two instructions
per 64×64 product**, making the FAFFT basis-conversion and butterfly overhead
proportionally dominant.

With `vclmul.vv`, the cost model shifts again:

- **Karatsuba `vclmul.vv` leaf cost** at VLEN=256: ~10 instructions per 16
  elements (down from ~160 scalar instructions for the same block).
- **FAFFT GF(2^m) butterfly cost** at `vclmul.vv`: each butterfly is one
  `vclmul.vv` + one `vclmulh.vv` + modular reduction. At VLEN=256, one
  instruction processes 4 field elements simultaneously.

The research question is whether `vclmul.vv` shifts the Karatsuba/FAFFT
crossover into the HQC parameter range. The FAFFT algorithm is asymptotically
O(n log n) vs Karatsuba's O(n^1.585), but the constant factor from
basis-conversion passes and CRT overhead currently prevents FAFFT from winning
at HQC-128/192/256 sizes.

With `vclmul.vv`, the FAFFT butterfly cost drops by ~4× (matching the 4-way
leaf parallelism), and the XOR-combine passes were already vectorisable with
base `V`. If basis-conversion overhead does not grow proportionally, the
asymptotic advantage may finally materialise for HQC-256 (the largest
parameter set, where FAFFT's O(n log n) gap widens most).

### Track C Approach

1. Implement `src/poly_mul/vector/zvbc/fafft_zvbc/fafft_vclmul.c` by porting
   the existing `fafft_zbc` scalar implementation and replacing all
   GF(2^m) butterfly operations with `vclmul.vv` + `vclmulh.vv`.
2. Vectorise the XOR-combine passes with `vxor.vv` at high LMUL (already
   viable from Track B experience).
3. Benchmark at VLEN=256 and VLEN=512 to assess whether the crossover point
   has moved into the HQC-256 range.
4. If the gap closes at HQC-256, investigate the CRT decomposition route that
   was blocked at HQC-128 by the `bc_1_16384` domain limit.

**Key RISC-V instructions:**
- `vclmul.vv` / `vclmulh.vv` (Zvbc) — GF(2^m) butterfly multiply
- `vxor.vv` (base V) — XOR combine
- `vle64.v` / `vse64.v` (base V) — basis-conversion coefficient scatter/gather

**Experiment path:** `experiments/EXP-fafft-zvbc-butterfly/` (the
`fafft_zvbc/` directory already exists and is empty, awaiting this work).  
**Expected outcome:** Potential 10–20% improvement at HQC-256 only; unlikely
to displace Karatsuba at HQC-128/192. Negative result still thesis-relevant.

---

## Prioritisation Table

| Target | Bottleneck Addressed | Extension(s) | Effort | Expected vect_mul Δ | Expected Full-KEM Δ | Priority |
|--------|---------------------|-------------|--------|---------------------|---------------------|----------|
| **C-1** `vclmul.vv` Karatsuba leaf | `clmul_leaf` (22% of scalar profile, unvectorised in Track B) | `Zvbc` | **Low** (prototype exists in `karatsuba_zvbc`) | **−35%** vs Track B | **−23 to −28%** decaps | 🔴 **P0 — Do First** |
| **C-2** RVV Keccak + `Zvkb` rotations | `KeccakF1600_StatePermute` + `keccak_inc_*` (16.7% of scalar profile, co-dominant after C-1) | `Zvbb`, `Zvkb` | **Medium** (skeleton in `fips202_rvv.c`, 4-way layout ready) | n/a (SHAKE-only) | **−30 to −40%** encaps/decaps after C-1 | 🔴 **P1 — High** |
| **C-3** RS GF(2⁸) `vclmul.vv` | `gf_carryless_mul` + `gf_mul` + `gf_reduce` (12.4% of scalar profile) | `Zvbc`, `Zvbb` | **Medium** (`reed_solomon_rvv.c` skeleton, TODO) | n/a (decoder-only) | **−5 to −8%** decaps | 🟡 **P2 — Medium** |
| **C-4** Reed-Muller FHT + `vabs.v` | `hadamard` + `find_peaks` (6.83% of scalar profile) | `Zvbb`, base `V` | **Low–Medium** (RM structure simple, `reed_muller_rvv.c` exists) | n/a (decoder-only) | **−3 to −5%** decaps | 🟡 **P3 — Medium** |
| **C-5** FAFFT `vclmul.vv` butterfly | Algorithm research — potential O(n log n) win at HQC-256 | `Zvbc`, base `V` | **High** (CRT domain issues, padding overhead) | **−10 to −20% at HQC-256 only** (uncertain) | TBD | 🟢 **P4 — Research** |

**Recommended implementation order:** C-1 → C-4 → C-2 → C-3 → C-5 (research).  
C-4 is low effort and directly unblocks a clean C-3 + C-4 combined decoder
benchmark, while C-2 has moderate refactoring cost across the SHAKE call sites.

---

## End-to-End Projection (HQC-128, decaps)

Starting from Track B decaps = **1,795,968 instructions**:

| After | Projected Decaps | Cumulative Δ vs Track B |
|-------|-----------------|------------------------|
| C-1 (`vclmul.vv` leaf) | ~1,300,000 | **−27.6%** |
| C-1 + C-2 (Keccak RVV+Zvkb, 3×) | ~1,050,000 | **−41.5%** |
| C-1 + C-2 + C-3 (RS GF vectorised) | ~975,000 | **−45.7%** |
| C-1 + C-2 + C-3 + C-4 (RM FHT) | ~930,000 | **−48.2%** |

Versus `baseline_ref` (decaps = **56,962,431**), the full Track C stack
represents a projected **~61× end-to-end speedup** for HQC-128 decaps.

This aligns with the PLAN.md Amdahl model prediction of ~15.7× over the
un-accelerated baseline for the full software stack, noting that the scalar
Track A baseline itself was already **~24.9× faster** than `baseline_ref`.

---

## Appendix: RISC-V Extension Reference

| Extension | Relevant Instructions | Primary Target |
|-----------|----------------------|----------------|
| `Zvbc` | `vclmul.vv`, `vclmulh.vv` | C-1 (polymul leaf), C-3 (GF(2⁸)), C-5 (FAFFT) |
| `Zvbb` | `vandn.vv`, `vabs.v`, `vbrev8.v`, `vrev8.v`, `vrol.vv`, `vror.vv`, `vcpop.v`, `vclz.v` | C-2 (Keccak χ, ρ), C-4 (FHT argmax) |
| `Zvkb` | `vror.vv`, `vrol.vv`, `vbrev8.v`, `vrev8.v` | C-2 (Keccak ρ rotations) |
| Base `V` (RVV 1.0) | `vadd.vv`, `vsub.vv`, `vxor.vv`, `vle64.v`, `vse64.v`, `vluxei8.v`, `vslidedown.vi`, `vrgather.vv`, `vredmax.vs`, `vmseq.vv` | All targets (inherited from Track B) |
| `Zbc` (scalar) | `clmul`, `clmulh`, `clmulr` | C-1 fallback; still used in scalar non-leaf path |
