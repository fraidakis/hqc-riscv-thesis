# HQC-RISC-V: Implementation Plan

Phase-by-phase roadmap and 12-week execution schedule for the HQC
post-quantum KEM optimisation thesis.

**Context:** This thesis addresses a **significant research gap**: as of March 2026, **no publication presents a pure-software, ratified-extensions-only HQC implementation on RISC-V**. Ras et al. (2025) demonstrated FAFFT achieves 3.52–8.08× speedup but resorted to custom hardware (ANDROMEDA accelerator). Jang et al. optimized FAFFT for Cortex-M4 (which lacks `clmul`). OptHQC used x86 AVX2. This thesis fills the gap by combining `Zbc` (carry-less multiply), `V` (RVV), and `Zbkb` (bit manipulation) into the first optimized pure-software RISC-V HQC implementation, empirically comparing algorithm choices (Karatsuba vs. FAFFT vs. Hybrid) when `clmul` is available.

For day-to-day build/run commands see [`WORKFLOW.md`](WORKFLOW.md).
For toolchain and methodology rationale see [`TOOLCHAIN.md`](TOOLCHAIN.md).
For detailed literature review and five research proposals, see [`../HQC/1_Literature.md`](../HQC/1_Literature.md) and [`../HQC/2_Proposals.md`](../HQC/2_Proposals.md).
**Worked examples:** Binary polynomial multiplication with `Zbc` in [`../HQC/4_Zbc_example.md`](../HQC/4_Zbc_example.md); RVV-vectorized RM/RS decoding in [`../HQC/5_Decoding_RVV.md`](../HQC/5_Decoding_RVV.md).

---

## Table of Contents

