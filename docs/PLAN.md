# HQC-RISC-V: Implementation Plan

Feature-based roadmap and execution plan for the HQC optimisation thesis.

## Table of Contents

- [Research Context & Bottleneck Progression](#research-context--bottleneck-progression)
- [Algorithm Selection: Why Karatsuba vs. FAFFT vs. Hybrid Matters](#algorithm-selection-why-karatsuba-vs-fafft-vs-hybrid-matters)
- [Codebase Reuse Strategy](#codebase-reuse-strategy)
- [Detailed Track Breakdown](#detailed-track-breakdown)
- [Execution Order](#execution-order)
- [Twelve-Week Schedule](#twelve-week-schedule)
- [Track Checklist](#track-checklist)
- [Success Criteria](#success-criteria)
- [Mapping Reference](#mapping-reference)

---

## Research Context & Bottleneck Progression

This thesis is structured around Amdahl's law: each optimisation track
accelerates the current dominant bottleneck until the next one surfaces.

### Baseline (`baseline_ref`)

| Component | Time (normalized) | % of Total |
|---|---|---|
| Polynomial multiplication (`karatsuba_ref`) | 95 | **95%** |
| SHAKE/Keccak seed expansion | 3.5 | 3.5% |
| RM/RS concatenated decoding | 1.5 | 1.5% |
| **Total** | **100** | |

### After polymul acceleration (`polymul_zbc` + `polymul_zvbc` class work)

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 95 / 20 = **4.75** | **49%** |
| SHAKE/Keccak seed expansion | 3.5 | **36%** |
| RM/RS concatenated decoding | 1.5 | **15%** |
| **Total** | **~9.75** | |

### Target full stack (`polymul_zvbc` + `keccak_rvv` + decoder tracks)

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 | **65%** |
| SHAKE/Keccak (vectorized) | 3.5 / 3 ≈ **1.17** | **16%** |
| RM/RS decoding (vectorized) | 1.5 / 3.5 ≈ **0.43** | **6%** |
| **Total** | **~6.35** | |

This corresponds to roughly **15.7x** end-to-end speedup over baseline.

### Upper-bound model (vector carry-less multiply + full stack)

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 / 4 ≈ **1.19** | **40%** |
| SHAKE/Keccak (vectorized) | 1.17 | **40%** |
| RM/RS decoding (vectorized) | 0.43 | **15%** |
| **Total** | **~2.79** | |

This gives an approximate **35.8x** upper bound over baseline.

---

## Algorithm Selection: Why Karatsuba vs. FAFFT vs. Hybrid Matters

Ras et al. demonstrated 3.52-8.08x FAFFT speedup on baseline RISC-V without
`clmul`. Adding `clmul` changes the practical crossover:

| Factor | `polymul_zbc` | `fafft_zbc` | hybrid tracks |
|---|---|---|---|
| Base-case cost | ~4 cycles (`clmul` + reduction) | ~6-8 cycles + field ops | Karatsuba outer, FAFFT inner |
| Overhead per level | 5 XOR additions | basis conversion + combine + permutation | depends on crossover depth |
| Padding / CRT overhead | none | ~30% for HQC-128 without usable CRT kernel | depends on subproblem-domain coupling |
| Memory locality | excellent | poor | intermediate |

Empirical conclusion so far:

- `polymul_zbc` is the scalar winner for HQC-128/192/256.
- `fafft_zbc`, `hybrid_zbc`, and `hybrid_2way_zbc` are still thesis-critical
  because they identify where and why the expected crossover fails at HQC sizes.
- `polymul_zvbc` and `polymul_zvbc_v2` show that vector carry-less multiply can
  shift practical limits again, and therefore remain important.

---

## Codebase Reuse Strategy

| Component | Source codebase | Reuse level | RISC-V adaptation |
|---|---|---|---|
| KEM framework (KeyGen, Encaps, Decaps, FO transform) | HQC Reference C | 100% reuse | compile for `riscv64-unknown-elf` |
| KAT vectors and baseline behaviour | HQC Reference C | 100% reuse | run on QEMU and Spike for each track |
| FAFFT algorithm (recursion, basis conversion, CRT ideas) | Jang et al. FAFFT_HQC | partial reuse | port C logic; replace ARM-specific arithmetic |
| FAFFT GF(2^m) butterfly | this thesis | new code | `clmul` / `clmulh` + reduction |
| FAFFT XOR combines | this thesis | new code | RVV `vle64.v` / `vxor.vv` / `vse64.v` helpers |
| Table-driven RS decoder | OptHQC | algorithm reuse, code rewrite | replace AVX2 table lookup with RVV `vluxei8.v` |
| Keccak-f[1600] | Zhang et al. RVV Keccak | near-drop-in reuse | adapt to HQC SHAKE wrappers |
| RM Hadamard decoder | HQC reference + this thesis | new code | RVV `vadd.vv` / `vsub.vv` / `vslidedown.vi` |

---

## Detailed Track Breakdown

### `baseline_ref`

**Goal:** Preserve correctness baseline and performance denominator.

**Implementation steps:**
1. Build `karatsuba_ref` for HQC-128/192/256.
2. Validate KAT on QEMU/Spike.
3. Record `vect_mul` and full-KEM instruction counts on Spike.

**Status:** COMPLETE.

**Current Results & Observations:**
- `vect_mul` instruction counts:
  - HQC-128: **18,578,007** (~18.58M)
  - HQC-192: **56,469,320** (~56.47M)
  - HQC-256: **139,077,901** (~139.08M)
- Full KEM instruction counts:
  - HQC-128: keypair **18,816,325**, encaps **37,636,615**, decaps **56,962,431**
  - HQC-192: keypair **56,986,059**, encaps **113,992,185**, decaps **171,660,064**
  - HQC-256: keypair **140,074,066**, encaps **279,984,371**, decaps **421,305,306**
- Observation: `vect_mul` remains ~98-99% of keypair baseline cost.

### `polymul_zbc`

**Goal:** Accelerate Karatsuba leaf operations using `clmul`/`clmulh`.

**Implementation steps:**
1. Maintain `gf2x_clmul` path with tuned recursion threshold.
2. Keep KAT clean across levels.
3. Track polymul and full-KEM speedups against `baseline_ref`.

**Status:** COMPLETE.

**Current Results & Observations:**
- `vect_mul` instruction counts and speedups vs `baseline_ref`:
  - HQC-128: **354,187** (52.5x)
  - HQC-192: **1,078,641** (52.4x)
  - HQC-256: **2,257,476** (61.6x)
- Full-KEM speedups vs `baseline_ref`:
  - HQC-128: keypair **31.8x**, encaps **31.7x**, decaps **24.9x**
  - HQC-192: keypair **35.7x**, encaps **35.5x**, decaps **31.3x**
  - HQC-256: keypair **43.0x**, encaps **44.1x**, decaps **38.8x**
- Best threshold: `KARATSUBA_THRESHOLD=16`.
- Root cause for gap between polymul and full-KEM speedups: after polymul drops,
  SHAKE and decoding become co-dominant (Amdahl transition).

### `fafft_zbc`

**Goal:** Evaluate scalar `Zbc`-accelerated FAFFT against `polymul_zbc`.

**Implementation steps:**
1. Keep FAFFT kernels KAT-stable for HQC-128/192/256.
2. Benchmark against `baseline_ref` and `polymul_zbc`.
3. Document CRT feasibility limits.

**Status:** COMPLETE.

**Current Results & Observations:**
- `vect_mul` instruction counts:
  - HQC-128: **1,462,166**
  - HQC-192: **3,256,399**
  - HQC-256: **3,258,453**
- Speedups vs `baseline_ref`: **12.7x / 17.3x / 42.7x**.
- Relative to `polymul_zbc`: **0.24x / 0.33x / 0.69x** (still slower).
- Root-cause analysis:
  - CRT attempt blocked at HQC-128 because 17669-bit inputs occupy 553 words,
    exceeding the 512-word fit for available `bc_1_16384` assumptions.
  - Missing `bmul2_4096_to_4096` path prevents the expected smaller-domain route.
  - Basis-conversion and butterfly overhead remain dominant at HQC sizes.

### `hybrid_zbc`

**Goal:** Test Karatsuba-outer + FAFFT-inner crossover depth hypothesis.

**Implementation steps:**
1. Sweep outer depth `d` (0, 1, 2).
2. Measure HQC-128/192/256 polymul counts.
3. Compare against both `fafft_zbc` and `polymul_zbc`.

**Status:** COMPLETE.

**Current Results & Observations:**
- HQC-128: d=0 **1,463,531**, d=1 **4,390,073**, d=2 **13,160,160**
- HQC-192: d=0 **3,254,008**, d=1 **9,773,238**, d=2 **13,176,205**
- HQC-256: d=0 **3,258,265**, d=1 **9,785,206**, d=2 **13,197,465**
- Best depth is d=0 (pure FAFFT), so no beneficial crossover appeared.
- Root cause: fixed-domain FAFFT wrappers do not shrink work proportional to
  Karatsuba subproblem size, so extra recursion mostly multiplies FAFFT calls.

### `hybrid_zbc_v2`

**Goal:** Preserve alternate hybrid benchmark-set experiments and verify if they
change crossover conclusions.

**Implementation steps:**
1. Keep benchmark set reproducible in report pipeline.
2. Compare to `hybrid_zbc` and `polymul_zbc`.

**Status:** BENCHMARKED.

**Current Results & Observations:**
- `vect_mul` counts from report: **1.1M / 2.6M / 3.1M** for HQC-128/192/256.
- Speedups vs `baseline_ref`: **16.6x / 21.9x / 44.2x**.
- Observation: improved over pure FAFFT-like baselines in some points but still
  does not displace `polymul_zbc` as scalar winner.

### `hybrid_2way_zbc`

**Goal:** Implement and test the 2-way FAFFT-Karatsuba split from newer
literature for reduced domain overhead.

**Implementation steps:**
1. Use half-domain split kernels per level.
2. Validate KAT across HQC levels.
3. Benchmark against `fafft_zbc` and `polymul_zbc`.

**Status:** COMPLETE.

**Current Results & Observations:**
- `vect_mul` counts:
  - HQC-128: **1,304,473**
  - HQC-192: **2,950,035**
  - HQC-256: **3,523,625**
- Relative to `polymul_zbc`:
  - HQC-128: **3.68x slower** (1,304,473 vs 354,187)
  - HQC-192: **2.73x slower** (2,950,035 vs 1,078,641)
  - HQC-256: **1.56x slower** (3,523,625 vs 2,257,476)
- Root-cause analysis:
  - `clmul` reduces Karatsuba leaf cost to roughly two instructions per 64x64
    partial product.
  - Even with half-domain FAFFT savings, basis-conversion and butterfly work is
    still too expensive at HQC sizes.
  - Net effect: crossover point shifts to substantially larger polynomials than
    HQC-128/192/256.

### `polymul_zvbc`

**Goal:** Benchmark vector carry-less multiply polymul path.

**Implementation steps:**
1. Keep Spike Zvbc benchmarking stable for all HQC levels.
2. Compare against scalar `polymul_zbc`.
3. Prepare integration path for full-KEM-level interpretation.

**Status:** BENCHMARKED.

**Current Results & Observations:**
- `vect_mul` counts and speedups vs `baseline_ref`:
  - HQC-128: **187.1K** (99.3x)
  - HQC-192: **575.5K** (98.1x)
  - HQC-256: **816.4K** (170.4x)
- Observation: already materially faster than scalar `polymul_zbc` in micro-benchs.

### `polymul_zvbc_v2`

**Goal:** Preserve alternate Zvbc benchmark-set variant and compare ceilings.

**Implementation steps:**
1. Keep alternate vector path benchmarked and reproducible.
2. Clarify metadata for Karatsuba-vs-FAFFT interpretation in reports.

**Status:** BENCHMARKED.

**Current Results & Observations:**
- `vect_mul` counts and speedups vs `baseline_ref`:
  - HQC-128: **154.5K** (120.3x)
  - HQC-192: **474.9K** (118.9x)
  - HQC-256: **663.8K** (209.5x)
- Observation: best currently observed polymul micro-benchmark set; still needs
  final algorithm-level interpretation before being declared the end-state choice.

### `keccak_rvv`

**Goal:** Accelerate SHAKE/Keccak to address post-polymul bottlenecks.

**Implementation steps:**
1. Integrate RVV Keccak-f[1600] path and wire `HQC_KECCAK_IMPL=rvv`.
2. Validate KAT with each active polymul winner.
3. Benchmark SHAKE subsystem and full KEM impact.
4. If needed, add multi-instance interleaving to improve practical throughput.

**Status:** TODO / PLANNED.

**Current Results & Observations:**
- Expected gain from Amdahl model: **~3x SHAKE speedup**.
- Expected share reduction: **~36% -> ~16%** of post-polymul runtime.
- Key open question: whether drop-in RVV is sufficient or interleaving is needed
  to realize subsystem-level gains in end-to-end runs.

### `reed_muller_rvv`

**Goal:** Accelerate Reed-Muller decoding (Hadamard-heavy part).

**Implementation steps:**
1. Implement RVV FHT stages with vector add/sub and shuffle primitives.
2. Validate decoder correctness in isolation and in full KAT.
3. Measure decapsulation contribution and combined decoder gains.

**Status:** TODO / PLANNED.

**Current Results & Observations:**
- Part of combined decoder projection.
- Expected combined decoder improvement with RS track: **~3.5x**.
- Expected decoder share reduction (combined): **~15% -> ~6%**.

### `reed_solomon_rvv`

**Goal:** Accelerate RS syndrome and related table-driven decode path.

**Implementation steps:**
1. Implement RVV indexed table lookup path (`vluxei8.v`) for GF operations.
2. Integrate with RM output path and validate KAT.
3. Benchmark standalone and fused RM->RS decapsulation improvements.

**Status:** TODO / PLANNED.

**Current Results & Observations:**
- Works jointly with `reed_muller_rvv` in the Amdahl projection.
- Expected combined decoder speedup remains **~3.5x** with share reduction from
  **~15% to ~6%** after polymul acceleration.

### `custom_ise_upper_bound`

**Goal:** Quantify architecture upper bound beyond pure software-only thesis path.

**Implementation steps:**
1. Keep as optional branch after software stack stabilizes.
2. Model candidate fused instructions and projected cycle impact.
3. Use simulator-based estimation for upper-bound claims.

**Status:** TODO / PLANNED (OPTIONAL).

**Current Results & Observations:**
- Not required for core thesis claim.
- Potential incremental gains beyond software baseline remain valuable as future
  work once `polymul_zvbc` + subsystem tracks are finalized.

---

## Execution Order

1. `baseline_ref`
2. `polymul_zbc`
3. `fafft_zbc`
4. `hybrid_zbc`
5. `hybrid_2way_zbc`
6. `polymul_zvbc`
7. `polymul_zvbc_v2`
8. `keccak_rvv`
9. `reed_muller_rvv`
10. `reed_solomon_rvv`
11. `custom_ise_upper_bound` if time remains

---

## Twelve-Week Schedule

| Weeks | Focus | Deliverable |
|---|---|---|
| 1 | `baseline_ref` audit and reproducible measurement | clean reference benchmark set |
| 2-4 | `polymul_zbc` refinement and validation | stable scalar `Zbc` winner |
| 5-6 | `fafft_zbc` implementation and measurement | direct Karatsuba vs FAFFT comparison |
| 7 | `hybrid_zbc` and `hybrid_2way_zbc` comparison | crossover analysis with scoreboard |
| 8 | `polymul_zvbc` and `polymul_zvbc_v2` | vector polymul result set |
| 9 | `keccak_rvv` integration | subsystem-level SHAKE speedup data |
| 10 | `reed_muller_rvv` | decoder micro-benchmark and KAT validation |
| 11 | `reed_solomon_rvv` | complete decoder track or bounded partial result |
| 12 | thesis tables, plots, and write-up | publication-ready figures and tables |

---

## Track Checklist

1. Build the relevant target in the appropriate build directory.
2. Run KAT on QEMU or Spike, depending on ISA requirements.
3. Run a polymul Spike micro-benchmark into `analysis/benchmarks/<bench_set>/`.
4. Run full-KEM Spike measurements only for milestone-quality variants.
5. Generate histogram data when instruction mix matters for the claim.
6. Regenerate report tables with `scripts/report/gen_report.sh --bench-set <bench_set>`.
7. Record conclusions in `analysis/report/` and thesis notes.

---

## Success Criteria

- All named benchmark sets are discoverable directly from `analysis/benchmarks/`.
- Reports, docs, and scripts use descriptive benchmark-set names.
- `baseline_ref` remains the shared denominator for reported speedups.
- Every performance claim is traceable to a concrete benchmark-set directory.
- Planned tracks include explicit TODO implementation steps and expected gains.

---

## Mapping Reference

| Old name | New name |
|---|---|
| `phase0` / `phase0b` | `baseline_ref` |
| `phase1a` | `polymul_zbc` |
| `phase1b` | `fafft_zbc` |
| `phase1c` | `hybrid_zbc` |
| mixed `phase1b` hybrid rerun | `hybrid_zbc_v2` |
| `phase1d` | `hybrid_2way_zbc` |
| `phase1plus` (Karatsuba Zvbc path) | `polymul_zvbc` |
| alternate Zvbc benchmark set | `polymul_zvbc_v2` |
| planned RVV Keccak work | `keccak_rvv` |
| planned RVV RM decoder work | `reed_muller_rvv` |
| planned RVV RS decoder work | `reed_solomon_rvv` |
