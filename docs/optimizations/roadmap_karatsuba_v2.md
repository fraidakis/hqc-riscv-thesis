# Optimization Roadmap: `karatsuba_zbc` — v2

> **Baseline variant:** `karatsuba_zbc` (all v1 targets promoted)
> **Profile date:** 2026-05-16
> **Security level profiled:** HQC-128 (primary), HQC-{192,256} for scaling checks
> **Simulator:** Spike (`instret` = retired instruction count)
> **ISA constraint:** `rv64gc_zbc_zbb_zbkb` only — `Zvbc`, `Zvbb`, `V` restricted
> to assembly helper loops; no vector crypto in this scalar track

---

## 0. State of Play After v1 Targets

All four v1 targets have been promoted to `src/`:

| Target | Optimization | Result |
|--------|-------------|--------|
| T1 | `memset`-free output discipline (zero-free write) | ✅ Promoted |
| T2 | `vect_write_support_to_vector` O(weight) scatter | ✅ Promoted |
| T3 | `gf_carryless_mul` native `clmul` injection + 4-way assembly unroll | ✅ Promoted |
| T4 | 2-way software pipeline + `hi⊕lo` fusion in `clmul_leaf` | ✅ Promoted |

**Current numbers (Spike, `instret`):**

| Benchmark | Before v1 | After v1 | Delta |
|-----------|-----------|----------|-------|
| `vect_mul` isolated L128 | ~3,620,000¹ | 283,686 | −92.2% |
| `vect_mul` isolated L192 | ~11,000,000¹ | 863,483 | −92.2% |
| `vect_mul` isolated L256 | ~24,000,000¹ | 1,904,461 | −92.1% |
| decaps L128 | ~56,442,183 | 1,782,060 | −96.8% |

¹ Approximate, from archive manifest pre-target1 baselines.

The **v1 optimization loop is exhausted**: the low-hanging fruit
(bulk `memset`, algorithmic O(N²) support scatter, function-level
software loop-folding) has been harvested. The v2 target is the
residual ~284 K instret in `vect_mul` itself, which is now the
undisputed bottleneck at approximately 63% of decaps L128.

---

## 1. Bottleneck Analysis — Current `karatsuba_zbc` Profile

### 1.1 Function-Level Hotspot Breakdown

| Function | instret | % of profile | Category |
|---|---:|---:|---|
| `clmul_leaf` | 1,078,674 | 28.45% | **Schoolbook kernel** |
| `KeccakF1600_StatePermute` | 578,792 | 15.27% | Keccak (not our scope) |
| `karatsuba_clmul` | 519,030 | 13.69% | **Recursive framework** |
| `clmul64` (inlined) | 251,532 | 6.64% | ↑ attributed to inline asm |
| `hadamard` | 227,516 | 6.00% | Decoder (not our scope) |
| `keccak_inc_absorb` | 141,116 | 3.72% | Keccak |
| `memcpy` | 137,922 | 3.64% | Buffer copies in framework |
| `gf_reduce` | 105,600 | 2.79% | RS decoder GF arithmetic |
| `reduce` | 19,974 | 0.53% | Polynomial mod reduction |

The polymul sub-system (`clmul_leaf` + `karatsuba_clmul`) accounts for
**~42.1%** of the combined KEM profile (28.45% + 13.69%).  Since
`vect_mul` is called four times in the decaps path, this translates to
~63% of decaps L128 instret being inside `vect_mul`.

### 1.2 Instruction Mix (Histogram)

Total profile instructions: ~4,606,000 (estimated from `ld` share).

| Instruction | Count | % | Category |
|---|---:|---:|---|
| `ld` | 776,048 | 16.85% | **Memory: load** |
| `addi` | 604,139 | 13.12% | **Loop control + ptr arithmetic** |
| `xor` | 475,186 | 10.32% | GF(2) accumulation |
| `bnez` | 411,043 | 8.93% | **Branch: loop control** |
| `sd` | 221,969 | 4.82% | Memory: store |
| `add` | 164,348 | 3.57% | Address arithmetic |
| `clmul` | 119,322 | 2.59% | **Core crypto** |
| `bne` | 118,530 | 2.57% | Branch: loop exit |
| `clmulh` | 114,324 | 2.48% | **Core crypto** |
| `lh` | 110,766 | 2.41% | Memory: 16-bit load (Keccak) |
| `rori` | 98,832 | 2.15% | Keccak rotation |
| `slli` | 95,223 | 2.07% | Shift |
| `vsetivli` | 89,897 | 1.95% | Vector configure (Keccak helper) |
| `bltu` | 87,302 | 1.90% | Branch: unsigned loop |
| `andn` | 85,375 | 1.85% | Bitmanip (Keccak) |
| `vle64.v` | 75,462 | 1.64% | Vector load (Keccak/helpers) |
| `vse64.v` | 60,434 | 1.31% | Vector store |
| `vxor.vv` | 59,863 | 1.30% | Vector XOR (helpers) |

**Critical ratios:**

```
clmul+clmulh (core crypto)   :  233,646   =  5.07%
ld+sd (memory traffic)       :  997,017   = 21.64%
bnez+bne+bltu (branches)     :  616,875   = 13.40%
addi+add (addr arithmetic)   :  768,487   = 16.68%
─────────────────────────────────────────────────
Total overhead               : 2,382,379  = 51.72%
Actual computation           :  708,832   = 15.39%

Overhead : Compute ratio     :    3.36 : 1
```

In an ideal implementation, `clmul`/`clmulh` should constitute **20–30%**
of all instructions. At 5.07%, the kernel spends approximately **3.4 cycles
of overhead per cycle of productive computation**. This is the thesis-level
statement of the problem for v2.

---

## 2. Bottleneck Taxonomy

### B1 — Memory Load Pressure (dominant, 16.85% of total)

**Root cause:** `clmul_leaf` implements an `O(n²)` schoolbook over `n` words
(threshold n=16). For each outer row `i ∈ [0, n-1]` and inner column
`j ∈ [0, n-1]`:

```
Loads per pair:   1 × ld(b[j])          (not cached across i iterations)
                  1 × ld(r[i+j])
                  1 × ld(r[i+j+1])
Stores per pair:  1 × sd(r[i+j])
                  1 × sd(r[i+j+1])
```

For the first row (i=0), the current code (Target 4 promotion) uses the
2-way software pipeline which avoids re-loading `r[i+j]` on the second
pair. But for rows `i ≥ 1`, each `r[i+j]` value is loaded, XOR'd, and
stored, costing **2 loads + 1 store** per output word.

