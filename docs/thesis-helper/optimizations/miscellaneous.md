# 5.5 Utility, Runtime, and Build-System Optimizations

## 1. Scope and Motivation

The preceding sections documented the three primary algorithmic subsystems
targeted for acceleration: polynomial multiplication (§5.2), Keccak/SHA-3
(§5.3), and error-correction decoding (§5.4). However, a complete accounting
of all modifications relative to the upstream `vendor/hqc-ref` baseline
reveals a set of cross-cutting optimizations that do not belong to any single
subsystem. These modifications target the vector-sampling utilities
(`src/vector.c`), the GF(2^8) arithmetic primitives (`src/common/gf.c`), the
bare-metal runtime library (`sim/common/mem.c`), and the CMake build
infrastructure. Each is documented below in descending order of expected
performance impact.

---

## 2. Vector Sampling Utilities (`src/vector.c`)

### 2.1 O(weight) Direct-Scatter Bit-Vector Construction

#### Baseline Reference Behaviour

The upstream `vect_write_support_to_vector` (in `vendor/hqc-ref/src/ref/vector.c`)
implements a constant-time mask-and-accumulate algorithm. For each of the
`VEC_N_SIZE_64` output words, the function iterates over all `weight` support
entries, computing a constant-time equality mask between the current word index
and each entry's target word. The resulting complexity is
$O(\texttt{VEC\_N\_SIZE\_64} \times \texttt{weight})$:

```c
// Vendor reference: O(VEC_N_SIZE_64 × weight) per call
for (uint32_t i = 0; i < VEC_N_SIZE_64; i++) {
    val = 0;
    for (uint32_t j = 0; j < weight; j++) {
        uint32_t tmp = i - index_tab[j];
        int val1 = 1 ^ (((tmp | -tmp) >> 31));
        uint64_t mask = -val1;
        val |= (bit_tab[j] & mask);
    }
    v[i] |= val;
}
```

At HQC-128 ($\texttt{VEC\_N\_SIZE\_64} = 277$, $\texttt{weight} = 75$), this
executes $277 \times 75 = 20{,}775$ inner-loop iterations per call, each
requiring approximately 8 instructions (subtraction, OR, shift, XOR, negate,
AND, OR, store). Profiling attributed 20.99% of total KEM instruction count
to this single function in the initial scalar baseline.

#### Optimized RISC-V Behaviour

The promoted implementation replaces the nested loop with a direct-scatter
pattern of complexity $O(\texttt{weight})$:

```c
for (size_t i = 0; i < weight; i++) {
    uint32_t word_idx = support[i] >> 6;
    uint32_t bit_pos  = support[i] & 0x3f;
    v[word_idx] |= (UINT64_C(1) << bit_pos);
}
```

Each iteration requires three instructions on RV64: `srli` (word index
extraction), `andi` (bit position extraction), and `sll` + `or` (bit set).
On targets with the Zbs extension, the shift-and-OR pair collapses to a
single `bset` instruction. At HQC-128, the total iteration count drops from
20,775 to 75 — a reduction factor of 277×.

#### Microarchitectural Rationale

The direct-scatter pattern exploits the fact that the support array indices
are public (or computationally indistinguishable from uniform for ephemeral
vectors). The memory access pattern `v[support[i] >> 6]` is data-dependent,
which would constitute a timing side-channel if the support were secret.
For the HQC encryption path (vectors $r_1$, $r_2$, $e$), the support is
derived from public randomness and the optimization is safe. For key-generation
vectors ($x$, $y$), the support is secret; the original constant-time
mask-and-accumulate must be retained in deployments requiring side-channel
resistance. The current implementation uses the fast path for both contexts
as a performance experiment.

---

### 2.2 RVV-Vectorized Bitwise Addition (`vect_add`)

#### Baseline Reference Behaviour

The upstream `vect_add` performs element-wise XOR over `size` 64-bit words
using a scalar loop:

```c
for (uint32_t i = 0; i < size; ++i) {
    o[i] = v1[i] ^ v2[i];
}
```

At HQC-128 ($\texttt{VEC\_N\_SIZE\_64} = 277$), this executes 277 iterations
of load-load-XOR-store, totalling approximately $277 \times 4 = 1{,}108$
instructions per call. The function is invoked 3–5 times per KEM operation
(once in keygen, twice in encrypt, twice in decrypt).

#### Optimized RISC-V Behaviour

The promoted implementation uses an RVV 1.0 VLA (vector-length-agnostic)
loop at LMUL=8, SEW=64:

```c
while (i < n) {
    size_t vl = __riscv_vsetvl_e64m8(n - i);
    vuint64m8_t a = __riscv_vle64_v_u64m8(v1 + i, vl);
    vuint64m8_t b = __riscv_vle64_v_u64m8(v2 + i, vl);
    vuint64m8_t c = __riscv_vxor_vv_u64m8(a, b, vl);
    __riscv_vse64_v_u64m8(o + i, c, vl);
    i += vl;
}
```

At VLEN=256 with LMUL=8, the hardware vector length is 32 elements (64-bit
each). The 277-word XOR completes in $\lceil 277/32 \rceil = 9$ vector
iterations, each comprising 4 vector instructions (2 loads, 1 XOR, 1 store)
plus loop control. Total instruction count: approximately $9 \times 6 = 54$
instructions — a 20× reduction versus the scalar path.

#### Microarchitectural Rationale

The XOR operation is data-independent and introduces no timing side-channel.
The LMUL=8 configuration maximizes throughput per iteration at the cost of
consuming the entire vector register file; since `vect_add` is a leaf function
with no other vector state to preserve, this is optimal. The VLA loop
structure ensures correctness at any VLEN ≥ 64 without compile-time
assumptions.

---

### 2.3 RVV-Vectorized Constant-Time Duplicate Detection (`support_contains_value`)

#### Baseline Reference Behaviour

The upstream `vect_generate_random_support1` uses a scalar inner loop to
detect duplicate support entries:

```c
inc = 1;
for (size_t k = 0; k < i; k++) {
    if (support[k] == support[i]) {
        inc = 0;
    }
}
```

This loop executes $O(i)$ comparisons per candidate, accumulating to
$O(\texttt{weight}^2 / 2)$ total comparisons across the full sampling
procedure. At HQC-128 ($\texttt{weight} = 75$), this is approximately
2,812 scalar comparisons.

The upstream `vect_generate_random_support2` uses a similar pattern with
`compare_u32` (a constant-time equality function) in a loop scanning from
$i+1$ to `weight`.

#### Optimized RISC-V Behaviour

The promoted `support_contains_value` helper replaces the scalar scan with
an RVV vectorized comparison using LMUL=8 at SEW=32:

```c
while (i < end) {
    size_t vl = __riscv_vsetvl_e32m8(end - i);
    vuint32m8_t candidates = __riscv_vle32_v_u32m8(&support[i], vl);
    vbool4_t matches = __riscv_vmseq_vx_u32m8_b4(candidates, value, vl);
    global_match_count += __riscv_vcpop_m_b4(matches, vl);
    i += vl;
}
return (global_match_count != 0);
```

At VLEN=256 with LMUL=8, SEW=32, the vector length is 64 elements. For
HQC-128 ($\texttt{weight} = 75$), the entire support array fits in
$\lceil 75/64 \rceil = 2$ vector iterations. Each iteration performs a
broadcast-compare (`vmseq.vx`) and a population count (`vcpop.m`) — both
fixed-latency operations that do not introduce data-dependent timing
variation.

#### Constant-Time Preservation

The implementation guarantees constant-time execution by:
1. Always scanning the entire specified range (no early exit on match).
2. Using `vcpop_m` (a fixed-latency vector population count) rather than
   `vfirst_m` (which may exhibit data-dependent timing on some
   microarchitectures).
3. Converting the accumulated count to a boolean via the branchless
   `(global_match_count != 0)` expression, which compiles to a single
   `snez` instruction on RISC-V.

---

## 3. GF(2^8) Arithmetic Primitives (`src/common/gf.c`)

### 3.1 Zbc-Accelerated `gf_square`

#### Baseline Reference Behaviour

The upstream `gf_square` computes $a^2$ in GF(2^8) via a bit-interleaving
loop that inserts a zero bit between each pair of adjacent bits:

```c
uint32_t b = a;
uint32_t s = b & 1;
for (size_t i = 1; i < PARAM_M; ++i) {
    b <<= 1;
    s ^= b & (1 << 2 * i);
}
return gf_reduce(s);
```

This loop executes $\texttt{PARAM\_M} - 1 = 7$ iterations, each requiring a
shift, AND with a computed mask, and XOR — approximately 21 instructions for
the squaring step alone, plus the `gf_reduce` call.

#### Optimized RISC-V Behaviour

The promoted implementation exploits the algebraic identity that squaring in
GF(2)[x] is equivalent to carry-less self-multiplication ($a^2 = a \otimes a$).
On Zbc targets, this is computed with a single `clmul` instruction:

```c
#ifdef __riscv_zbc
    uint64_t s;
    __asm__ volatile ("clmul %0, %1, %1" : "=r"(s) : "r"((uint64_t)(a & 0xFFu)));
    return gf_reduce((uint16_t)s);
#endif
```

The self-multiplication produces the identical 14-bit result as the
bit-interleaving loop (since $a \otimes a$ in GF(2)[x] is precisely the
polynomial with zero-inserted coefficients). The total cost is 1 instruction
for the product plus the `gf_reduce` call — replacing the 7-iteration loop
entirely.

#### Microarchitectural Rationale

The `gf_square` function is invoked 5 times per `gf_inverse` call (which
itself is called once per error location in the Forney formula). At HQC-256
with $\delta = 29$ error locations, this yields up to 145 `gf_square`
invocations per decapsulation. The per-call saving of approximately 18
instructions (21 → 3) accumulates to approximately 2,610 instructions saved
per decapsulation at HQC-256.

---

### 3.2 Inlined Fixed-Shift Modular Reduction (`gf_reduce`)

#### Baseline Reference Behaviour

The upstream `gf_reduce` implements modular reduction via a two-pass loop
that iterates over a `gf_reduction_taps[]` array stored in `.rodata`:

```c
static const uint8_t gf_reduction_taps[] = {4, 3, 2};

uint16_t gf_reduce(uint16_t x) {
    uint64_t mod;
    for (int i = 0; i < 2; ++i) {
        mod = x >> PARAM_M;
        x &= (1 << PARAM_M) - 1;
        x ^= mod;
        uint16_t z1 = 0;
        for (size_t j = 3; j; --j) {
            uint16_t z2 = gf_reduction_taps[j - 1];
            uint16_t dist = z2 - z1;
            mod <<= dist;
            x ^= mod;
            z1 = z2;
        }
    }
    return x;
}
```

Each outer pass executes 3 inner iterations with array loads, subtraction,
variable shift, and XOR. The two-pass structure totals approximately 30–35
instructions including loop control, array indexing, and the memory loads
from `gf_reduction_taps[]`.

#### Optimized RISC-V Behaviour

The promoted `gf_reduce` eliminates both loops and the constant array by
encoding the tap positions as compile-time shift amounts:

```c
uint16_t gf_reduce(uint16_t x) {
    uint16_t mod = x >> 8;
    x = (x & 0xFF) ^ mod ^ (mod << 2) ^ (mod << 3) ^ (mod << 4);
    mod = x >> 8;
    return (x & 0xFF) ^ mod ^ (mod << 2) ^ (mod << 3) ^ (mod << 4);
}
```

This compiles to a straight-line sequence of approximately 12 instructions
(shifts, masks, and XORs) with zero branches, zero memory loads, and zero
loop control. The reduction taps $\{2, 3, 4\}$ are embedded as immediate
operands in `slli` instructions.

#### Microarchitectural Rationale

`gf_reduce` is the single most frequently called function in the GF(2^8)
arithmetic layer — invoked by every `gf_mul`, `gf_square`, and the inline
Barrett reduction in the vectorized RS decoder. The elimination of the inner
loop and array loads removes approximately 20 instructions per call. Given
that `gf_reduce` is called thousands of times per decapsulation (once per
`gf_mul` in the scalar path, and once per vector reduction pass in the
vectorized path), the cumulative saving is substantial. The branch-free
structure also eliminates branch-predictor pressure on in-order RISC-V cores
that lack speculative execution.

---

## 4. Bare-Metal Runtime Library (`sim/common/mem.c`)

### 4.1 RVV-Vectorized `memcpy`

#### Baseline Reference Behaviour

A standard freestanding `memcpy` implementation copies bytes one at a time:

```c
for (size_t i = 0; i < n; i++)
    d[i] = s[i];
```

For the HQC workload, `memcpy` is invoked during key serialization, ciphertext
parsing, and internal buffer management. Typical call sizes range from 32 bytes
(seed copies) to 2,216 bytes (`VEC_N_SIZE_BYTES` at HQC-128).

#### Optimized RISC-V Behaviour

The promoted `memcpy` uses an RVV VLA byte-streaming loop at LMUL=8:

```c
while (n > 0) {
    size_t vl = __riscv_vsetvl_e8m8(n);
    vuint8m8_t v = __riscv_vle8_v_u8m8(s, vl);
    __riscv_vse8_v_u8m8(d, v, vl);
    s += vl; d += vl; n -= vl;
}
```

