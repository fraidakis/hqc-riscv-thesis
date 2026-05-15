# Optimization Roadmap v2: The Final 30%

> **Based on:** `zvbb_gather` + `karatsuba_zvbc` profile — date 2026-05-15  
> **Source variant:** `karatsuba_zvbc` with `HQC_KECCAK_IMPL=zvbb_gather`  
> **Security level profiled:** HQC-128  
> **Simulator:** Spike (instruction-accurate; `instret` = retired instruction count)  
> **ISA:** `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b`  
> **Baseline:** decaps L128 = **1,225,209 instret**

---

## Profile Snapshot (decaps L128)

### Function-Level Hotspots

| Function | instret | % of decaps |
|---|---:|---:|
| `memset` | 924,801 | 75.5%¹ |
| `KeccakF1600_StatePermute` | 521,424 | 42.6%¹ |
| `karatsuba_vclmul` | 309,024 | 9.01% |
| `vclmul_leaf` | 256,668 | 7.48% |
| `hadamard` | 227,516 | 6.63% |
| `keccak_inc_absorb` | 141,116 | 4.11% |
| `memcpy` | 137,922 | 4.02% |
| `find_peaks` | 105,919 | 3.09% |
| `gf_reduce` | 105,600 | 3.08% |
| `keccak_inc_squeeze` | 96,430 | 2.81% |
| `expand_and_sum` | 93,748 | 2.73% |

¹ Profiles are not additive as a percentage of decaps — a `memset` invoked
  inside `vclmul_leaf` counts toward `memset`, not `vclmul_leaf`. Total
  retired instructions = 1,225,209.

### Line-Level Hotspots (Top 5)

| Location | instret | % |
|---|---:|---:|
| `memset` @ `mem.c:40` — loop control (`addi`/`bne`) | 615,498 | 17.94% |
| `memset` @ `mem.c:41` — byte store (`sb`) | 306,195 | 8.92% |
| `KeccakF1600_StatePermute` @ `fips202_zvbb_gather.c:148` — Chi `vle64.v` | 194,256 | 5.66% |
| `KeccakF1600_StatePermute` @ `fips202_zvbb_gather.c:143` — Rho+Pi `vle64.v` | 170,400 | 4.97% |
| `compare_u32` @ `vector.c:43` | 83,250 | 2.43% |

---

## Root Cause Analysis

### The `memset` bottleneck (mem.c:40–41)

`sim/common/mem.c` provides a bare-metal `memset` via a **scalar byte loop**:

```c
// line 40:  for (size_t i = 0; i < n; i++)
// line 41:      d[i] = (unsigned char)c;
```

Clang emits `sb` + `addi` + `bne` — **3 instructions per byte**. The profiler
sees 615,498 instructions on line 40 (loop control) and 306,195 on line 41
(`sb`). Despite the comment in `mem.c` claiming RVV auto-vectorisation at -O3,
the `uint8_t *` alias defeats the auto-vectorizer.

**Confirmed callsites in the decaps path:**

| Callsite | Bytes zeroed | Est. instret | Removable? |
|---|---:|---:|---|
| `vclmul_leaf:131` — `memset(r, 0, 2*n*8)`, ~243 leaf calls × 3 `vect_mul` | ~700 KB total | **~600–800k** | ✅ assign-first-write |
| `hqc_pke_decrypt` — `memset_zero(y, tmp1, tmp2)`, each 2,216 B | 6,648 B | ~20k | ❌ security |
| `hqc_pke_encrypt` — `memset_zero(r1, r2, e, tmp, ctx)` | ~9,080 B | ~27k | ❌ security |
| `crypto_kem_dec` — `memset_zero` on small keys/seeds | 192 B | ~580 | ❌ security |

**Key insight:** `karatsuba_vclmul`'s combine step already uses the zero-free
assign-first-write discipline (Loops 1+2 cover all `r[]` words before Loop 3A/3B
reads them, note at line 282). That fix was never applied to the leaf multiplier
`vclmul_leaf`, which still pre-zeros its result buffer because the inner loop
XOR-accumulates into `r[]`. The same assign-first-write pattern can be applied
to the leaf's first outer iteration to make the `memset` unnecessary.