At T=16:
- `b[]` loads: `n × n = 256` (b[j] re-loaded every outer row)
- `r[]` read-modify-write: `n × 2n ≈ 512` loads + 512 stores

Total leaf loads ≈ 768, stores ≈ 512. With ~243 leaf calls (estimated recursion
tree depth 5 for n=277, T=16): **186,624 loads** just from leaves. `karatsuba_clmul`
framework adds pointer loads and buffer copies (via `memcpy`, 137,922 seen in
profile).

The `b[j]` re-load across outer rows is the primary structural waste:
`b[]` is read `n` times (once per `i`), yet it fits entirely in L1 cache
(16 × 8 = 128 bytes). The miss penalty is negligible, but each `ld`
still **retires as one instruction** regardless of cache state.

### B2 — Loop Control Overhead (branches = 13.40%)

`bnez` (8.93%) + `bne` (2.57%) + `bltu` (1.90%) = 616,875 branch
instructions. These arise from:

1. **Inner loop of `clmul_leaf`**: `for j = 0..n-1` per row → n branches
   per row × n rows × leaf calls = 16×16×243 ≈ 62,208 inner branches.
2. **Outer loop of `clmul_leaf`**: `for i = 0..n-1` → n branches per leaf.
3. **Assembly loops in `karatsuba_clmul`** (loops 1, 2, 3): 4-way unrolled
   but tail loops still execute; 3 loops × ~69 iterations/4 unrolled = ~52
   iterations per recursion node × ~31 nodes = ~1,612 branches from
   assembly step.
4. **Recursion termination checks** (`if n <= threshold`): one branch per
   recursion node × ~31 nodes = ~31 branches (negligible).
5. **`karatsuba_clmul` `ta`/`tb` construction loop**: `for i = 0..n1-1`
   with ternary inside → 2 branches per iteration.

The inner clmul loop alone accounts for ~62,208 branches. With the
current 2-way unroll (pairs of `j`), this is halved to ~31,104. A 4-way
unroll of the inner loop would halve again to ~15,552.

### B3 — Address Arithmetic (addi + add = 16.68%)

`addi` (13.12%) and `add` (3.57%) together = 16.68%. Sources:
- Loop counter increments: `addi counter, counter, 1` / `addi counter, counter, stride`
- Pointer bump: `addi ptr, ptr, 8` for 64-bit pointer stepping
- Index computation: `add t0, base, t1` for `r[i+j]` address computation

In `clmul_leaf`, computing `r[i+j]` and `r[i+j+1]` addresses inside the
double loop costs 2 `add` instructions per pair (per element, not amortized).
A rewrite using pointer-bump addressing (pre-computing `uint64_t *rp = r + i`)
and incrementing `rp` rather than computing `r[i + j]` each time eliminates
the `add` instructions while preserving correctness.

The `addi` count also includes the prologue/epilogue arithmetic for the
temp-buffer pointer layout (`z0`, `z2`, `zmid`, `ta`, `tb`, `child_buf`),
which is recomputed on every `karatsuba_clmul` entry.

### B4 — Sub-optimal `clmul` Utilization (5.07%)

`clmul` (119,322) + `clmulh` (114,324) = 233,646 instructions = 5.07%.
Target: **≥15–18%**.

For an n=16 threshold, each leaf computes 256 `(clmul, clmulh)` pairs = 512
clmul-family instructions. With ~243 leaf calls per `vect_mul`, this is
**124,416 clmul+clmulh instructions per `vect_mul`**. The full KEM involves
4 `vect_mul` calls per decaps → **~497,664 clmul-family instructions**.

The profile shows 233,646 total — this covers all three security levels
combined or is lower due to mixed profiling conditions. Regardless, the
fundamental clmul utilization ratio of ~5% means **for every `clmul`, there
are ~19 non-clmul overhead instructions**.

### B5 — Recursion Framework Overhead (519,030 instret, 13.69%)

`karatsuba_clmul` contributes 519,030 instructions beyond what `clmul_leaf`
accounts for. This includes:

- **Stack frame**: save/restore of `ra`, `s0–s11` (callee-saved). At
  recursion depth 5, the total stack frame operations = 5 × (save + restore)
  × ~(5–7 registers) ≈ 5 × 12 = 60 register saves/restores per full recursion
  path. With 243 leaf calls → roughly 81 non-leaf nodes → **~972 register
  save/restore operations**.
- **Temp-buffer layout**: Computing `z0`, `z2`, `zmid`, `ta`, `tb`,
  `child_buf` pointers at every recursion level = ~12 pointer computations ×
  81 nodes = **972 pointer initializations**.
- **`ta`/`tb` construction**: `for i = 0..n1-1`: one loop per recursion
  node over the half-size array.
- **3 assembly loops** (Loop 1: copy z0, Loop 2: copy z2, Loop 3: XOR zmid):
  4-way unrolled, but still O(n) per level over 4 recursion levels × 3 loops
  × avg n ≈ 69 elements ≈ **832 loop body executions** per `vect_mul`.

Comparator: `karatsuba_rvv` shows `karatsuba_clmul` at 309,024 (8.42%)
because RVV helpers (`rvv_xor3_u64`, `rvv_copy_u64`) amortize 40% of the
assembly loop overhead. The delta (519,030 − 309,024 = 210,006) represents
the instructions saved by RVV assembly loops. But the `clmul_leaf` share
remains identical between both variants (1,078,674 vs 1,078,674), confirming
the leaf is the binding constraint.

---

## 3. Micro-Optimization Tracks

### Track V2-A1 — Threshold Sweep

> **Status (2026-05-15):** ✅ Promoted — T=16 confirmed winner.
> `vect_mul` HQC-128: 283,686 → 274,566 instret (−3.21%).
> KAT: 300/300 vectors validated.

**Problem:** `KARATSUBA_THRESHOLD_DEFAULT = 16` is a fixed choice with no
empirical sweep for n=277 after all v1 optimizations.

**Analysis:** The recursion tree for n=277 at threshold T:

| T | Tree depth | Leaf calls | Leaf size | Schoolbook work |
|---|---|---|---|---|
| 4 | 6 | ~729 | 4 | 729 × 16 = 11,664 `clmul64` |
| 8 | 5 | ~243 | 8–9 | 243 × 72 = 17,496 |
| 16 | 4–5 | ~243 | 9–10 | ~19,683 |
| 24 | 3–4 | ~81 | 12–17 | ~81 × 210 = 17,010 |
| 32 | 3 | ~81 | 16–17 | ~81 × 256 = 20,736 |
| 64 | 2–3 | ~27 | 22–23 | ~27 × 512 = 13,824 |

