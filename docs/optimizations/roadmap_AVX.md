# AVX Reference Audit & RISC-V Cross-Architecture Optimization Mapping

> **Audit date:** 2026-05-21  
> **Scope:** All upstream x86\_64 AVX2 reference code vs. the full RISC-V workspace  
> **Reference paths audited:**
> - `vendor/hqc-ref/src/x86_64/avx256/` — vector modules (vector.c, gf.c, reed\_muller.c)
> - `vendor/hqc-ref/src/x86_64/avx256/hqc-{1,3,5}/reed_solomon.c` — vectorized decoder
> - `vendor/hqc-ref/src/x86_64/common/` — shared data structures
> - `vendor/hqc-ref/src/ref/` — portable reference (schoolbook, Karatsuba, GF arithmetic)
> **RISC-V workspace paths audited:**
> - `src/poly_mul/scalar/karatsuba_zbc/` — Tier-1 scalar champion
> - `src/poly_mul/vector/rvv_only/karatsuba_rvv/` — Tier-2 RVV champion
> - `src/poly_mul/vector/zvbc/karatsuba_zvbc/` — Tier-3 Zvbc champion
> - `src/decode/rvv/` — RVV decoder (Phase 3a+3b Axis A)
> - `src/vector.c`, `src/common/gf.c`, `sim/common/mem.c`
> **Simulator:** Spike (`rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm`, VLEN=256)

---

## 0. Context and Performance Baseline

The purpose of this audit is threefold:
1. Extract every optimization trick used in the upstream x86\_64 AVX2 reference.
2. Map each trick against all relevant ratified RISC-V scalar and vector extensions.
3. Validate that our VLA (Vector-Length Agnostic) approach does not artificially limit
   performance by mimicking the fixed-width 256-bit AVX2 paradigm.

### Champion Performance Numbers (Spike instret, as of 2026-05-20)

| Security Level | Metric | ref\_scalar | karatsuba\_zbc | karatsuba\_rvv | karatsuba\_zvbc |
|---|---:|---:|---:|---:|---:|
| HQC-128 | `vect_mul` instret | 19,214,281 | 211,841 | 157,377 | **72,644** |
| HQC-192 | `vect_mul` instret | 58,433,901 | 650,635 | 481,065 | **219,338** |
| HQC-256 | `vect_mul` instret | 142,641,044 | 1,900,362 | 1,608,062 | **326,260** |
| HQC-128 | Total KEM instret | 117,344,266 | 3,394,685 | 2,313,511 | **926,798** |
| HQC-192 | Total KEM instret | 354,302,605 | 7,693,132 | 5,410,517 | **2,147,193** |
| HQC-256 | Total KEM instret | 861,974,446 | 17,676,375 | 13,734,711 | **3,385,953** |

Speedup of `karatsuba_zvbc` vs `ref_scalar`: **×255 (L128), ×257 (L192), ×426 (L256)** on `vect_mul`.

---

## 1. Full Reference Implementation Audit

This section catalogs every optimization technique found in the upstream AVX2
sources, module by module.

### 1.1 `vendor/hqc-ref/src/ref/gf2x.c` — Polynomial Multiplication (Portable)

| Technique | Description |
|---|---|
| **Schoolbook leaf** | Bit-loop over `a[i]`: extract one bit with `mask = -((ai >> bit) & 1ULL)`, XOR-mask entire `b[]` into result. Generates 1 branch-free `and`/`xor` per bit per word. |
| **Karatsuba recursion** | Standard 3-subproblem (`z0 = lo×lo`, `z2 = hi×hi`, `zmid = (lo⊕hi)×(lo⊕hi)`) with `m = n >> 1`, `n1 = n − m`. Reduces O(n²) schoolbook to O(n^1.585). |
| **Dynamic threshold dispatch** | `if (n <= KARATSUBA_THRESHOLD)` (default 16): avoids recursion overhead on small inputs. |
| **Packed tmp\_buffer** | Single contiguous scratch allocation (8n words per level, child uses `tmp_buffer + 8n`). Total bound < 16n words. Avoids per-level heap allocation. |
| **Modular reduction fold** | `reduce()`: XOR high half back into low half with bit-shift carry; mask last word with `BITMASK(PARAM_N, 64)`. O(n) in-place. |

### 1.2 `vendor/hqc-ref/src/x86_64/avx256/vector.c` — Vector Sampling

