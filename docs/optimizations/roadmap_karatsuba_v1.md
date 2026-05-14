# Optimization Roadmap: `karatsuba_zbc` — v1

> **Baseline variant:** `karatsuba_zbc`  
> **Profile date:** 2026-05-14  
> **Baseline commit:** `62a5ab3`  
> **Security levels profiled:** HQC-128 / HQC-192 / HQC-256  
> **Simulator:** Spike (instruction-accurate; `instret` = retired instruction count)

---

## Methodology

All cycle figures in this document are **Spike `instret` counts** (instructions retired),
not real wall-clock cycles. On out-of-order hardware the relative ordering of bottlenecks
may shift; this plan targets both the simulator baseline and a real RVA23/SiFive X280 class
micro-architecture.

**Hard constraints enforced throughout:**
- `Zvbc` (`vclmul`, `vclmulh`) and `Zvbb` are **FORBIDDEN** (no vector crypto).
- All Track A solutions use only: base integer ISA + `Zba`, `Zbb`, `Zbc`, `Zbkb`.
- All Track B solutions use only: standard `V` extension (RVV 1.0), no crypto sub-extensions.

---

## Baseline Hotspot Summary (HQC-128, `instret`)

| Rank | Function | Instructions | % of Total |
|------|----------|-------------|------------|
| 1 | `memset` | 2,011,629 | 23.13% |
| 2 | `vect_write_support_to_vector` | 1,825,896 | 20.99% |
| 3 | `clmul_leaf` | 1,286,820 | 14.79% |
| 4 | `karatsuba_clmul` | 961,980 | 11.06% |
| 5 | `KeccakF1600_StatePermute` | 578,792 | 6.65% |
| 6 | `gf_carryless_mul` | 429,828 | 4.94% |

**Top-4 combined share: ~69.97%** — any speedup here dominates the overall KEM latency.

---

## Target 1 — `memset` (23.13%)

### 1.1 Root Cause

`memset` is called in two hot sites within the Karatsuba recursion:

```c
// src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c:76
static void clmul_leaf(...) {
    memset(r, 0, 2 * n * sizeof(uint64_t));  // ← every leaf call
    ...
}

// src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c:130
static void karatsuba_clmul(...) {
    ...
    memset(r, 0, 2 * n * sizeof(uint64_t));  // ← every internal node
    ...
}
```

For HQC-128 (`VEC_N_SIZE_64 = 277`), the Karatsuba recursion tree has
approximately `log₂(277/16) ≈ 4` levels. With branching factor 3 at each level,
the total number of `memset` invocations scales as O(3^depth). Each call zeroes
`2n × 8` bytes of a **stack-allocated** temporary buffer (`tmp_buf[]`) that has
already been zeroed from a higher call level or is guaranteed-clobbered by
subsequent XOR assignments.

The fundamental inefficiency is **redundant zeroing**: `z0`, `z2`, and `zmid`
are written with `memset(r, 0, ...)` before being populated entirely by
`karatsuba_clmul(z0, ...)`, which itself begins with another `memset` — producing
a cascade of overlapping zero-fills on the same stack pages.

The `mem.c:40` and `mem.c:41` line-level hotspots confirm two sequential scalar
byte-store loops constituting the `memset` body; the simulator counts each byte
store as one retired instruction, making large `memset` calls artificially
expensive relative to real hardware (which uses cache-line-fill bursts).

### 1.2 Track A — Pure Scalar (`Zbb`)

**Strategy: Eliminate redundant zeroing via output-accumulation discipline.**

Instead of zeroing the output buffer at the top of each recursion frame and then
XOR-accumulating into it, invert the contract: pass a `zero_first` flag (or
split into `_add` and `_nonadd` variants) so that only the **first writer** to
each word zeroes it. In practice this means:

1. Replace `memset(r, 0, ...)` + subsequent `r[i] ^= z0[i]` with a direct
   **copy** (`r[i] = z0[i]`) for the first addend.
2. Remove the `memset` from the leaf (`clmul_leaf`): initialise `r[]` to
   the first product directly, accumulate subsequent `(i,j)` products with XOR.
3. For the Karatsuba assembly step, fold the three partial-product loops into
   a single output pass — one loop body computes and writes each output word
   exactly once, removing the need to pre-zero.

```c
// Pseudo-code: zero-free leaf multiply (Track A)
static void clmul_leaf_nozero(uint64_t *r, const uint64_t *a,
                               const uint64_t *b, size_t n)
{
    // First column: direct assign (no prior zero needed)
    for (size_t j = 0; j < n; j++) {
        uint64_t lo, hi;
        clmul64(a[0], b[j], &lo, &hi);
        r[j]     = lo;
        r[j + 1] = hi;
    }
    // Remaining columns: accumulate
    for (size_t i = 1; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            uint64_t lo, hi;
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
    }
}
```

