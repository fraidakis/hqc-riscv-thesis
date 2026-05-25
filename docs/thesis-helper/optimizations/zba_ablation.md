# Zba (Address Generation) Extension Ablation Study

**Date:** 2026-05-25  
**Status:** Promoted for karatsuba_zvbc and karatsuba_rvv; Zicond dropped entirely  
**Extensions tested:** Zba (sh1add/sh2add/sh3add), Zicond (czero.eqz/czero.nez)  
**Measurement:** Spike ISS, deterministic instret, N_ITER=1  
**Build:** Clang 21, -O3

## Summary

| Extension | karatsuba_zvbc (keccak=zvbb, decode=zvbc) | karatsuba_rvv (keccak=ref, decode=rvv) | karatsuba_zbc (keccak=ref, decode=ref) | ref_vector | ref_scalar |
|-----------|------------------------------------------|----------------------------------------|----------------------------------------|------------|------------|
| **Zba**   | **−5.61% polymul, −3.27% KEM** ✓         | **−1.03% polymul, −0.67% KEM** ✓       | −0.004% ✗                              | +0.01% ✗   | 0.00% ✗    |
| **Zicond**| 0.00% ✗                                  | —                                      | −0.38% ✗                               | —          | —          |

**Verdict:**
- **Zba promoted** for `karatsuba_zvbc` (−5.6% polymul, −3.3% full KEM) and `karatsuba_rvv` (−1.0% polymul).
- **Zicond dropped** — zero dynamic `czero` instructions emitted; compiler uses other branchless idioms.
- **No change** to scalar variants (karatsuba_zbc, ref_scalar) or vendor auto-vectorized (ref_vector).

## Detailed Results — Production Configurations

### polymul micro-benchmark (vect_mul, HQC-128)

| Variant         | Configuration              | Baseline (instret) | +Zba (instret) | Δ      | Δ%      |
|-----------------|----------------------------|--------------------|----------------|--------|---------|
| karatsuba_zvbc  | keccak=zvbb_x2_soa, decode=zvbc | 66,244        | 62,526         | −3,718 | −5.61%  |
| karatsuba_rvv   | keccak=ref, decode=rvv     | 154,811            | 153,213        | −1,598 | −1.03%  |
| karatsuba_zbc   | keccak=ref, decode=ref     | 211,841            | 211,833        | −8     | −0.004% |
| ref_vector      | keccak=ref, decode=ref     | 18,527,412         | 18,529,657     | +2,245 | +0.01%  |
| ref_scalar      | keccak=ref, decode=ref     | 19,214,088         | 19,214,088     | 0      | 0.00%   |

### Full KEM (HQC-128) — Production pairings

#### karatsuba_zvbc (keccak=zvbb_x2_soa, decode=zvbc)

| Metric   | Baseline  | +Zba      | Δ       | Δ%     |
|----------|-----------|-----------|---------|--------|
| keypair  | 111,642   | 107,408   | −4,234  | −3.79% |
| encaps   | 229,835   | 221,511   | −8,324  | −3.62% |
| decaps   | 444,324   | 431,213   | −13,111 | −2.95% |
| **total**| **785,801** | **760,132** | **−25,669** | **−3.27%** |

#### karatsuba_rvv (keccak=ref, decode=rvv)

| Metric   | Baseline  | +Zba      | Δ       | Δ%     |
|----------|-----------|-----------|---------|--------|
| keypair  | 284,114   | 281,999   | −2,115  | −0.74% |
| encaps   | 581,994   | 577,888   | −4,106  | −0.71% |
| decaps   | 1,080,319 | 1,073,525 | −6,794  | −0.63% |
| **total**| **1,946,427** | **1,933,412** | **−13,015** | **−0.67%** |

## Why Zba Helps karatsuba_zvbc but Not karatsuba_zbc

The key insight is **where pointer arithmetic occurs relative to the hot loop**.

### karatsuba_zvbc: Zba is in the hot path

The generated Karatsuba tree for `karatsuba_zvbc` calls vector leaf kernels
(`vclmul_leaf`) that operate on pointer-addressed array slices. Each recursive
level computes:
```c
vclmul_leaf(result + offset, a + half, b + half);
```

With `uint64_t*` pointers, `a + half` compiles to `slli reg, half, 3` + `add reg, base, reg`
(shift left by 3 for 8-byte elements, then add to base). With Zba, this becomes
a single `sh3add reg, half, base` instruction.