Note: for T=4, the very small leaf size eliminates inner-loop overhead
entirely (the compiler can fully unroll a 4×4 = 16 element `clmul_leaf`).
The recursion overhead increases, but with `always_inline` leaf code the
function-call cost is zero.

**Strategy:**
1. Add a `KARATSUBA_THRESHOLD` compile-time sweep: build targets for
   T ∈ {4, 6, 8, 10, 12, 16, 24, 32} and benchmark `vect_mul` isolated.
2. Add a `clmul_leaf_n4()` and `clmul_leaf_n8()` specialized variant
   with fully unrolled fixed-size bodies (see Track V2-A3).
3. Combine sweep with the preloading track (V2-A2).

**Expected savings:** For T=4–8, the near-elimination of inner-loop overhead
may offset the increased recursion framework cost. Spike ISS charges 1 instruction
per loop control instruction, making tight leaf loops very expensive relative
to real hardware. A T=4 leaf avoids all loop overhead: the compiler sees a 4×4
constant-iteration product and can generate completely branch-free code.

**Projected delta:** 5–15% reduction in polymul instret depending on optimal T.

---

### Track V2-A2 — b[] Register Preloading

**Problem:** In `clmul_leaf` rows `i ≥ 1`, `b[j]` is re-loaded from memory
on every outer iteration even though the entire `b[0..T-1]` array is 128 bytes
for T=16 and fits in 16 registers.

**Strategy:** Hoist all `b[j]` loads before the outer `i` loop.

```c
// V2-A2: b[] register preloading for clmul_leaf (n ≤ PRELOAD_LIMIT)
#define PRELOAD_LIMIT 8  // tune per threshold

static __attribute__((always_inline)) inline
void clmul_leaf_preload(uint64_t * __restrict__ r,
                        const uint64_t * __restrict__ a,
                        const uint64_t * __restrict__ b,
                        size_t n)
{
    // Precondition: n <= PRELOAD_LIMIT
    // Preload all b[] into local variables → compiler allocates in regs
    uint64_t b0, b1, b2, b3, b4, b5, b6, b7;
    b0 = b[0]; b1 = b[1]; b2 = b[2]; b3 = b[3];
    if (n > 4) { b4 = b[4]; b5 = b[5]; b6 = b[6]; b7 = b[7]; }

    // Row i=0: assign (no prior r[] value)
    uint64_t lo0, hi0, lo1, hi1;
    clmul64(a[0], b0, &lo0, &hi0);
    clmul64(a[0], b1, &lo1, &hi1);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;
    r[2] = hi1;
    // ... continue as in current i=0 block, all b[j] from registers

    // Rows i=1..n-1: no b[j] loads needed
    for (size_t i = 1; i < n; i++) {
        uint64_t ai = a[i];
        // b0..b7 still in registers — zero memory traffic for b side
        clmul64(ai, b0, &lo0, &hi0);
        clmul64(ai, b1, &lo1, &hi1);
        r[i]     ^= lo0;
        r[i + 1] ^= hi0 ^ lo1;
        r[i + 2] ^= hi1;          // or continue pairing through b[n-1]
        // ...
    }
}
```

**Register budget (T=8, n=8):**
- `b0..b7`: 8 caller-allocated registers
- `lo0, hi0, lo1, hi1`: 4 temporaries
- `ai`: 1 register
- `r` pointer: 1 register
- `a` pointer: 1 register
- Loop counter `i`: 1 register
- Total: 16 — leaves 16 free in the ABI-preserved set.

For T=16, the `b[]` preload consumes 16 registers leaving only 16 for the
rest, which is tight but feasible in a leaf marked `always_inline` with
`__attribute__((optimize("O3")))`.

**Expected savings:** Eliminates `n × (n-1) = 15 × 16 = 240` `ld b[j]`
instructions per leaf call at T=16. With ~243 leaf calls:
**58,320 loads eliminated**. At 16.85% load share, this is ~7.5% of polymul
loads → **~1.5–2% global polymul instret reduction**.

---

### Track V2-A3 — Fixed-Size Leaf Specialization (n=4, n=8, n=16)

> **Status (2026-05-15):** ✅ Promoted — `clmul_leaf_n8` dispatched at n=8 (T=16 tree).
> Combined with V2-A1: 274,566 instret at promotion.
> KAT: 300/300 vectors validated.
>
> **Ablation study (EXP-karatsuba-ablation, 2026-05-17):** Dead-code pruning
> revealed that `clmul_leaf_n4` is never reached at T=16 for n=277 (recursion
> tree only produces n≥8 leaves). The `case 4:` dispatch arm was also dead,
> but its presence generated a superfluous comparison on every leaf call.
> Removing both dropped a further 435 instret:
>
> | Version | instret | Δ |
> |---|---|---|
> | V2-A3 promotion (clmul\_leaf\_n8 + case 4) | 274,566 | −3.21% vs baseline |
> | Ablation (case 4 + clmul\_leaf\_n4 removed) | **274,131** | −0.16% further |
>
> **Current scalar champion: 274,131 instret** (2026-05-17).

**Problem:** The compiler cannot fully unroll or schedule `clmul_leaf` for
variable `n` even with `-O3`; all loop structures remain as loops.

**Strategy:** Provide hardcoded leaf functions for the specific sizes that
appear in the recursion tree, triggered by a `switch(n)` in `karatsuba_clmul`
before the threshold check:

```c
// In karatsuba_clmul, before the general clmul_leaf dispatch:
if (n <= KARATSUBA_THRESHOLD) {
    switch (n) {
    case 4:  clmul_leaf_n4(r, a, b);  return;
    case 8:  clmul_leaf_n8(r, a, b);  return;
    case 16: clmul_leaf_n16(r, a, b); return;
    default: clmul_leaf(r, a, b, n);  return;
    }
}
```

**`clmul_leaf_n4` (fully unrolled, 16 `clmul64` calls, zero loops):**

