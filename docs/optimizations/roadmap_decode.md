# Optimization Roadmap: Decoder Acceleration (Section 3.4)
## Reed-Muller (Fast Hadamard Transform) + Reed-Solomon

> **Status:** Phase 3a (RM vectorization) — **PROMOTED**.  Phase 3b Axis A (RS
> syndrome vectorization) — **IMPLEMENTED** (`src/decode/rvv/reed_solomon_rvv.c`).
>
> **Phase 3b Axis A commit:** `feat(decode): implement vectorized GF(2^8) syndrome computation using Zvbc lane-packing.`
>
> **Profile date:** 2026-05-20  
> **Commit:** `6d51344`  
> **Simulator:** Spike (instruction-accurate; `instret` = retired instruction count)  
> **ISA:** `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm`  
> **VLEN baseline:** 256 bits (Spike `--isa zvl256b`)

---

## 0. Decoder Pipeline Architecture

The concatenated HQC decoder runs in strict series during `decaps`:

```
reed_muller_decode(rm_msg, rm_cdw)
    └─ expand_and_sum()   : MULTIPLICITY × N1 copies → 128-element int16 array
    └─ hadamard()         : 7-pass butterfly FHT on 128 int16 elements
    └─ find_peaks()       : argmax over 128 elements → decoded byte
    (× N1 symbols)

reed_solomon_decode(rs_msg, rm_msg)
    └─ compute_syndromes()   : 2·DELTA polynomial evaluations over GF(2⁸), N1 terms each
    └─ compute_elp()         : Berlekamp-Massey over GF(2⁸), O(DELTA²) GF muls
    └─ compute_roots()       : additive FFT (Gao-Mateer) → error locations
    └─ compute_z_poly()      : error-evaluator polynomial, O(DELTA²) GF muls
    └─ compute_error_values(): Forney formula, O(N1 · DELTA) GF muls
    └─ correct_errors()      : N1 XOR corrections
```

**Source files:**

| Role | Current path |
|---|---|
| RM decoder — scalar reference | `vendor/hqc-ref/src/ref/reed_muller.c` |
| RM decoder — RVV (Phase 3a, **active**) | `src/decode/rvv/reed_muller_rvv.c` |
| RS decoder — scalar reference | `vendor/hqc-ref/src/ref/reed_solomon.c` |
| RS decoder — RVV (Phase 3b Axis A, **active**) | `src/decode/rvv/reed_solomon_rvv.c` |
| GF(2⁸) arithmetic (Zbc-accelerated) | `src/common/gf.c` |
| Additive FFT (Gao-Mateer) | `vendor/hqc-ref/src/common/fft.c` |
| Concatenated decode wrapper | `vendor/hqc-ref/src/common/code.c` |
| Benchmark harness | `tests/bench/bench_decode.c` |

**CMake selection** (`cmake/options.cmake`, `CMakeLists.txt` L150–165):

```
HQC_DECODE_IMPL=ref   → scalar reed_muller.c + scalar reed_solomon.c
HQC_DECODE_IMPL=rvv   → reed_muller_rvv.c   + reed_solomon_rvv.c (Phase 3b Axis A active)
```

---

## 1. Security-Level Parameters

| Level | N₁ | DELTA (δ) | N₂ | MULT | FFT | K |
|-------|---:|----------:|---:|-----:|----:|--:|
| HQC-128 | 46 | 15 | 384 | 3 | 2⁴=16 | 16 |
| HQC-192 | 56 | 16 | 640 | 5 | 2⁵=32 | 24 |
| HQC-256 | 90 | 29 | 640 | 5 | 2⁵=32 | 32 |

- **MULT** = `CEIL_DIVIDE(N₂, 128)` — copies of each 128-bit RM codeword block.
- **FFT** — exponent for the additive FFT used in `compute_roots`.
- **N₁** — length of the RS codeword (bytes processed by all RS sub-functions).

Quadratic and superlinear scaling with DELTA and N₁ makes HQC-256 RS cost
**3.8×** the HQC-128 RS cost:

| Loop | HQC-128 gf_muls | HQC-256 gf_muls |
|---|---:|---:|
| `compute_syndromes` outer×inner | 2·15·45 = **1,350** | 2·29·89 = **5,162** |
| BM inner SAXPY (avg DELTA/2 len) | ~15·7.5 = **113** | ~29·14.5 = **421** |
| BM inner dot-product (avg) | ~15·7.5 = **113** | ~29·14.5 = **421** |
| `compute_error_values` beta scatter | 90·29 = **2,610** | 90·29 = **2,610** |
| `compute_error_values` e_j eval | 29·29 = **841** | 29·29 = **841** |

---

## 2. Baseline Instret Measurements

All measurements: `HQC_MUL_VARIANT=karatsuba_zbc`, `HQC_KECCAK_IMPL=ref`, Spike
ISA `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b`. Commit `6d51344` (2026-05-20).

### 2.1 Scalar Reference (`HQC_DECODE_IMPL=ref`)

| Level | `decode` instret | Code size (bytes) |
|-------|----------------:|------------------:|
| HQC-128 | **395,808** | 18,590 |
| HQC-192 | **577,483** | 19,296 |
| HQC-256 | **1,080,683** | 26,768 |

### 2.2 Phase 3a Partial RVV (`HQC_DECODE_IMPL=rvv`, RM vectorized only)

| Level | `decode` instret | Code size (bytes) | Speedup vs ref |
|-------|----------------:|------------------:|---------------:|
| HQC-128 | **178,360** | 16,760 | **2.22×** |
| HQC-192 | **231,901** | 17,800 | **2.49×** |
| HQC-256 | **525,287** | 25,272 | **2.06×** |

### 2.3 RM Vectorization Savings (Phase 3a contribution)

| Level | Saved instret | % of ref total |
|-------|--------------|----------------|
| HQC-128 | 395,808 − 178,360 = **217,448** | **54.9%** |
| HQC-192 | 577,483 − 231,901 = **345,582** | **59.8%** |
| HQC-256 | 1,080,683 − 525,287 = **555,396** | **51.4%** |

### 2.4 Phase 3b Axis A — Vectorized Syndrome Computation (IMPLEMENTED)

Implemented in `src/decode/rvv/reed_solomon_rvv.c` using Zvbc `vclmul.vv`
lane-packing (SEW=64, LMUL=4, one GF(2⁸) element per lane).  KAT gate:
**300/300 vectors matched** across HQC-128/192/256.

| Level | Phase 3a residual | Phase 3b Axis A | Axis A speedup | vs ref baseline |
|-------|------------------:|----------------:|---------------:|----------------:|
| HQC-128 | 178,360 | **134,879** | **1.32×** | **2.94×** |
| HQC-192 | 231,901 | **175,270** | **1.32×** | **3.30×** |
| HQC-256 | 525,287 | **360,313** | **1.46×** | **3.00×** |

> Measured 2026-05-20 on Spike, VLEN=256, `-O3`, Clang RISC-V cross-compiler.

The RS fraction grows with DELTA because the BM and Forney loops dominate at
higher correction capacity.

---

## 3. Source Audit: Primary Execution Loops

### 3.1 Reed-Muller — `reed_muller_rvv.c` (Phase 3a — DONE)

**`hadamard_rvv`** — 7-pass butterfly FHT:
```c
// VLEN=256, SEW=16, LMUL=4 → vl=64 (all 64 butterfly pairs per pass)
vint16m4_t even = vlse16_v(in,     stride=4, vl=64); // p1[0,2,...,126]
vint16m4_t odd  = vlse16_v(in + 1, stride=4, vl=64); // p1[1,3,...,127]
vse16_v(out,      vadd_vv(even, odd, vl), vl);        // p2[0..63]
vse16_v(out + 64, vsub_vv(even, odd, vl), vl);        // p2[64..127]
```
7 passes × 2 vector loads + 2 arithmetic + 2 stores = 42 vector instructions
per symbol.  Replaces 7 × 128 = **896 scalar** add/sub/store sequences.