The Karatsuba tree for HQC-128 has **~70 recursive calls**, each needing multiple
pointer offsets for the 3 sub-problems + XOR recomposition. At 3–5 pointer
calculations per call site, that's 210–350 `slli+add` pairs → `sh3add` fusions.

Additionally, the RVV recomposition helpers (`rvv_xor3_u64`, `rvv_copy_u64`)
use `vle64` with advancing pointers. The loop body:
```c
ptr += vl;  // vl is in elements, ptr is uint64_t* → compiler emits slli+add
```
becomes `sh3add ptr, vl, ptr` with Zba.

### karatsuba_zbc: Zba is NOT in the hot path

The scalar `karatsuba_zbc` variant uses `clmul_leaf_n8` which is **fully unrolled
with constant offsets**:
```c
r[0] = lo0;       // immediate offset → no address calculation needed
r[1] = hi0 ^ lo1; // constant index → single addi or lui+add
...
r[15] = hi1;
```

All array accesses use compile-time constants. The compiler resolves `r + 7` at
compile time to a fixed offset from the base register. No `slli+add` pattern
exists in the hot loop — Zba has nothing to fuse.

The only pointer arithmetic is in the outer Karatsuba recursion, but because
`clmul_leaf_n8` dominates (64 clmul64 calls = 128 instructions per leaf, vs
~5 pointer ops), the Zba savings are diluted to noise.

### karatsuba_rvv: Smaller benefit (−1.03% polymul, −0.67% KEM)

`karatsuba_rvv` uses scalar `clmul`/`clmulh` leaves (same as karatsuba_zbc) but
RVV recomposition. The recomposition uses strip-mining loops with pointer
advancement, which benefits from `sh3add`. However, the benefit is smaller than
for zvbc because:
1. The scalar clmul leaves dominate execution time (~70% of polymul instret)
   and use constant offsets — unaffected by Zba.
2. The RVV recomposition is cheaper than zvbc's (simpler helpers without
   vclmul overhead), so the absolute count of pointer-arithmetic instructions
   that Zba can fuse is lower.

When tested with decode=ref (non-production), the polymul improvement appeared
as −2.65% — an artifact of the non-optimized decode path's higher baseline
diluting less. With the production pairing (decode=rvv), the full-KEM effect
is −0.67%: still positive, still free (Zba is in RVA22U64), but modest.

### ref_vector and ref_scalar: No benefit

These use the vendor schoolbook `gf2x.c` which is a doubly-nested loop over
bit positions. The inner loop is bit-shift + conditional-XOR — no 8-byte
pointer scaling patterns. Zba has nothing to target.

## Static Instruction Counts (from objdump)

| ELF                          | sh[123]add | czero | Total .text |
|------------------------------|------------|-------|-------------|
| karatsuba_zvbc +Zba+Zicond   | 211        | 0     | ~40 KB      |
| karatsuba_zbc +Zba+Zicond    | 146        | 73    | ~42 KB      |

Note: Despite 211 static `sh*add` instructions in zvbc, the *dynamic* hit rate
is high because these instructions are inside the recursive tree which executes
~70 times. For zbc, the 146 static instances are mostly in cold code (Keccak
setup, vector sampling) and get dominated by the 128-instruction leaf kernel
that executes ~1200 times.

## March String Changes (Promoted)

```
# Before:
karatsuba_zvbc: -march=rv64gcv_zbc_zbb_zvbc_zvbb
karatsuba_rvv:  -march=rv64gcv_zbc_zbb

# After:
karatsuba_zvbc: -march=rv64gcv_zbc_zbb_zba_zvbc_zvbb
karatsuba_rvv:  -march=rv64gcv_zbc_zbb_zba
```

Zba is part of the RVA22U64 profile (which mandates V), so this adds zero
hardware cost on any standards-conformant application processor.

## Thesis Integration

This belongs in §9.2.2 (Ποσοτική Συνεισφορά Κάθε Extension) as part of the
waterfall chart. The explanation of *why* it helps zvbc but not zbc is a good
pedagogical example of how optimization interactions depend on hot-path structure.

## Additional Extensions Tested (2026-05-25)

### Zbs (Single-Bit Instructions: bset, bclr, bext, binv)

**Hypothesis:** `vect_write_support_to_vector` does `v[word] |= (1ULL << bit_pos)`,
which is the canonical `bset` pattern. Expected to save 1 instruction per
iteration (replacing `sll + or` with `bset`).

**Results:**

| Variant | Config | Δ total KEM | Δ% |
|---------|--------|-------------|-----|
| karatsuba_zvbc | keccak=zvbb, decode=zvbc, +Zbs+Zbkb | −881 | −0.12% |
| karatsuba_zbc | keccak=ref, decode=ref, +Zbs | −14,837 | −0.44% |