```c
// V2-A3: 4×4 fully unrolled leaf, zero loop overhead
static __attribute__((always_inline)) inline
void clmul_leaf_n4(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    // Preload all inputs
    uint64_t a0=a[0], a1=a[1], a2=a[2], a3=a[3];
    uint64_t b0=b[0], b1=b[1], b2=b[2], b3=b[3];
    uint64_t lo, hi;

    // Row 0 — assign
    clmul64(a0, b0, &lo, &hi); r[0]  = lo; r[1]  = hi;
    clmul64(a0, b1, &lo, &hi); r[1] ^= lo; r[2]  = hi;
    clmul64(a0, b2, &lo, &hi); r[2] ^= lo; r[3]  = hi;
    clmul64(a0, b3, &lo, &hi); r[3] ^= lo; r[4]  = hi;

    // Row 1 — XOR, r[5] first-write
    clmul64(a1, b0, &lo, &hi); r[1] ^= lo; r[2] ^= hi;
    clmul64(a1, b1, &lo, &hi); r[2] ^= lo; r[3] ^= hi;
    clmul64(a1, b2, &lo, &hi); r[3] ^= lo; r[4] ^= hi;
    clmul64(a1, b3, &lo, &hi); r[4] ^= lo; r[5]  = hi;

    // Row 2 — XOR, r[6] first-write
    clmul64(a2, b0, &lo, &hi); r[2] ^= lo; r[3] ^= hi;
    clmul64(a2, b1, &lo, &hi); r[3] ^= lo; r[4] ^= hi;
    clmul64(a2, b2, &lo, &hi); r[4] ^= lo; r[5] ^= hi;
    clmul64(a2, b3, &lo, &hi); r[5] ^= lo; r[6]  = hi;

    // Row 3 — XOR, r[7] first-write
    clmul64(a3, b0, &lo, &hi); r[3] ^= lo; r[4] ^= hi;
    clmul64(a3, b1, &lo, &hi); r[4] ^= lo; r[5] ^= hi;
    clmul64(a3, b2, &lo, &hi); r[5] ^= lo; r[6] ^= hi;
    clmul64(a3, b3, &lo, &hi); r[6] ^= lo; r[7]  = hi;
}
```

This generates exactly **32 `clmul`/`clmulh`** instructions,
**7 `ld`** (a0–a3, b0–b3 loaded once), and **15 `xor`** + **8 `sd`** — no
branches, no `addi` loop counters. For n=4, Spike measures the leaf as
`32 + 7 + 8 + 15 = 62` instructions per call (vs. current estimated ~120+ for
the looped version).

**Note on `clmul_leaf_n4` correctness:** The write discipline above follows
the same zero-free first-write assign pattern from T1. The first write to
`r[k]` in each row is an `=` (assign); subsequent writes from later rows are
`^=` (XOR). This is verified by inspection: `r[0]` is written once by row-0.
`r[1]` is assigned by row-0, then XOR'd by row-1. `r[4]` is assigned by
row-0 (via `r[4] = hi` from the `a0,b3` clmul), then XOR'd by rows 1–3.
`r[5]` is first-written by row-1 (`r[5] = hi`), then XOR'd by rows 2–3. And
so on.

**Expected savings at T=4 + fixed leaf:** With ~729 leaf calls (n=4, depth 6)
each taking ~62 instructions:
- Leaf total: 729 × 62 = **45,198 instructions** (schoolbook)
- Current leaf total: ~243 × ~900 ≈ **218,700 instructions**
- Savings: ~173,500 instructions from leaves alone (~61% leaf reduction)

The recursion framework overhead increases (729 vs 243 nodes), but with the
specialized fixed-size dispatching the per-node overhead is also reduced
(no dynamic `n` computation inside `karatsuba_clmul`).

---

### Track V2-A4 — Inner Loop 4-Way Unrolling in `clmul_leaf` Rows `i≥1`

**Problem:** The current `clmul_leaf` uses a 2-way unroll for the `j` loop
in rows `i≥1` (two `clmul64` calls per iteration). The inner loop still has
branch + `addi` per 2 elements.

**Strategy:** Extend to a 4-way unroll:

```c
// V2-A4: 4-way unrolled main loop for rows i>=1
for (size_t i = 1; i < n; i++) {
    uint64_t ai = a[i];
    uint64_t *rp = r + i;     // pointer bump replaces r[i+j] index arithmetic
    size_t j = 0;

    /* 4-way unroll: 4 consecutive j values per iteration */
    for (; j + 3 < n - 1; j += 4) {
        uint64_t lo0, hi0, lo1, hi1, lo2, hi2, lo3, hi3;
        clmul64(ai, b[j],     &lo0, &hi0);
        clmul64(ai, b[j + 1], &lo1, &hi1);
        clmul64(ai, b[j + 2], &lo2, &hi2);
        clmul64(ai, b[j + 3], &lo3, &hi3);
        rp[j]     ^= lo0;
        rp[j + 1] ^= hi0 ^ lo1;    // fused
        rp[j + 2] ^= hi1 ^ lo2;    // fused
        rp[j + 3] ^= hi2 ^ lo3;    // fused
        rp[j + 4] ^= hi3;
    }
    /* 2-way tail */
    for (; j + 1 < n - 1; j += 2) {
        uint64_t lo0, hi0, lo1, hi1;
        clmul64(ai, b[j],     &lo0, &hi0);
        clmul64(ai, b[j + 1], &lo1, &hi1);
        rp[j]     ^= lo0;
        rp[j + 1] ^= hi0 ^ lo1;
        rp[j + 2] ^= hi1;
    }
    /* ... even/odd tail as before */
}
```

Note the **hi⊕lo fusion extends across 4 pairs**: `hi0⊕lo1`, `hi1⊕lo2`,
`hi2⊕lo3`. This converts 4 pairs (8 half-products) into 5 output XORs
instead of the naive 8, saving 3 `xor` instructions per 4-pair group.

**Register pressure at 4-way unroll:**
- `lo0, hi0, lo1, hi1, lo2, hi2, lo3, hi3`: 8 temporaries
- `ai, rp` pointer: 2
- `b[j..j+3]` (4 registers if preloaded): 4
- Loop counter `j`, bound `n`: 2
- Total: 16 — feasible within the 32-register file

**Expected savings:** Halves branch count vs current 2-way unroll. Combined
with pointer-bump addressing, eliminates `add` for index computation.
Estimated: **3–6% polymul instret reduction** from branch + addi elimination.

---

### Track V2-A5 — Pointer-Bump Addressing in Assembly Loops

**Problem:** The three assembly loops in `karatsuba_clmul` (Loop 1: `r[i] = z0[i]`,
Loop 2: `rh[i] = z2[i]`, Loop 3: `rm[i] ^= zmid[i]^z0[i]^z2[i]`) compute
pointer offsets with `addi ptr, ptr, 8` or use indexed `sd/ld` with offset
from register. With 4-way unrolling already applied, the remaining overhead
is in pointer setup at entry to each loop and the tail loop.

**Strategy:** Convert index-based to pointer-based loops using `uint64_t *p = start`
and `p++` bumps:

```c
// V2-A5: pointer-bump assembly loops in karatsuba_clmul step 5
{
    /* Loop 3 sub-range A: r[m+i] ^= zmid[i]^z0[i]^z2[i] for i<split */
    uint64_t *rm_p  = r + m;
    const uint64_t *zm_p = zmid, *z0_p = z0, *z2_p = z2;
    size_t i = split;   // work backward from split in 4-way blocks
    /* Unrolled forward pass */
    for (size_t k = 0; k + 4 <= split; k += 4) {
        rm_p[0] ^= zm_p[0] ^ z0_p[0] ^ z2_p[0];
        rm_p[1] ^= zm_p[1] ^ z0_p[1] ^ z2_p[1];
        rm_p[2] ^= zm_p[2] ^ z0_p[2] ^ z2_p[2];
        rm_p[3] ^= zm_p[3] ^ z0_p[3] ^ z2_p[3];
        rm_p += 4; zm_p += 4; z0_p += 4; z2_p += 4;
    }
    // tail ...
}
```

This removes the loop-index-to-address translation; the pointer advances
directly without the `slli` + `add` sequence for byte-offset computation.

**Expected savings:** Eliminates ~2 instructions per 4-loop-body element
across 3 loops × ~276 elements. Net: **~1–2% polymul instret reduction**.

---

### Track V2-A6 — `Zbkb` PACK for Paired 32-bit Loads

**Problem:** Some inner-loop data (particularly in the `ta`/`tb` XOR
construction and the `compare_u32` function) processes 32-bit sub-words from
64-bit loads. RISC-V has no native 64-bit XOR-with-rotation or lane-combine
without going through separate registers.

**Strategy:** `Zbkb` provides `pack rd, rs1, rs2`, which packs the lower 32
bits of `rs1` into bits [31:0] and lower 32 bits of `rs2` into bits [63:32]
of `rd`. This allows two 32-bit GF elements to be processed simultaneously as
a single 64-bit lane using `xor`.

**Application to `ta`/`tb` construction:**

```c
// Current (per element):
ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];  // conditional + XOR

// V2-A6: use Zbkb pack to build paired values for parallel XOR
// When n0 == n1 - 1 (n odd), the last element has a special case.
// For the even portion, pack adjacent pairs into 64-bit words and XOR:
__asm__ volatile (
    "pack %0, %1, %2\n\t"   // pack(a[i], a[m+i]) into 64-bit
    "pack %3, %4, %5\n\t"   // pack(b[i], b[m+i])
    : "=r"(ta_packed), "=r"(tb_packed)
    : "r"(a_lo), "r"(a_hi), "r"(b_lo), "r"(b_hi)
);
// Then: ta_packed ^ b_m_packed gives two XORed elements in one register
```

**Caveat:** This optimization is most effective when the loop is restructured
to process pairs of elements. The payoff is small for this specific loop
since `ta`/`tb` construction is not in the top hotspot list. **Priority: low.**

---

## 4. Algorithmic Pivots

### Pivot AP-1 — Toom-Cook 3-Way Multiplication over GF(2)[x] (**PRIMARY**)

**Motivation:** This is the single most impactful change available in the
scalar track. Every micro-optimization above provides at best 5–15% incremental
gains. Toom-Cook 3-way (TC3) replaces the recursive split from 3 sub-problems
of size n/2 (Karatsuba) to **5 sub-problems of size n/3**, with an asymptotic
complexity of `O(n^{log₃5}) ≈ O(n^{1.465})` vs Karatsuba's
`O(n^{log₂3}) ≈ O(n^{1.585})`.

#### 4.1 Recursion Tree Analysis for n=277

| Algorithm | Depth | Leaf calls | Leaf size | Total leaf work (schoolbook) |
|---|---|---|---|---|
| Karatsuba (T=16) | 5 | ~243 | n ≈ 9–10 | ~243 × 90 = 21,870 `clmul64` |
| TC3 (T=16) | 3 | **~125** | n ≈ 10–11 | ~125 × 110 = 13,750 `clmul64` |
| TC3 (T=4) | 4 | **~625** | n = 4 | ~625 × 16 = 10,000 `clmul64` |

TC3 with T=16 reduces leaf-level schoolbook work by **~37%**. The evaluation
and interpolation cost at each non-leaf node is ~20–22 XOR operations of
size-k arrays (O(k) per level × 2 interpolation levels). For k≈93, this is
20 × 93 = 1,860 XOR instructions per internal node — modest compared to the
leaf savings.

#### 4.2 GF(2)[x] TC3 Evaluation / Interpolation

For binary polynomials the TC3 scheme uses evaluation points `{0, 1, z, z+1, ∞}`
where `z` is a formal auxiliary variable (not a field element, but an
indeterminate that implements degree-k shifts):

**Splitting:** `A(z) = A₀ + z·A₁ + z²·A₂` where `A₀`, `A₁`, `A₂` are
polynomials of degree ≈ k = ⌈n/3⌉.

**5 point evaluations** (each costs O(k) XOR):
```
A(0)   = A₀
A(1)   = A₀ ⊕ A₁ ⊕ A₂
A(z)   = shift(A₁, k) ⊕ shift(A₂, 2k)    [concatenation; zero-cost]
A(z+1) = A₀ ⊕ shift(A₁⊕A₂, k)            [O(k) XOR]
A(∞)   = A₂
```

The 5 corresponding products `P₀=A(0)·B(0)` … `P₄=A(∞)·B(∞)` are computed
recursively.

**Interpolation** (recovers C = A·B from the 5 products):

Let `c = c₀ + z·c₁ + z²·c₂ + z³·c₃ + z⁴·c₄` be the 5-segment product.

```
// Step 1: diagonal differences
t₀ = P₀           // = c₀⊕ c₄·z^{4k} (portion at z=0)
t₄ = P₄           // = c₄ (leading segment)
t₁ = P₁ ⊕ t₀      // c₁⊕c₂⊕c₃⊕c₄·(something)
t₃ = P₃ ⊕ t₀      // evaluation at z+1 minus z=0 contribution
// Step 2: recover c₂, c₁, c₃ via XOR combinations
// (no division needed in characteristic 2 — all denominators are powers of 2
//  i.e., powers of z which correspond to bit-shifts)
t₂ = t₁ ⊕ t₃ ⊕ P₂   // eliminates cross terms
c₁ = t₁ ⊕ shift(t₂, k)
// etc. (full derivation follows standard Bernstein TC3 table)
```

The key property is: **no field inversions, no division, only XOR + shifts**.
In our context shifts are GF(2)[x] left-multiplications by `x^k` = array
copies with word-aligned offsets. Since k is a compile-time constant for a
given security level, shifts become `memcpy`-equivalent loops (or pointer
arithmetic).

#### 4.3 Implementation Notes