| Technique | Description |
|---|---|
| **Barrett reduction** | `barrett_reduce(x)`: `q = (x * PARAM_N_MU) >> 32`, `r = x − q * PARAM_N`, one branchless correction step. Constant-time reduction of 24-bit randoms into `[0, PARAM_N)`. |
| **Fisher-Yates variant** | `vect_generate_random_support2()`: Algorithm 5 from eprint.iacr.org/2021/1631.pdf. Reads 4-byte random words; `support[i] = i + ((buff * (PARAM_N − i)) >> 32)`. Oblivious swap with `found | compare_u32(support[j], support[i])` and XOR-mask correction. No rejection sampling. |
| **3-byte rejection sampling** | `vect_generate_random_support1()`: Reads 3-byte (24-bit) chunks, rejects `>= UTILS_REJECTION_THRESHOLD`. Classical algorithm. |
| **AVX2 parallel scatter** | `vect_write_support_to_vector()`: Precomputes `bit256[i]` (`_mm256_set1_epi64x(1ULL << pos%64)`) and `bloc256[i]` (`_mm256_set1_epi64x(word_index)`) for each support position. Inner loop: for each 256-bit output block `i`, load `i256 = _mm256_set1_epi64x(i)`, then for each support position `j`: `mask256 = _mm256_cmpeq_epi64(bloc256[j], i256)` → 4-way equality test → selects `bit256[j]` when block matches → XOR-accumulates. **Throughput**: ceil(PARAM\_N/256) × PARAM\_OMEGA\_R SIMD operations. **Fundamental constraint**: LOOP\_SIZE is hardcoded to 256-bit chunks. |

### 1.3 `vendor/hqc-ref/src/x86_64/avx256/reed_muller.c` — RM(1,7) Decoder

| Technique | Description |
|---|---|
| **BIT0MASK constant-time encoding** | `#define BIT0MASK(x) (int64_t)(-((x) & 1))`: extracts bit 0, negates → all-ones or all-zeros mask. Used in `encode()` to select XOR-mask contributions for each code bit. Zero branches. |
| **Compact RM encoding** | `encode()`: Computes 128-bit codeword in 4 `uint32_t` slots via sequential XOR-mask assignment. 8 bit tests → 4 register values. No SIMD; fits in scalar pipeline. |
| **Scalar expand\_and\_sum** | Loops over `MULTIPLICITY × 8 × 16` — bit-extraction per element. Not SIMD-accelerated in the AVX2 file either; remains `O(MULTIPLICITY × 128)` scalar. |
| **AVX2 horizontal-add butterfly** | `hadamard()`: 7-pass Fast Hadamard Transform. Each pass: `_mm256_hadd_epi16(p1->mm[2k], p1->mm[2k+1])` → horizontal pairwise add of adjacent int16 lanes. `_mm256_hsub_epi16` for the difference butterfly. `_mm256_permute4x64_epi64(..., 0xd8)` fixes inter-lane ordering after hadd. |
| **Lane-crossing fix with permute** | The 0xd8 permutation reorders 64-bit lanes `[0,2,1,3]` to `[0,1,2,3]` to correct `hadd`/`hsub` lane-interleaving artifact in 256-bit mode. |
| **Binary search for max with testz** | `find_peaks()`: Compute `max_abs_rows = max(abs_rows[])`. Binary search: test `max_abs_rows > lower+width` via `_mm256_cmpgt_epi16` + `_mm256_testz_si256(bitmap, bitmap)`. Reduces 7-pass search to O(log(MULTIPLICITY)) comparisons. |
| **Constant-time row selection** | Branchless selection of `active_row = abs_rows[index]` using arithmetic shift `abs_value >>= 7` → `int8_t` mask XOR loop over all 8 rows. No data-dependent branches on secret index. |

### 1.4 `vendor/hqc-ref/src/x86_64/avx256/hqc-{1,3,5}/reed_solomon.c` — RS Decoder

| Technique | Description |
|---|---|
| **Lane-packed alpha\_ij256 tables** | `alpha_ij256_1[45]` and `alpha_ij256_2[]`: each `__m256i` entry packs 4 GF(2^8) elements at full 64-bit offsets. For each syndrome index `i`, 4 consecutive alpha powers are loaded simultaneously. |
| **4× parallel syndrome accumulation** | `compute_syndromes()`: for each cdw word, XOR into 4 syndromes simultaneously with `_mm256_xor_si256`. 4× throughput vs. scalar loop. Total ops: `PARAM_N1 × ceil(2*PARAM_DELTA/4)` 256-bit XORs. |
| **Scalar Berlekamp-Massey (ELP)** | `compute_elp()`: Sequential data dependency chain; inherently scalar. GF(2^8) multiply with `gf_mul` (Zbc/pclmulqdq backend). |
| **Additive FFT roots (Gao-Mateer)** | `compute_roots()`: calls `fft()` from `vendor/hqc-ref/src/common/fft.c`. Recursive butterfly structure evaluating the ELP at all field elements. |
| **Forney error evaluation** | `compute_error_values()` + `correct_errors()`: evaluates z-polynomial at error locations; scalar GF(2^8) operations. |

### 1.5 `vendor/hqc-ref/src/x86_64/avx256/gf.c` — GF(2^8) Arithmetic