At VLEN=256 with LMUL=8, SEW=8, the vector length is 256 bytes per iteration.
A 2,216-byte copy completes in $\lceil 2216/256 \rceil = 9$ iterations versus
2,216 scalar byte copies — a 246× iteration reduction.

#### Microarchitectural Rationale

The vectorized `memcpy` is critical for the freestanding build environment
where no optimized C library is available. The Keccak documentation (§5.3,
C6) notes that `__builtin_memcpy` with compile-time-constant size is lowered
to native `ld`/`sd` instructions, bypassing this symbol entirely. However,
runtime-variable-size copies (key serialization, ciphertext parsing) dispatch
through this symbol and benefit from the vectorized path.

### 4.2 RVV-Vectorized `memset` (Aligned Zero-Fill Fast Path)

#### Baseline Reference Behaviour

A standard freestanding `memset` fills bytes one at a time:

```c
for (size_t i = 0; i < n; i++)
    d[i] = (unsigned char)c;
```

#### Optimized RISC-V Behaviour

The promoted `memset` includes a fast path for the dominant use case —
8-byte-aligned zero-fills (which covers all `memset_zero` calls on
stack-allocated `uint64_t` arrays):

```c
if (c == 0 && ((uintptr_t)dst & 7u) == 0 && (n & 7u) == 0) {
    uint64_t *d = (uint64_t *)dst;
    size_t words = n >> 3;
    while (words > 0) {
        size_t vl = __riscv_vsetvl_e64m8(words);
        __riscv_vse64_v_u64m8(d, __riscv_vmv_v_x_u64m8(0, vl), vl);
        d += vl; words -= vl;
    }
    return dst;
}
```

At VLEN=256 with LMUL=8, SEW=64, the vector length is 32 words (256 bytes)
per iteration. The `VEC_N_SIZE_64 = 277` word zero-fill in `hqc_pke_keygen`
completes in $\lceil 277/32 \rceil = 9$ iterations versus 277 scalar stores.

#### Microarchitectural Rationale

The zero-fill fast path is triggered by the `memset_zero` macro used
throughout the HQC codebase for zeroizing sensitive data (keys, intermediate
buffers). The alignment and size guards (`& 7u`) ensure the fast path is
taken only when the pointer and length are naturally aligned to 64-bit
boundaries — a condition satisfied by all stack-allocated `uint64_t` arrays
in the HQC implementation. The SEW=64 configuration avoids byte-lane
inefficiency on implementations where wider element stores are more efficient
than byte stores.

---

## 5. Build-System Optimizations (`CMakeLists.txt`, `cmake/options.cmake`)

### 5.1 Link-Time Optimization for Reference Baselines

#### Baseline Reference Behaviour

The upstream HQC reference is compiled as independent translation units with
no cross-module optimization. Function calls between `kem.c`, `hqc.c`,
`gf2x.c`, `fips202.c`, and `vector.c` are opaque to the compiler, preventing
inlining, interprocedural constant propagation, and dead-code elimination
across module boundaries.

#### Optimized Build Behaviour

The CMake infrastructure applies Profile-Guided Optimization (PGO) to the
reference baseline target (`scalar_ref`) via the
`hqc_apply_ref_pgo()` function. PGO enables the compiler to make informed
decisions about branch prediction, code layout, and selective inlining based
on actual execution profiles.

Note: LTO (`-flto`) was evaluated and found to provide no benefit on real
hardware (BPI-F3, SpacemiT K1 X60): +0.3% cycle regression due to I-cache
layout disruption on the in-order pipeline. On Spike ISS, the instret
difference is <0.001%. LTO is therefore not applied.

PGO alone provides ~4.8% cycle reduction on hardware by improving IPC
through better branch prediction and spatial locality in the I-cache.

### 5.2 Profile-Guided Optimization Infrastructure

#### Baseline Reference Behaviour

No profiling infrastructure exists in the upstream reference.

#### Optimized Build Behaviour

The build system provides a two-phase PGO workflow via the `HQC_REF_PGO`
cache variable:

1. **Instrumentation phase** (`HQC_REF_PGO=generate`): Compiles reference
   targets with `-fprofile-instr-generate`, producing raw profile data when
   the ELF is executed on Spike.
2. **Optimization phase** (`HQC_REF_PGO=use`): Recompiles with
   `-fprofile-instr-use`, enabling the compiler to make branch-probability
   and code-layout decisions based on actual execution profiles.