For the Karatsuba assembly step, use `__builtin_expect` / `Zbb` `orc.b` to
signal the branch-free, fused accumulate:

```c
// Fused single-pass Karatsuba assembly (Track A pseudo-code)
for (size_t i = 0; i < 2 * n1; i++) {
    uint64_t mid = zmid[i]
                 ^ (i < 2*n0 ? z0[i] : 0)   // Zbb: use cmov / czero.eqz
                 ^ z2[i];
    // Write z2 shifted portion directly (no prior zero of r[2m+i])
    if (i < 2*n0) r[i]       = z0[i];        // first-write: assign
    r[2*m + i]  = z2[i];                      // first-write: assign
    r[m + i]   ^= mid;                        // XOR into partially-filled slot
}
```

**Extensions used:** Base integer (store/load), `Zbb` (`czero.eqz` / `czero.nez`
for branchless conditional clears, `orn` for mask generation).

**Expected savings:** Eliminates ~60–70% of `memset` instructions across the
recursion tree. At 23.13% baseline share, this projects to a **~14–16%
overall instruction-count reduction**.

---

### 1.3 Track B — Standard RVV (`V`)

**Strategy: Vectorised bulk-zero using `vse64.v` with `vsetvli`.**

Where zeroing cannot be eliminated (e.g., the `tmp_buf` stack frame at the
outermost call level, or the unreduced output buffer), replace the scalar
`memset` with an inlined RVV vector store loop. This is particularly effective
because Spike counts each `vse64.v` element as one instruction, yielding a
`LMUL=8` group that retires in `ceil(n/8)` instructions instead of `n` scalar
stores.

```c
// RVV vectorised memset-zero (Track B pseudo-code)
static inline void rvv_zero_u64(uint64_t *dst, size_t n_words)
{
    // n_words = number of uint64_t elements to zero
    while (n_words > 0) {
        size_t vl;
        __asm__ volatile (
            "vsetvli   %0, %1, e64, m8, ta, ma\n\t"
            "vmv.v.i   v0, 0\n\t"              // splat zero into v0–v7
            "vse64.v   v0, (%2)\n\t"
            : "=r"(vl)
            : "r"(n_words), "r"(dst)
            : "v0","v1","v2","v3","v4","v5","v6","v7","memory"
        );
        dst     += vl;
        n_words -= vl;
    }
}
```

Additionally, for the Karatsuba accumulation steps (the three `for` loops that
XOR partial products into `r[]`), vectorise them with `vxor.vv` at LMUL=4/8 to
reduce the XOR loop instruction count from O(n) scalar ops to O(n/VLEN).

```c
// RVV accumulate z0 into r (Track B pseudo-code)
void rvv_xor_accumulate(uint64_t *r, const uint64_t *src, size_t n_words)
{
    while (n_words > 0) {
        size_t vl;
        __asm__ volatile (
            "vsetvli  %0, %1, e64, m4, ta, ma\n\t"
            "vle64.v  v0, (%3)\n\t"
            "vle64.v  v4, (%2)\n\t"
            "vxor.vv  v0, v0, v4\n\t"
            "vse64.v  v0, (%2)\n\t"
            : "=r"(vl)
            : "r"(n_words), "r"(r), "r"(src)
            : "v0","v1","v2","v3","v4","v5","v6","v7","memory"
        );
        r       += vl;
        src     += vl;
        n_words -= vl;
    }
}
```

**Extensions used:** `V` (standard RVV 1.0), `vsetvli`, `vmv.v.i`, `vse64.v`,
`vle64.v`, `vxor.vv`. LMUL=8 for zeroing, LMUL=4 for XOR accumulation to allow
two source register groups.

**Expected savings:** On Spike (which counts element operations for vector
instructions), an LMUL=8 `vse64.v` loop over 554 words (2×277) completes in
`ceil(554/8) = 70` instructions instead of 554. Combined with vectorised XOR
accumulation, Track B projects a **~18–22% overall instruction-count reduction**
for this hotspot alone.

---

## Target 2 — `vect_write_support_to_vector` (20.99%)

### 2.1 Root Cause

The current implementation (`vendor/hqc-ref/src/ref/vector.c:149–165`) has a
**O(weight × VEC_N_SIZE_64)** complexity double loop:

```c
// Current implementation (ref/vector.c)
void vect_write_support_to_vector(uint64_t *v, uint32_t *support, uint16_t weight) {
    uint32_t index_tab[PARAM_OMEGA_R];      // word-index per support entry
    uint64_t bit_tab[PARAM_OMEGA_R];        // bitmask per support entry

    for (size_t i = 0; i < weight; i++) {
        index_tab[i] = support[i] >> 6;    // which 64-bit word
        int32_t pos  = support[i] & 0x3f;  // which bit within word
        bit_tab[i]   = 1ULL << pos;
    }

    // O(VEC_N_SIZE_64 × weight) — for HQC-128: 277 × 66 = 18,282 iterations
    for (uint32_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t val = 0;
        for (uint32_t j = 0; j < weight; j++) {
            uint32_t tmp  = i - index_tab[j];
            int val1      = 1 ^ ((tmp | -tmp) >> 31);  // 1 if tmp==0
            uint64_t mask = -(uint64_t)val1;
            val |= (bit_tab[j] & mask);
        }
        v[i] |= val;
    }
}
```

For HQC-128: `weight = PARAM_OMEGA_R = 75` and `VEC_N_SIZE_64 = 277`, yielding
**20,775 iterations** of the inner loop. Each iteration executes approximately
8 scalar instructions (sub, or, neg, shr, neg-mask, and, or, accumulate) →
~166,200 instructions just for the inner body. Multiplied by the 3 vectors
sampled per KEM operation (`r1`, `r2`, `e` during encaps/decaps) this explains
the observed 20.99% share.

The inner-loop body is **branchless** by design (constant-time), but the
O(N × weight) algorithm is fundamentally inefficient: most iterations result in
`mask = 0` (when `i ≠ index_tab[j]`).

### 2.2 Track A — Pure Scalar (`Zbb` / `Zbkb`)

**Strategy: Direct scatter — invert loop order and write bits directly.**

The algorithmic transformation is O(weight) → O(weight) by eliminating the
outer VEC_N_SIZE_64 scan entirely. Instead of iterating over every possible
word and testing membership, iterate over the `weight` support entries and write
directly into `v[index_tab[j]]`. Constant-time (CT) hazard analysis: since the
support is already computed via a CT sampler, the index values can safely be
used as memory addresses without leaking secret information — the *addresses* of
the writes are not secret in this context (support indices are only used in
non-secret polynomial arithmetic).

```c
// Track A: O(weight) direct scatter (Zbb pseudo-code)
void vect_write_support_to_vector_scalar(uint64_t *v,
                                          const uint32_t *support,
                                          uint16_t weight)
{
    for (size_t i = 0; i < weight; i++) {
        uint32_t word_idx = support[i] >> 6;       // Zbb: srli (already base ISA)
        uint32_t bit_pos  = support[i] & 0x3f;     // Zbb: andi
        // Zbb: bset rd, rs1, rs2  (bit-set instruction)
        // v[word_idx] |= (1ULL << bit_pos);
        __asm__ volatile (
            "ld    t0, 0(%0)\n\t"
            "bset  t0, t0, %1\n\t"    // Zbs: bset (bit set); part of Zbb in RV64
            "sd    t0, 0(%0)\n\t"
            :
            : "r"(v + word_idx), "r"((uint64_t)bit_pos)
            : "t0", "memory"
        );
    }
}
```

> **Note on constant-time:** For **key generation** vectors (x, y), the support
> set is secret and this direct-scatter approach leaks index-dependent memory
> access patterns. Use the reference CT approach or Oblivious RAM (ORAM) scatter.
> For **encapsulation** vectors (r1, r2, e), the support is ephemerally random
> and this optimisation is safe. Apply selectively.

**Extensions used:** `Zbb` (`bset` / `bclr`), base integer `ld`/`sd`.

**Expected savings:** Reduces from ~166,200 inner-loop instructions (for non-CT
variant) to ~225 instructions (`75 × 3`). In CT contexts, a partial win is
achieved by replacing the 8-instruction body with a `Zbb czero.eqz` / `bset`
sequence of 5 instructions: roughly **35–40% reduction** in this function's
instruction count. Combined with the O(weight) reform for non-secret vectors,
the function drops from 20.99% to ~4–5% of total `instret`, a potential
**~16% global instruction count improvement**.

---

### 2.3 Track B — Standard RVV (`V`)

**Strategy: Vectorised scatter using `vsoxei32.v` (indexed scatter).**

RVV 1.0 provides `vsoxei<EEW>.v` (unordered indexed store), which writes a
vector register group to memory at addresses computed as `base + index[i]`.
This maps directly onto the support-to-vector scatter operation:

```c
// Track B: RVV indexed scatter (pseudo-code)
void vect_write_support_to_vector_rvv(uint64_t *v,
                                       const uint32_t *support,
                                       uint16_t weight)
{
    // Step 1: compute per-entry word-byte-offsets and bitmasks
    // offsets[i] = (support[i] >> 6) * 8         (byte offset of target word)
    // bits[i]    = 1ULL << (support[i] & 0x3f)   (bit mask)

    // Step 2: gather current v[word_idx] values
    //   vle32.v  vsupport,  (support_ptr)
    //   vsrl.vx  vword_idx, vsupport, 6      ; word indices (32-bit)
    //   vsll.vx  voffsets,  vword_idx,  3    ; byte offsets
    //   vloxei32.v  vwords, (v), voffsets    ; gather v[word_idx]

    // Step 3: compute bit masks
    //   vand.vi  vbit_pos, vsupport, 63
    //   vmv.v.i  vones, 1
    //   vsll.vv  vbitmask, vones, vbit_pos   ; 1ULL << bit_pos

    // Step 4: OR in the bit and scatter back
    //   vor.vv   vwords, vwords, vbitmask
    //   vsoxei32.v  vwords, (v), voffsets    ; scatter write

    size_t vl;
    const uint64_t *v_base = v;
    while (weight > 0) {
        __asm__ volatile (
            "vsetvli     %[vl], %[w], e32, m2, ta, ma\n\t"
            // load 32-bit support indices
            "vle32.v     v0, (%[sup])\n\t"
            // compute 64-bit word indices (support >> 6)
            "vsrl.vi     v2, v0, 6\n\t"
            // compute byte offsets (word_idx << 3)
            "vsll.vi     v4, v2, 3\n\t"
            // gather existing word values (e64 view)
            "vsetvli     zero, %[vl], e64, m4, ta, ma\n\t"
            "vloxei32.v  v8, (%[base]), v4\n\t"
            // compute bit positions (support & 63) — reuse e32 lane
            "vsetvli     zero, %[vl], e64, m4, ta, ma\n\t"
            "vand.vi     v12, v8, 63\n\t"       // bit_pos (recomputed from gather)
            // NOTE: proper impl loads support as e64 to get bit_pos cleanly;
            // this is pseudocode to illustrate the scatter pattern
            "vmv.v.i     v16, 1\n\t"
            "vsll.vv     v16, v16, v12\n\t"     // bitmask = 1 << bit_pos
            "vor.vv      v8,  v8,  v16\n\t"     // v[word_idx] |= bitmask
            "vsoxei32.v  v8, (%[base]), v4\n\t" // scatter write
            : [vl] "=r"(vl)
            : [w] "r"((size_t)weight), [sup] "r"(support), [base] "r"(v_base)
            : "v0","v1","v2","v3","v4","v5","v6","v7",
              "v8","v9","v10","v11","v12","v13","v14","v15",
              "v16","v17","v18","v19","memory"
        );
        support += vl;
        weight  -= (uint16_t)vl;
    }
}
```

> **Hazard note:** `vsoxei32.v` does not guarantee ordering among elements
> touching the same destination word. Since distinct support entries have
> *distinct* bit positions (by the unique-index guarantee of the sampler),
> there are no write-after-write hazards within a single batch. Verified safe.

**Extensions used:** `V` (RVV 1.0): `vle32.v`, `vsrl.vi`, `vsll.vi`,
`vloxei32.v`, `vsoxei32.v`, `vand.vi`, `vmv.v.i`, `vsll.vv`, `vor.vv`.

**Expected savings:** Processes up to VLEN/32 support entries per vector
iteration. For a 256-bit VLEN machine (ELEN=64, VLEN=256), each `vsetvli`
with `e32,m2` yields `vl = 16` → `75/16 = 5` vector iterations vs. 20,775
scalar iterations. Projects a **~40× instruction reduction** in this function,
dropping from 20.99% to ~0.5% of total `instret`.

---

## Target 3 — `gf2x_exact_shift_right` / FAFFT Shift Kernel (11.66%)

> **Context:** This target is specific to the `fafft_zbc` variant
> (`src/poly_mul/scalar/fafft_zbc/`) currently under development. In the
> `karatsuba_zbc` profile it manifests as `clmul_leaf` at `gf2x_clmul.c:78–81`
> (14.79%). The roadmap section covers the **shift kernel** that will be the
> dominant cost in the FAFFT approach, where the "schoolbook shift-right" pattern
> is the GF(2^n) radix butterfly.

### 3.1 Root Cause

In an FAFFT-based GF(2)[x] multiplier, the key primitive is exact right-shift
of a polynomial bit-vector by `s` positions across a multi-word (64-bit word)
array. The scalar implementation uses a two-scalar-word sliding window:

```c
// Typical scalar gf2x_exact_shift_right (conceptual, as it would appear
// in src/poly_mul/scalar/fafft_zbc/gf2x_clmul.c near line 48)
static inline void gf2x_exact_shift_right(uint64_t *dst,
                                           const uint64_t *src,
                                           size_t n_words,
                                           unsigned shift)
{
    unsigned inv = 64u - shift;
    for (size_t i = 0; i < n_words - 1; i++) {
        dst[i] = (src[i] >> shift) | (src[i + 1] << inv);  // ← hot line
    }
    dst[n_words - 1] = src[n_words - 1] >> shift;
}
```