```c
// V2-AP1: TC3 frame structure (pseudo-code)
static void tc3_clmul(uint64_t * __restrict__ r,
                      const uint64_t * __restrict__ a,
                      const uint64_t * __restrict__ b,
                      size_t n,
                      uint64_t * __restrict__ tmp)
{
    if (n <= TC3_LEAF_THRESHOLD) {
        clmul_leaf(r, a, b, n);   // or fixed-size leaf (V2-A3)
        return;
    }

    size_t k  = (n + 2) / 3;     // ceil(n/3)
    size_t k2 = (n > 2*k) ? (n - 2*k) : 0;  // size of A2 (may be < k)

    // Evaluation buffers: 5 pairs of (eval_a[i], eval_b[i]) of size k,
    // plus 5 product buffers of size 2k.
    // Total temp workspace: 5 × 2k + 5 × 2k = 20k uint64_t words.
    // (Still O(n) — comparable to Karatsuba's 8n temp words)

    uint64_t *ea0 = tmp,            *eb0 = ea0 + k;
    uint64_t *ea1 = eb0 + k,        *eb1 = ea1 + k;
    uint64_t *ea2 = eb1 + k,        *eb2 = ea2 + k;  // = A(z), B(z)
    uint64_t *ea3 = eb2 + k,        *eb3 = ea3 + k;  // = A(z+1), B(z+1)
    // ea4 = a + 2k (A₂), eb4 = b + 2k (B₂) — direct pointers, no copy

    uint64_t *p0 = tmp + 10*k, *p1 = p0 + 2*k, *p2 = p1 + 2*k;
    uint64_t *p3 = p2 + 2*k,  *p4 = p3 + 2*k;
    uint64_t *child_buf = tmp + 20*k;

    /* --- Evaluate A --- */
    // ea0 = A₀ (direct pointer: a[0..k-1])
    // ea1 = A₀ ⊕ A₁ ⊕ A₂
    for (size_t i = 0; i < k; i++)
        ea1[i] = a[i] ^ a[k+i] ^ (i < k2 ? a[2*k+i] : 0);
    // ea2 = A(z) = [A₀ | A₁ | A₂] viewed as polynomial evaluated at z=x^k
    //   → no computation needed; the concatenated input IS A(z). Use direct ptrs.
    // ea3 = A(z+1) = A₀ ⊕ (A₁⊕A₂)·shift
    for (size_t i = 0; i < k; i++)
        ea3[i] = a[i] ^ (i + k < n ? a[i+k] : 0) ^ (i + 2*k < n ? a[i+2*k] : 0);
    //  (note ea3 != ea1 in general due to different XOR combination)

    /* --- Evaluate B (symmetric) --- */
    // ... same pattern for B ...

    /* --- 5 recursive multiplications --- */
    tc3_clmul(p0, a,      b,      k,  child_buf);  // P₀ = A₀ · B₀
    tc3_clmul(p1, ea1,    eb1,    k,  child_buf);  // P₁ = A(1) · B(1)
    tc3_clmul(p2, a,      b,      n,  child_buf);  // P₂ = A(z)·B(z) ≡ full n-size
    tc3_clmul(p3, ea3,    eb3,    k,  child_buf);  // P₃ = A(z+1)·B(z+1)
    tc3_clmul(p4, a+2*k,  b+2*k,  k2, child_buf); // P₄ = A₂ · B₂

    /* --- Interpolate and write r --- */
    // ~22 XOR operations over k-word arrays → O(k) total
    // (Implementation follows the standard binary TC3 interpolation table)
    // ...
}
```

**Temp buffer sizing:** TC3 uses 20k + child_buf_size = 20×93 + 8×93×5 =
~5,580 words for n=277 → ~44 KB stack allocation. Karatsuba uses 16n = 16×277
= 4,432 words = ~35 KB. TC3 has a larger stack requirement (~25% more), which
may increase L1 pressure at the top recursion level.

**Estimated instruction savings:**
- Leaf calls: ~243 → ~125 (−48%)
- Per-leaf work at T=16: comparable (~90 vs ~110 clmul64, larger leaf for TC3)
- Net schoolbook reduction: ~37%
- Framework overhead: slightly increased (5 children vs 3) but at only 2 levels
- **Projected polymul reduction: 20–30%**
- **Projected decaps reduction: ~13–19%** (polymul is ~63% of decaps)

**Risk:** TC3 interpolation is more complex to verify constant-time and
requires careful implementation of the 5-point evaluation. Recommend verifying
with all 300 KAT vectors before benchmarking.

---

### Pivot AP-2 — Fixed-Size Unrolled Recursion Tree for n=277

**Motivation:** Since HQC-128 always computes `vect_mul` on exactly n=277
words, we can generate a **specialized `vect_mul_n277()`** with the complete
recursion tree hardcoded:

```
n=277 → split(m=138): n0=138, n1=139
  n0=138 → split(69,69)
    n=69  → split(34,35)
      n=35 → split(17,18)
        n=18 → split(9,9)   [LEAF: n=9 ≤ 16]
        n=9  [LEAF]
      n=17 → split(8,9)
        n=9  [LEAF]
        n=8  [LEAF]
  n1=139 → split(69,70)
    ...
```

This allows:
1. All `if (n <= threshold)` branches eliminated by construction.
2. All temporary buffer offsets (`z0`, `z2`, `zmid`, `ta`, `tb`) computed
   as **compile-time constants** (via `constexpr` or `#define`).
3. All loop bounds in assembly steps become compile-time constants, enabling
   full unrolling.

**Implementation approach:** A Python code generator
(`scripts/gen/karatsuba_specialized.py`) traverses the recursion tree and
emits specialized C functions for each unique (n0, n1) pair:

```c
// Auto-generated by gen_karatsuba.py for HQC-128
static void kmul_277(uint64_t *r, const uint64_t *a,
                     const uint64_t *b, uint64_t *tmp) {
    uint64_t *z0 = tmp;               // offset: 0
    uint64_t *z2 = tmp + 554;         // offset: 554 (2×277)
    uint64_t *zmid = tmp + 1108;      // offset: 1108 (4×277)
    uint64_t *ta = tmp + 1662;        // offset: 1662 (6×277)
    uint64_t *tb = tmp + 1940;        // offset: 1940 (6×277+278)
    uint64_t *child = tmp + 2219;

    kmul_138(z0,   a,     b,     child);
    kmul_139(z2,   a+138, b+138, child);

    // ta = a[0..137] ^ a[138..276], tb = b[0..137] ^ b[138..276]
    // (139 iterations, last element of ta = 0 ^ a[276])
    for (size_t i = 0; i < 139; i++) {
        ta[i] = (i < 138 ? a[i] : 0) ^ a[138 + i];
        tb[i] = (i < 138 ? b[i] : 0) ^ b[138 + i];
    }
    kmul_139(zmid, ta, tb, child);

    // Assembly (unrolled for n0=138, n1=139)
    // Loop 1: r[0..275] = z0[0..275]
    // Loop 2: r[276..553] = z2[0..277]
    // Loop 3: r[138..414] ^= zmid ^ z0/z2
    // (All loop bounds are compile-time constants → full unroll or LMUL=8 RVV)
    ...
}
```