**`find_peaks_rvv`** — VLEN-wide argmax:
```c
// VLEN=256, SEW=16, LMUL=8 → vl=128 (full array)
vint16m8_t v     = vle16_v(t, 128);
vint16m8_t abs_v = vmax_vv(v, vrsub_vx(v, 0, 128), 128);
max_red = vredmaxu(...);  // scalar max
peak_mask = vmseq_vx(uabs_v, max_abs);
peak_pos  = vfirst_m(peak_mask);  // ← see §5.3 CT note
```

**`expand_and_sum`** — bit-extraction via `vid` + `vsrl` + `vncvt`:
```c
// SEW=32, LMUL=4, vl=32 → extract all 32 bits of one u32 per pass
vuint32m4_t shifts = vid_v(32);    // [0,1,...,31]
vuint32m4_t bits   = vand_vx(vsrl_vv(vword, shifts), 1);
vuint16m2_t b16    = vncvt_x(bits);  // narrow to u16
vse16_v(dest + part*32, b16, 32);
```
4 parts × (MULTIPLICITY copies) = well-vectorized, replaces 4×32×MULT scalar
shifts.

### 3.2 Reed-Solomon — scalar reference (Phase 3b target)

**`compute_syndromes`** (`reed_solomon.c:L112`):
```c
for (i = 0; i < 2*DELTA; i++) {
    for (j = 1; j < N1; j++)
        syndromes[i] ^= gf_mul(cdw[j], alpha_ij_pow[i][j-1]);
    syndromes[i] ^= cdw[0];
}
```
Hot path: 2·DELTA × (N1−1) = 1,350 (L128) to 5,162 (L256) scalar `gf_mul`
calls.  Each `gf_mul` calls `gf_carryless_mul` (Zbc CLMUL, ~15 inst) +
`gf_reduce` (2-step feedback reduction, ~12 inst) = **~27 inst/mul**.

**`compute_elp` (Berlekamp-Massey)** (`reed_solomon.c:L150`):
```c
for (mu = 0; mu < 2*DELTA; mu++) {
    dd = gf_mul(d, gf_inverse(d_p));
    for (i = 1; i <= mu+1 && i <= DELTA; i++)
        sigma[i] ^= gf_mul(dd, X_sigma_p[i]);   // SAXPY over GF(2⁸)
    // ... mask-select update of X_sigma_p, pp, d_p ...
    for (i = 1; i <= mu+1 && i <= DELTA; i++)
        d ^= gf_mul(sigma[i], syndromes[mu+1-i]); // dot-product
}
```
Outer loop (2·DELTA iterations) is **sequentially dependent**; inner SAXPY and
dot-product loops grow from length 1 to DELTA — vectorizable.  Total inner
loop iterations: Σ_{mu=0}^{2DELTA-1} min(mu+1, DELTA) = O(DELTA²) ≈ 225 (L128)
to ≈ 841 (L256) gf_muls.  Each gf_mul ~27 inst → **~6,075 (L128) to ~22,707
(L256) instructions** in BM inner loops.

**`compute_roots` via `fft_rec`** (`fft.c:L174`):
```c
// innermost leaf (m_f==1):
for (i = 0; i < m; i++) tmp[i] = gf_mul(betas[i], f[1]);
// breadth-first XOR broadcast:
for (j = 0; j < m; j++)
    for (k = 0; k < x; k++) w[x + k] = w[k] ^ tmp[j];
// per-beta scaling pass:
for (k = 0; k < (1 << m); k++) w[k] ^= gf_mul(betas[0], w[k]);
```
The additive FFT evaluates a degree-(DELTA) polynomial at all 2^M = 256 points
of GF(2⁸).  Recursive tree depth = PARAM_M = 8.  Leaf-level arrays are small
(16–32 elements); scaling passes are dense GF multiplications on arrays of
power-of-2 size.