### The Keccak loop index reload (fips202_zvbb_gather.c:143, 148)

The four constant index tables (`pi_src`, `ror_amounts`, `chi_x1`, `chi_x2`)
are reloaded with `vle64.v` on **every round** of the 24-round loop. These
tables are loop-invariant constants. Over 24 rounds: **4 × 24 = 96 redundant
`vle64.v` instructions** per `KeccakF1600_StatePermute` call.

```c
for (int round = 0; round < NROUNDS; round++) {
    // ...
    vuint64m8_t idx_pi  = __riscv_vle64_v_u64m8(pi_src,     vl); // ← every round
    vuint64m8_t t       = __riscv_vrgather_vv_u64m8(v0, idx_pi, vl);
    vuint64m8_t idx_ror = __riscv_vle64_v_u64m8(ror_amounts, vl); // ← every round
    // ...
    vuint64m8_t idx_x1  = __riscv_vle64_v_u64m8(chi_x1, vl);     // ← every round
    vuint64m8_t idx_x2  = __riscv_vle64_v_u64m8(chi_x2, vl);     // ← every round
}
```

The LMUL=8 register layout (32 registers → 4 groups of 8) fits all four index
vectors simultaneously alongside the state register group `v0–v7`, provided the
temporaries (`t`, `vB`, `vC`) reuse groups. This is already the design intent —
the existing comment documents the 4-group layout — so hoisting is a natural
consequence of that design.

Additionally, the Iota step does two `vsetvl` calls per round (set `vl=1`, XOR
lane 0, restore `vl=25`), adding 48 unnecessary `vsetvl` instructions over 24
rounds.

### Reed-Muller decode (hadamard + expand_and_sum + find_peaks)

| Function | instret | % of decaps |
|---|---:|---:|
| `hadamard` | 227,516 | 18.57% |
| `find_peaks` | 105,919 | 8.65% |
| `expand_and_sum` | 93,748 | 7.65% |
| **Subtotal** | **427,183** | **34.87%** |

These three functions collectively cost nearly as much as Keccak (42.6%) and are
entirely scalar. They are called once per decoded Reed-Solomon symbol: N1 = 46
times per `code_decode` invocation.

`hadamard` (reed_muller.c:97–99) performs a 7-pass butterfly over 64
`int16_t` elements:

```c
(*p2)[i]      = (*p1)[2*i]   + (*p1)[2*i+1];   // line 97/98
(*p2)[i + 64] = (*p1)[2*i]   - (*p1)[2*i+1];   // line 99
```

With SEW=16 and LMUL=4 at VLEN=256, a single `vadd.vv`/`vsub.vv` handles all
64 elements in one instruction. The full 7-pass Hadamard reduces to **7 pairs of
vector adds and subtracts**.

---

## Optimization Targets

### Target C-3: Eliminate `vclmul_leaf` memset + Vectorize Security Zeros

#### C-3a — Assign-First-Write in `vclmul_leaf` (primary)

**File:** `src/poly_mul/vector/zvbc/karatsuba_zvbc/gf2x_vclmul.c`

The leaf multiplier's inner loop reads `r[i+j]` before writing it only for
`i ≥ 1`. For `i = 0`, the write is unconditional and first — so assign is
correct. Restructure the 4×-unrolled outer loop so the `i = 0` strip uses
direct `vse64.v` (assign) instead of `vle64.v`/`vxor.vv`/`vse64.v`. Remove
the `memset(r, 0, 2*n*sizeof(uint64_t))` entirely.

**Implementation steps:**
1. In `vclmul_leaf`, before the `for (i = 0; ...)` loop, add a dedicated
   first-strip block (i=0) using `vclmul`/`vclmulh` + `vse64` (no pre-load,
   no XOR).
2. Start the existing 4×-unrolled loop at `i = 1` (or `i = 4` if i=0 is
   handled as a 1-element strip, then the unrolled block starts at `i = 4`).
   Alternatively, add a branch `i == 0 ? assign : xor` inside the unrolled
   body with the expectation the compiler will hoist it.
3. Apply the same pattern to the scalar tail loop (the `for (; i < n; i++)`
   remainder after the 4× unroll).
4. Remove the `memset` call (line 131).