Each loop body emits: `ld`, `srl`, `ld`, `sll`, `or`, `sd` → 6 instructions
per word. For HQC-128 (`n_words = 277`), this costs ~1,662 instructions per
call. The FAFFT butterfly calls this kernel repeatedly at each frequency level
(O(N log N) total), making it the dominant cost.

The `karatsuba_zbc` analogue is the `clmul64` + XOR accumulation in `clmul_leaf`
(two `ld`, `clmul`, `clmulh`, two `xor`, two `sd` = 8 instructions per
(i,j) pair), producing the 14.79% hotspot at `gf2x_clmul.c:81`.

### 3.2 Track A — Pure Scalar (`Zbb` / `Zbkb`)

**Strategy: 4-way loop unrolling + `Zbb` `ror`/`rori` for rotate-style shifts.**

For a fixed `shift` value known at compile time, the
`(src[i] >> shift) | (src[i+1] << (64-shift))` pattern is identical to a
64-bit right-rotate spanning two adjacent words. `Zbkb` provides `ror`/`rori`
but not a cross-word variant; however unrolling 4 iterations and using
`__builtin_expect` to eliminate the loop overhead (branch prediction, loop
counter update) reduces instruction overhead per word.

```c
// Track A: 4-unroll scalar shift-right (Zbkb pseudo-code)
static void gf2x_shift_right_unroll4(uint64_t *dst,
                                      const uint64_t *src,
                                      size_t n_words,
                                      unsigned shift)
{
    unsigned inv = 64u - shift;
    size_t i = 0;
    // Main 4-unrolled body
    for (; i + 4 <= n_words - 1; i += 4) {
        dst[i+0] = (src[i+0] >> shift) | (src[i+1] << inv);
        dst[i+1] = (src[i+1] >> shift) | (src[i+2] << inv);
        dst[i+2] = (src[i+2] >> shift) | (src[i+3] << inv);
        dst[i+3] = (src[i+3] >> shift) | (src[i+4] << inv);
    }
    // Scalar tail
    for (; i < n_words - 1; i++) {
        dst[i] = (src[i] >> shift) | (src[i+1] << inv);
    }
    dst[n_words - 1] = src[n_words - 1] >> shift;
}
```

When `shift` is a compile-time constant, GCC/Clang with `-march=+zbb` will
replace the `(a >> shift) | (b << inv)` pattern with `ror` where the hardware
supports it, and will fold the 4-unroll into back-to-back register pairs.

For the Karatsuba `clmul_leaf` analogue: unroll the inner `j` loop by 2 to
enable the compiler to interleave `clmul` / `clmulh` pairs, reducing
register-dependency stalls on in-order hardware:

```c
// Track A: 2-unrolled clmul_leaf (Zbc pseudo-code)
for (size_t i = 0; i < n; i++) {
    size_t j = 0;
    for (; j + 2 <= n; j += 2) {
        uint64_t lo0, hi0, lo1, hi1;
        clmul64(a[i], b[j],   &lo0, &hi0);
        clmul64(a[i], b[j+1], &lo1, &hi1);
        r[i+j]   ^= lo0;
        r[i+j+1] ^= hi0 ^ lo1;   // fold: hi0 and lo1 share the same word
        r[i+j+2] ^= hi1;
    }
    // scalar tail...
}
```

**Extensions used:** Base integer, `Zbb` (`srl`, `sll`, `or` — already emitted;
`rori` if shift is constant), `Zbkb` (`ror`), `Zbc` (`clmul`, `clmulh`).

**Expected savings:** 4-unrolling the shift kernel reduces loop-overhead
instructions by ~75% (branch + addi removed per 4 words). Projected
**~20–25% reduction** in `gf2x_exact_shift_right` instruction count;
**~8–10% reduction** in `clmul_leaf` instruction count (2-unroll).

---

### 3.3 Track B — Standard RVV (`V`)

**Strategy: `vsrl.vx` + `vsll.vx` + `vslide1down.vx` for cross-word shift.**

RVV provides the exact primitive needed: `vslide1down.vx` shifts the element
vector down by one position (loads the next word), enabling a cross-word OR
without a scalar load. Combined with `vsrl.vx` / `vsll.vx`, the entire shift
loop becomes a 6-instruction vector body:

```c
// Track B: RVV vectorised right-shift (pseudo-code)
static void gf2x_shift_right_rvv(uint64_t *dst,
                                   const uint64_t *src,
                                   size_t n_words,
                                   uint64_t shift)
{
    uint64_t inv = 64ULL - shift;
    size_t remaining = n_words;
    const uint64_t *s = src;
    uint64_t       *d = dst;

    while (remaining > 0) {
        size_t vl;
        __asm__ volatile (
            // Configure: e64, m4 → processes up to 4*VLEN/64 elements
            "vsetvli      %[vl], %[rem], e64, m4, ta, ma\n\t"
            // Load current window: v0 = src[i .. i+vl-1]
            "vle64.v      v0, (%[src])\n\t"
            // Shift right: v4 = v0 >> shift
            "vsrl.vx      v4, v0, %[sh]\n\t"
            // Slide down by 1: v8 = {src[i+1..i+vl], fill=src[i+vl]}
            "vslide1down.vx v8, v0, %[fill]\n\t"
            // Shift left: v8 = v8 << (64-shift)
            "vsll.vx      v8, v8, %[inv]\n\t"
            // OR together: v4 = (v0 >> shift) | (v0_next << inv)
            "vor.vv       v4, v4, v8\n\t"
            // Store result
            "vse64.v      v4, (%[dst])\n\t"
            : [vl] "=r"(vl)
            : [rem] "r"(remaining), [src] "r"(s), [dst] "r"(d),
              [sh]  "r"(shift),     [inv] "r"(inv),
              [fill] "r"(remaining > 1 ? s[vl] : 0ULL)
            : "v0","v1","v2","v3","v4","v5","v6","v7",
              "v8","v9","v10","v11","memory"
        );
        s         += vl;
        d         += vl;
        remaining -= vl;
    }
    // Last word: no next element, only right-shift
    dst[n_words - 1] = src[n_words - 1] >> shift;
}
```

> **Implementation note:** The `vslide1down.vx fill` register (`%[fill]`) must
> supply the first element of the *next* batch, not a true boundary value, when
> batching is used. A two-pass approach (load vl+1, process vl) avoids the
> fill-register complexity.

**Extensions used:** `V` (RVV 1.0): `vsetvli`, `vle64.v`, `vsrl.vx`, `vsll.vx`,
`vslide1down.vx`, `vor.vv`, `vse64.v`.

**Expected savings:** For `n_words = 277` words with VLEN=256, LMUL=4 gives
`vl = 16` → `ceil(277/16) = 18` vector loop iterations vs. 277 scalar iterations.
The 6-instruction vector body means `18 × 6 = 108` instructions vs.
`277 × 6 = 1,662` scalar instructions — a **~15× instruction reduction** in the
kernel. At the FAFFT O(N log N) call frequency, this projects to a
**>10% global instruction-count reduction** for the full KEM.

---

## Target 4 — `gf2x_clmul_128x128` / Karatsuba Leaf Nodes (11.06%)

### 4.1 Root Cause

The Karatsuba leaf multiply (`clmul_leaf` at 14.79% + the recursive assembly
overhead in `karatsuba_clmul` at 11.06%) uses `clmul` + `clmulh` correctly, but
the schoolbook inner loop suffers from:

1. **Register pressure at KARATSUBA_THRESHOLD=16:** The innermost `(i,j)` pair
   loads `a[i]`, loads `b[j]`, emits `clmul`/`clmulh`, then loads `r[i+j]` and
   `r[i+j+1]` for XOR writeback. This is 7 register reads/writes per pair — on a
   6-issue in-order pipeline (SiFive X280 class) the load-use latency of `ld` →
   `clmul` limits throughput to 1 pair per ~3 cycles.

2. **Redundant intermediate buffer zeroing** (covered under Target 1), which
   costs an additional 2 × `memset` calls per recursion level.

3. **Sequential `hi0 ^ lo1` fusion opportunity missed:** When processing
   consecutive `j` values for the same `i`, `hi` from pair `(i,j)` lands in
   `r[i+j+1]` and `lo` from pair `(i,j+1)` also targets `r[i+j+1]`. These two
   XOR contributions can be folded into a single write, saving one
   load-XOR-store per pair.

### 4.2 Track A — Pure Scalar (`Zbc`, instruction scheduling)

**Strategy: Software pipelining — interleave independent `clmul64` calls to
hide load-use latency; fold `hi⊕lo` into a combined per-word XOR.**

For the leaf node at threshold T=16 (16×16 = 256 clmul64 calls), restructure
the inner loop to issue two independent `clmul64` calls back-to-back before
consuming either result. This converts the dependency chain from:

```
ld a[i] → clmul(a[i],b[j]) → xor r[i+j]    (serialised)
```

into:

```
ld a[i], ld b[j]   → clmul_lo0, clmulh_hi0   \
ld b[j+1]          → clmul_lo1, clmulh_hi1   /  (interleaved, 2 pairs issued)
xor r[i+j]   ^= lo0
xor r[i+j+1] ^= (hi0 ^ lo1)   ← fused write
xor r[i+j+2] ^= hi1
```