**`compute_error_values` (Forney)** (`reed_solomon.c:L255`):
```c
// beta_j scatter: N1 × DELTA comparison + conditional accumulate
for (i = 0; i < N1; i++) {
    for (j = 0; j < DELTA; j++) beta_j[j] += mask1 & mask2 & gf_exp[i];
}
// e_j evaluation: DELTA × DELTA inverse power chain + Horner
for (i = 0; i < DELTA; i++) {
    for (j = 1; j <= DELTA; j++) tmp1 ^= gf_mul(inverse_power_j, z[j]);
    for (k = 1; k < DELTA; k++) tmp2 = gf_mul(tmp2, 1 ^ gf_mul(inv, beta_j[...]));
}
```
Inner Horner evaluation has **sequential dependency on `inverse_power_j`** (each
iteration multiplies by the same inverse) — partially vectorizable via scatter.
The beta_j scatter (N1 × DELTA) is fully vectorizable with masked `vadd`.

---

## 4. Vectorization Opportunities (RVV / Zvbc / Zvbb)

### 4.1 Axis A — Syndrome Computation (highest priority)

**Opportunity**: the 2·DELTA syndrome evaluations are fully independent, and
each inner loop (length N1−1) is a dense GF(2⁸) dot-product — the ideal SIMD
pattern.

**Approach (Zvbc vclmul.vv lane-packing)**:

With `vclmul.vv` (SEW=64, Zvbc), each instruction computes 4 carry-less products
(one per 64-bit lane) by packing four GF(2⁸) elements per lane into bits [7:0],
[15:8], [23:16], [31:24] of each 64-bit word.  After `vclmul.vv`, each 16-bit
product sub-result occupies a known bit-field and is reduced independently
using a fixed-tap shift+XOR (`gf_reduce`).

```
vl = vsetvl_e64m4(N1_ceil_to_4)     // vl = ceil(N1/4)
// pack cdw[0..N1-1] into 4 elements/lane u64 vector
// pack alpha_ij_pow[i][0..N1-2] into same layout
vclmul.vv → vectorized carry-less product of 4 GF pairs per lane
// apply reduction taps: vsrl + vxor in feedback pattern
vredxor.vs → XOR-reduce across lanes → syndrome[i]
```

With VLEN=256, SEW=64, LMUL=4: vl=16 × 4 elements/lane = 64 GF muls per pass.
- L128: ceil(45/64)=1 pass × 30 syndromes = **30 vector passes** vs 1,350 scalar gf_muls
- L256: ceil(89/64)=2 passes × 58 syndromes = **116 vector passes** vs 5,162 scalar gf_muls

Each vector pass ≈ 8–12 instructions (load + vclmul + reduce + vredxor).
Estimated savings: **~100× reduction** in syndrome computation.

**Alternative (log/antilog table lookup)**:
```
vluxei8.v(gf_log, cdw[1..N1])           // gather log(cdw[j])
vluxei8.v(gf_log, alpha_ij_pow[i][*])   // gather log(alpha)
vadd.vv + modular reduce                 // add logs mod 255
vluxei8.v(gf_exp, sum_indices)           // gather exp[sum]
vredxor.vs                               // XOR-reduce
```
This avoids Zvbc dependency but requires GF log/exp tables in hot cache.
Both approaches maintain constant-time semantics (no data-dependent branches).

### 4.2 Axis B — Berlekamp-Massey Inner Loops (medium priority)

The outer BM loop (2·DELTA iterations, sequentially chained) cannot be
vectorized.  The **two inner loops** per BM iteration are vectorizable:

**SAXPY** (`sigma[i] ^= gf_mul(dd, X_sigma_p[i])` for i=1..μ+1):
```c
// Broadcast dd scalar; load X_sigma_p[1..mu+1] as u8 vector
// Vectorized gf_mul: broadcast scalar × vector using vluxei8 + vadd + vluxei8
// XOR into sigma[1..mu+1]
```
Loop length grows from 1 to DELTA; at DELTA=29 (L256) a single vector pass
covers the full iteration.