**Estimated saving:** 600–800k instret (dominant fraction of the 924k total).

#### C-3b — Vectorized `memset` for security zeroization

**File:** `sim/common/mem.c`

Add an RVV fast-path for the `c == 0, aligned pointer, size multiple of 8`
case (all `memset_zero` callers satisfy this for their u64 array arguments):

```c
void *memset(void *dst, int c, size_t n) {
#if defined(__riscv_v)
    if (c == 0 && ((uintptr_t)dst & 7) == 0 && (n & 7) == 0) {
        uint64_t *d = (uint64_t *)dst;
        size_t words = n >> 3;
        while (words > 0) {
            size_t vl = __riscv_vsetvl_e64m8(words);
            __riscv_vse64_v_u64m8(d, __riscv_vmv_v_x_u64m8(0, vl), vl);
            d += vl; words -= vl;
        }
        return dst;
    }
#endif
    // scalar fallback (unaligned / non-zero / byte granularity)
    unsigned char *d = (unsigned char *)dst;
    for (size_t i = 0; i < n; i++) d[i] = (unsigned char)c;
    return dst;
}
```

With LMUL=8 and VLEN=256 this stores 256 bytes per `vse64.v` vs 1 byte per
`sb`. All `VEC_N_SIZE_64`-sized u64 arrays are naturally 8-byte aligned.

**Estimated saving:** ~45k instret.

#### Projected C-3 outcome

| Scenario | instret saved | Projected decaps |
|---|---:|---:|
| C-3a only (leaf assign-first-write) | ~650k (conservative) | ~575k |
| C-3a + C-3b | ~695k | ~530k |
| % reduction from baseline | ~57% | |

---

### Target C-4: Keccak Inner Loop Micro-Optimizations

**File:** `src/keccak/fips202_zvbb_gather.c`

#### C-4a — Hoist constant index vectors outside the round loop (primary)

All four index tables are loop-invariant. Move the four `vle64.v` loads before
the `for (round = 0; ...)` loop:

```c
// Before the loop — loaded ONCE for all 24 rounds:
vuint64m8_t idx_pi  = __riscv_vle64_v_u64m8(pi_src,     vl);
vuint64m8_t idx_ror = __riscv_vle64_v_u64m8(ror_amounts, vl);
vuint64m8_t idx_x1  = __riscv_vle64_v_u64m8(chi_x1,     vl);
vuint64m8_t idx_x2  = __riscv_vle64_v_u64m8(chi_x2,     vl);

for (int round = 0; round < NROUNDS; round++) {
    // Theta: scalar round-trip (unchanged)
    // Rho+Pi: use pre-loaded idx_pi, idx_ror
    // Chi: use pre-loaded idx_x1, idx_x2
    // Iota: unchanged
}
```

**Register conflict analysis:** The 32 LMUL=8 registers give 4 groups. The
current code uses `v0–v7` for state, `v8–v15` for temporaries (`t`, `vB`),
`v16–v23` for Chi indices, `v24–v31` for `idx_ror`/`vC`. Hoisting all four
index vectors does not add register pressure beyond what the current code
already uses per-round — the temporaries `t`, `vB`, `vC` are all dead before
the next index vector is needed.

**Estimated saving:** ~40–80k instret (96 `vle64.v` × number of KeccakF calls).

#### C-4b — Iota: use mask instead of double `vsetvl`

Currently Iota uses two `vsetvl` per round (48 total over 24 rounds):

```c
size_t vl1 = __riscv_vsetvl_e64m8(1);
v0 = __riscv_vxor_vx_u64m8(v0, KeccakF_RoundConstants[round], vl1);
vl = __riscv_vsetvl_e64m8(25);
```

Replace with a masked XOR at `vl=25`, keeping `vl` constant throughout the
loop. Construct a mask with only bit 0 set and use `__riscv_vxor_vx_u64m8`
with that mask. Saves 2 `vsetvl` per round = 48 instructions per permute.

#### C-4c — 2× manual loop unroll

Unrolling the 24-round loop by 2 eliminates 12 back-branch `bne` + 12
`addi round` instructions. Minor benefit but free alongside C-4a.

#### C-4d — Theta scalar round-trip (medium-term, separate spike)