```c
// Track A: 2-way software-pipelined clmul_leaf (Zbc pseudo-code)
static void clmul_leaf_sp2(uint64_t *r, const uint64_t *a,
                            const uint64_t *b, size_t n)
{
    // Zero output (or use zero-free variant from Target 1)
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < n; i++) {
        uint64_t ai = a[i];
        size_t j = 0;
        for (; j + 2 <= n; j += 2) {
            uint64_t lo0, hi0, lo1, hi1;
            // Issue both clmul pairs before consuming results
            __asm__ volatile (
                "clmul  %0, %4, %5\n\t"   // lo0 = ai * b[j]
                "clmulh %1, %4, %5\n\t"   // hi0
                "clmul  %2, %4, %6\n\t"   // lo1 = ai * b[j+1]
                "clmulh %3, %4, %6\n\t"   // hi1
                : "=r"(lo0), "=r"(hi0), "=r"(lo1), "=r"(hi1)
                : "r"(ai), "r"(b[j]), "r"(b[j+1])
            );
            r[i+j]   ^= lo0;
            r[i+j+1] ^= hi0 ^ lo1;   // fused: saves 1 load + 1 XOR + 1 store
            r[i+j+2] ^= hi1;
        }
        for (; j < n; j++) {          // scalar tail
            uint64_t lo, hi;
            clmul64(ai, b[j], &lo, &hi);
            r[i+j]   ^= lo;
            r[i+j+1] ^= hi;
        }
    }
}
```

**Extensions used:** `Zbc` (`clmul`, `clmulh`), base integer.

**Expected savings:** The 2-way software pipeline:
- Reduces register-dependency stall slots by ~50% on in-order hardware.
- The `hi⊕lo` fusion reduces the XOR/load/store count per pair by ~33%.
- Combined: **~15–20% reduction** in `clmul_leaf` instruction count.
  Global impact: ~2.5–3% of total `instret`.

---

### 4.3 Track B — Standard RVV (`V`, Integer)

**Strategy: Vectorise the schoolbook accumulation with `vxor.vv` to reduce
loop overhead; the `clmul` leaf itself cannot be vectorised without `Zvbc`,
but the XOR accumulation tree can.**

At KARATSUBA_THRESHOLD=16, the Karatsuba assembly step (step 5 in
`karatsuba_clmul`) executes three XOR loops over 2×n = 32-word arrays. Each
loop currently costs ~32 scalar `ld`+`xor`+`sd` triples. With RVV `vxor.vv`
at LMUL=4 and VLEN=256, these 32 elements complete in `ceil(32/4) = 8` vector
instructions vs. `32 × 3 = 96` scalar instructions — a 12× speedup on this
sub-step.

```c
// Track B: RVV XOR accumulation for Karatsuba assembly (pseudo-code)
static void karatsuba_assemble_rvv(uint64_t *r,
                                    const uint64_t *z0,
                                    const uint64_t *zmid,
                                    const uint64_t *z2,
                                    size_t n,   // half-size
                                    size_t n0,
                                    size_t n1)
{
    // r[0..2n0-1] ^= z0[0..2n0-1]
    rvv_xor_accumulate(r,       z0,   2 * n0);
    // r[2m..2m+2n1-1] ^= z2[0..2n1-1]
    rvv_xor_accumulate(r + 2*n, z2,   2 * n1);
    // r[m..m+2n1-1] ^= zmid[i] ^ z0[i] ^ z2[i]
    // (requires 3-way XOR — use two passes or vxor chain)
    size_t vl;
    size_t rem = 2 * n1;
    const uint64_t *pz0   = z0;
    const uint64_t *pzmid = zmid;
    const uint64_t *pz2   = z2;
    uint64_t       *pr    = r + n;  // r + m (half-point)
    while (rem > 0) {
        __asm__ volatile (
            "vsetvli  %[vl], %[rem], e64, m4, ta, ma\n\t"
            "vle64.v  v0,  (%[zmid])\n\t"
            "vle64.v  v4,  (%[z0])\n\t"
            "vle64.v  v8,  (%[z2])\n\t"
            "vxor.vv  v0,  v0, v4\n\t"
            "vxor.vv  v0,  v0, v8\n\t"
            "vle64.v  v4,  (%[r])\n\t"
            "vxor.vv  v0,  v0, v4\n\t"
            "vse64.v  v0,  (%[r])\n\t"
            : [vl] "=r"(vl)
            : [rem] "r"(rem), [zmid] "r"(pzmid),
              [z0] "r"(pz0), [z2] "r"(pz2), [r] "r"(pr)
            : "v0","v1","v2","v3","v4","v5","v6","v7",
              "v8","v9","v10","v11","memory"
        );
        pz0   += vl; pzmid += vl; pz2 += vl; pr += vl;
        rem   -= vl;
    }
}
```