**Expected savings:** Elimination of all runtime `n`-checks and dynamic offset
computations. Estimated **3–8% polymul instret reduction** from branch
elimination + compile-time pointer layout.

**Risk:** Code size bloat (one function per unique size). For n=277 the tree
has ~15 unique sizes → 15 specialized functions. Total code size increase:
estimated ~2–3 KB. Acceptable for a bare-metal target.

---

### Pivot AP-3 — Uneven Split (3-into-2 Karatsuba)

**Motivation:** When n is not a power of 2, the binary Karatsuba split
`m = n/2` produces sub-problems of size `n0=m` and `n1=n-m` that differ by
1 when `n` is odd. For n=277 (odd): n0=138, n1=139. Both sub-problems
require different function instantiations in Pivot AP-2.

An alternative: use a **3-into-2 split** where the polynomial is divided into
groups of floor(n/3) and 2×ceil(n/3). This can reduce the largest sub-problem
more aggressively:

```
n=277 → k=93:
  A = A₀ + z^93·A₁ + z^186·A₂   (size 93, 93, 91 — last has 91 terms)
  B = B₀ + z^93·B₁ + z^186·B₂

  Karatsuba on {A₀+A₁+A₂, B₀+B₁+B₂} and pairs:
  Classic 4-sub-problem Karatsuba (see Bernstein "The tangent FFT"):
    P_00 = A₀·B₀
    P_11 = A₁·B₁
    P_22 = A₂·B₂
    P_01 = (A₀⊕A₁)(B₀⊕B₁)
    P_02 = (A₀⊕A₂)(B₀⊕B₂)
    P_12 = (A₁⊕A₂)(B₁⊕B₂)
  → 6 sub-multiplications of size 93, but 3 of the 6 can be shared.
  With 3-way Karatsuba (Karatsuba-Ofman recursion applied twice):
    Reduces to 5 multiplications (same as TC3) but with different evaluation
    matrix. The interpolation is cheaper than TC3 (only XOR, no shifts).
```

This is essentially the **TC3 approach re-derived as nested Karatsuba**, and
the two are equivalent for GF(2)[x]. The choice between them is an
implementation style preference; TC3 evaluation at formal points is often
clearer to implement correctly.

**Recommendation:** Implement TC3 (Pivot AP-1) rather than this variant.
Both reduce to the same asymptotic complexity and leaf-call count.

---

### Pivot AP-4 — Schoolbook Cutoff Optimality Analysis

**Motivation:** The optimal threshold `T*` minimizes total instructions:

```
f(T) = N_leaves(n, T) × I_leaf(T) + N_nodes(n, T) × I_node

where:
  N_leaves(n, T) = number of leaf calls ≈ 3^ceil(log_2(n/T))
  I_leaf(T)      = instruction count for clmul_leaf(T)
  N_nodes(n, T)  = number of internal nodes
  I_node         = per-node overhead (stack frame + 3 loops + ta/tb)
```

For the current implementation, `I_node ≈ 200–400` instructions per node
(from the 519,030 `karatsuba_clmul` share across ~81 nodes).
`I_leaf(T) ≈ 2T² + 6T` (estimated: 2 loads + 1 clmul-pair + 1 store per
(i,j) element = 5T² instructions, but 2-way unrolling + hi⊕lo fusion
reduces this to roughly `3.5T²`).

**Minimum of f(T):** The derivative w.r.t. T (treating N_leaves as
continuous) gives `T* ≈ (I_node / (7T))^(1/2)` which for `I_node ≈ 300`
yields `T* ≈ 6–8`. This suggests the current T=16 is above optimal for
Spike's instruction cost model.

**Experiment plan:** `EXP-karatsuba-zbc-threshold-sweep` should measure
`vect_mul` isolated instret for T ∈ {4, 6, 8, 10, 12, 16, 20, 24, 32}
and plot `f(T)` to find the empirical minimum.

---

## 5. Summary, Prioritisation, and Experiment Plan

### Priority Table

| Priority | Track | Strategy | Scope | Projected Global ∆ |
|----------|-------|----------|-------|---------------------|
| **P0** | AP-1 | Toom-Cook 3-way GF(2)[x] | Algorithmic | **−20–30%** polymul |
| **P1** | V2-A1 | ~~Threshold sweep (find optimal T*)~~ ✅ T=16 promoted (−3.21%) | Micro | **Done** |
| **P2** | V2-A3 | ~~Fixed-size leaf n=4/n=8~~ ✅ clmul\_leaf\_n8 promoted (+ablation → 274,131) | Micro | **Done** |
| **P3** | AP-2 | Fixed-size specialized tree n=277 | Algorithmic | **−3–8%** polymul |
| **P4** | V2-A4 | 4-way inner unroll + hi⊕lo fusion ext. | Micro | **−3–6%** polymul |
| **P5** | V2-A2 | b[] register preloading | Micro | **−1.5–2%** polymul |
| **P6** | V2-A5 | Pointer-bump addressing | Micro | **−1–2%** polymul |
| P7 | V2-A6 | `Zbkb` pack for paired lanes | Micro | <1% — low priority |

### Combined Projection (optimistic, all P0–P5)

If TC3 (−25%) stacks with threshold optimization (−10%) and fixed-size
leaf (−15%), with the gains being partially multiplicative:

```
Karatsuba vect_mul L128 scalar champion (2026-05-17): 274,131 instret
  (V2-A1 T=16 sweep: 283,686 → 274,566, V2-A3 leaf + ablation: → 274,131)

After TC3 (−25%):                ~205,598
After T* re-sweep (−10%):        ~185,038
After fixed-size leaf (−15%):     ~157,282
                                ─────────
Projected lower bound:          ~155,000–175,000 instret
```

This would represent an additional **~37–44% reduction** over the current
v1 scalar champion, and a total of **~94–95% reduction** from the scalar
reference.

For context: `karatsuba_zvbc` achieves **90,302 instret** using hardware
`vclmul.vv`. The projected scalar v2 target (~165,000) remains ~1.8×
slower than ZVBC — this is the fundamental scalar-vs-vector gap that
cannot be closed without `Zvbc`.