Theta currently spills all 25 lanes to a scalar stack buffer, computes 5-column
XORs, and reloads. Each permute call pays 2 `vle64.v`/`vse64.v` pairs × 24
rounds for this round-trip. Replacing with a masked vector column-reduction
(`vrgather`-based sum) eliminates the spill/reload entirely. This is the most
invasive change and warrants its own implementation branch and re-profiling
before proceeding.

#### Projected C-4 outcome

| Sub-target | instret saved |
|---|---:|
| C-4a (hoist indices) | ~50–80k |
| C-4b (Iota mask) | ~25k |
| C-4c (2× unroll) | ~12k |
| C-4d (Theta vectorize) | TBD after spike |

---

### Target C-5: Vectorize Reed-Muller Decode

**File:** `vendor/hqc-ref/src/ref/reed_muller.c`

#### C-5a — RVV `hadamard`

The 7-pass butterfly over `int16_t[128]` maps directly to RVV SEW=16 with
LMUL=4 (VLEN=256 → 64 `int16_t` per vector instruction):

```
Pass k: for i in [0, 64):
    p2[i]      = p1[2i]   + p1[2i+1]
    p2[i + 64] = p1[2i]   - p1[2i+1]
```

Each pass requires:
- `vle16.v` ×2 (load even and odd strided halves, or use `vlse16.v`)
- `vadd.vv` ×1
- `vsub.vv` ×1
- `vse16.v` ×2

7 passes × ~6 vector instructions = ~42 vector instructions total per codeword,
vs the current ~896 scalar add/subtract operations. Estimated reduction in
`hadamard` cost: **~20–40×**; with 46 codewords per decode this translates to
roughly **150–200k instret saved**.

#### C-5b — RVV `expand_and_sum`

`expand_and_sum` bit-expands packed `uint32_t` words from each codeword copy
into `int16_t` accumulators. This can use `vand.vx` + `vmsne` + `vzext.vf2`
or `vnsrl.wx` patterns to broadcast bit positions across vector lanes.

#### C-5c — `find_peaks`

128-element linear scan for the maximum absolute `int16_t`. Direct candidate
for `vabs.v` (emulated as `vmax(v, vneg(v))`) + `vredmaxu.vs`. The
constant-time mask-select pattern in the current scalar code maps to
`vmsgtu.vv` + `vmerge`.

#### Projected C-5 outcome

| Sub-target | instret saved |
|---|---:|
| C-5a (`hadamard` RVV) | ~150–200k |
| C-5b (`expand_and_sum`) | ~60–80k |
| C-5c (`find_peaks`) | ~70–90k |
| **Total C-5** | **~280–370k** |

---

## Summary: Projected Savings Per Target

| Target | Mechanism | File(s) | Est. Saving | Projected Decaps |
|---|---|---|---:|---:|
| **Baseline** | `zvbb_gather` + `karatsuba_zvbc` | — | — | **1,225,209** |
| **C-3a** | Assign-first-write in `vclmul_leaf` | `gf2x_vclmul.c` | ~650k | ~575k |
| **C-3b** | Vectorized `memset` (aligned zero) | `mem.c` | ~45k | ~530k |
| **C-4a** | Hoist index vectors out of round loop | `fips202_zvbb_gather.c` | ~65k | ~465k |
| **C-4b/c** | Iota mask + 2× unroll | `fips202_zvbb_gather.c` | ~37k | ~428k |
| **C-5** | RVV Hadamard + expand_and_sum + peaks | `reed_muller.c` | ~325k | ~103k |
| **Post-C-5 floor** | Unavoidable crypto work (Keccak permute body, GF arith) | — | — | **~100k** |

> Projections are conservative–optimistic bounds. Actual savings depend on
> Clang codegen for the restructured `vclmul_leaf` and the exact depth of the
> Karatsuba recursion tree at KARATSUBA_THRESHOLD=16, n=277.

---

## Recommended Implementation Order

```
C-3a → re-profile → C-4a → C-3b → C-4b/c → re-profile → C-5a → C-5b/c
```

Re-profile after C-3a: the `memset` total should drop dramatically and the
true relative shares of Keccak and Reed-Muller will become visible without the
current masking effect of the leaf memset overhead.