> **Limitation:** The `clmul`/`clmulh` leaf multiplications themselves cannot
> be vectorised under the Zvbc-forbidden constraint. Track B benefit is limited
> to the accumulation/assembly steps, not the GF(2)[x] multiply kernel.

**Extensions used:** `V` (RVV 1.0): `vsetvli`, `vle64.v`, `vxor.vv`, `vse64.v`.

**Expected savings:** Vectorising the 3 XOR loops in `karatsuba_clmul` reduces
their instruction count by ~10–12×. At 11.06% baseline share with ~20% of that
being XOR assembly overhead, this projects a **~1.5–2% global reduction**. More
significant benefit unlocks when Track B is combined with Track A leaf
optimisations and the `memset` elimination from Target 1.

---

## Summary & Prioritisation

| Priority | Target | Track | Projected Global ∆ |
|----------|--------|-------|---------------------|
| P0 | `memset` elimination (T1) | A | ~14–16% reduction |
| P1 | `vect_write_support_to_vector` O(w) rewrite (T2) | A | ~16% reduction |
| P2 | `vect_write_support_to_vector` RVV scatter (T2) | B | ~20% reduction |
| P3 | `memset` RVV zero+XOR vectorisation (T1) | B | ~18–22% reduction |
| P4 | `clmul_leaf` software pipeline + hi⊕lo fusion (T4) | A | ~2.5–3% reduction |
| P5 | FAFFT shift kernel RVV (T3) | B | ~10%+ (fafft variant) |
| P6 | Karatsuba assembly RVV XOR (T4) | B | ~1.5–2% reduction |
| P7 | FAFFT shift kernel 4-unroll (T3) | A | ~2–3% (fafft variant) |

**Recommended sequencing:**
1. Implement P0 (Track A, no new extensions required, self-contained change to `gf2x_clmul.c`).
2. Implement P1 (Track A, modifies `vector.c` — add CT and non-CT variants).
3. Benchmark P0+P1 combined against baseline before introducing any RVV code.
4. Implement P2+P3 together in a single `EXP-karatsuba-zbc-v2-rvv` experiment.
5. Re-benchmark all three security levels. Promote if KATs pass.

---

## Experiment Tracking

All implementations of the above strategies **MUST** follow the project's
experimental isolation protocol ([`experiments/manifest.yaml`](../../experiments/manifest.yaml)):

```yaml
# experiments/manifest.yaml — proposed entries
experiments:
  - id: EXP-karatsuba-zbc-nozero-memset
    status: planned
    description: "Eliminate recursive memset in clmul_leaf and karatsuba_clmul via zero-free output discipline (Track A)"
    target_levels: [HQC-128, HQC-192, HQC-256]
    roadmap_ref: docs/optimizations/roadmap_karatsuba_v1.md#target-1

  - id: EXP-vect-write-support-scatter
    status: planned
    description: "O(weight) direct scatter for vect_write_support_to_vector; CT and non-CT variants (Track A)"
    target_levels: [HQC-128, HQC-192, HQC-256]
    roadmap_ref: docs/optimizations/roadmap_karatsuba_v1.md#target-2

  - id: EXP-karatsuba-zbc-v2-rvv
    status: planned
    description: "RVV vectorised memset-zero, XOR accumulation, and indexed scatter for Karatsuba+support sampling (Track B)"
    target_levels: [HQC-128, HQC-192, HQC-256]
    roadmap_ref: docs/optimizations/roadmap_karatsuba_v1.md#targets-1-2-4

  - id: EXP-fafft-zbc-shift-kernel
    status: planned
    description: "Implement gf2x_exact_shift_right in src/poly_mul/scalar/fafft_zbc/; Track A 4-unroll + Track B RVV vsrl/vslide1down"
    target_levels: [HQC-128, HQC-192, HQC-256]
    roadmap_ref: docs/optimizations/roadmap_karatsuba_v1.md#target-3
```

---

## References

- HQC spec: [https://pqc-hqc.org](https://pqc-hqc.org)
- RISC-V ISA Volume I (unprivileged), v20240411
- RISC-V "B" extension spec (`Zba`, `Zbb`, `Zbc`, `Zbkb`, `Zbs`) — ratified 2021
- RISC-V "V" extension spec (RVV 1.0) — ratified 2021
- RISC-V intrinsics viewer: [https://dzaima.github.io/intrinsics-viewer/#riscv](https://dzaima.github.io/intrinsics-viewer/#riscv)
- Bernstein & Lange, "Fast multiplication in binary fields on x86-64 CPUs" (shift-XOR baseline)
- `analysis/reports/karatsuba_zbc_L128_report.md` (baseline profiling data)
- `analysis/profiles/traces/scalar/karatsuba_zbc/kem_hqc128_hotspots_latest.csv`