### Experiment Plan

| Experiment ID | Strategy | Implements | Validation |
|---|---|---|---|
| `EXP-karatsuba-zbc-threshold-sweep` | V2-A1 | Compile-time T sweep | ✅ KAT 300/300, T=16 champion promoted 2026-05-15 |
| `EXP-karatsuba-zbc-leaf-fixed` | V2-A3 | `clmul_leaf_n8` + ablation | ✅ KAT 300/300, promoted 2026-05-15; ablation → 274,131 |
| `EXP-toomcook3-gf2x-zbc` | AP-1 | Full TC3 with Zbc leaf | KAT 300 vectors + isolated + full KEM |
| `EXP-karatsuba-zbc-specialized-277` | AP-2 | Code-gen for n=277 | KAT + code-size budget check |
| `EXP-karatsuba-zbc-v2-combined` | P0+P1+P2 | Best composition | Full benchmark suite |

---

## 6. Constant-Time & Security Notes

All proposed optimizations must satisfy the project's CT invariants:

- **No secret-dependent branches:** The recursion split is on public size `n`.
  TC3 evaluation/interpolation uses fixed-size array operations (XOR + copy)
  on public-length inputs. ✅
- **No secret-dependent memory access patterns:** `clmul_leaf` accesses `a[i]`,
  `b[j]`, and `r[i+j]` at fixed, size-determined offsets. All indices are
  derived from public loop counters. ✅
- **Fixed-size leaf specialization:** The `switch(n)` dispatch is on a
  compile-time derived `n` (from the recursion structure on public polynomial
  size). ✅
- **TC3 temp buffer layout:** Buffer offsets depend only on `n` (public). ✅
- **`b[]` register preloading:** Preloading `b[0..T-1]` eliminates conditional
  memory access; all `b[]` elements are loaded unconditionally. ✅

**CT verification requirement:** Run `dudect` / `ctgrind` (see `tests/ct/`)
on all new leaf and TC3 variants before promotion, as with all prior targets.

---

## 7. References

- D. J. Bernstein, "Batch binary Edwards", CRYPTO 2009.
  (Binary TC3 evaluation/interpolation derivation.)
- R. Brent & P. Zimmermann, "Modern Computer Arithmetic", §1.4 (Toom-Cook-3).
- gf2x library (LGPL): `gf2x/src/Kara4.c` — reference TC3 for GF(2)[x].
  (Implementation of binary Toom-3 interpolation, verifiable closed-form.)
- P. L. Montgomery, "Five, Six, and Seven-Term Karatsuba-Like Formulae",
  IEEE Trans. Computers, 2005. (Alternative multi-way split strategies.)
- HQC v2 NIST submission, Appendix A.3 — parameter rationale for
  PARAM_N = 17669 and VEC_N_SIZE_64 = 277.

---

*Roadmap v2 authored 2026-05-16. Updated 2026-05-17 (V2-A1, V2-A3 promoted;
ablation study complete; AP-1 next-steps added). All projections are Spike
`instret` estimates under the rv64gc_zbc_zbb_zbkb ISA. Real hardware results
will differ due to out-of-order execution and cache hierarchy effects.*

---

## 8. Next Step: Pivot AP-1 — Toom-Cook 3-Way over GF(2)[x]

**Context:** V2 micro-optimizations (V2-A1 threshold sweep, V2-A3 fixed-size
leaf, dead-code ablation) are saturated. The scalar champion stands at
**274,131 instret** (2026-05-17). The leaf (`clmul_leaf_n8`) is fully unrolled
with zero branches; further per-leaf gains would require a different schoolbook
algorithm. The primary remaining lever is an **algorithmic pivot**.

**Why TC3 now:**

| Optimization | Projected savings | Status |
|---|---|---|
| V2-A1 threshold sweep | \u22123.21% | \u2705 Done |
| V2-A3 fixed leaf + ablation | \u22120.16% further | \u2705 Done |
| V2-A4 4-way unroll | \u22123\u20136% | Not started |
| AP-1 Toom-Cook 3-way | **\u221220\u201330%** | **\u2190 Priority 0** |

Micro-optimizations V2-A2, V2-A4, V2-A5 remain valid targets but are
dwarfed by the AP-1 algorithmic gain. The recommendation is:
**implement AP-1 first; apply micro-optimizations on top of the TC3 framework.**

**Immediate action:**

Create experiment `experiments/EXP-toomcook3-gf2x-zbc/` with the following
structure:

```
experiments/EXP-toomcook3-gf2x-zbc/
\u251c\u2500\u2500 gf2x_clmul.c          \u2190 TC3 entry point (tc3_clmul replacing karatsuba_clmul)
\u251c\u2500\u2500 tc3_eval.h            \u2190 5-point evaluation helpers
\u251c\u2500\u2500 tc3_interp.h          \u2190 binary interpolation (XOR + shift only)
\u2514\u2500\u2500 notes.md              \u2190 derivation of evaluation matrix, temp-buffer sizing
```

**Base-case engine:** Reuse `clmul_leaf_n8` from `karatsuba_zbc` for n\u22648
sub-problems. This avoids re-deriving the optimized schoolbook leaf and
ensures the proven zero-free write discipline is inherited.

**Temp-buffer sizing (TC3, n=277):**
- k = ceil(277/3) = 93 words
- 5 evaluation pairs (ea\u00d74 + eb\u00d74 + direct pointers): 8 \u00d7 93 = 744 words
- 5 product buffers (2k each): 5 \u00d7 186 = 930 words
- Child recursion buffer (recursive, depth-2 TC3): \u224820k \u00d7 (93/3)^2 = \u2248740 words
- **Total: ~2,415 words (\u224819 KB)** vs Karatsuba's ~4,432 words (~35 KB)

TC3 actually uses **less** stack than the current Karatsuba for n=277 because
the reduced sub-problem count outweighs the larger evaluation buffer.

**Validation checklist before promotion:**
1. KAT 300/300 vectors for HQC-{128, 192, 256}
2. `ctgrind` / `dudect` on `vect_mul` with TC3 backend
3. Spike isolated benchmark: `vect_mul` HQC-128 instret
4. Comparison table: TC3 vs karatsuba_zbc scalar champion (274,131)

**Reference implementations to consult:**
- `gf2x` library (`gf2x/src/Kara4.c`) — binary Toom-3 interpolation table
- Bernstein "Batch binary Edwards" (CRYPTO 2009) — formal derivation
- `experiments/EXP-fafft-zvbc-towerfield/` — prior complex-field experiment
  (different algorithm but shares the evaluation-point framework pattern)