**Dot product** (`d ^= Σ sigma[i] * syndromes[mu+1-i]`):
```c
// Load sigma[1..mu+1] and syndromes in reverse-index order
// Vectorized gf_mul; vredxor.vs → scalar d update
```
Both loops share the same vectorized `gf_mul` implementation as Axis A.

Estimated speedup for BM inner paths: **3–6×** reduction in BM instruction count
(the outer loop overhead remains, limiting total BM speedup to ~2–3×).

### 4.3 Axis C — Additive FFT Root Finding (medium priority)

The Gao-Mateer additive FFT in `fft_rec` has a recursive tree structure.
At each level, two operations dominate:

1. **Beta scaling**: `w[k] ^= gf_mul(betas[0], w[k])` for all 2^m elements  
   → Entirely vectorizable: `vclmul_scalar_vector(betas[0], w[0..2^m])` + reduce.

2. **Subset-sum XOR broadcast**: `w[x+k] = w[k] ^ tmp[j]` for k=0..x-1  
   → `vxor.vx` on a contiguous block — single vector instruction per j.

Leaf-level arrays are small (2^(FFT-1) = 8–16 elements); these fit in a single
`vl` pass.  The recursion-to-iteration transformation (loop peeling) eliminates
function call overhead.

Estimated speedup for `compute_roots`: **4–8×** for the FFT scaling passes,
**2–3×** overall including recursive bookkeeping.

### 4.4 Axis D — Error Value Computation (lower priority)

**Beta-scatter** (`beta_j[j] += mask1 & mask2 & gf_exp[i]` for j=0..DELTA-1):
```c
// Outer i=0..N1-1 loop: mask = -error[i];
// Inner j=0..DELTA-1: conditional accumulate with delta_counter comparison
// → Use vmerge.vvm / vadd.vim with mask; vredsum to find delta_counter
```

**e_j Horner evaluation**: sequential chain on `inverse_power_j` prevents
full vectorization.  The outer loop over i=0..DELTA-1 provides parallelism
if all DELTA Horner evaluations are batched simultaneously (strip-mining).
This requires an SIMD polynomial evaluation over DELTA independent evaluations
sharing the same coefficients `z[j]` — a SIMD Horner pattern using `vmadd`.

---

## 5. Constant-Time Safety Preservation

### 5.1 Principles

The scalar RS decoder is already constant-time by construction, using:
- Arithmetic masking: `mask = -(condition)` (all-ones or all-zeros)
- `mask & value` instead of conditional branches
- `volatile` qualifiers on critical intermediate masks (e.g. `mask12__` in BM)

Any RVV replacement **must preserve these invariants**.

### 5.2 RVV CT-Safe Patterns

| Scalar pattern | RVV equivalent | Safety |
|---|---|---|
| `mask & val` (arithmetic) | `vand.vv` / `vmerge.vvm` | ✅ CT: always executes both paths |
| `a > b ? x : y` (masked select) | `vmerge.vxm` with `vmsgtu` mask | ✅ CT |
| `Σ gf_mul(a[i], b[i])` (dot product) | `vclmul.vv` + `vredxor.vs` | ✅ CT |
| `a[i] ^= dd * b[i]` (SAXPY) | `vclmul.vv` + `vxor.vv` | ✅ CT |
| `gf_reduce` feedback XOR | `vsrl` + `vxor` in fixed pattern | ✅ CT |

### 5.3 Open CT Concern: `vfirst_m` in `find_peaks_rvv`

The current `find_peaks_rvv` uses `vfirst_m_b2` to locate the argmax lane.
On physical RISC-V hardware, `vfirst_m` may have execution latency proportional
to the position of the first set bit (implementation-defined).  On Spike
(instruction-accurate simulator) this is not observable.

**Mitigation options for physical deployment**:
- Replace `vfirst_m` with a masked `viota_m` + `vredmaxu` pattern that computes
  the argmax index without early-exit semantics.
- Or: use a scalar fallback loop over the 128-element result (N=128 is fixed,
  loop count data-independent — CT-safe on OOO cores).

**Decision**: Accept `vfirst_m` for Spike-only development; add `TODO-CT` marker
for hardware validation gate.