- [Research Context & Bottleneck Progression](#research-context--bottleneck-progression)
- [Algorithm Selection: Why Karatsuba vs. FAFFT vs. Hybrid Matters](#algorithm-selection-why-karatsuba-vs-fafft-vs-hybrid-matters)
- [Codebase Reuse Strategy](#codebase-reuse-strategy)
- [Phase Overview](#phase-overview)
- [Phase 0 — Baseline KAT + Spike Benchmarks (Week 1)](#phase-0--baseline-kat--spike-benchmarks-week-1)
- [Phase 1a — Scalar Zbc Karatsuba (Weeks 2–4)](#phase-1a--scalar-zbc-karatsuba-weeks-24)
- [Phase 1b — FAFFT with Zbc (Weeks 5–7)](#phase-1b--fafft-with-zbc-weeks-57)
- [Phase 1c — Hybrid Zbc (Week 8)](#phase-1c--hybrid-zbc-week-8)
- [Phase 1+ — Vectorised FAFFT with Zvbc (Weeks 9–10)](#phase-1--vectorised-fafft-with-zvbc-weeks-910)
- [Phase 2 — RVV Keccak (Weeks 7–8 or as secondary branch)](#phase-2--rvv-keccak-weeks-7-8-or-as-secondary-branch)
- [Phase 3 — RVV RM/RS Decoder (Weeks 8–9 or as secondary branch)](#phase-3--rvv-rmrs-decoder-weeks-8-9-or-as-secondary-branch)
- [Phase 4 — Optional: Custom ISE Extensions (Upper Bound Analysis)](#phase-4--optional-custom-ise-extensions-upper-bound-analysis)
- [Weeks 11–12: Thesis Tables and Paper](#weeks-1112-thesis-tables-and-paper)
- [Measurement Checklist (per phase)](#measurement-checklist-per-phase)
- [Code Volume and Effort Estimation](#code-volume-and-effort-estimation)

---

## Research Context & Bottleneck Progression

This thesis is structured around **Amdahl's law**: each optimization phase accelerates the current dominant bottleneck until the next one surfaces. The following analysis (derived from Ras et al. 2025 profiling) tracks how runtime shifts across HQC's three core components:

### Baseline: Karatsuba Reference (no optimization)

| Component | Time (normalized) | % of Total |
|---|---|---|
| Polynomial multiplication (Karatsuba) | 95 | **95%** |
| SHAKE/Keccak seed expansion | 3.5 | 3.5% |
| RM/RS concatenated decoding | 1.5 | 1.5% |
| **Total** | **100** | |

→ Polynomial multiplication completely dominates. All other components are negligible.

### After Phase 1 (Zbc + RVV + Algorithm Optimization)

Assuming ~4× additional speedup from `Zbc`-optimized base case + `Zvbc` vectorization:

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 95 / 20 = **4.75** | **49%** |
| SHAKE/Keccak seed expansion | 3.5 | **36%** |
| RM/RS concatenated decoding | 1.5 | **15%** |
| **Total** | **~9.75** | |

→ **Critical transition point:** Poly-mul drops below 50%. **SHAKE is now co-dominant** (36%). Further poly-mul optimization alone yields diminishing returns—Phase 2 and 3 now become essential to reach 15–20× total speedup.

### Target: Full Stack (Phases 1+2+3)

Adding vectorized Keccak (~3× speedup, Proposal 3) and vectorized decoding (~3.5× speedup, Proposal 2):

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 | **65%** |
| SHAKE/Keccak (vectorized) | 3.5 / 3 ≈ **1.17** | **16%** |
| RM/RS decoding (vectorized) | 1.5 / 3.5 ≈ **0.43** | **6%** |
| **Total** | **~6.35** | |

→ **~15.7× total end-to-end speedup** over baseline. Approaching the ~19× achieved by ANDROMEDA hardware in pure software.

### Upper Bound: With `Zvbc` (Phase 1+)

`Zvbc` vector carry-less multiply (`vclmul.vv`) processes VLEN/64 butterflies per instruction (e.g., 4 for VLEN=256):

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 / 4 ≈ **1.19** | **40%** |
| SHAKE/Keccak (vectorized) | 1.17 | **40%** |
| RM/RS decoding (vectorized) | 0.43 | **15%** |
| **Total** | **~2.79** | |

→ **~35.8× total speedup** — exceeding ANDROMEDA's 19.22×. This justifies investment in `Zvbc` standardization and vendor deployment.

---

## Algorithm Selection: Why Karatsuba vs. FAFFT vs. Hybrid Matters

Ras et al. demonstrated 3.52–8.08× FAFFT speedup on baseline RISC-V **without `clmul`**. However, adding `clmul` fundamentally changes the cost model:

| Factor | Karatsuba+Zbc | FAFFT+Zbc | Hybrid (Proposal 1) |
|---|---|---|---|
| **Base-case cost** | ~4 cycles (`clmul`+reduction) | ~6–8 cycles + field operations | Karatsuba outer → FAFFT inner |
| **Overhead per level** | 5 XOR additions | Basis conversion + combine + permutation | Adapts crossover dynamically |
| **Recursion depth** | d* = 3–4 for HQC-128 | Single-pass FFT | 1–2 Karatsuba levels |
| **Padding / CRT overhead** | None | ~30% for HQC-128 (n=17,669, next power of 2 = 32K) | Via Jang et al. CRT (Ref. 3.1) |
| **Memory locality** | Excellent (cache-friendly recursion) | Poor (permutations) | Good (hybrid adapts) |

**Key insight:** The `clmul` instruction (2-cycle latency, 1-cycle throughput on modern cores) shifts the Karatsuba/FAFFT crossover point. **Phase 1a–c empirically determines the optimal algorithm** for each HQC parameter set. Preliminary hypothesis: Karatsuba+Zbc may dominate Karatsuba-ref by ≥10×, but FAFFT+Zbc still wins overall once vectorization (Phases 1b+) kicks in.

> **Reference:** See `../HQC/3_thesisHQC.md` for detailed cost-model analysis showing Karatsuba/FAFFT trade-offs as a function of `clmul` cost.

---

## Codebase Reuse Strategy

Rather than building from scratch, this thesis **assembles from 5 existing open-source codebases**, each optimized for a different HQC component. Novel RISC-V-specific contributions are concentrated where they matter most:

| Component | Source Codebase | Reuse Level | RISC-V Adaptation |
|---|---|---|---|
| **KEM framework** (KeyGen, Encaps, Decaps, FO transform) | HQC Reference C (gitlab.com/pqc-hqc/hqc) | ✅ 100% reuse | Compile with `riscv64-unknown-elf-gcc`. No changes. |
| **KAT test vectors** | HQC Reference C | ✅ 100% reuse | Run on Spike to validate every optimization step. |
| **FAFFT algorithm** (recursion, basis conversion, CRT) | Jang et al. FAFFT_HQC (Cortex-M4 variant, Ref. 3.1) | ⚠️ ~70% reuse | Port C logic; replace ARM asm GF(2^m) multiply with `clmul`/`clmulh` sequences. |
| **FAFFT GF(2^m) butterfly** (**NEW**) | *This thesis* | 🆕 New code | Hand-optimized RV64 asm: `clmul`+`clmulh` + Barrett reduction (4–6 instructions). **Core contribution.** |
| **FAFFT XOR combines** (**NEW**) | *This thesis* | 🆕 New code | RVV intrinsics: `vle64.v` → `vxor.vv` → `vse64.v` with LMUL=4. |
| **Table-driven RS decoder** | OptHQC (AVX2, Ref. 2.1) | ⚠️ Algorithm reuse, code rewrite | Port precomputed GF(2^8) tables. Replace AVX2 `vpshufb` with RVV `vluxei8.v`. |
| **Keccak-f[1600]** | Zhang et al. RVV Keccak | ✅ ~100% reuse | Drop-in replacement for SHAKE calls. Already RVV-optimized. |
| **RM Hadamard decoder** (**NEW**) | HQC Ref + RVV (Proposal 2) | 🆕 New code | Port scalar FHT to RVV `vadd.vv`/`vsub.vv` + `vslidedown.vi` shuffles (7 stages). |

**Assembly order (Proposed iteration strategy):**

```
Step 1: Fork HQC Reference C → compile on Spike → verify KAT ✓
Step 2: Integrate Jang FAFFT C code → replace Karatsuba → verify KAT ✓
Step 3: Write clmul asm for FAFFT butterflies → verify KAT ✓      ← Phase 1 Core
Step 4: Vectorize XOR combines with RVV → verify KAT ✓             ← Phase 1+
Step 5: Drop in Zhang RVV Keccak → verify KAT ✓                    ← Phase 2
Step 6: Port OptHQC RS table approach to RVV → verify KAT ✓        ← Phase 3a
Step 7: Implement RVV RM Hadamard decoder → verify KAT ✓           ← Phase 3b
Step 8: Benchmark all configurations on Spike + gem5
```

> **Key principle:** KAT vectors are verified after every step. If a test fails, the bug is isolated to the most recent change.

---

## Phase Overview

| Phase | Variant | Target speedup | Dependency | Effort (LOC) | Status |
|-------|---------|---|---|---|--------|
| 0  | `karatsuba_ref` | baseline KAT + Spike instr counts | — | 0 | ✅ complete |
| 1a | `karatsuba_zbc` | ≥ 2× on `vect_mul` | 0 | 600–800 | planned |
| 1b | `fafft_zbc` | ≥ 3× over 1a | 1a | 1000–1200 | planned |
| 1c | `hybrid_zbc` | crossover optimum | 1a+1b | 400–600 | planned |
| 1+ | `fafft_zvbc` | ≥ 4× over 1b | 1b | 500–700 | planned |
| 2  | `*_rvv_keccak` | ≥ 2× Keccak throughput | 0b | 300–400 (reuse) | planned |
| 3a | `*_rvv_rs_table` | ≥ 2× RS throughput | 0b | 200–300 (reuse) | planned |
| 3b | `*_rvv_rm_fht` | ≥ 3× RM decoding | 0b | 600–800 | planned |

> **Total estimated new code (Phases 1+2+3): ~4000–5500 lines.** Significant reuse from existing OSS codebases (HQC-ref, Zhang Keccak, Jang FAFFT, OptHQC) keeps effort feasible for 12-month thesis.

> **Iteration rule:** QEMU KAT after every change → Spike polymul micro-bench
> after QEMU passes → Spike full KEM bench only at phase milestones.
> See [WORKFLOW.md §8](WORKFLOW.md) for the three-tier summary table.

---

## Phase 0 — Baseline KAT + Spike Benchmarks (Week 1)

**Status: ✅ complete.** All deliverables committed under `analysis/benchmarks/phase0/`.

**What was done:**
- Compiled `karatsuba_ref` for all three security levels on Spike (`build-spike/`)
- Ran KAT (100/100 PASS) on QEMU for correctness validation
- Measured Spike instruction counts for `vect_mul` (polymul micro-bench) and full KEM (keypair/encaps/decaps)
- Generated instruction-frequency histograms for all three levels
- Ran `llvm-mca` on `vendor/hqc-ref/src/ref/gf2x.c` for static IPC analysis

**Baseline results (Spike ISS, VLEN=256, N=1):**

| Level | `vect_mul` instret | keypair instret | encaps instret | decaps instret |
|---|---|---|---|---|
| HQC-128 | 18,578,007 | 18,816,325 | 37,636,615 | 56,962,431 |
| HQC-192 | 56,469,320 | 56,986,059 | 113,992,185 | 171,660,064 |
| HQC-256 | 139,077,901 | 140,074,066 | 279,984,371 | 421,305,306 |

**Key check (confirmed):** `vect_mul` instruction count ≈ full keypair count for all levels, confirming that polynomial multiplication accounts for **≥ 98% of total KEM instruction count**.

**Deliverables (all present):**
- `analysis/benchmarks/phase0/hqc{128,192,256}_karatsuba_ref_polymul_N1.csv` — `vect_mul` instruction counts ✅
- `analysis/benchmarks/phase0/hqc{128,192,256}_karatsuba_ref_hist.txt` — instruction frequency histograms ✅
- `analysis/benchmarks/phase0/hqc{128,192,256}_karatsuba_ref_N1.csv` — full KEM instruction counts ✅
- `analysis/llvm_mca/reports/gf2x_mca.txt` — MCA baseline (IPC 0.38, 5890 instrs/iteration) ✅

---

## Phase 1a — Scalar Zbc Karatsuba (Weeks 2–4)

**Goal:** Replace the inner Karatsuba base-case multiplication with `clmul`/`clmulh`
scalar instructions from the Zbc extension. **Proposal 1** suggests a much more ambitious approach (hand-optimized asm, optimal recursion depth, lazy reduction); Phase 1a is the minimal version.

**Implementation steps:**

1. Implement `src/poly_mul/karatsuba_zbc/gf2x_clmul.c`:
   - Leaf multiplications: single `clmul` + `clmulh` per 64-bit word pair
   - Keep the Karatsuba recursion structure from vendor `gf2x.c`
   - Inline reduction modulo HQC irreducible (x^n−1): a single XOR of upper half shifted by n mod 64
   - **Optional (if time):** Lazy reduction (defer reduction until Karatsuba tree completes) — can double intermediate buffer size but save XOR/shift overhead
2. Run KAT to verify correctness against all three levels
3. Benchmark on Spike vs. `karatsuba_ref` — target ≥ 2× speedup on `gf2x_mul` (conservative; hand-optimized asm should reach 8–10×)
4. Profile instruction mix: `clmul` + `clmulh` fraction in `gf2x_mul` should approach 95%+ (vs. ~5% for shift/XOR)
5. **Optional:** Implement a configurable recursion-depth knob to empirically find the optimal Karatsuba cutoff depth for each HQC parameter set

> **Worked example:** See [`../HQC/4_Zbc_example.md`](../HQC/4_Zbc_example.md) for a concrete baseline schoolbook multiply, then Zbc-accelerated base-case, then full Karatsuba tree.

**Per-change workflow:**

```bash
cmake --build build --target hqc128_karatsuba_zbc -j$(nproc)

./tests/kat/run_kat.sh build/hqc128_karatsuba_zbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp 2>/dev/null
# Expected: PASS 100/100

scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc
./analysis/llvm_mca/run_mca.sh src/poly_mul/karatsuba_zbc/gf2x_clmul.c
# Compare against phase0 baseline:
# analysis/benchmarks/phase0/hqc128_karatsuba_ref_polymul_N1.csv  (18,578,007 instret)
```

**Key metric:** In histogram output, `clmul` + `clmulh` should dominate;
`xor`, `slli`, `srli` counts should drop by > 90%.

---

## Phase 1b — FAFFT with Zbc (Weeks 5–7)

**Goal:** Replace Karatsuba with a Frobenius Additive FFT polynomial multiplication
using `clmul`-accelerated GF(2^m) arithmetic. This implements **Proposal 1** (scalar phase).

**Implementation steps:**

1. Port `src/poly_mul/fafft_zbc/fafft.c` from Jang et al. (Ref. 3.1, Cortex-M4 version):
   - Additive NTT over GF(2^m) with configurable extension degree (typically m ∈ {8, 16})
   - For HQC-128 with n=17,669 bits: use **CRT decomposition** (Jang's key innovation) to avoid power-of-2 padding overhead
   - Replace ARM asm GF(2^m) multiplications with `clmul`/`clmulh` sequences + reduction
2. **Critical optimization:** Implement GF(2^m) butterfly using `clmul` base-case:
   - Two 64-bit operands in GF(2^m) → `clmul`+`clmulh` → 128-bit product → reduction modulo irreducible polynomial for GF(2^m)
   - This is the **hottest inner loop** of FAFFT; hand-optimized asm here yields largest gains
3. XOR accumulation for linear-complexity combine layers (initially scalar; vectorize in Phase 1+)
4. KAT + benchmark comparison with Phase 1a — target ≥ 3× speedup on `vect_mul` vs. Phase 1a

**Reference:** Ras et al. (2025, Ref. 1.1) achieve 3.52–8.08× over Karatsuba on baseline RISC-V **without `Zbc`**. With `Zbc` base-case optimization, expect FAFFT ≥ 5× faster than Phase 1a. Combined with Phase 1c (hybrid) or Phase 1+ (Zvbc), target 12–20× over baseline Karatsuba.

**Per-change workflow:**

```bash
cmake --build build --target hqc128_fafft_zbc -j$(nproc)
./tests/kat/run_kat.sh build/hqc128_fafft_zbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp 2>/dev/null

scripts/bench/run_spike_bench_phase.sh phase1b --variant fafft_zbc
./analysis/llvm_mca/run_mca.sh src/poly_mul/fafft_zbc/fafft.c
```

Full KEM Spike benchmarks only at phase completion (add `--full`); runs in seconds.

---

## Phase 1c — Hybrid Zbc (Week 8)

**Goal:** Combine Karatsuba outer levels (low overhead, cache-friendly) with FAFFT inner levels (theoretically faster asymptotically, but with permutation/padding overhead). This implements the **"adaptive crossover" contribution** of **Proposal 1**.

**Implementation steps:**

1. Implement `src/poly_mul/hybrid_zbc/hybrid.c`:
   - Apply 1–3 levels of Karatsuba splitting first (reducing problem to 3–27 sub-multiplications)
   - For each sub-multiplication of size ≤ threshold, invoke FAFFT (using Phase 1b code)
   - Threshold is determined empirically via profiling
2. Benchmark crossover depths on Spike: for each HQC parameter (n=17,669 / 35,851 / 57,637), measure total cycle count as a function of crossover depth
3. Plot a U-shaped curve and identify the optimal depth for each parameter
4. Compare end-to-end performance of Hybrid vs. pure Karatsuba (1a) vs. pure FAFFT (1b)

**Why it matters:** Jang et al. (Ref. 3.1) demonstrated that the CRT decomposition in Phase 1b eliminates padding overhead, making FAFFT competitive even for odd polynomial sizes. However, the outer-level Karatsuba overhead (5 XOR additions per level) is small with `clmul` base-case, so a **hybrid strategy** may outperform both pure algorithms. This phase confirms or refutes that hypothesis empirically on RISC-V.

---

## Phase 1+ — Vectorised FAFFT with Zvbc (Weeks 9–10)

**Goal:** Replace scalar `clmul` butterflies with `vclmul.vv` (Zvbc extension),
targeting ≥ 4× over `fafft_zbc` (≥ 14× over baseline). This is the upper-bound contribution for pure-software, standard-extension HQC.

**Implementation steps:**

1. Implement `src/poly_mul/fafft_zvbc/fafft_vclmul.c` using RVV intrinsics + `Zvbc` carry-less multiply:
   - Vectorized GF(2^m) butterfly: replace scalar `clmul`+`clmulh` pairs with `vclmul.vv`+`vclmulh.vv`
   - Each `vclmul.vv` processes VLEN/64 64-bit operand pairs simultaneously (e.g., 4 pairs for VLEN=256)
   - XOR combines use `vxor.vv` across vector register groups
2. Validate on Spike:

```bash
cmake --build build --target hqc128_fafft_zvbc -j$(nproc)

# fafft_zvbc requires Zvbc — QEMU does not support it, use Spike KAT:
scripts/bench/spike_kat.sh build/hqc128_fafft_zvbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp

scripts/bench/run_histogram_all.sh phase1plus --variant fafft_zvbc
./analysis/llvm_mca/run_mca.sh src/poly_mul/fafft_zvbc/fafft_vclmul.c
```

**Paper table target at Phase 1+ completion:**

| Phase | `vect_mul` instret (HQC-128) | vs Phase 0 baseline | vs Phase 1a |
|-------|---------------------------|-------------|---------------------|
| **0 (ref)** | **18,578,007** | 1.0× | — |
| 1a (zbc) | ~1,858K (est.) | ~10× | 1.0× |
| 1b (fafft+zbc) | ~900K (est.) | ~20× | ~2× |
| 1c (hybrid+zbc) | ~820K (est.) | ~22× | ~2.3× |
| 1+ (zvbc) | ~460K (est.) | ~40× | ~4× |

*(Numbers are illustrative based on Ras et al. baselines; actual values from Spike runs will differ.)*

**Throughput argument:** `vclmul.vv` processes VLEN/SEW = 256/64 = 4 carry-less products simultaneously.  FAFFT butterfly operations are data-independent, so the loop is **throughput-bound** (not latency-bound). Under sustained throughput execution, even if `vclmul.vv` has latency L > 1 cycle, the 4× parallelism more than compensates for any pipeline stall, yielding ~4× end-to-end speedup.

---

## Phase 2 — RVV Keccak (Weeks 7–8 or as secondary branch)

**Goal:** Replace the scalar Keccak-f[1600] with a vectorized implementation. Addresses **Proposal 3**. Once polynomial multiplication is accelerated below 50% of total time, SHAKE becomes a co-dominant bottleneck (36% from the Amdahl analysis above).

**Implementation steps:**

1. **Integrate Zhang et al. RVV Keccak** (already RISC-V optimized, github.com/nibrunieAtSi5/rvv-keccak) under `src/keccak/rvv/`
2. Wrap in HQC's `shake_absorb` / `shake_squeeze` interface for SHAKE-256
3. **Optional: Implement multi-instance interleaved Keccak** — HQC often generates multiple independent SHAKE streams (e.g., for different columns of public matrix `h`). Group 4 independent Keccak states into vector registers (each element processes one instance). This yields **4× throughput** for VLEN=256 with zero algorithmic change—pure data-level parallelism.
4. Configure with `-DHQC_KECCAK_IMPL=rvv`
5. KAT across all levels and all polymul variants (1a, 1b, 1c, 1+)
6. Benchmark: target ≥ 2× Keccak speedup (conservative; multi-instance interleaving should yield 3–4×)

**Priority & Timeline:** 
- **Primary path (if tight on time):** Defer to Phase 3 or merge into Phase 1+. Polynomial multiplication is the thesis flagship; Keccak is secondary.
- **Full stack (recommended):** Execute Phases 2 and 3 in parallel with Phase 1b/1c to maximize end-to-end speedup. Zhang's Keccak is a drop-in replacement, so integration is low-risk.

**Reference:** See [`../HQC/2_Proposals.md`](../HQC/2_Proposals.md) (Proposal 3) for detailed multi-instance technique.

---

## Phase 3 — RVV RM/RS Decoder (Weeks 8–9 or as secondary branch)

**Goal:** Vectorise the RM/RS concatenated decoding path (128-point Fast Hadamard Transform + Reed-Solomon syndrome computation + table-driven field operations). Addresses **Proposal 2**. Once polynomial multiplication AND Keccak are accelerated, decoding becomes the next bottleneck (15% from Amdahl analysis).

**Implementation steps:**

**Phase 3a — Vectorized RM Hadamard Decoding:**
1. Implement `src/decode/rvv/reed_muller_rvv.c` using RVV intrinsics:
   - 7-stage Fast Hadamard Transform (128 points = 2^7) on soft-decision LLR values (int16_t)
   - **Stages 1–4 (stride ≤ VLEN/SEW=16 for VLEN=256, SEW=16):** Butterflies within single vector registers using `vslidedown.vi` + `vadd.vv`/`vsub.vv`
   - **Stages 5–7 (stride > 16):** Cross-register butterflies with no memory access
   - **Argmax:** Use `vabs.v` + `vredmax.vs` + `vmseq.vv` to find decoded codeword index (replaces scalar loop over 128 values with 3 instructions)
   - **Batch processing:** With LMUL=4, process 4 RM codewords in parallel (HQC-128 has ~139 codewords, so ~35 batches = significant overhead reduction)
2. Benchmark: target ≥ 2× speedup on per-RS-symbol RM decoding vs. scalar FHT

**Phase 3b — Vectorized RS Syndrome & Table Lookup:**
3. Implement `src/decode/rvv/reed_solomon_rvv.c` using **table-driven GF(2^8) multiplication** (adapted from OptHQC, Ref. 2.1):
   - Precompute 256-byte lookup tables for GF(2^8) multiplication by each RS evaluation point α^i
   - Use RVV's **`vluxei8.v`** (indexed vector load, 8-bit indices) to perform 32 simultaneous table lookups in a single instruction (for VLEN=256, SEW=8)
   - Accumulate syndrome contributions with `vxor.vv`
   - Total RS syndrome computation for HQC-128: from ~552 scalar operations to ~24 vector instructions
4. Implement the **fused RM→RS pipeline** (Proposal 2 innovation):
   - Instead of two-pass decode (all RM codewords first, then RS), interleave them
   - Decode RM codewords vectorially, immediately feed resulting RS symbols into accumulator before L1 cache eviction
   - Eliminates full intermediate buffer write/read cycle
5. Configure with `-DHQC_DECODE_IMPL=rvv`

**Validation:**
6. KAT + benchmark across all levels (HQC-128/192/256) and all polymul variants (1a, 1b, 1c, 1+)
7. Measure overall Decaps speedup: target ≥ 2× on decoding component, ≥ 1.5× on overall Decaps (since decoding is 20–30% of Decaps runtime after poly-mul acceleration)

**Priority & Timeline:** 
- **Recommended:** Execute Phases 2 and 3 in parallel with Phase 1b/1c for maximum end-to-end impact
- **Fallback:** If integration time is tight, prioritize Phase 1b+1c (poly-mul is 40–65% of runtime); Phase 2+3 are nice-to-have for reaching 15–20× total speedup

**Reference:** See [`../HQC/2_Proposals.md`](../HQC/2_Proposals.md) (Proposal 2) and worked example in [`../HQC/5_Decoding_RVV.md`](../HQC/5_Decoding_RVV.md).

---

## Phase 4 — Optional: Custom ISE Extensions (Upper Bound Analysis)

**Goal (Out of Scope for 12-month thesis; planning only):** Characterize the performance ceiling if custom hardware instructions were added. Informs future RISC-V extension design.

**Why it matters:** After reaching 15–20× with pure software (Phases 1–3), further gains require algorithmic breakthroughs or hardware support. Phase 4 quantifies the "free lunch" available from targeted custom ISE proposals.

**Candidate custom instructions (from `../HQC/customISE.md`):**

| Instruction | What it does | Estimated impact | Hardware complexity |
|---|---|---|---|
| `BFY.GF2M` | Fused GF(2^m) butterfly (multiply + reduce + XOR combine) | 2–4× per-butterfly | Medium |
| `VBFY.GF2M` | Vector fused butterfly (processes 4 butterflies/cycle for VLEN=256) | 3–8× over scalar `clmul` | High |
| `CLMUL.RED` | Fused carry-less multiply + field reduction | 1.3–2× in reduction-heavy kernels | Medium |
| `KEC.ROUND` | Keccak substep bundle (e.g., θ+ρ+π or χ+ι) | 2–3× Keccak throughput | Medium |

**Preliminary findings:**
- Scalar `Zbc` + `Zvbc` likely reaches **35–40× total speedup** vs. baseline
- Custom `VBFY.GF2M` could add another 1.5–2× (bringing to 50–80×, approaching theoretical hardware limits)
- `KEC.ROUND` could add 1.2–1.5× if Keccak remains a bottleneck after Phase 2

**Methodology (if pursued in future work):**
1. Extend gem5 RISC-V model with proposed ISE definitions
2. Estimate cycle counts for each custom instruction (based on hardware synthesis or expert analysis)
3. Simulate end-to-end HQC on extended gem5; compare to Phase 1+ pure-software baseline
4. Write position paper for RISC-V ISA WG documenting findings

---

## Weeks 11–12: Thesis Tables and Paper

**Mandatory results:**
1. Spike instruction counts for `vect_mul` and full KEM — all 3 levels × all variants.
2. Instruction mix histograms showing the scalar → `clmul` → `vclmul` transition.
3. llvm-mca IPC estimates for each hot loop.
4. Speedup table vs. NIST reference (`karatsuba_ref`) and vs. Ras et al. (RISC-V FAFFT baseline). **Phase 0 baseline numbers already committed** (`analysis/benchmarks/phase0/`).

**Generate all tables:**

```bash
python3 scripts/report/extract_results.py --latex \
    > analysis/report/table_results.tex
scripts/report/gen_report.sh
```

**Thesis table format:**

```
Table N: HQC instruction counts on RISC-V (Spike ISS, VLEN=256)
          Keypair        Encaps         Decaps
HQC-128
  Phase 0  18.8M          37.6M          57.0M    ← HQC-128 committed
  Phase 1a XXXM (X.Xx)    XXXM (X.Xx)    XXXM (X.Xx)
  Phase 1b XXXM (X.Xx)    XXXM (X.Xx)    XXXM (X.Xx)
  Phase 1+ XXXM (X.Xx)    XXXM (X.Xx)    XXXM (X.Xx)
...
Note: "instructions" = retired instruction count (Spike ISS rdcycle).
      Speedup in parentheses vs. Phase 0 (karatsuba_ref) baseline.
      Spike ISS models no pipeline; see Table N+1 for throughput analysis.
```

**Optional** (if FPGA flow completed): CV32E40P FPGA cycle counts for
`karatsuba_ref` and `karatsuba_zbc` — validates that the instruction count
reduction translates to cycle reduction on a real in-order pipeline.
See [TOOLCHAIN.md §13.4](TOOLCHAIN.md) for FPGA guidance.

**Thesis contributions (in order of priority):**
1. ✅ **First empirical algorithm selection study** (Karatsuba vs. FAFFT vs. Hybrid) on RISC-V with `Zbc` — resolves §9.3 Question 1 of Literature.md
2. ✅ **First end-to-end pure-software HQC on RV64GCV_Zbc** — fills research gap (Lit. §9.2)
3. ✅ **Vectorized Keccak and RM/RS decoding integration** — Proposals 2 & 3
4. 🎯 **Quantified Amdahl's law progression** showing SHAKE and decoding bottleneck transitions — justifies multi-component optimization
5. 🎯 **gem5 cycle-accurate simulation** comparing theoretical VLEN=256/512 impact
6. 🎯 **Codebase reuse recipe** — demonstrates feasibility of composing 5 OSS projects for RISC-V HQC (methodology for future post-quantum work)

---

## Measurement Checklist (per phase)

After completing each phase:

- [ ] QEMU KAT passes 100/100 for all three levels
- [ ] Spike polymul micro-bench CSV committed to `analysis/benchmarks/<phase>/`
- [ ] Spike instruction histogram committed alongside CSV
- [ ] llvm-mca report committed to `analysis/llvm_mca/reports/`
- [ ] Speedup computed vs. Phase 0b baseline and written to `docs/REPORT.md`
- [ ] Full Spike KEM bench (keypair/encaps/decaps) run and committed (~0.7–5 s per level)

---

## Code Volume and Effort Estimation

**New code written (vs. reused from existing OSS):**

| Category | Lines of Code | Effort (% of total) | Complexity |
|---|---|---|---|
| HQC reference C (reused) | ~8000–10K | 0% | — |
| Zhang RVV Keccak (reused) | ~1.5K | 0% | — |
| Jang FAFFT C logic (reused/adapted) | ~2K C (rewrite 30%) | ~8% | Medium |
| **Phase 1a: clmul base-case asm** | 600–800 | ~18% | Medium-High |
| **Phase 1b: FAFFT GF(2^m) butterfly asm** | 1000–1200 | ~25% | High |
| **Phase 1c: Hybrid crossover logic** | 400–600 | ~12% | Medium |
| **Phase 1+: Zvbc vectorization** | 500–700 | ~15% | High |
| **Phase 2: Multi-instance Keccak interleaving** | 200–300 | ~6% | Low-Medium |
| **Phase 3a: RVV FHT decoder** | 400–600 | ~12% | Medium |
| **Phase 3b: RVV RS table lookup & fused pipeline** | 200–300 | ~8% | Medium |
| **Integration, CMake, build system** | ~200 | — | — |
| **TOTAL NEW CODE** | **~3500–5500** | **100%** | |

**Timeline breakdown (12 weeks, assuming full-time student):**

- **Weeks 1–2:** Baseline (Phase 0b), literature review, tool setup → ~200 lines (build/test harness)
- **Weeks 2–4:** Phase 1a (clmul base-case) → ~700 lines asm + ~200 lines C wrapper
- **Weeks 5–7:** Phase 1b (FAFFT + butterflies) → ~1100 lines asm + ~100 lines C
- **Week 8:** Phase 1c (Hybrid) + benchmark comparison → ~500 lines
- **Weeks 9–10:** Phase 1+ (Zvbc) → ~600 lines intrinsics
- **Week 7 (parallel):** Phase 2 (Keccak, if time) → ~250 lines (mostly integration)
- **Weeks 8–9 (parallel):** Phase 3a+3b (Decoding) → ~700 lines intrinsics + asm
- **Weeks 10–12:** Benchmarking, gem5 simulation, paper writing, table generation

**Risk factors:**
- ❌ Spike/gem5 tool instability (mitigated by early verification)
- ❌ Zvbc not yet deployed in Spike (fallback: model in gem5)
- ⚠️ RVV intrinsic compiler support varies by toolchain version (test early)
- ⚠️ Keccak multi-instance interleaving requires careful state layout (prototype first)

**Success criteria (by end of Week 12):**
1. Phase 0 complete: baseline instruction counts committed ✓ (**done**)
2. Phase 1a complete: Zbc Karatsuba ≥ 2× faster than baseline KAT passes
3. Phase 1b complete: FAFFT ≥ 3× faster than Phase 1a KAT passes
4. Phase 1c complete: Hybrid algorithm characterized, speedup table computed
5. Phase 1+ benchmarked on Spike (with Zvbc enabled); gem5 simulation started
6. (Optional) Phase 2+3 integrated and KAT passing
7. Thesis document with all tables, figures, and recommendations

---

*Last updated: 2026-04-30. Phase 0 complete (KAT + Spike baselines committed). Phase 1a next.*