This infrastructure strengthens the reference baseline by allowing the
compiler to optimize branch prediction, function ordering, and register
allocation based on the actual HQC execution profile — ensuring that any
speedup claimed by the hand-optimized variants is not attributable to
suboptimal compiler decisions on the baseline.

### 5.3 Section-Based Dead-Code Elimination

#### Baseline Reference Behaviour

Standard compilation places all functions and data into monolithic `.text`
and `.data` sections. The linker includes entire object files even if only
a subset of their symbols are referenced.

#### Optimized Build Behaviour

The GCC fallback flags in `CMakeLists.txt` include `-ffunction-sections`,
`-fdata-sections`, and `-Wl,--gc-sections`. This combination:

1. Places each function in its own `.text.<name>` section.
2. Places each global variable in its own `.data.<name>` section.
3. Instructs the linker to discard any section not reachable from the entry
   point (`_start`).

For the HQC freestanding build, this eliminates unused vendor utility
functions (e.g., `gf_generate`, `vect_print` in non-VERBOSE builds) from
the final ELF, reducing both code size and instruction-cache pressure.

### 5.4 Compiler Auto-Vectorization Suppression for Scalar Variants

#### Baseline Reference Behaviour

When compiled with `-O3` and a vector-capable `-march` string, the compiler
may auto-vectorize scalar loops, introducing vector instructions that
confound instruction-count comparisons between scalar and vector variants.

#### Optimized Build Behaviour

Scalar variants (`ref_scalar`, `karatsuba_zbc`) are compiled with explicit
auto-vectorization suppression:

```cmake
-fno-tree-vectorize                    # GCC tree-vectorizer
-fno-vectorize                         # Clang loop vectorizer
-fno-slp-vectorize                     # Clang SLP vectorizer
```

This ensures that scalar variant instruction counts reflect purely scalar
code generation, providing a clean baseline for measuring the explicit
vectorization benefit of Tracks B and C.

---

## 6. Impact Ranking

The following ranking orders the miscellaneous optimizations by expected
contribution to total KEM instruction reduction:

### High Expected Impact

- **O(weight) direct-scatter `vect_write_support_to_vector`** (§2.1):
  Eliminated a 20.99% hotspot from the scalar baseline; 277× iteration
  reduction at HQC-128.

### Medium Expected Impact

- **RVV-vectorized `memcpy`/`memset`** (§4.1–4.2): Affects all buffer
  operations in the freestanding environment; 246× iteration reduction for
  large copies.
- **Inlined `gf_reduce`** (§3.2): Called thousands of times per
  decapsulation; ~20 instructions saved per call.
- **RVV-vectorized `vect_add`** (§2.2): 20× instruction reduction per call;
  invoked 3–5 times per KEM operation.
- **RVV-vectorized `support_contains_value`** (§2.3): Reduces duplicate
  detection from O(weight) scalar comparisons to 1–2 vector iterations.

### Low Expected Impact

- **Zbc `gf_square`** (§3.1): ~18 instructions saved per call; limited
  invocation count (5 per `gf_inverse`).
- **LTO/PGO for reference baselines** (§5.1–5.2): Strengthens the baseline
  rather than accelerating the optimized variants.
- **Section-based dead-code elimination** (§5.3): Reduces code size but does
  not affect hot-path instruction count.
- **Auto-vectorization suppression** (§5.4): Measurement hygiene rather than
  performance optimization.

---

## 7. Hardware Keccak Micro-Benchmark: Auto-Vectorization Effect

### 7.1 Observation

When running `run_bench.sh --platform hw --mode keccak --variant all`, the
`vector_rvv` keccak results show ~12% improvement over `scalar_ref`/`scalar_zb`:

| Variant       | Keccak L128 cycles (median) | vs scalar_ref |
|---------------|----------------------------|---------------|
| scalar_ref    | 359,382                    | baseline      |
| scalar_zb     | 358,812                    | −0.16%        |
| vector_rvv    | 313,486                    | −12.77%       |

### 7.2 Root Cause

The keccak micro-bench binary (`bench_hw_keccak_hqc${LEVEL}`) is
**variant-independent in source** — it always compiles `fips202.c` (the
reference scalar Keccak implementation). However, the `-march` flag and
auto-vectorization policy differ by variant:

| Variant      | MARCH (from Makefile.hw)       | Auto-Vectorization |
|--------------|-------------------------------|--------------------|
| ref_scalar   | rv64gc_zbc_zbb_zbs            | `-fno-tree-vectorize` (explicit suppression) |
| karatsuba_zbc| rv64gc_zbc_zbb_zbs            | `-fno-tree-vectorize` (explicit suppression) |
| karatsuba_rvv| rv64gcv_zbc_zbb_zba_zbs       | (default, GCC auto-vectorizes with `-O3`) |

The `karatsuba_rvv` variant includes the `v` extension in its
`-march` string **without** `-fno-tree-vectorize`, allowing GCC to
auto-vectorize the Keccak permutation.

### 7.3 What GCC Auto-Vectorizes in Keccak

With `-O3 -march=rv64gcv... -ftree-vectorize`, GCC vectorizes:
- **θ step**: column parity XOR operations (5-element reductions become
  vector XOR chains)
- **χ step**: row-wise NOT-AND-XOR pattern (data-parallel, no cross-lane
  dependency)
- **State load/store**: bulk 64-bit lane transfers via vle64/vse64

GCC does **not** effectively vectorize:
- **ρ step**: irregular lane rotations (each lane has a different rotation
  offset) — without `vror.vv` (Zvbb), each rotation requires 3 instructions
  (vsll + vsrl + vor), limiting the benefit
- **π step**: irregular permutation of the 25 lanes

The net result is a modest ~12% cycle reduction from auto-vectorization,
primarily from θ/χ step vectorization and memory operation batching.

### 7.4 Why This Is Not a Bug

The per-variant keccak iteration in `--mode keccak` on hardware serves to
measure the **same keccak implementation under the compilation flags that
each variant actually uses in the full KEM**. This is the correct behavior:

- In a full `bench_hw_kem_hqc128_vector_rvv` binary, the keccak code is
  compiled with the same `-march=rv64gcv...` flags, so it genuinely benefits
  from auto-vectorization in the real KEM execution.
- The keccak micro-bench correctly reflects this: "what does keccak cost in
  the context of this variant's compilation environment?"

### 7.5 Relationship to Thesis Sections

- **§7.1.2 (RVV Paradox)**: Documents that *hand-written* base-RVV Keccak
  (without Zvbb) increases instret vs scalar — this is measured in Spike
  (instret) and refers to explicit RVV intrinsics code. The HW
  auto-vectorization observation is complementary: GCC's auto-vectorizer
  makes different (more conservative) choices than hand-written RVV, and
  achieves a modest real-cycle benefit.

- **§8.5 (Hardware Evaluation)**: Should note that the ~12% keccak
  improvement in vector variants comes "for free" from compiler
  auto-vectorization when the V extension is in the `-march` string. This is
  a GCC-specific observation — the Spike ISS (which measures instret) would
  show a similar effect, but the absolute benefit may differ due to pipeline
  effects (auto-vectorized code may have better IPC from reduced scalar
  register pressure and memory coalescing).

- **§9.4 (Generalizable Design Principles)**: The limits of
  auto-vectorization in crypto kernels — GCC can auto-vectorize ~12% of
  Keccak's cost (θ/χ steps) but cannot exploit Zvbb for the dominant ρ step,
  nor can it restructure the algorithm for multi-block SoA parallelism
  (§7.1.3). This quantifies the "free" benefit boundary.

### 7.6 Implication for bench_keccak Output Files

The script generates output files named by *keccak implementation*, not by
polymul variant:
- `hw_keccak_scalar-ref_latest.csv` — scalar_ref measurement
- `hw_keccak_scalar-zb_latest.csv` — scalar_zb measurement
- `hw_keccak_vector-rvv_latest.csv` — vector_rvv measurement

**Note**: The file naming (`scalar-ref`, `vector-rvv`, etc.) reflects the
polymul variant under which keccak was compiled — it does NOT mean different
keccak source implementations. The keccak source is always `fips202.c`; only
the compilation flags differ. For thesis presentation, the relevant
comparison is:
- "Keccak without auto-vectorization" (scalar variants): ~359k cycles
- "Keccak with auto-vectorization" (vector variants): ~314k cycles
- "Keccak with Zvbb 2-way SoA" (Spike only, no HW): TBD instret

### 7.7 Syntax Error in run_bench.sh

The terminal output shows:
```
/scripts/bench/run_bench.sh: line 777: syntax error near unexpected token `fi'
```
This is an unrelated shell syntax error in `run_bench.sh` (likely an
unbalanced if/fi block near line 777) that occurs after all benchmarks have
completed and results are stored. It does not affect the benchmark data.