### 5.4 BM Volatile Mask

`compute_elp` uses `volatile uint16_t mask12__` to prevent the compiler from
optimizing away the mask update.  In the vectorized version, the same
`volatile` discipline must be applied to any scalar-resident BM control
variables that gate conditional `vmerge` passes.  Only the inner SAXPY and
dot-product vectors may be replaced with non-volatile RVV paths.

---

## 6. Algorithmic Bounds: Estimated Instruction Reductions

### 6.1 Per-sub-function estimates

All estimates at VLEN=256, using LMUL=4, SEW=8/64 depending on axis.

| Sub-function | L128 scalar instret (est.) | Vectorized instret (est.) | Reduction |
|---|---:|---:|---:|
| `compute_syndromes` | ~36,450 | ~900 | ~40× |
| BM inner loops | ~6,075 | ~1,500 | ~4× |
| `compute_roots` / FFT | ~20,000 | ~5,000 | ~4× |
| `compute_z_poly` | ~5,000 | ~2,000 | ~2.5× |
| `compute_error_values` | ~30,000 | ~8,000 | ~3.8× |
| `correct_errors` | ~270 | ~270 | 1× |
| **RS total (est.)** | **~97,795** | **~17,670** | **~5.5×** |

*Scalar estimates derived from per-call gf_mul counts × ~27 inst/mul.*
*Vectorized estimates assume Zvbc lane-packing at 4 GF muls/vclmul + reduction.*

### 6.2 Full-pipeline projected instret after Phase 3b

| Level | Partial-rvv (current) | Target (full RS rvv, ~5× RS speedup) | Projected gain |
|-------|----------------------:|-------------------------------------:|---------------:|
| HQC-128 | 178,360 | ~50,000–70,000 | **2.5–3.6×** vs partial-rvv |
| HQC-192 | 231,901 | ~60,000–90,000 | **2.6–3.9×** |
| HQC-256 | 525,287 | ~120,000–175,000 | **3.0–4.4×** |

Cumulative speedup relative to scalar-only baseline:

| Level | ref baseline | Projected Phase 3b | Total speedup |
|-------|-------------:|-------------------:|--------------:|
| HQC-128 | 395,808 | ~50,000–70,000 | **5.7–7.9×** |
| HQC-192 | 577,483 | ~60,000–90,000 | **6.4–9.6×** |
| HQC-256 | 1,080,683 | ~120,000–175,000 | **6.2–9.0×** |

---

## 7. Experiment Execution Queue

### Priority 1 — `EXP-rs-rvv-syndrome` (implement first)

**Goal**: Replace scalar `compute_syndromes` in `reed_solomon_rvv.c` with a
fully vectorized GF(2⁸) polynomial evaluation using Zvbc `vclmul.vv`
lane-packing.

**Rationale**: Highest instruction-count savings (~40×), cleanest loop structure
(no data dependencies), serves as the integration scaffold for the full RS rvv
file.

**Implementation steps**:
1. Implement a `gf_mul_vec_u8` helper that packs N uint8 elements into
   `ceil(N/4)` uint64 lanes, applies `vclmul.vv` (SEW=64, Zvbc), and unpacks
   8-bit products after `gf_reduce_vec` feedback XOR.
2. Replace the double nested loop in `compute_syndromes` with a single vector
   dot-product pass per syndrome using `gf_mul_vec_u8` + `vredxor.vs`.
3. Keep BM, FFT, Forney on the scalar path initially; call the scalar
   sub-functions from within the new `reed_solomon_decode` stub.
4. Validate with `bench_decode --decode opt` across all three levels.
5. Run KAT suite: `cmake --build build --target run_kat_tests`.

**Experiment path**: `experiments/EXP-rs-rvv-syndrome/`  
**manifest.yaml entry**: `hardware_class: crypto_vector_zvbc`, `tracks: [AP-3]`

---

### Priority 2 — `EXP-rs-rvv-bm-inner` (implement second)

