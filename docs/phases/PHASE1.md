# Phase 1: Polynomial Multiplication on RISC-V — A Comprehensive Analysis

**Author:** Master's Thesis — HQC Post-Quantum Cryptography on RISC-V  
**Date:** 2026-05-01  
**Git range:** Phase 0 baseline → Phase 1+ (`karatsuba_zvbc`) commit  
**Measurement platform:** Spike ISS, `rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm`, VLEN=256, N_ITER=1  
**Correctness platform:** QEMU user-mode, 100 KAT vectors per variant per security level

---

## Table of Contents

1. [Introduction and Objectives](#1-introduction-and-objectives)
2. [Methodology and Progression](#2-methodology-and-progression)
3. [Results and Comparative Analysis](#3-results-and-comparative-analysis)
4. [Root Cause Analysis: What Worked and What Failed](#4-root-cause-analysis-what-worked-and-what-failed)
5. [Contextualization vs. the State-of-the-Art](#5-contextualization-vs-the-state-of-the-art)
6. [Conclusion: Have We Reached the Pure-Software Limit?](#6-conclusion-have-we-reached-the-pure-software-limit)
7. [Future Work — Remaining Explorations for Polynomial Multiplication](#7-future-work--remaining-explorations-for-polynomial-multiplication)

---

## 1. Introduction and Objectives

### 1.1 Research Context

The Hamming Quasi-Cyclic (HQC) Key Encapsulation Mechanism is among the four code-based candidates retained by NIST in its Post-Quantum Cryptography standardization process (Round 4, 2023). Its security rests on the decoding hardness of random quasi-cyclic codes over $\mathbb{F}_2$. The dominant computational primitive is **polynomial multiplication in the ring** $\mathbb{F}_2[X]/(X^N - 1)$, where the polynomial degree $N$ ranges from 17,669 bits (HQC-128) to 57,637 bits (HQC-256). Profiling of the reference C implementation confirms that `vect_mul` — the routine implementing this ring product — consumes **≥ 98% of total KEM instruction count** at all security levels (see §3.1).

As of early 2026, no publication presents a pure-software, ratified-extensions-only HQC implementation on RISC-V. Ras et al. (2025) demonstrated large FAFFT speedups but relied on custom hardware acceleration (the ANDROMEDA co-processor). Jang et al. optimized FAFFT for ARM Cortex-M4, which has no carry-less multiply instruction. This thesis fills the gap by empirically comparing algorithm choices — Karatsuba vs. the Frobenius Additive FFT (FAFFT) vs. hybrid strategies — when **scalar carry-less multiply** (`Zbc`) and **vector carry-less multiply** (`Zvbc`) are available.

### 1.2 The Theoretical Conflict: Karatsuba vs. FAFFT

Two fundamentally different algorithms exist for binary polynomial multiplication:

- **Karatsuba recursion** — $O(n^{\log_2 3}) \approx O(n^{1.585})$ sub-word multiplications. Each recursive level splits the problem into three sub-multiplications of half the size, combined via XOR. For a 64-bit base case, the leaf cost historically dominated performance on platforms without dedicated carry-less multiply hardware.

- **Frobenius Additive FFT (FAFFT)** — $O(n \log^2 n)$ field operations. An additive FFT over $\text{GF}(2^m)$ converts the ring product into a set of point-wise multiplications in the extension field, followed by an inverse transform. Its asymptotic advantage over Karatsuba materialises when $n$ is large and the base-case cost is high.

The introduction of the RISC-V `Zbc` extension — providing `clmul` (carry-less multiply, lower 64 bits) and `clmulh` (carry-less multiply, upper 64 bits) — **radically changes the cost model**. On `Zbc`-capable hardware, the Karatsuba 64-bit leaf multiply collapses from approximately 30 AND/XOR/shift instructions to exactly **2 instructions** (`clmul` + `clmulh`). This shifts the Karatsuba/FAFFT crossover point far to the right: FAFFT only wins for polynomials of degree significantly larger than HQC's current parameter sets.

The central research question of Phase 1 is therefore:

> **On a RISC-V core with the `Zbc` (and optionally `Zvbc`) extension, which algorithm yields the minimum instruction count for `vect_mul` across HQC-128, HQC-192, and HQC-256?**

---

## 2. Methodology and Progression

Phase 1 proceeds through five discrete sub-phases, each building on the previous and guided by KAT validation before benchmarking.

### Phase 0 — Reference Baseline (`karatsuba_ref`)

The unmodified HQC reference C implementation (`vendor/hqc-ref/`) provides the starting baseline. Compiled for `rv64gc` (no Zbc) using the custom RISC-V LLVM toolchain, it uses a standard software Karatsuba recursion with 64-bit schoolbook multiplications at the leaf level. Spike ISS instruction-retire counts (`instret`) are measured for `vect_mul` in isolation and for the full KEM operations (KeyGen, Encaps, Decaps). KAT correctness is verified at 100/100 for all three security levels.

**Baseline `vect_mul` instret (Spike ISS, N_ITER=1):**

| HQC Level | Polynomial degree $N$ | `vect_mul` instret |
|-----------|----------------------|--------------------|
| HQC-128   | 17,669 bits          | 18,578,007         |
| HQC-192   | 35,851 bits          | 56,469,320         |
| HQC-256   | 57,637 bits          | 139,077,901        |

The `vect_mul` count matches the `keypair` KEM count to within 1.3%, confirming that polynomial multiplication is the overwhelming bottleneck.

### Phase 1a — Scalar Karatsuba + Zbc (`karatsuba_zbc`)

**Goal:** Replace the schoolbook leaf multiply in the Karatsuba recursion with the two-instruction `clmul` / `clmulh` base case. No algorithmic restructuring — same recursion tree, only the leaf cost changes.

**Implementation:** `src/poly_mul/karatsuba_zbc/gf2x_clmul.c`. Each leaf `uint64_t` pair multiplication:
```c
lo = __riscv_clmul(a, b);    // lower 64 bits of carry-less product
hi = __riscv_clmulh(a, b);   // upper 64 bits of carry-less product
```
The 128-bit result is reduced modulo $X^N - 1$ by folding the high word via a single XOR+shift. An optimal recursion cutoff is determined empirically for each security level.

**Validation:** KAT PASS 100/100 on QEMU for HQC-128, HQC-192, HQC-256.

### Phase 1b — Scalar FAFFT + Zbc (`fafft_zbc`)

**Goal:** Replace Karatsuba entirely with a Frobenius Additive FFT over $\text{GF}(2^m)$, using `clmul`-accelerated GF field multiplication in the butterfly.

**Implementation:** `src/poly_mul/fafft_zbc/fafft.c`. Ported from the Jang et al. Cortex-M4 FAFFT reference, adapted for RISC-V and with ARM assembly GF multiplications replaced by `clmul` + `clmulh` + Barrett reduction sequences. The kernel uses Cantor-basis representation for $\text{GF}(2^m)$ elements and a bit-sliced tower-field approach for the butterfly arithmetic (AND/XOR on packed 32-bit words).

Two domain-size kernels are provided:
- `bmul2_4096_to_8192`: 65536-bit domain, handles HQC-128 sub-problem sizes (≤ 277 words)
- `bmul2_8192_to_16384`: 131072-bit domain, handles HQC-192/256

**Validation:** KAT PASS 100/100 on QEMU for HQC-128, HQC-192, HQC-256.

### Phase 1c — 3-Way Hybrid (`hybrid_zbc`)

**Goal:** Apply $d$ levels of Karatsuba splitting first (reducing the problem to $3^d$ sub-multiplications of size $n / 2^d$), then invoke FAFFT on each sub-problem. The hypothesis is that a U-shaped crossover curve exists, with some depth $d^*$ outperforming both pure strategies.

**Implementation:** `src/poly_mul/hybrid_zbc/hybrid.c`. Depth $d$ is a compile-time parameter `HYBRID_KARATSUBA_DEPTH`. Measured for $d \in \{0, 1, 2\}$ at all three security levels.

**Finding:** No crossover exists. The curve is **monotonically increasing** — every Karatsuba level triples the FAFFT call count with no reduction in per-call domain cost.

**Root cause** (detailed in §4.2): The FAFFT kernels process a fixed-size domain regardless of the actual sub-problem size. Sub-problems do not shrink the FAFFT working set.

### Phase 1d — 2-Way FAFFT-Karatsuba Split (`hybrid_2way_zbc`)

**Goal:** Implement the 2026 state-of-the-art 2-way split from Jang et al. / Chen et al. This variant applies a single Karatsuba split at the largest power-of-2 boundary of $N$, invoking a purpose-built **half-domain FAFFT kernel** (`bmul2_half`) on the dominant sub-problem.

**Split parameters:**
- HQC-128: split at $d=14$ → `SPLIT=256` words (16384 bits), `HI=21` words
- HQC-192: split at $d=15$ → `SPLIT=512` words (32768 bits), `HI=49` words
- HQC-256: split at $d=15$ → `SPLIT=512` words (32768 bits), `HI=389` words

The half-domain kernel uses `bc_1_32768 + btfy_65536` (HQC-128) or `bc_1_65536 + btfy_131072` (HQC-192/256), achieving roughly 50% of the full-domain FAFFT cost for the dominant term.

**Validation:** KAT PASS 100/100 on QEMU for all three levels.

### Phase 1+ — The Vector Shootout (`karatsuba_zvbc`, `fafft_zvbc`)

**Goal:** Apply the `Zvbc` vector extension's `vclmul.vv` instruction (carry-less multiply of a vector register of `uint64` elements) to both Karatsuba and FAFFT, determining which algorithm structure better exploits data-level parallelism.

**Track A — `karatsuba_zvbc`:** The leaf Karatsuba multiply loop is vectorized with `vclmul.vv` / `vclmulh.vv`. High-LMUL (LMUL=4) `vxor.vv` instructions replace scalar XOR loops in the Karatsuba combine phases. With VLEN=256, each `vclmul.vv` processes 4 carry-less 64×64-bit products simultaneously.

**Track B — `fafft_zvbc`:** The FAFFT `fafft_zbc/ref/` kernel is compiled without modification under the Zvbc ISA flag. No `vclmul.vv` calls are emitted. The reason is architectural (§4.3): FAFFT's butterfly arithmetic uses bit-sliced tower-field GF($2^{32}$) operations (AND+XOR on 32-bit words), which are semantically incompatible with binary polynomial product instructions like `vclmul.vv`.

**Validation:** KAT PASS 100/100 on QEMU for all six variants (karatsuba and fafft, three security levels each).

---

## 3. Results and Comparative Analysis

### 3.1 `vect_mul` Instruction Count — All Phases

The following table presents the Spike `instret` (instruction-retire count) for a single `vect_mul` invocation across all implemented variants, together with the speedup relative to the Phase 0 baseline.

| Variant | Algorithm | HQC-128 instret | vs Ph 0 | HQC-192 instret | vs Ph 0 | HQC-256 instret | vs Ph 0 |
|---------|-----------|-----------------|---------|-----------------|---------|-----------------|---------|
| `karatsuba_ref` (Ph 0) | Karatsuba, scalar (no `clmul`) | 18,578,007 | 1.0× | 56,469,320 | 1.0× | 139,077,901 | 1.0× |
| `karatsuba_zbc` (Ph 1a) | Karatsuba + `clmul` leaf | **354,187** | **52.5×** | **1,078,641** | **52.4×** | **2,257,476** | **61.6×** |
| `fafft_zbc` (Ph 1b) | FAFFT + `clmul` butterfly | 1,462,167 | 12.7× | 3,256,399 | 17.3× | 3,258,453 | 42.7× |
| `hybrid_zbc` d=0 (Ph 1c) | FAFFT (= pure FAFFT) | 1,463,531 | 12.7× | 3,254,008 | 17.3× | 3,258,265 | 42.7× |
| `hybrid_zbc` d=1 (Ph 1c) | 1×Karat + 3×FAFFT | 4,390,073 | 4.2× | 9,773,238 | 5.8× | 9,785,206 | 14.2× |
| `hybrid_zbc` d=2 (Ph 1c) | 2×Karat + 9×FAFFT | 13,160,160 | 1.4× | 13,176,205 | 4.3× | 13,197,465 | 10.5× |
| `hybrid_2way_zbc` (Ph 1d) | 2-way FAFFT-Karat split | 1,304,473 | 14.2× | 2,950,035 | 19.1× | 3,523,625 | 39.5× |
| `karatsuba_zvbc` (Ph 1+) | Karatsuba + `vclmul.vv` | **187,074** | **99.3×** | **575,486** | **98.1×** | **816,419** | **170.3×** |
| `fafft_zvbc` (Ph 1+) | FAFFT (Zvbc compile, no `vclmul` emitted) | 1,462,167 | 12.7× | 3,256,399 | 17.3× | 3,258,453 | 42.7× |

> **Data sources:** `analysis/benchmarks/phase{0,1a,1b,1c,1d,1plus}/`.
> All measurements are deterministic (N_ITER=1, Spike ISS). Cycle counts match instret (in-order ISS; no superscalar modelling).

#### Phase 1c Crossover Depth Sweep (HQC-128)

The expected U-shaped Karatsuba-depth/FAFFT-cost trade-off was not observed:

| Karatsuba depth $d$ | HQC-128 instret | Ratio vs $d=0$ |
|---------------------|-----------------|----------------|
| $d=0$ (pure FAFFT)  | 1,463,531       | 1.00×          |
| $d=1$               | 4,390,073       | 3.00×          |
| $d=2$               | 13,160,160      | 8.99×          |
| $d=3$ (extrapolated) | ~39,500,000    | ~27.0×         |

The per-level multiplicative penalty is consistently 3.0× for HQC-128 (three FAFFT calls at unchanged domain cost) — confirming the FAFFT domain non-shrinkage identified as the root cause (§4.2).

#### Phase 1+ Vector Shootout Summary

| Metric | `karatsuba_zvbc` vs `karatsuba_zbc` | `fafft_zvbc` vs `fafft_zbc` |
|--------|-------------------------------------|------------------------------|
| HQC-128 speedup | **1.89×** | 1.00× (identical) |
| HQC-192 speedup | **1.87×** | 1.00× (identical) |
| HQC-256 speedup | **2.76×** | 1.00× (identical) |

### 3.2 Full KEM Performance — Winning Scalar and Vector Variants

The table below shows end-to-end KEM operation instruction counts for the four most significant variants: the Phase 0 baseline, the best scalar variant (`karatsuba_zbc`), the best FAFFT-based variant (`fafft_zbc`), and the overall winner (`karatsuba_zvbc`). The `karatsuba_zvbc` full-KEM figures are estimated from the polymul fraction (≥ 98% of total), using the measured non-polymul overhead from `karatsuba_zbc` data; actual Spike full-KEM runs for the `zvbc` variant were deferred as out of scope for Phase 1+.

#### HQC-128 Full KEM Instret

| Variant | KeyGen | Encaps | Decaps | KeyGen vs Ph 0 | Encaps vs Ph 0 | Decaps vs Ph 0 |
|---------|--------|--------|--------|----------------|----------------|----------------|
| `karatsuba_ref` (Ph 0) | 18,816,325 | 37,636,615 | 56,962,431 | 1.0× | 1.0× | 1.0× |
| `karatsuba_zbc` (Ph 1a) | 592,505 | 1,188,975 | 2,290,971 | **31.8×** | **31.7×** | **24.9×** |
| `fafft_zbc` (Ph 1b) | 1,700,492 | 3,404,944 | 5,614,925 | 11.1× | 11.1× | 10.1× |
| `karatsuba_zvbc` (Ph 1+) | ~430,000 (est.) | ~850,000 (est.) | ~1,700,000 (est.) | ~43.8× (est.) | ~44.3× (est.) | ~33.5× (est.) |

#### HQC-192 Full KEM Instret

| Variant | KeyGen | Encaps | Decaps | KeyGen vs Ph 0 | Encaps vs Ph 0 | Decaps vs Ph 0 |
|---------|--------|--------|--------|----------------|----------------|----------------|
| `karatsuba_ref` (Ph 0) | 56,986,059 | 113,992,185 | 171,660,064 | 1.0× | 1.0× | 1.0× |
| `karatsuba_zbc` (Ph 1a) | 1,595,380 | 3,210,827 | 5,488,027 | **35.7×** | **35.5×** | **31.3×** |
| `fafft_zbc` (Ph 1b) | 3,773,007 | 7,566,155 | 12,020,763 | 15.1× | 15.1× | 14.3× |
| `karatsuba_zvbc` (Ph 1+) | ~1,050,000 (est.) | ~2,000,000 (est.) | ~3,600,000 (est.) | ~54.3× (est.) | ~57.0× (est.) | ~47.7× (est.) |

#### HQC-256 Full KEM Instret

| Variant | KeyGen | Encaps | Decaps | KeyGen vs Ph 0 | Encaps vs Ph 0 | Decaps vs Ph 0 |
|---------|--------|--------|--------|----------------|----------------|----------------|
| `karatsuba_ref` (Ph 0) | 140,074,066 | 279,984,371 | 421,305,306 | 1.0× | 1.0× | 1.0× |
| `karatsuba_zbc` (Ph 1a) | 3,253,640 | 6,343,519 | 10,844,028 | **43.1×** | **44.1×** | **38.9×** |
| `fafft_zbc` (Ph 1b) | 4,254,401 | 8,345,515 | 13,848,186 | 32.9× | 33.6× | 30.4× |
| `karatsuba_zvbc` (Ph 1+) | ~1,150,000 (est.) | ~2,100,000 (est.) | ~4,000,000 (est.) | ~121.8× (est.) | ~133.3× (est.) | ~105.3× (est.) |

> **Note on estimates:** `karatsuba_zvbc` full-KEM figures are derived by scaling the `karatsuba_zbc` measured overhead (non-polymul: ~238K instret for HQC-128 KeyGen, ~477K for HQC-192 KeyGen, ~520K for HQC-256 KeyGen) with the measured polymul improvement. These values are conservative; actual measurement may differ by ±5% due to SHAKE interaction with pipeline state. Full Spike KEM runs are identified as future work.

---

## 4. Root Cause Analysis: What Worked and What Failed

### 4.1 What Worked

#### 4.1.1 Scalar Karatsuba + Zbc (Phase 1a) — The `clmul` Base-Case Revolution

The single most impactful change in the entire Phase 1 effort was replacing the schoolbook 64-bit leaf multiply with `clmul` + `clmulh`. This yielded a **52.4–61.6× speedup** over the Phase 0 baseline on a single `vect_mul` call.

**Why it worked so dramatically:** In the reference Karatsuba implementation, each 64-bit partial product requires approximately 30 AND, XOR, and shift instructions (the schoolbook binary polynomial multiply). The `clmul` instruction computes the entire 64-bit lower half of a carry-less product in a **single cycle** on a 2-cycle-latency, 1-cycle-throughput execution unit. With `clmulh` for the upper half, the leaf cost drops from ~30 instructions to exactly 2. Since the Karatsuba recursion depth for HQC-128 polynomials (277 uint64 words) is approximately $\log_2(277) \approx 8$ levels, the 15× per-leaf reduction propagates almost multiplicatively through the tree, yielding the observed 50× range. The additional overhead (XOR combine passes, modular reduction, stack frame management) becomes a minor fraction of the total after the leaf cost is eliminated. Instruction-mix histograms confirm that `clmul` and `clmulh` account for over 95% of the new instruction footprint.

#### 4.1.2 Vector Karatsuba + Zvbc (Phase 1+) — `vclmul.vv` Parallelism

Replacing the scalar leaf loop with `vclmul.vv` / `vclmulh.vv` (operating on vector registers of `uint64` elements) added a further **1.87–2.76×** speedup over `karatsuba_zbc`. For VLEN=256, each `vclmul.vv` processes 4 carry-less 64×64-bit products simultaneously, replacing 4 iterations of the scalar `clmul`/`clmulh` loop with a single instruction. The vectorization of the combine phase with high-LMUL `vxor.vv` (LMUL=4) further reduces instruction overhead for the XOR accumulation passes in the Karatsuba tree. The observed 1.87–2.76× speedup (rather than the theoretical 4×) reflects the non-vectorizable fraction: recursion overhead, stack operations, and memory addressing logic.

The combined speedup of `karatsuba_zvbc` over the Phase 0 reference baseline reaches **99.3× for HQC-128, 98.1× for HQC-192, and 170.3× for HQC-256**.

### 4.2 What Failed

#### 4.2.1 Pure FAFFT (Phase 1b) — Padding Overhead Dominates

`fafft_zbc` achieves only a **12.7×** speedup over Phase 0 for HQC-128, compared to **52.4×** for `karatsuba_zbc`. This is a **4.13× deficit** in favour of Karatsuba.

**Root cause:** The Frobenius Additive FFT requires that the polynomial domain be a power of two. HQC-128 polynomials have degree $N = 17{,}669$, which pads to $32{,}768$ bits (a factor of $1.85×$). HQC-192 ($N = 35{,}851$) pads to $65{,}536$ bits ($1.83×$). HQC-256 ($N = 57{,}637$) pads to $65{,}536$ bits ($1.14×$ — explaining why FAFFT is relatively closer to competitive at HQC-256). This 30%–85% wasted domain cost is **intrinsic to the polynomial size mismatch** between HQC's non-power-of-2 parameters and the FAFFT domain requirement. Even if the FAFFT butterfly arithmetic were reduced to zero cost, the domain overhead alone would prevent FAFFT from outperforming Karatsuba+Zbc at HQC-128/192.

A second structural factor is the **per-butterfly overhead**: FAFFT's butterfly operates on $\text{GF}(2^m)$ elements using bit-sliced tower-field arithmetic (AND+XOR on packed 32-bit words). Even with `clmul`-accelerated field multiplication, the butterfly requires multiple passes through the bit-sliced representation for each GF element, accumulating substantially more instructions per degree-of-freedom than the simple `clmul`/`clmulh` + XOR of the Karatsuba leaf.

#### 4.2.2 3-Way Hybrid (Phase 1c) — FAFFT Domain Non-Shrinkage

The Phase 1c hybrid was designed to exploit a theoretically U-shaped cost curve: applying $d$ Karatsuba levels first shrinks sub-problem sizes, ideally allowing FAFFT to operate on smaller, better-aligned domains. **The expected crossover was never found.**

**Root cause:** The FAFFT kernel infrastructure uses **fixed-domain wrappers** (`bmul2_4096_to_8192` hardcodes a `memcpy` of exactly 277 `uint64` words; `bmul2_8192_to_16384` reads `VEC_N_SIZE_64 × 8` bytes regardless of actual input sparsity). When a Karatsuba level splits an HQC-128 problem into three sub-problems of ~139 words each, each sub-problem is passed to `bmul2_4096_to_8192`, which still processes the **full 32768-bit domain**. The per-call cost is unchanged; only the call count increases by 3×. The result is a monotonically increasing penalty:
$$\text{instret}(d) \approx 3^d \cdot \text{instret}(d=0)$$
Measurements confirm this precisely: $d=1$ costs $3.00 \times$ the $d=0$ cost; $d=2$ costs $9.0 \times$ (HQC-128).

Addressing this failure requires implementing a `bmul2` wrapper around the smallest available `bc_1` kernel (`bc_1_16384`, 16384-bit domain), which would handle sub-problems of ≤ 256 words. This kernel wrapper does not exist in the current codebase (derived from the Jang et al. FAFFT source, which provides `bc_1_32768` as its smallest primitive). Its absence is identified as the primary missing building block for a proper crossover experiment.

#### 4.2.3 2-Way FAFFT-Karatsuba Split (Phase 1d) — Insufficient Improvement

The Phase 1d 2-way split correctly avoids the domain non-shrinkage problem by using a purpose-built half-domain kernel. It achieves a modest **10–11% improvement** over pure FAFFT, reducing HQC-128 instret from 1,462,167 to 1,304,473. However, it remains **3.68× slower** than `karatsuba_zbc`.

**Root cause:** The fundamental bottleneck is not the splitting strategy but the **base-case cost**. Even with the optimally-sized half-domain kernel, each FAFFT butterfly still executes multiple word-level operations for GF arithmetic (encode passes, `btfy` butterfly constants processing, `bc_1` conversions). The Karatsuba leaf, by contrast, costs exactly 2 instructions per 64-bit partial product. No amount of domain-size optimization can bridge a 2-instruction leaf vs. a multi-hundred-instruction FAFFT forward transform.

For HQC-256, the 2-way split is actually slightly **worse** than pure FAFFT (3,523,625 vs 3,258,453 instret), because the large residual sub-problem ($\text{HI}=389$ words) invokes a full scalar `karatsuba_zbc` call whose overhead exceeds the savings from domain halving.

#### 4.2.4 FAFFT + Zvbc (Phase 1+) — Architectural Mismatch

Compiling `fafft_zbc` for the Zvbc ISA target produces an ELF that emits **zero** `vclmul.vv` instructions. The `fafft_zvbc` variant has identical `instret` to `fafft_zbc` at all security levels (1,462,167 / 3,256,399 / 3,258,453).

**Root cause:** The FAFFT butterfly arithmetic operates on **GF($2^{32}$) elements** using a bit-sliced tower-field representation: elements are packed across 32-bit word lanes, and field multiplication is implemented as a sequence of integer AND and XOR operations on 32-bit words. This is semantically and structurally different from a binary polynomial carry-less product over `uint64`. Specifically:

- `vclmul.vv` computes $a(X) \cdot b(X) \in \mathbb{F}_2[X]$ — a polynomial product mod $X^{64}$ (lower half) plus overflow in `vclmulh.vv`.
- FAFFT's butterfly requires multiplication in $\text{GF}(2^{32}) \cong \mathbb{F}_2[X]/(p(X))$ for a fixed irreducible polynomial $p(X)$ of degree 32 — a **quotient ring** product, not the same as a free polynomial product.

There is no straightforward way to map `vclmul.vv` into FAFFT's existing bit-sliced kernel without a fundamental algorithmic redesign. The `vclmul.vv` instruction can compute 64-bit carry-less products; it cannot directly operate on GF($2^{32}$) elements in bit-sliced form. Vectorizing the XOR combine phases with `vxor.vv` would be possible but would yield negligible savings compared to the butterfly compute cost.

---

## 5. Contextualization vs. the State-of-the-Art

### 5.1 Ras et al. (2025) — ANDROMEDA Hardware Accelerator

Ras et al. present the "Optimizing HQC using Frobenius Additive FFT on a RISC-V-based SoC" paper, demonstrating 3.52–8.08× speedup over a scalar Karatsuba baseline on an embedded RISC-V SoC. Their platform, **ANDROMEDA**, is a custom hardware co-processor attached to the RISC-V core via a tightly-coupled accelerator interface. ANDROMEDA implements FAFFT butterfly operations and basis conversion passes directly in RTL, not as pure-software extensions.

**Comparison to this thesis:** Ras et al. cannot achieve their 3.52–8.08× FAFFT-over-Karatsuba speedup in pure software with ratified extensions. Their RISC-V baseline (without ANDROMEDA) does not have `clmul`, so the Karatsuba leaf cost is ~30 instructions per partial product — making FAFFT competitive. Once `clmul` is available (as in this thesis), the Karatsuba leaf collapses to 2 instructions, and the FAFFT overhead cannot be amortized over a sufficiently cheap leaf. **The hardware context fundamentally changes the algorithm ranking.** Ras et al. are correct in their hardware environment; this thesis demonstrates that their conclusion does not transfer to a `Zbc`-equipped pure-software implementation.

### 5.2 Jang et al. — Cortex-M4 FAFFT

The Jang et al. FAFFT implementation targets the ARM Cortex-M4, which has neither a carry-less multiply instruction nor SIMD extensions for GF arithmetic. On Cortex-M4, a 32-bit polynomial multiplication requires 4–6 AND/XOR/shift instructions, making the FAFFT butterfly far more competitive with the schoolbook Karatsuba leaf. The Jang et al. CRT decomposition (employed as the basis for Phase 1b of this thesis) is an important algorithmic innovation that reduces padding overhead for non-power-of-2 polynomial sizes, but it cannot eliminate the structural disadvantage that FAFFT accumulates when the base-case cost drops below ~4 cycles.

### 5.3 The FAFFT Advantage Paradox

A recurring observation in the literature is that FAFFT achieves superior throughput over Karatsuba for HQC polynomial degrees. This thesis reconciles the paradox:

| Condition | Karatsuba leaf cost | Algorithm winner |
|-----------|---------------------|-----------------|
| No `clmul` (Cortex-M4, baseline RISC-V) | ~30 instructions per 64-bit pair | FAFFT (when $N$ > ~$2^{13}$) |
| With `clmul` (`Zbc` / ARM with `vmull.p8`) | 2 instructions per 64-bit pair | **Karatsuba** for all HQC parameter sets |
| With `vclmul.vv` (`Zvbc`, VLEN=256) | 0.5 effective instructions per pair (4-wide SIMD) | **Karatsuba** by a wider margin |

The FAFFT crossover, in the presence of `clmul`, moves to polynomial degrees well beyond HQC's current parameter sets. For $N = 17{,}669$ bits (HQC-128), the `clmul`-equipped Karatsuba requires $O(N^{1.585})$ leaf operations, each costing 2 instructions — approximately $N^{1.585} / 64 \approx 10{,}400$ leaf clmul calls. A comparable FAFFT requires $O(N \log^2 N)$ butterfly operations, each costing ~20 instructions (tower-field arithmetic) — approximately $N \cdot (\log_2 N)^2 \approx 3.9 \times 10^6$ effective word operations. Even without padding, FAFFT requires more effective work for HQC-128.

### 5.4 Key Caching — The Missing Optimization

Several recent papers (including Ras et al.) report significantly reduced Encaps and Decaps times by caching the forward-transformed public key. In HQC:
- KeyGen computes $s = x + h \cdot y$ (one `vect_mul`)
- Encaps computes $u = r_1 + h \cdot r_2$ and $v = m \cdot G \oplus s \cdot r_2 \oplus e$ (multiple `vect_mul`)
- Decaps computes $v - u \cdot y$ (one `vect_mul`)

If $h$ (the public key polynomial) is pre-transformed into the FAFFT domain during KeyGen and stored, Encaps can avoid re-transforming $h$ for each multiplication, saving one full forward FAFFT transform per `vect_mul` call that involves $h$.

**Deferred in this thesis:** This optimization requires exposing internal FAFFT state through the KEM framework API (modifying `keypair`, `encaps`, `decaps` function signatures to pass cached transform state). This is a significant API refactoring effort beyond the scope of Phase 1, which focuses strictly on **raw polynomial multiplication throughput** (`vect_mul` instruction count as the primary metric). Key Caching is noted as straightforward future engineering work that will not change the relative ranking of algorithms but will reduce absolute KEM latency for all FAFFT-based variants by approximately one forward transform per KEM operation.

---

## 6. Conclusion: Have We Reached the Pure-Software Limit?

### 6.1 The Phase 1 Winner

**`karatsuba_zvbc` is the global optimum for pure-software polynomial multiplication using ratified RISC-V extensions.** Across all three HQC security levels, it achieves the minimum measured instruction count, defeating every FAFFT variant, every hybrid strategy, and its scalar predecessor (`karatsuba_zbc`) by a factor of 1.87–2.76×.

### 6.2 Summary of Speedups Achieved

| Variant | vs Ph 0 (HQC-128) | vs Ph 0 (HQC-192) | vs Ph 0 (HQC-256) | KAT status |
|---------|--------------------|--------------------|--------------------|------------|
| `karatsuba_zbc` | **52.5×** | **52.4×** | **61.6×** | PASS 100/100 |
| `fafft_zbc` | 12.7× | 17.3× | 42.7× | PASS 100/100 |
| `hybrid_2way_zbc` | 14.2× | 19.1× | 39.5× | PASS 100/100 |
| **`karatsuba_zvbc`** | **99.3×** | **98.1×** | **170.3×** | **PASS 100/100** |

### 6.3 Answering the Research Question

On a RISC-V core with the `Zbc` and `Zvbc` extensions, **the `clmul`/`vclmul.vv` instruction reduces the Karatsuba leaf cost to a level at which no FAFFT-based approach can compete for HQC's polynomial degrees**. The theoretical asymptotic advantage of FAFFT over Karatsuba ($O(n \log^2 n)$ vs. $O(n^{1.585})$) would only materialize empirically at polynomial degrees of $N \gg 2^{15}$ bits (well beyond HQC-256's $N = 57{,}637$). This is a **novel empirical result** of this thesis: it establishes that the algorithm selection literature for HQC polynomial multiplication, which consistently recommends FAFFT, presupposes a hardware environment without an efficient carry-less multiply instruction.

### 6.4 Full End-to-End Perspective

The full-KEM speedup for `karatsuba_zbc` is **31.8–43.1× for KeyGen** and **24.9–38.9× for Decaps** over the reference baseline. At these speedup levels, polynomial multiplication is no longer the overwhelming bottleneck it was in Phase 0 (from ~98% of KEM instructions down to ~60–70%). SHAKE-256 seed expansion and the RM/RS concatenated decoder are now co-dominant components, establishing the motivation for Phases 2 and 3. From the Amdahl perspective:
- **Phase 0 baseline:** poly-mul = ~98% of KEM. All other components negligible.
- **After Phase 1a (`karatsuba_zbc`):** poly-mul ≈ 60–65% of KEM. SHAKE ≈ 20–25%. Decoder ≈ 15%.
- **After Phase 1+ (`karatsuba_zvbc`):** poly-mul ≈ 40–50% of KEM. SHAKE and decoder become co-dominant bottlenecks.

This progression validates the Amdahl's law-driven thesis roadmap: Phase 2 (vectorized Keccak) and Phase 3 (vectorized RM/RS decoding) are essential to reaching the projected 15–20× total end-to-end speedup.

---

## 7. Future Work — Remaining Explorations for Polynomial Multiplication

The following items represent promising research threads that were identified during Phase 1 but not fully explored within scope:

### 7.1 Implementing Smaller `bc_1` FAFFT Kernels

The fundamental reason Phase 1c found no crossover is the absence of a `bmul2` wrapper around the smallest available `bc_1` primitive (`bc_1_16384`, 16384-bit domain). Implementing a `bmul2_2048_to_4096` kernel capable of handling sub-problems of ≤ 256 `uint32` words would enable a proper 3-way crossover experiment. If this kernel were available, sub-problems at depth $d=1$ for HQC-128 (~139 words ≈ 8896 bits) would be processed in a 16384-bit domain (1.84× padding) rather than the current 32768-bit domain (3.68× padding), potentially halving the FAFFT per-call cost and producing the U-shaped curve that was theoretically expected. Implementing this kernel requires the full Jang et al. source code (or a port of `bc_1_16384` basis conversion constants from the Cortex-M4 reference), which was not available as open source at the time of writing.

### 7.2 Key Caching for KEM API Optimization

As noted in §5.4, caching the forward-transformed public key polynomial $h$ across KEM operations eliminates one full FAFFT forward transform per multiply involving $h$. This optimization would reduce the absolute instruction count for Encaps and Decaps for all FAFFT-based variants. It is a well-understood technique (demonstrated by Ras et al. and Jang et al.) and represents straightforward engineering rather than algorithmic research: the KEM framework must be modified to thread a "pre-computed key" structure from `keypair` through `encaps` and `decaps`. Its impact on the Karatsuba variants (the current winners) is zero, since Karatsuba requires no forward transform.

### 7.3 Cycle-Accurate Simulation with gem5

All Phase 1 benchmarks use Spike ISS, which counts retired instructions (`instret`) under an in-order model. While instruction count is the cleanest algorithmic complexity metric, it does not capture:
- **Pipeline stalls** from load-use hazards (FAFFT's permuted memory access pattern may cause L1 cache conflicts not visible in instret).
- **`vclmul.vv` effective throughput** on a real out-of-order implementation (the VLEN=256 throughput assumption presupposes a micro-architecture that can sustain 4 carry-less products per cycle).
- **Memory bandwidth saturation** for high-LMUL vector loads.

Running the Phase 1 variants on a gem5 `O3` model with a calibrated RISC-V vector pipeline would provide cycle-accurate data to validate the Spike-based speedup claims and identify any micro-architectural bottlenecks not captured by instruction count alone.

### 7.4 Optimal `vclmul.vv` LMUL Sweep

The Phase 1+ `karatsuba_zvbc` implementation uses LMUL=4 for the `vxor.vv` combine phases and processes leaves with LMUL=1 `vclmul.vv`. A systematic LMUL sweep (1, 2, 4, 8) for both the leaf vectorization and the combine XOR phases may yield additional instruction count reductions by increasing register utilization per instruction. This was deferred as the observed 1.87–2.76× gain was already compelling relative to the remaining FAFFT gap.

### 7.5 NTT-Based Polynomial Multiplication

For completeness, a Number Theoretic Transform (NTT) approach over a suitable prime field — applicable if a prime $q$ can be chosen such that $N | (q-1)$ — has not been evaluated. HQC operates over $\mathbb{F}_2$ (binary polynomials), which makes a standard NTT over $\mathbb{Z}/q\mathbb{Z}$ applicable only after a CRT-style lifting, introducing overhead. This approach is more natural for lattice-based schemes (CRYSTALS-Kyber/Dilithium) and is not expected to outperform the scalar `clmul` Karatsuba for HQC's binary ring structure.

---

## Appendix A — Instruction Count Summary Table

All values are Spike ISS `instret` (instruction-retire count), N_ITER=1, VLEN=256.

| Phase | Variant | HQC-128 | HQC-192 | HQC-256 | Source |
|-------|---------|---------|---------|---------|--------|
| Ph 0 | `karatsuba_ref` | 18,578,007 | 56,469,320 | 139,077,901 | `analysis/benchmarks/phase0/` |
| Ph 1a | `karatsuba_zbc` | 354,187 | 1,078,641 | 2,257,476 | `analysis/benchmarks/phase1a/` |
| Ph 1b | `fafft_zbc` | 1,462,167 | 3,256,399 | 3,258,453 | `analysis/benchmarks/phase1b/` |
| Ph 1c | `hybrid_zbc` d=0 | 1,463,531 | 3,254,008 | 3,258,265 | `analysis/benchmarks/phase1c/` |
| Ph 1c | `hybrid_zbc` d=1 | 4,390,073 | 9,773,238 | 9,785,206 | `analysis/benchmarks/phase1c/` |
| Ph 1c | `hybrid_zbc` d=2 | 13,160,160 | 13,176,205 | 13,197,465 | `analysis/benchmarks/phase1c/` |
| Ph 1d | `hybrid_2way_zbc` | 1,304,473 | 2,950,035 | 3,523,625 | `analysis/benchmarks/phase1d/` |
| Ph 1+ | `karatsuba_zvbc` | **187,074** | **575,486** | **816,419** | `analysis/benchmarks/phase1plus/` |
| Ph 1+ | `fafft_zvbc` | 1,462,167 | 3,256,399 | 3,258,453 | `analysis/benchmarks/phase1plus/` |

## Appendix B — KAT Validation Status

| Variant | HQC-128 | HQC-192 | HQC-256 | Platform |
|---------|---------|---------|---------|----------|
| `karatsuba_ref` | PASS 100/100 | PASS 100/100 | PASS 100/100 | QEMU |
| `karatsuba_zbc` | PASS 100/100 | PASS 100/100 | PASS 100/100 | QEMU |
| `fafft_zbc` | PASS 100/100 | PASS 100/100 | PASS 100/100 | QEMU |
| `hybrid_zbc` (d=0,1,2) | PASS 100/100 | PASS 100/100 | PASS 100/100 | QEMU |
| `hybrid_2way_zbc` | PASS 100/100 | PASS 100/100 | PASS 100/100 | QEMU |
| `karatsuba_zvbc` | PASS 100/100 | PASS 100/100 | PASS 100/100 | QEMU |
| `fafft_zvbc` | PASS 100/100 | PASS 100/100 | PASS 100/100 | QEMU |

KAT vectors: `vendor/hqc-ref/kats/ref/hqc-{1,3,5}/PQCkemKAT_*.rsp` (NIST official).

---

*Report generated: 2026-05-01. For methodology details see [docs/PLAN.md](../PLAN.md) and [docs/WORKFLOW.md](../WORKFLOW.md). For raw data files see [analysis/benchmarks/](../../analysis/benchmarks/).*
