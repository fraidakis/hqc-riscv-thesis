# HQC-RISC-V: Implementation Plan

Feature-based roadmap and execution plan for the HQC optimisation thesis.

**Context:** This thesis addresses a significant research gap: as of March 2026,
no publication presents a pure-software, ratified-extensions-only HQC
implementation on RISC-V. Ras et al. (2025) demonstrated FAFFT achieves
3.52-8.08x speedup but relied on custom hardware (ANDROMEDA). Jang et al.
optimised FAFFT for Cortex-M4, which lacks `clmul`. OptHQC used x86 AVX2.
This thesis fills the gap by combining `Zbc` (carry-less multiply), `V` (RVV),
and `Zbkb` (bit manipulation) into the first optimised pure-software RISC-V HQC
implementation, while empirically comparing Karatsuba, FAFFT, and hybrid
strategies once `clmul` is available.

For day-to-day commands see `WORKFLOW.md`.
For toolchain rationale and measurement notes see `TOOLCHAIN.md`.
For detailed literature review and proposal notes see `../HQC/1_Literature.md`
and `../HQC/2_0_Proposals.md`.
Worked examples remain in `../HQC/4_Zbc_example.md` and
`../HQC/5_Decoding_RVV.md`.

---

## Table of Contents

- [Research Context & Bottleneck Progression](#research-context--bottleneck-progression)
- [Algorithm Selection: Why Karatsuba vs. FAFFT vs. Hybrid Matters](#algorithm-selection-why-karatsuba-vs-fafft-vs-hybrid-matters)
- [Codebase Reuse Strategy](#codebase-reuse-strategy)
- [Optimization Tracks](#optimization-tracks)
- [Results & Observations](#results--observations)
- [Execution Order](#execution-order)
- [Track Checklist](#track-checklist)
- [Twelve-Week Schedule](#twelve-week-schedule)
- [Success Criteria](#success-criteria)
- [Mapping Reference](#mapping-reference)

---

## Research Context & Bottleneck Progression

This thesis is structured around Amdahl's law: each optimisation track
accelerates the current dominant bottleneck until the next one surfaces. The
following analysis, adapted from Ras et al. 2025 profiling, tracks how runtime
shifts across HQC's three core components.

### Baseline: `baseline_ref`

| Component | Time (normalized) | % of Total |
|---|---|---|
| Polynomial multiplication (`karatsuba_ref`) | 95 | **95%** |
| SHAKE/Keccak seed expansion | 3.5 | 3.5% |
| RM/RS concatenated decoding | 1.5 | 1.5% |
| **Total** | **100** | |

Polynomial multiplication completely dominates. All other components are
negligible until the scalar and vector polymul tracks land.

### After the polymul tracks (`polymul_zbc` + `polymul_zvbc` class work)

Assuming the dominant polynomial multiplication path is reduced by roughly 20x
through the scalar `Zbc` and later vector carry-less multiply work:

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 95 / 20 = **4.75** | **49%** |
| SHAKE/Keccak seed expansion | 3.5 | **36%** |
| RM/RS concatenated decoding | 1.5 | **15%** |
| **Total** | **~9.75** | |

This is the critical transition point. Once the polymul tracks stop dominating
the total KEM cost, `keccak_rvv`, `reed_muller_rvv`, and `reed_solomon_rvv`
become necessary for further end-to-end gains.

### Target: Full software stack (`polymul_zvbc` + `keccak_rvv` + decoder tracks)

Adding vectorised Keccak (~3x speedup) and vectorised decoding (~3.5x speedup):

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 | **65%** |
| SHAKE/Keccak (vectorized) | 3.5 / 3 ≈ **1.17** | **16%** |
| RM/RS decoding (vectorized) | 1.5 / 3.5 ≈ **0.43** | **6%** |
| **Total** | **~6.35** | |

That corresponds to roughly **15.7x** end-to-end speedup over `baseline_ref`,
which is the practical justification for treating Keccak and decoding as
first-class tracks rather than optional polish.

### Upper bound: vector carry-less multiply + full-stack acceleration

`Zvbc` vector carry-less multiply (`vclmul.vv`) processes VLEN/64 butterflies
per instruction, for example 4 at VLEN=256:

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 / 4 ≈ **1.19** | **40%** |
| SHAKE/Keccak (vectorized) | 1.17 | **40%** |
| RM/RS decoding (vectorized) | 0.43 | **15%** |
| **Total** | **~2.79** | |

This gives an approximate **35.8x** upper bound over `baseline_ref`, which is
why the vector polymul tracks and the optional `custom_ise_upper_bound` notes
remain relevant even after the scalar polymul question has effectively been
settled.

---

## Algorithm Selection: Why Karatsuba vs. FAFFT vs. Hybrid Matters

Ras et al. demonstrated 3.52-8.08x FAFFT speedup on baseline RISC-V without
`clmul`. Adding `clmul` fundamentally changes the cost model:

| Factor | `polymul_zbc` | `fafft_zbc` | hybrid tracks |
|---|---|---|---|
| Base-case cost | ~4 cycles (`clmul` + reduction) | ~6-8 cycles + field ops | Karatsuba outer, FAFFT inner |
| Overhead per level | 5 XOR additions | basis conversion + combine + permutation | depends on crossover depth |
| Recursion depth | d* = 3-4 for HQC-128 | single-pass FFT | 1-2 outer Karatsuba levels were the original hypothesis |
| Padding / CRT overhead | none | ~30% for HQC-128 without a usable CRT kernel | depends on whether the subproblem actually shrinks the FAFFT domain |
| Memory locality | excellent | poor | intermediate |

The key insight is that `clmul` shifts the Karatsuba/FAFFT crossover point far
enough that FAFFT overhead is hard to justify at HQC sizes. That is exactly why
the scalar roadmap had to benchmark `polymul_zbc`, `fafft_zbc`, `hybrid_zbc`,
and `hybrid_2way_zbc` separately instead of assuming the asymptotically faster
algorithm would win in practice.

The current evidence supports a narrower conclusion than the original plan:

- `polymul_zbc` is the scalar winner for HQC-128/192/256.
- `fafft_zbc` is still valuable as a comparison point because it shows where the
  FAFFT overhead comes from and why the crossover did not materialise.
- `hybrid_zbc` and `hybrid_2way_zbc` are thesis contributions precisely because
  they falsify the expectation that a hybrid would recover the crossover.
- `polymul_zvbc` and `polymul_zvbc_v2` remain the place to test whether vector
  carry-less multiply changes that conclusion enough to justify more aggressive
  FAFFT-oriented work.

---

## Codebase Reuse Strategy

Rather than rebuilding HQC from scratch, this thesis assembles the working
implementation from several existing open-source codebases while concentrating
new RISC-V-specific work where it changes the result.

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

Recommended assembly order for the software stack:

```text
1. Keep `baseline_ref` reproducible and KAT-clean.
2. Implement `polymul_zbc` and establish the scalar winner.
3. Port `fafft_zbc` and quantify the FAFFT overhead under `clmul`.
4. Explore `hybrid_zbc` and `hybrid_2way_zbc` as crossover tests.
5. Benchmark `polymul_zvbc` and `polymul_zvbc_v2` on Spike.
6. Integrate `keccak_rvv`.
7. Integrate `reed_muller_rvv` and `reed_solomon_rvv`.
8. Use `custom_ise_upper_bound` only if time remains after the main thesis path.
```

The key methodological principle is unchanged: KAT vectors are verified after
every substantial change so failures stay local to the most recent track edit.

---

## Optimization Tracks

| Track | Primary artifact | Goal | Status |
|---|---|---|---|
| `baseline_ref` | `karatsuba_ref` | correctness baseline and measurement denominator | complete |
| `polymul_zbc` | `karatsuba_zbc` | scalar `clmul` base-case acceleration | complete |
| `fafft_zbc` | `fafft_zbc` | additive-FFT polymul with scalar `Zbc` butterflies | complete |
| `hybrid_zbc` | `hybrid_zbc` | Karatsuba outer levels with FAFFT inner kernel | complete |
| `hybrid_zbc_v2` | `hybrid_zbc` | alternate hybrid benchmark set for crossover experiments | benchmarked |
| `hybrid_2way_zbc` | `hybrid_zbc` | two-way split hybrid from the newer literature design | complete |
| `polymul_zvbc` | `karatsuba_zvbc` | vector carry-less multiply polymul path | benchmarked |
| `polymul_zvbc_v2` | `fafft_zvbc`-oriented experiment | alternate vector carry-less multiply benchmark set | benchmarked |
| `keccak_rvv` | `fips202_rvv` | RVV SHAKE acceleration for seed expansion | planned |
| `reed_muller_rvv` | `reed_muller_rvv` | RVV Hadamard / RM decoding | planned |
| `reed_solomon_rvv` | `reed_solomon_rvv` | RVV RS decoding path | planned |
| `custom_ise_upper_bound` | custom ISE notes | architecture upper bound outside the main thesis path | optional |

---

## Results & Observations

### `baseline_ref`

- `vect_mul` baseline instruction counts are 18.58M, 56.47M, and 139.08M for
  HQC-128/192/256.
- Full KEM counts confirm `vect_mul` accounts for roughly 98-99% of keypair cost,
  which validates the decision to prioritise the polymul tracks first.

### `polymul_zbc`

- This is the current scalar winner.
- `vect_mul` speedups versus `baseline_ref` are **52.5x**, **52.4x**, and
  **61.6x** for HQC-128/192/256.
- Full-KEM speedups remain strong but lower because Keccak and decoding become
  co-dominant after the polymul cost collapses: 31.8x/31.7x/24.9x for HQC-128
  keypair/encaps/decaps and up to 44.1x on HQC-256 encaps.
- The threshold sweep converged on `KARATSUBA_THRESHOLD=16`.
- A wider 2-way dispatch variant regressed on Spike by about 9%, so the simpler
  sequential leaf is retained for now.

### `fafft_zbc`

- `fafft_zbc` does beat `baseline_ref`, but it loses to `polymul_zbc` at all HQC
  sizes.
- `vect_mul` speedups versus `baseline_ref` are 12.7x, 17.3x, and 42.7x, but the
  same runs are only 0.24x, 0.33x, and 0.69x of `polymul_zbc`.
- The CRT decomposition attempt failed because HQC-128's 17669-bit polynomial
  occupies 553 words, exceeding the 512-word limit required by the available
  `bc_1_16384` path. Without Jang et al.'s missing `bmul2_4096_to_4096` kernel,
  the smaller-domain CRT route could not be made KAT-clean.
- Conclusion: pure FAFFT is structurally disadvantaged at HQC-128/192/256 once
  `clmul` lowers the Karatsuba leaf cost.

### `hybrid_zbc` and `hybrid_zbc_v2`

- The original crossover hypothesis did not hold.
- `hybrid_zbc` is monotonically worse as outer Karatsuba depth increases; the
  best point is effectively depth 0, which is just pure FAFFT.
- Root cause: the available FAFFT kernels are fixed-domain. Splitting the input
  into smaller Karatsuba subproblems multiplies the number of FAFFT calls but
  does not shrink the actual FFT domain at the same rate.
- `hybrid_zbc_v2` preserves an alternate benchmark-set snapshot, but it still
  does not overturn the conclusion that `polymul_zbc` is the better scalar HQC
  polymul path.

### `hybrid_2way_zbc`

- The two-way split from the newer literature improves on pure FAFFT for
  HQC-128 and HQC-192, but it still loses to `polymul_zbc`.
- Measured `vect_mul` results:
  - HQC-128: 1,304,473 vs 354,187 for `polymul_zbc` → **3.68x slower**
  - HQC-192: 2,950,035 vs 1,078,641 for `polymul_zbc` → **2.73x slower**
  - HQC-256: 3,523,625 vs 2,257,476 for `polymul_zbc` → **1.56x slower**
- Root cause analysis: `clmul` drops the Karatsuba base-case cost to roughly two
  instructions per 64x64 partial product, which shifts the Karatsuba/FAFFT
  crossover point far enough that the half-domain FAFFT savings are still not
  enough to recover FAFFT's basis-conversion and butterfly overhead at HQC sizes.
- This is a core thesis result, not a failed side experiment.

### `polymul_zvbc` and `polymul_zvbc_v2`

- These vector carry-less multiply benchmark sets are already materially faster
  than `polymul_zbc` on Spike micro-benchmarks.
- Current `vect_mul` speedups versus `baseline_ref` are:
  - `polymul_zvbc`: 99.3x, 98.1x, 170.4x
  - `polymul_zvbc_v2`: 120.3x, 118.9x, 209.5x
- They are therefore benchmarked and thesis-relevant, but they still need final
  algorithm-level interpretation and broader KEM integration before they can be
  treated as the definitive end-state winner.
- The report pipeline currently records both as vector carry-less multiply tracks;
  if the final thesis wants to distinguish Karatsuba-vs-FAFFT vector behaviour
  more sharply, the benchmark metadata should be tightened rather than inferred
  from old phase labels.

### `keccak_rvv`

- This remains planned work.
- Amdahl projections suggest around **3x** speedup for the SHAKE subsystem,
  reducing its share from roughly 36% of post-polymul runtime to around 16%.
- The important open question is whether a mostly drop-in RVV Keccak integration
  is enough, or whether multi-instance interleaving is required to make the
  subsystem-level gain visible in end-to-end KEM measurements.

### `reed_muller_rvv` and `reed_solomon_rvv`

- These remain planned work and are still justified by the bottleneck model.
- The expected combined decoder gain is around **3.5x**, reducing the decoder
  share from roughly 15% of post-polymul runtime to around 6%.
- The remaining design question is how much of the speedup comes from the RM
  Hadamard path versus the RS syndrome / Chien-search path, and whether the
  fused RM→RS pipeline is necessary for a meaningful end-to-end decapsulation win.

### `custom_ise_upper_bound`

- Still optional.
- Only worth deeper investment after the main software-only path is stable, as
  the primary thesis claim is already supported by the descriptive benchmark-set
  comparison in pure software.

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

This order is pragmatic rather than purely chronological. The early tracks all
improve the dominant hotspot. Later tracks only become thesis-critical once the
polymul share has materially dropped.

---

## Track Checklist

Every implementation track should complete the same validation loop:

1. Build the relevant target in the appropriate build directory.
2. Run KAT on QEMU or Spike, depending on ISA requirements.
3. Run a polymul Spike micro-benchmark into `analysis/benchmarks/<bench_set>/`.
4. Run full-KEM Spike measurements only for milestone-quality variants.
5. Generate histogram data when instruction mix matters for the claim.
6. Regenerate report tables with `scripts/report/gen_report.sh --bench-set <bench_set>`.
7. Record conclusions in `analysis/report/` and thesis notes.

Minimum artifact set for a benchmarked track:

- `hqc{128,192,256}_<variant>_polymul_N1.csv`
- `run_events.log`
- optional `hqc{128,192,256}_<variant>_N1.csv`
- optional `hqc{128,192,256}_<variant>_hist.txt`

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

If later subsystem work slips, the fallback thesis still remains valid as a
complete study of HQC polymul design space on RISC-V because the benchmark sets
already preserve that comparison cleanly.

---

## Success Criteria

- All named benchmark sets are discoverable directly from `analysis/benchmarks/`.
- Reports, docs, and scripts refer to benchmark sets by descriptive names rather
  than numbered phases.
- `baseline_ref` remains the shared denominator for reported speedups.
- Every performance claim in the thesis is traceable to a concrete benchmark set directory.
- The workspace structure makes it obvious which files belong to scalar polymul,
  vector polymul, Keccak, decoder, and optional upper-bound work.

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