**Goal**: Vectorize the SAXPY and dot-product inner loops of `compute_elp`
(Berlekamp-Massey) while preserving the sequential outer loop and all CT masks.

**Key constraint**: The outer BM loop body contains `volatile uint16_t mask12__`;
the corresponding vector `vmerge` / `vadd` operations inside the inner loops
must reference scalar-resident control variables to maintain the CT discipline.
Do not hoist `mask12` into a vector register across iterations.

**Expected instret reduction**: 3–6× for the BM inner paths; total BM speedup
~2–3× including overhead.

**Experiment path**: `experiments/EXP-rs-rvv-bm-inner/`

---

### Priority 3 — `EXP-rs-rvv-fft-scaling` (implement third)

**Goal**: Vectorize the beta-scaling passes and XOR-broadcast steps within
`fft_rec`, replacing the scalar per-element `gf_mul` loops with `vclmul.vv`
scaling vectors.

**Note**: The recursive structure of `fft_rec` may benefit from an explicit
loop-peeling transformation at the top two levels of the recursion tree,
converting function call overhead to straight-line vector code for the
largest (most instruction-rich) tree levels.

**Experiment path**: `experiments/EXP-rs-rvv-fft-scaling/`

---

### Priority 4 — `EXP-rs-rvv-forney` (implement last)

**Goal**: Vectorize the beta-scatter and e_j evaluation in `compute_error_values`
using masked `vadd.vim` for beta scatter and `vmadd.vv` for batched Horner
polynomial evaluation across all DELTA evaluations simultaneously.

**Note**: The sequential `inverse_power_j` chain in the Horner loop limits the
inner loop vectorization.  A register-to-register dependency can be broken by
precomputing `[inverse^1, inverse^2, ..., inverse^DELTA]` in a single vector
scan (`vmul.vv` prefix product), then using `vdot` with `z[j]` coefficients.

**Experiment path**: `experiments/EXP-rs-rvv-forney/`

---

## 8. Promotion Criteria (Phase 3b gate)

Before any RS rvv experiment is promoted to replace `reed_solomon.c` in the
`rvv` decode path:

- [x] All three KAT suites pass (`tests/kat/`, all levels). ✓ **300/300 @ 2026-05-20**
- [ ] `bench_decode --decode opt` shows ≥ 3× improvement over current partial-rvv
      baseline at each level. *(Axis A alone: 1.32–1.46×; full-RS target pending Axes B/C/D)*
- [ ] CT property verified: no `vfirst_m` or other variable-exit instructions
      inside RS code paths (or documented exception with hardware timing evidence).
- [x] `volatile` discipline preserved for all BM outer-loop control masks. ✓ (`volatile uint16_t mask12__` kept)
- [ ] Code size does not exceed scalar size by more than 2× (avoid cache pressure).

---

## 9. Appendix: gf_mul Implementation Notes

The current `src/common/gf.c` uses Zbc `clmul` for carry-less multiplication
with a 2-step feedback `gf_reduce`.  Each scalar `gf_mul` costs approximately:

```
gf_carryless_mul: ~15 instructions (Zbc clmul + shift/xor bit-extraction)
gf_reduce:        ~12 instructions (2 × {mod=x>>8; x &= 0xff; x ^= mod; 3×shift+xor})
total:            ~27 instructions / gf_mul
```

For vectorized GF arithmetic using Zvbc `vclmul.vv` (SEW=64, 4 elements/lane):
```
vclmul.vv on packed lanes: 1 instruction × ceil(N/4) passes
gf_reduce_vec (per lane):  3–4 instructions × ceil(N/4) passes
vredxor.vs (dot product):  1 instruction
total per N-element GF dot product: ~(5 instructions × ceil(N/4)) + 1
```

For N=45 (L128 syndrome inner loop): ceil(45/4)=12 passes × 5 inst = 60 inst.
Compare to scalar: 45 × 27 = 1,215 inst. **~20× reduction per syndrome**.
Over 30 syndromes: 60 × 30 = 1,800 inst (vectorized) vs 36,450 inst (scalar).