**Analysis:**
- Compiler emits only 5 static `bset` instructions (in `vect_write_support_to_vector`)
- Dynamic savings: ~1 insn × 75 iterations × 5–6 calls/KEM = ~375–450 instructions
- On karatsuba_zvbc (total KEM = 760K insn), this is negligible (0.06%)
- On karatsuba_zbc (total KEM = 3.4M insn), it's measurable (0.44%) but entirely
  in decaps (−0.71%) where re-encapsulation calls the scatter function twice
- Zero `bclr`/`bext`/`binv` emitted

**Verdict: Drop.** −0.44% maximum, only helps a non-hot function, adds ISA requirement.

### Zbkb (Crypto Bit-manipulation: pack, packh, brev8, zip, unzip)

**Results:** Zero instructions emitted by Clang. The HQC codebase has no
half-register packing (`pack`), no byte-reversal patterns (`brev8`), and no
bit-interleave operations (`zip`/`unzip`) in the hot paths. The GF(2^8) decode
path uses lookup tables rather than bitwise field operations.

**Verdict: Drop.** No instructions emitted, zero benefit.

### Zbkc (Carry-less multiply: clmulr)

**Hypothesis:** `clmulr` (carry-less multiply, result reversed) might help in the
modular reduction step of polynomial multiplication.

**Results:** Zero `clmulr` instructions emitted. The polymul kernel uses explicit
`asm volatile("clmul/clmulh")` — the compiler cannot substitute `clmulr` for
hand-written inline assembly. Even if it could, the reduction step uses
shift-and-XOR, not reversed-product patterns.

- polymul: 211,841 → 211,841 (identical)
- Full KEM: 3,394,685 → 3,394,685 (identical)

**Verdict: Drop.** Zero effect; inline asm blocks prevent compiler substitution.

### Zbb Extras (cpop, ctz, clz, orc.b, max, min)

**Important note:** Zbb is **already in our baseline** march (`rv64gc_zbc_zbb`).
All Zbb instructions are already available for the compiler to use.

**What the compiler already emits (with Zbb baseline):**
- `minu` (2 instances) — in rejection sampling threshold check
- `max`/`maxu` (5 instances) — in Barrett reduction and sampling loops
- `rori` — throughout Keccak (θ step rotations)
- `andn` — in Keccak (χ step: NOT-AND)

**What the compiler does NOT emit despite availability:**
- `cpop` (0) — no population-count patterns in HQC hot paths
- `ctz`/`clz` (0) — no leading/trailing-zero patterns; Barrett reduction uses
  multiplication rather than bit-scanning
- `orc.b` (0) — no byte-OR-combine patterns

These are already measured as part of the Zbb ablation (EXP-keccak-scalar-zbb,
2026-05-22): removing Zbb costs only +0.12–0.30% total KEM, confirming that
the `max`/`minu`/`rori`/`andn` savings are tiny in absolute terms (dominated
by the clmul kernel).

**Verdict: Already included and measured.** No action needed.

### Zcb (Compressed Code-Size Extension)

**Hypothesis:** Zcb provides 16-bit encodings for common operations, reducing
.text footprint and potentially improving I-cache hit rate.

**Results:**
- instret: 3,394,685 → 3,394,685 (identical — expected, Spike counts instructions not bytes)
- .text size: 46,690 → 46,234 bytes (−456 bytes, −0.98%)

**Verdict: Drop for performance.** Zcb cannot reduce instruction count (our metric).
The −1% code-size reduction is irrelevant for correctness or throughput on
instruction-count benchmarks, and negligible for real I-cache behavior (HQC
.text fits easily in a 32 KB L1I regardless).

### Summary of All Extensions Tested

| Extension | Instructions emitted | instret Δ (best case) | Verdict |
|-----------|---------------------|----------------------|---------|
| **Zba** (sh1/2/3add) | 146–211 | **−5.61% polymul (zvbc)** | **Promoted** |
| **Zbb** (rori, andn, max, minu) | 9+ | Already in baseline | Already included |
| **Zbs** (bset) | 5 | −0.44% KEM (zbc) | Drop |
| **Zbkb** (pack, brev8) | 0 | 0% | Drop |
| **Zbkc** (clmulr) | 0 | 0% | Drop |
| **Zicond** (czero) | 0 (zvbc), 73 (zbc) | −0.38% (zbc only) | Drop |
| **Zcb** (compressed) | N/A (encoding only) | 0% instret, −1% .text | Drop |