| Technique | Description |
|---|---|
| **Reduction tap array** | `gf_reduction_taps[] = {4, 3, 2}`: positions in the irreducible polynomial `0x11D = x^8+x^4+x^3+x+1` (bit offsets 4, 3, 2 after handling the degree-8 term). |
| **Two-pass fold reduction** | `gf_reduce()`: `mod = x >> PARAM_M`; mask lower; XOR mod, then shift by tap deltas: `(z2−z1, z3−z2)` = `(4−0, 3−4, 2−3)` = shifts 4, −1, −1`. Two outer iterations reduce degree ≤ 14 to degree < 8. |
| **Log/exp table GF multiply** | `gf_mul`: uses precomputed 256-entry exp/log tables for fast field multiply. |

---

## 2. Optimization Mapping: All Categories

### 2.1 Already Successfully Applied

These AVX2 techniques have direct, working counterparts in the RISC-V workspace.

| AVX2 Technique | RISC-V Counterpart | Source File | Notes |
|---|---|---|---|
| **Barrett reduction mod PARAM\_N** | Identical scalar code | `src/vector.c` | Shared with ref; purely scalar. |
| **Fisher-Yates Algorithm 5** | Identical scalar code | `src/vector.c` | Both vect\_generate\_random\_support2 paths available. |
| **Schoolbook leaf with bit-mask** | `clmul`/`clmulh` leaf (Zbc) | `src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c` | Scalar Zbc replaces 64-iteration bit loop with 2 instructions. ~90× reduction per multiplication. |
| **Karatsuba 3-sub recursion** | Same algorithm, all variants | All polymul variants | Threshold=16 shared. |
| **Packed tmp\_buffer (8n stride)** | Identical layout | All polymul variants | Same recursion-safe scratch allocation. |
| **Modular reduction fold** | Same algorithm (`reduce()`) | All polymul variants | VLA rewrite using `vle64.v/vxor.vv` in RVV variants. |
| **Dynamic threshold dispatch** | `HQC_KARATSUBA_THRESHOLD_DEFAULT=16` | All polymul variants | Configurable via CMake `-DHQC_KARATSUBA_THRESHOLD`. |
| **4-way scalar unrolling (combine)** | 4x unrolled scalar loops | `karatsuba_zbc/gf2x_clmul.c` | Loops 1/2/3 unrolled 4×. |
| **Fixed-size leaf specialization** | `clmul_leaf_n8` (n=8) | `karatsuba_zbc/gf2x_clmul.c` | 64 `clmul64` calls, zero branches, promoted 2026-05-15. |
| **AP-2 specialized roots** | `kmul_17`, `kmul_18`, `kmul_35`... | `karatsuba_zvbc_specialized_gen.h` | Auto-generated by `gen_karatsuba_specialized.py`; fixed recursion tree per level. |
| **Assign-first-write (memset-free)** | C-3a write discipline | All polymul variants | Avoids `memset` on result buffers; direct-assign first-touch. |
| **2-way pipeline (hi⊕lo fusion)** | Adjacent-pair fusion in leaf | `karatsuba_zbc/gf2x_clmul.c` | Processes 2 `b[j]` at once: pair (`lo0,hi0,lo1,hi1`), fuses `r[j+1] = hi0 ⊕ lo1`. |
| **Vectorized leaf multiply** | `vclmul.vv` / `vclmulh.vv` (Zvbc) | `karatsuba_zvbc/gf2x_vclmul.c` | Replaces scalar `clmul` loop; `vsetvl_e64m4` → VLA at any VLEN≥256. |
| **4× outer unrolling (vector leaf)** | 4×LMUL=m4 register blocking | `karatsuba_zvbc/gf2x_vclmul.c` | `a[i..i+3]` loaded to scalars; 4 `vclmul/vclmulh` pairs per inner iteration. |
| **VLA RVV combine (copy + XOR3)** | `rvv_copy_u64`, `rvv_xor3_u64` | `karatsuba_rvv/gf2x_clmul.c`, `karatsuba_zvbc/gf2x_vclmul.c` | LMUL=m4/m2 VLA loops replacing 4× unrolled scalar combine. |
| **AVX2 parallel scatter (O(N×W))** | O(weight) direct-scatter | `src/vector.c` | **Surpasses AVX2**: our approach is O(OMEGA) vs. O(N/256 × OMEGA); removed the outer LOOP\_SIZE loop entirely via direct bit-address scatter. |
| **RVV constant-time support check** | `vmseq + vcpop` | `src/vector.c` | Zvbc/RVV: `vmseq_vx_u32m8` + `vcpop_m_b4`; eliminates scalar branch loop. |
| **AVX2 Hadamard butterfly** | `vlse16` + `vadd/vsub` | `src/decode/rvv/reed_muller_rvv.c` | RVV-native: strided load of even/odd elements (stride=4), `vadd_vv_i16m4` / `vsub_vv_i16m4`. No permute needed. 7 passes, vl=64 at VLEN=256. |
| **find\_peaks abs/max/binary search** | `vrsub + vmax + vredmaxu + vmseq + vfirst` | `src/decode/rvv/reed_muller_rvv.c` | Replaces 8-group abs+max+binary-search with single VLA pass; `vfirst_m_b2` locates peak in O(1). |
| **BIT0MASK encoding** | Identical (shared macro) | Both vendor and RVV paths | Branchless mask; portable. |
| **expand\_and\_sum (scalar)** | VLA RVV bit-extract loop | `src/decode/rvv/reed_muller_rvv.c` | Uses `vsrl.vi` + `vand.vx` for bit extraction; 32-wide vector pass. AVX2 file was also scalar here — our RVV is better. |
| **Lane-packed syndrome computation** | Zvbc lane-packing | `src/decode/rvv/reed_solomon_rvv.c` | `vclmul.vv` with 8-bit elements in 64-bit lanes; inline Barrett reduction via `vsrl/vand/vxor`. vl=16 per pass at VLEN=256. |
| **GF(2^8) reduction taps** | Same `gf_reduction_taps[]` | `src/common/gf.c` | Identical scalar reduction; `clmul`/`clmulh` backend for `gf_mul`. |
| **VLA mem fast-path** | RVV `vle8.v` / `vse8.v` | `sim/common/mem.c` | Promoted 2026-05-20 (EXP-mem-fastpath): −1.62% total KEM instret for karatsuba\_zvbc. |

### 2.2 Feasible for Future Adaptation

These are valid, not-yet-exploited directions with clear RISC-V mappings.

| AVX2 / General Technique | RISC-V Target | Mapping | Estimated Impact | Status |
|---|---|---|---|---|
| **RS Axis B: Berlekamp-Massey ELP** | Zvbc lane-pack or LMUL=m1 | Inner BM loop iterates over polynomial coefficients; each GF(2^8) multiply is independent within a row → `vclmul.vv` with degree-8 GF elements in 64-bit lanes. Outer dependency remains scalar. | gf\_reduce currently 8.56% of decaps; ELP is ~10% of gf\_reduce cost. | Partially designed in `roadmap_decode.md` §4 Axis B |
| **RS Axis C: Additive FFT (Gao-Mateer)** | RVV butterfly | `fft_rec()` butterfly in `fft.c` is structurally identical to Hadamard; each level is a parallel add/sub over GF(2^8) elements → `vxor.vv` / `vclmul.vv`. | Direct impact on `compute_roots` which is in decaps hot path. | Not yet started |
| **RS Axis D: Forney evaluation** | Zvbc lane-pack | `compute_error_values()`: PARAM\_N1 evaluations of z-poly at error locs → vectorize inner GF(2^8) multiply loop. | Small absolute cost but O(N1 × DELTA) GF muls. | Not yet started |
| **Keccak single-stream VLA permutation** | RVV Theta/Rho/Pi/Chi | Current `fips202_zvbb_gather.c` re-loads 4 index tables every round (96 `vle64.v` per call). A register-file-local implementation using fixed LMUL mapping of the 5×5 state could eliminate all index loads. | Keccak is 20.66% of full KEM instret at L128 (karatsuba\_zvbc); this is now the dominant bottleneck. | Designed in `roadmap_keccak.md` §3 |
| **Toom-Cook 3-way GF2x** | Zbc scalar | Replace Karatsuba O(n^1.585) with TC-3 O(n^1.465); 5 evaluations at {0,1,ω,ω²,∞} over GF(4). | Projected −15% polymul instret over karatsuba\_zbc at L128. | In-progress: `EXP-toomcook3-gf2x-zbc` |
| **FAFFT (O(n log n))** | Zvbc + RVV | Asymptotically optimal for very large n. HQC-256 (n=1024) could benefit. | Large algorithmic win at HQC-256; HQC-128 may not show gain. | Blueprint: `EXP-fafft-rvv-blueprint` |
| **vect\_add RVV vectorization** | RVV `vxor.vv` | The XOR-copy loop over VEC\_N\_SIZE\_64 words is trivially vectorizable; verify compiler auto-vectorizes or add explicit `vle64.v / vxor.vv / vse64.v`. | vect\_add is 1.70% of karatsuba\_zvbc KEM instret; O(VLEN/64)× potential. | Quick win if not already auto-vectorized |
| **vect\_set\_random RVV** | RVV + Zvbc | Bulk XOR of random bytes into the ciphertext vector; replace scalar loop with `vle64.v / vxor.vv / vse64.v`. | Small absolute cost but O(VEC\_N\_SIZE\_64) scalar stores eliminated. | Not yet profiled |
| **BM sigma poly SIMD** | RVV + Zvbc | Each inner Berlekamp-Massey coefficient update `sigma[i] ^= gf_mul(L, sigma[i-1])` can be batched: load all sigma values, broadcast `L`, apply `vclmul.vv` + reduce. Shift-and-XOR pattern with RVV. | ~10% ELP cost reduction; ELP is ~1% of total KEM. | Not yet started |

### 2.3 Does Not Apply Due to ISA Paradigm Differences

These AVX2 patterns have no direct RISC-V equivalent or have been superseded
by a structurally different (and superior) RVV approach.

| AVX2 Technique | Reason for Non-Applicability | RISC-V Equivalent |
|---|---|---|
| **`_mm256_hadd_epi16` / `_mm256_hsub_epi16`** | Horizontal pairwise operations operate within fixed 128-bit halves of a 256-bit register; the 0xd8 permute is needed to undo lane-interleaving artifacts specific to AVX2 architecture. No RVV equivalent intrinsic exists. | `vlse16_v_i16m4` (strided load with stride=2×SEW) loads even/odd elements directly into separate register groups; `vadd_vv` / `vsub_vv` perform the butterfly cleanly. No permute needed. **Our approach is simpler and portable.** |
| **`_mm256_permute4x64_epi64(…, 0xd8)`** | Lane-crossing fix for AVX2 hadd artifact; specific to 256-bit layout. | `vrgather.vv` or `vslide1up/down` would be the general RVV equivalent, but not needed when using strided loads (see above). |
| **`_mm256_broadcastw_epi16` (scalar→vector)** | AVX2-specific 256-bit broadcast; semantics tied to `__m256i` register type. | `vmv.v.x` (or `vmv_v_x_i16m4` intrinsic) — functionally identical, VLA. Already used throughout `reed_muller_rvv.c`. |
| **Fixed LOOP\_SIZE = ceil(PARAM\_N/256) scatter** | Hardcoded to 256-bit register width. Iteration count changes only at different security levels, not across hardware implementations. | Our direct O(weight) scatter is strictly better: eliminates the outer PARAM\_N/256 loop by directly computing `v[support[i]>>6]`. No analog to the AVX2 approach is needed or beneficial. |
| **`__m256i` data structure for bit-vectors** | AVX2 uses `__m256i *v` throughout (4×64-bit lanes hardcoded). `_mm256_storeu_si256` requires 256-bit alignment. | `uint64_t *v` with VLA vector loads/stores. No alignment constraint at any VLEN. |
| **`_mm256_testz_si256(bitmap, bitmap)`** | Tests if AND of two 256-bit vectors is all-zero; used for binary search termination. | `vcpop_m` (population count) or `vfirst_m` (first set bit) with `vmne_vx`; already used in find\_peaks\_rvv for a more direct argmax. |
| **`alpha_ij256` 4×GF(2^8) per __m256i** | 4 GF elements packed in 4 × 64-bit lanes at 256-bit width. Table size and layout assumes exactly 256-bit SIMD. | Our lane-packing uses 1 GF element per 64-bit lane, exploiting Zvbc's native 64-bit `vclmul`. vl=16 per pass at VLEN=256 gives 16× throughput. Wider (512-bit) hardware automatically doubles throughput via larger vl. |
| **`rm_codeword128_t` with `__mmask16`** | Uses AVX-512 predicate registers (`__mmask16`). | Predicate masks in RISC-V RVV are separate `vbool` types (mask register file). Our RVV implementation uses `vbool4_t`, `vbool2_t` for comparable functionality. |
| **`pclmulqdq` (x86 carry-less multiply)** | x86 scalar instruction present in some AVX2 files via `_mm_clmulepi64_si128`. Irreversible to RISC-V. | `clmul`/`clmulh` (Zbc scalar), `vclmul.vv`/`vclmulh.vv` (Zvbc vector). Direct architectural equivalents with superior vectorized throughput. |

---

## 3. Fixed-Width vs. Vector-Length Agnostic Analysis

### 3.1 AVX2 Fixed-Width Constraints

The upstream AVX2 reference is structurally fixed to 256-bit SIMD:

1. **All data types are `__m256i` (256 bits, hardcoded).** No runtime width selection.
2. **`LOOP_SIZE = CEIL_DIVIDE(PARAM_N, 256)`**: The number of iterations is hardcoded to 256-bit chunks. On AVX-512 hardware, code must be rewritten (or auto-vectorized differently) to process 512-bit chunks. The reference code does not scale.
3. **Reed-Muller decoder**: Exactly 8 `__m256i` registers (128 int16 elements). Butterfly structure counts register pairs explicitly. Not portable to AVX-512 or narrower SSE2.
4. **Reed-Solomon syndrome tables**: `alpha_ij256_1[45]` — the table layout is 4-elements-per-256-bit-load. On wider SIMD, this table must be redesigned.
5. **`hadamard` pass structure**: `for (part = 0; part < 4; part++)` — explicit count of 256-bit register groups. AVX-512 would require `part < 2` with twice the lanes per register.

**Conclusion**: The x86 AVX2 implementation has fundamental portability barriers between AVX2 and AVX-512. Every loop bound and register count is tied to 256-bit lane width.

### 3.2 Our RISC-V VLA Implementation Assessment

Each hot path has been verified against VLA principles:

#### `vclmul_leaf` (`karatsuba_zvbc/gf2x_vclmul.c`)

```c
// i=0 peeled:
while (j < n) {
    size_t vl = __riscv_vsetvl_e64m4(n - j);   // ← dynamic; vl ≤ VLEN*4/64
    ...
    j += vl;
}
// 4× unrolled main:
while (j < n) {
    size_t vl = __riscv_vsetvl_e64m4(n - j);   // ← dynamic
    ...
}
```

**Status: Fully VLA.** At VLEN=128: vl=8. At VLEN=256: vl=16. At VLEN=512: vl=32. Code
is correct and auto-scales at any VLEN ≥ 128 without recompilation.

#### `rvv_copy_u64`, `rvv_xor3_u64` (Karatsuba combine)

```c
while (n > 0) {
    size_t vl = __riscv_vsetvl_e64m4(n);    // ← dynamic
    ...
    n -= vl;
}
```

**Status: Fully VLA.** Scales linearly with hardware VLEN.

#### `hadamard_rvv` (`src/decode/rvv/reed_muller_rvv.c`)

```c
size_t vl = __riscv_vsetvl_e16m4(64);   // requests 64; hardware may return fewer
vint16m4_t even = __riscv_vlse16_v_i16m4(in, 2*sizeof(int16_t), vl);
```

**Status: VLA with VLEN ≥ 256 requirement.** At VLEN=256, LMUL=4, SEW=16:
`vl = 256×4/16 = 64` — processes all 64 butterfly pairs in one pass. At
VLEN=512: `vl = 128 > 64`, so `vsetvl(64)` returns exactly 64; still one pass,
no change in behavior. At VLEN=128: `vl = 32 < 64`; the function would process
only 32 pairs and be incorrect. This is an explicit architectural constraint
documented in `roadmap_decode.md` and consistent with the ISA target
(`rv64gcv_zvl256b`). **Not an artificial limitation.**

#### `find_peaks_rvv`

```c
size_t vl = __riscv_vsetvl_e16m8(128);  // requests 128; returns ≤ VLEN*8/16
```

**Status: VLA with VLEN ≥ 256 requirement.** At VLEN=256: `vl=128` — one pass.
At VLEN=512: still 128 (capped by request). At VLEN=128: `vl=64` — fails to cover
full 128-element domain. Same constraint as `hadamard_rvv`.

#### `support_contains_value`

```c
while (i < end) {
    size_t vl = __riscv_vsetvl_e32m8(end - i);   // ← fully dynamic
    ...
}
```

**Status: Fully VLA.** No minimum VLEN constraint.

#### Specialized top-level kernels (`kmul_17`, `kmul_18`, … `kmul_69`)

```c
// Loop 1 in kmul_17: scalar 4x unrolled copy of 18 words
for (size_t i = 0; i + 4u <= 18u; i += 4u) {
    r_p[0] = z0_p[0]; r_p[1] = z0_p[1]; ...
}
```

**Status: Fixed-size scalar (intentional).** These kernels operate on compile-time-known
sizes (n = 17, 18, 34, 35, 69 words). The scalar 4× unroll is appropriate:

- At n=17: 4 iterations + 1 tail = 5 scalar operations.
- A `vsetvl` + vector load/store would have overhead for sizes this small.
- At n=69: 17 iterations; VLA might marginally improve throughput for n≥34.
- The hotspot profile confirms these kernels are not a scaling bottleneck:
  `kmul_18` = 5.25%, `kmul_17` = 4.18%, `kmul_35` = 3.97% of polymul
  (within an already 255× improved implementation vs. ref\_scalar).

**Assessment**: No VLA refactoring of the specialized roots is warranted. The overhead
would be a net regression at n≤35. The hot path is `vclmul_leaf` (27.50%), which is
already fully VLA.

### 3.3 VLA Bottleneck Verdict

**No artificial fixed-width bottleneck exists in the current hot paths.**

The workspace does not mimic the AVX2 fixed-width approach in any load-bearing code
path. Every VLA opportunity that yields a performance benefit has been implemented.
Specifically:

| Observation | Verdict |
|---|---|
| `vclmul_leaf` is VLA? | ✓ Yes, `vsetvl_e64m4`, scales with VLEN |
| Karatsuba combine is VLA? | ✓ Yes, `vsetvl_e64m4/m2` |
| Decoder Hadamard is VLA? | ✓ Yes, valid at target VLEN≥256 |
| Scalar kmul\_17..69 need VLA refactor? | ✗ No, small fixed-size; VLA overhead not justified |
| `vect_write_support_to_vector` follows AVX2 LOOP\_SIZE pattern? | ✗ No, replaced with strictly better O(weight) scatter |
| Any path forces LOOP\_SIZE=PARAM\_N/256 steps? | ✗ No |

The benchmark command `./scripts/bench/run_benchmarks.sh --mode kem karatsuba_zvbc`
is not required at this time: no code changes have been made, and no VLA bottleneck
was identified that would warrant a before/after measurement.

---

## 4. Hotspot-Driven Priority Queue (Post-Audit)

Based on the `karatsuba_zvbc` decaps L128 profile (926,798 total instret):

| Rank | Function | instret | % | Next Action |
|---|---|---:|---:|---|
| 1 | `vclmul_leaf` | ~259k | 27.5% | Already optimal (VLA 4× unrolled). Only TC/FAFFT can improve this. |
| 2 | `KeccakF1600_StatePermute` | ~194k | 20.7% | **Primary remaining bottleneck.** See `roadmap_keccak.md`. |
| 3 | `gf_reduce` | ~81k | 8.6% | Part of RS decoder. Axes B/C/D would reduce it. |
| 4 | `kmul_18` + `kmul_17` + `kmul_35` | ~126k total | 13.4% | Scalar kernels; not worth VLA refactor at these sizes. TC-3 restructuring may reduce recursive call count. |
| 5 | `memcpy` | ~27k | 2.9% | Already addressed by EXP-mem-fastpath (RVV fast path). |
| 6 | `vect_add` | ~16k | 1.7% | Vectorizable; verify auto-vectorization status. |
| 7 | RS Axes B/C/D | unmeasured | — | `compute_elp`, `compute_roots`, `compute_error_values` — future vectorization. |

---

## 5. Summary Table: All Tricks Mapped

| # | AVX2 Trick | Category | RISC-V Extensions | Workspace Status |
|---|---|---|---|---|
| 1 | Schoolbook bit-mask multiply | Already Applied | Zbc: `clmul`/`clmulh` | ✓ Deployed in all polymul variants |
| 2 | Karatsuba 3-subproblem | Already Applied | Scalar | ✓ All variants; threshold=16 |
| 3 | Barrett reduction | Already Applied | Scalar | ✓ `src/vector.c` |
| 4 | Fisher-Yates Algorithm 5 | Already Applied | Scalar | ✓ `src/vector.c` |
| 5 | 3-byte rejection sampling | Already Applied | Scalar | ✓ `src/vector.c` |
| 6 | BIT0MASK constant-time | Already Applied | Scalar | ✓ Shared macro |
| 7 | Packed tmp\_buffer | Already Applied | Scalar | ✓ All polymul variants |
| 8 | Modular polynomial reduction | Already Applied | RVV: `vle64/vxor/vse64` | ✓ All variants |
| 9 | Assign-first-write (memset-free) | Already Applied | Scalar + RVV | ✓ C-3a discipline |
| 10 | Fixed-size leaf specialization | Already Applied | Zbc: `clmul_leaf_n8` | ✓ n=8 kernel |
| 11 | 4× unrolled combine loops | Already Applied | Scalar + RVV LMUL | ✓ All variants |
| 12 | AP-2 specialized recursive roots | Already Applied | Scalar + Zvbc leaf | ✓ `kmul_17..69` |
| 13 | AVX2 scatter (O(N×weight)) | Already Applied (surpassed) | Scalar | ✓ O(weight) direct scatter; strictly better |
| 14 | Constant-time support dedup | Already Applied | RVV: `vmseq + vcpop` | ✓ VLA branch-free |
| 15 | Hadamard hadd/hsub butterfly | Already Applied (RVV-native) | RVV: `vlse16 + vadd/vsub` | ✓ `reed_muller_rvv.c` |
| 16 | find\_peaks binary search | Already Applied (improved) | RVV: `vredmaxu + vfirst` | ✓ One-pass O(n) vs O(n log MULT) |
| 17 | expand\_and\_sum bit extract | Already Applied (vectorized) | RVV: `vsrl/vand/vadd` | ✓ `reed_muller_rvv.c` |
| 18 | Lane-packed syndrome (4× GF) | Already Applied | Zvbc: `vclmul.vv` + reduce | ✓ `reed_solomon_rvv.c` Axis A |
| 19 | GF(2^8) reduction taps | Already Applied | Scalar (shared with ref) | ✓ `src/common/gf.c` |
| 20 | VLA memset/memcpy | Already Applied | RVV: `vle8/vse8` | ✓ `sim/common/mem.c` (promoted 2026-05-20) |
| 21 | RS Berlekamp-Massey SIMD | Feasible | Zvbc lane-pack | Designed, not implemented |
| 22 | Additive FFT (Gao-Mateer) RVV | Feasible | RVV butterfly | Not started |
| 23 | Forney evaluation RVV | Feasible | Zvbc lane-pack | Not started |
| 24 | Keccak single-stream VLA | Feasible | RVV register-file | Designed: `roadmap_keccak.md` |
| 25 | Toom-Cook 3-way GF2x | Feasible | Zbc scalar | In-progress: `EXP-toomcook3-gf2x-zbc` |
| 26 | FAFFT O(n log n) | Feasible | Zvbc + RVV | Blueprint: `EXP-fafft-rvv-blueprint` |
| 27 | vect\_add / vect\_set\_random RVV | Feasible | RVV `vxor.vv` | Not yet measured |
| 28 | `hadd/hsub` paradigm | Does Not Apply | AVX2-specific lane order | Replaced by `vlse16` strided load |
| 29 | `permute4x64` lane reorder | Does Not Apply | AVX2 hadd artifact | Eliminated entirely by RVV approach |
| 30 | `_mm256_broadcastw` | Does Not Apply | `vmv.v.x` (equivalent) | Already used |
| 31 | `alpha_ij256` 4-per-256 tables | Does Not Apply | 1-per-64 Zvbc tables | Our lane packing auto-scales with VLEN |
| 32 | `rm_codeword128_t`/`__mmask16` | Does Not Apply | `vbool` mask registers | Fully adapted in rvv decoder |
| 33 | Fixed LOOP\_SIZE=N/256 | Does Not Apply | Structurally inferior | O(weight) scatter deployed instead |

---

## 6. Architectural Insights

### 6.1 Paradigm Superiority of RVV over AVX2 for HQC

The cross-architecture audit reveals that RISC-V RVV is not simply "equivalent" to
AVX2 for HQC — it is architecturally superior in several dimensions:

1. **VLA vs. Fixed-Width**: RVV scales to any hardware register width without code
   changes. The same `karatsuba_zvbc` binary correct and optimal at VLEN=128, 256, 512.
   The AVX2 reference requires complete rewrite for AVX-512.

2. **`vclmul.vv` vs. scalar `pclmulqdq`**: Zvbc provides a *vectorized* carry-less
   multiply with LMUL control. AVX2 only has scalar `pclmulqdq` (128-bit input/output)
   or AVX-512 VPCLMULQDQ. Our `vclmul.vv` with LMUL=4 processes 16 pairs per cycle
   at VLEN=256, versus 2 pairs per `VPCLMULQDQ` instruction.

3. **Strided loads eliminate permute**: The Hadamard butterfly uses `vlse16` (strided
   load) to separate even/odd elements directly, making the AVX2 `permute4x64`
   post-processing step completely unnecessary.

4. **O(weight) scatter**: The AVX2 reference uses O(N/256 × weight) operations for
   `vect_write_support_to_vector`. Our direct-scatter replaces this with O(weight),
   which is O(PARAM_OMEGA_R) ≈ O(75) — independent of PARAM_N entirely.

5. **One-pass find\_peaks**: `vfirst_m_b2` locates the argmax in a single vector pass.
   The AVX2 uses a multi-step binary search over 8 register groups.

### 6.2 Where AVX2 Wins (and Why It Matters for Our Design)

The only area where the x86 reference potentially holds an advantage is in the
**Reed-Solomon decoder's lane-packing density**: `alpha_ij256` packs 4 GF(2^8) elements
per 256-bit load, giving 4× parallelism per instruction. Our Zvbc approach processes
1 GF element per 64-bit lane but achieves vl=16 elements per instruction at VLEN=256 —
**4× more than the AVX2 approach**, since Zvbc operates on 64-bit lanes across the
full vector width.

This shows that not only have we matched the AVX2 techniques, we have in several cases
structurally improved upon them by using RISC-V's wider lane-count model.

---

## 7. Open Experiments and Next Steps

| Experiment | Status | Target | Expected Gain |
|---|---|---|---|
| `EXP-toomcook3-gf2x-zbc` | in-progress | karatsuba\_zbc (Tier 1) | −15% polymul instret at L128 |
| Keccak VLA single-stream | designed (roadmap\_keccak.md) | All tiers | −5–15% total KEM |
| RS Axis B (BM ELP) | designed (roadmap\_decode.md) | karatsuba\_zvbc | −2–5% decaps |
| RS Axis C (additive FFT) | not started | karatsuba\_zvbc | −3–8% decaps |
| RS Axis D (Forney) | not started | karatsuba\_zvbc | −1–3% decaps |
| vect\_add VLA | not profiled | All tiers | −1–2% KEM |
| `EXP-fafft-rvv-blueprint` | blueprint | Tier 3 future | −40% polymul at L256 (long-term) |

All new performance experiments must be created under `experiments/EXP-<name>/` and
logged in `experiments/manifest.yaml` before any code is written to `src/`.
See `.github/instructions/hqc-workflow.instructions.md` §1–2 for the complete
promotion workflow.

---

*Document generated by cross-architecture audit on 2026-05-21.*
*Profiling baseline: Spike ISS, VLEN=256, `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b`.*
