# HQC-RISC-V: Implementation Plan

Feature-based roadmap and execution plan for the HQC optimization thesis.

This thesis addresses a concrete research gap: as of March 2026, no
publication presents a pure-software, ratified-extensions-only HQC
implementation on RISC-V. Ras et al. (2025) demonstrated 3.52-8.08x FAFFT
speedups on baseline RISC-V but relied on custom hardware in their broader
ANDROMEDA platform. Jang et al. optimized FAFFT for Cortex-M4, which lacks
`clmul`. OptHQC targeted x86 AVX2. This work fills that gap by combining `Zbc`
(carry-less multiply), `V`/RVV, and `Zbkb` into the first optimized
pure-software RISC-V HQC implementation, while empirically comparing
Karatsuba, FAFFT, and hybrid strategies once `clmul` is available.

For day-to-day build and run commands see `WORKFLOW.md`. For toolchain and
methodology rationale see `TOOLCHAIN.md`. For the literature review and thesis
proposals see `../HQC/1_Literature.md`, `../HQC/2_0_Proposals.md`, and
`../HQC/3_thesisHQC.md`. Worked examples are documented in
`../HQC/4_Zbc_example.md` and `../HQC/5_Decoding_RVV.md`.

---

## Research Context & Bottleneck Progression

This thesis is organized around Amdahl's law: each optimization track attacks
the current dominant bottleneck until the next one surfaces.

### Baseline (`baseline_ref`)

| Component | Time (normalized) | % of Total |
|---|---|---|
| Polynomial multiplication (`karatsuba_ref`) | 95 | **95%** |
| SHAKE/Keccak seed expansion | 3.5 | 3.5% |
| RM/RS concatenated decoding | 1.5 | 1.5% |
| **Total** | **100** | |

Polynomial multiplication dominates completely. All other components are minor
until poly-mul is accelerated.

### After polymul acceleration (`polymul_zbc` + `polymul_zvbc`-class work)

Assuming roughly 20x cumulative poly-mul improvement from scalar `Zbc` plus
vector carry-less multiply:

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 95 / 20 = **4.75** | **49%** |
| SHAKE/Keccak seed expansion | 3.5 | **36%** |
| RM/RS concatenated decoding | 1.5 | **15%** |
| **Total** | **~9.75** | |

This is the key transition point: once poly-mul drops below 50%, SHAKE becomes
co-dominant and decoder work is no longer negligible.

### Target full stack (`polymul_zvbc` + `keccak_rvv` + decoder tracks)

Adding vectorized Keccak (~3x) and vectorized decoding (~3.5x):

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 | **65%** |
| SHAKE/Keccak (vectorized) | 3.5 / 3 = **1.17** | **16%** |
| RM/RS decoding (vectorized) | 1.5 / 3.5 = **0.43** | **6%** |
| **Total** | **~6.35** | |

That corresponds to roughly **15.7x** end-to-end speedup over baseline.

### Upper-bound model (`custom_ise_upper_bound`)

If vector carry-less multiply is combined with the full software stack and then
augmented with custom architectural support:

| Component | Time | % of Total |
|---|---|---|
| Polynomial multiplication | 4.75 / 4 = **1.19** | **40%** |
| SHAKE/Keccak (vectorized) | 1.17 | **40%** |
| RM/RS decoding (vectorized) | 0.43 | **15%** |
| **Total** | **~2.79** | |

This gives an approximate **35.8x** pure-software upper bound over baseline,
and motivates the custom-ISE study as future-looking ceiling analysis.

---

## Algorithm Selection: Why Karatsuba vs. FAFFT vs. Hybrid Matters

Ras et al. demonstrated 3.52-8.08x FAFFT speedup on baseline RISC-V without
`clmul`. Adding `clmul` changes the practical cost model substantially:

| Factor | `polymul_zbc` | `fafft_zbc` | hybrid tracks |
|---|---|---|---|
| Base-case cost | ~4 cycles (`clmul` + reduction) | ~6-8 cycles + field ops | Karatsuba outer, FAFFT inner |
| Overhead per level | 5 XOR additions | basis conversion + combine + permutation | depends on crossover depth |
| Recursion depth | low, cache-friendly | single-pass FFT style | 1-2 Karatsuba levels plus FAFFT core |
| Padding / CRT overhead | none | ~30% for HQC-128 without a usable CRT kernel | depends on subproblem-domain coupling |
| Memory locality | excellent | poor | intermediate |

The central thesis question is not whether FAFFT is asymptotically attractive,
but whether it still wins once the leaf multiply cost collapses to two
instructions (`clmul` + `clmulh`).

Empirical conclusion so far:

- `polymul_zbc` is the scalar winner for HQC-128/192/256.
- `fafft_zbc`, `hybrid_zbc`, and `hybrid_2way_zbc` remain thesis-critical
  because they explain where and why the expected crossover fails at HQC sizes.
- `polymul_zvbc` and `polymul_zvbc_v2` show that vector carry-less multiply can
  shift the limit again, making vector tracks central to the final thesis claim.

---

## Codebase Reuse Strategy

| Component | Source codebase | Reuse level | RISC-V adaptation |
|---|---|---|---|
| KEM framework (KeyGen, Encaps, Decaps, FO transform) | HQC Reference C | 100% reuse | compile for `riscv64-unknown-elf` |
| KAT vectors and baseline behavior | HQC Reference C | 100% reuse | run on QEMU and Spike for each track |
| FAFFT algorithm (recursion, basis conversion, CRT ideas) | Jang et al. FAFFT_HQC | partial reuse | port C logic; replace ARM-specific arithmetic |
| FAFFT GF(2^m) butterfly | this thesis | new code | `clmul` / `clmulh` + reduction |
| FAFFT XOR combines | this thesis | new code | RVV `vle64.v` / `vxor.vv` / `vse64.v` helpers |
| Table-driven RS decoder | OptHQC | algorithm reuse, code rewrite | replace AVX2 lookup with RVV `vluxei8.v` |
| Keccak-f[1600] | Zhang et al. RVV Keccak | near-drop-in reuse | adapt to HQC SHAKE wrappers |
| RM Hadamard decoder | HQC reference + this thesis | new code | RVV `vadd.vv` / `vsub.vv` / `vslidedown.vi` |

Assembly order used by the project:

1. Fork HQC Reference C and verify KAT on the RISC-V toolchain.
2. Integrate scalar `clmul` poly-mul work and keep KAT green.
3. Port FAFFT C logic and replace the hot arithmetic with RISC-V primitives.
4. Add vector XOR and `vclmul` work where the algorithm permits it.
5. Drop in RVV Keccak.
6. Port OptHQC-style decoder ideas to RVV.
7. Re-run Spike benchmarks and regenerate report tables.

The operating rule is unchanged throughout: KAT after every change, then Spike
micro-benchmark, then full-KEM measurement only at milestone-quality states.

---

## Detailed Track Breakdown

### `baseline_ref`

**Status:** COMPLETE.

**Goal:** Preserve the correctness baseline and performance denominator.

**Implementation steps and historical notes:**

1. Build the reference `karatsuba_ref` implementation for HQC-128/192/256.
2. Run KAT on QEMU and Spike to lock in correctness before any optimization.
3. Measure Spike instruction counts for both the `vect_mul` micro-benchmark and
   the full KEM operations (keypair, encaps, decaps).
4. Generate instruction-frequency histograms and an `llvm-mca` baseline for the
   vendor reference `gf2x.c` path.
5. Preserve this benchmark set as the fixed denominator for all later speedup
   claims.

**Current Results & Observations:**

- `vect_mul` instruction counts:
  - HQC-128: **18,578,007** (~18.58M)
  - HQC-192: **56,469,320** (~56.47M)
  - HQC-256: **139,077,901** (~139.08M)
- Full KEM instruction counts:
  - HQC-128: keypair **18,816,325**, encaps **37,636,615**, decaps **56,962,431**
  - HQC-192: keypair **56,986,059**, encaps **113,992,185**, decaps **171,660,064**
  - HQC-256: keypair **140,074,066**, encaps **279,984,371**, decaps **421,305,306**
- Observation: `vect_mul` remains roughly **98-99%** of baseline keypair cost,
  confirming that polymul is the correct first target.

### `polymul_zbc`

**Status:** COMPLETE.

**Goal:** Replace the Karatsuba leaf multiply with `clmul` / `clmulh` and make
scalar RISC-V `Zbc` the new baseline winner.

**Implementation steps and historical notes:**

1. Implement `src/poly_mul/karatsuba_zbc/gf2x_clmul.c` as a drop-in replacement
   for the reference `gf2x.c` interface.
2. Use a `clmul` + `clmulh` leaf multiply so each 64x64 carry-less product is
   computed in two instructions instead of a long shift/XOR schoolbook loop.
3. Keep the Karatsuba recursion structure from the vendor implementation and
   replace only the leaf path first.
4. Sweep `KARATSUBA_THRESHOLD` empirically to find the best recursion cutoff.
5. Preserve a full unreduced `2*n` intermediate buffer and reduce modulo
   `x^PARAM_N - 1` once at the end, matching the reference algebra while
   avoiding repeated reduction overhead.
6. Compare instruction mix and verify that `clmul` / `clmulh` dominate the hot
   path rather than shifts and XOR-heavy schoolbook code.
7. Keep the recursion-depth knob and reduction behavior configurable enough for
   repeated threshold experiments.

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
- Root cause for the gap between polymul and full-KEM speedups: once polymul
  collapses, SHAKE and decoding become co-dominant bottlenecks.
- Pending-idea check from the original proposal:
  - Reduction modulo `x^n - 1`: **implemented**, but as a final fold over the
    unreduced `2*n` buffer rather than per-leaf inline reduction.
  - Lazy reduction: **implemented**. The current code keeps the unreduced
    intermediate product and performs a single final reduction step.
  - Threshold sweep / recursion tuning: **implemented** with a compile-time
    knob and documented empirical sweep.

### `fafft_zbc`

**Status:** COMPLETE.

**Goal:** Test whether scalar `Zbc`-accelerated FAFFT can beat scalar
Karatsuba once the hot arithmetic is mapped to RISC-V primitives.

**Implementation steps and historical notes:**

1. Port `src/poly_mul/fafft_zbc/fafft.c` and the supporting FAFFT reference
   machinery from Jang et al.'s code structure.
2. Preserve the additive NTT / basis-conversion / CRT framing from that work.
3. For HQC-128, plan around CRT decomposition specifically to avoid gross
   power-of-two padding overhead on the 17,669-bit input size.
4. Replace the ARM-specific GF(2^m) multiply path with `clmul` / `clmulh`
   sequences plus reduction over the relevant tower-field representation.
5. Treat the GF(2^m) butterfly as the hottest FAFFT inner loop and optimize it
   first, because this is where scalar `Zbc` should have mattered most.
6. Keep XOR accumulation scalar initially, with later RVV vectorization left to
   the vector tracks.
7. Benchmark directly against both `baseline_ref` and `polymul_zbc`.

**Current Results & Observations:**

- `vect_mul` instruction counts:
  - HQC-128: **1,462,166**
  - HQC-192: **3,256,399**
  - HQC-256: **3,258,453**
- Speedups vs `baseline_ref`: **12.7x / 17.3x / 42.7x**.
- Relative to `polymul_zbc`: **0.24x / 0.33x / 0.69x**. It is still slower.
- Root-cause analysis:
  - The planned CRT route is blocked at HQC-128 because 17,669-bit inputs map
    to **553 words**, which exceeds the **512-word** fit assumed by the
    available `bc_1_16384` path.
  - The missing `bmul2_4096_to_4096` path prevents the smaller same-domain
    product route needed for the intended CRT decomposition.
  - Basis-conversion, butterfly, and permutation overhead remain dominant at
    HQC sizes even after replacing the multiply primitive.
- Historical conclusion that must remain documented: the asymptotic FAFFT idea
  did not survive the new scalar `clmul` cost model at HQC-128/192/256.

### `hybrid_zbc`

**Status:** COMPLETE.

**Goal:** Test the Karatsuba-outer / FAFFT-inner crossover hypothesis rather
than assuming either algorithm wins everywhere.

**Implementation steps and historical notes:**

1. Implement `src/poly_mul/hybrid_zbc/hybrid.c` so that the outer recursion
   performs 1-3 Karatsuba levels before dispatching into the FAFFT core.
2. Use the scalar `polymul_zbc` leaf behavior when the subproblem is small
   enough to avoid obvious FAFFT overhead.
3. Benchmark crossover depths `d = 0, 1, 2` at minimum, across all three HQC
   parameter sets.
4. Test the original expectation of a U-shaped cost curve: enough Karatsuba to
   shrink subproblems, but not enough to swamp the run with outer recursion.
5. Compare against both pure FAFFT and pure Karatsuba so the crossover claim is
   evidence-based rather than theoretical.

**Current Results & Observations:**

- HQC-128: d=0 **1,463,531**, d=1 **4,390,073**, d=2 **13,160,160**
- HQC-192: d=0 **3,254,008**, d=1 **9,773,238**, d=2 **13,176,205**
- HQC-256: d=0 **3,258,265**, d=1 **9,785,206**, d=2 **13,197,465**
- Best depth is `d=0`, i.e. pure FAFFT. No beneficial crossover appeared.
- Root cause: the fixed-domain FAFFT wrappers do not shrink work in proportion
  to Karatsuba subproblem size. Each extra Karatsuba level mainly multiplies
  the number of FAFFT invocations instead of reducing their cost.
- This negative result is thesis-important because it explains why the
  intuitive crossover story fails on the actual implementation surface.

### `hybrid_zbc_v2`

**Status:** BENCHMARKED.

**Goal:** Preserve the alternate hybrid benchmark-set experiments and verify
whether a rerun with mixed benchmark-set handling changes the crossover story.

**Implementation steps and historical notes:**

1. Re-run the hybrid track using the same underlying hybrid implementation but
   preserve it as a separate benchmark set in the reporting pipeline.
2. Keep the benchmark-set naming stable so the result remains reproducible in
   `analysis/report/` and later thesis tables.
3. Compare the alternate benchmark set against the original `hybrid_zbc`
   measurements and against the scalar winner `polymul_zbc`.
4. Use this track as a guard against reporting artifacts: if the conclusions
   change under the alternate run, the scalar-vs-hybrid story would need to be
   revisited.

**Current Results & Observations:**

- `vect_mul` counts from the report pipeline: **1.1M / 2.6M / 3.1M** for
  HQC-128/192/256.
- Speedups vs `baseline_ref`: **16.6x / 21.9x / 44.2x**.
- Observation: the rerun is somewhat better than the earlier FAFFT-like scalar
  results on some points, but it still does not displace `polymul_zbc` as the
  scalar winner and does not change the overall crossover conclusion.

### `hybrid_2way_zbc`

**Status:** COMPLETE.

**Goal:** Implement and evaluate the newer 2-way FAFFT-Karatsuba split that
uses a half-domain kernel to remove the worst fixed-domain overhead.

**Implementation steps and historical notes:**

1. Split `f = f_lo + x^(2^d) * f_hi` at the largest useful power-of-two
   boundary so the low part fits the half-domain FAFFT kernel cleanly.
2. Use the 2-way split sizes documented during the implementation:
   - HQC-128: `d=14`, `SPLIT=256` words (16,384 bits), `HI=21` words
   - HQC-192: `d=15`, `SPLIT=512` words (32,768 bits), `HI=49` words
   - HQC-256: `d=15`, `SPLIT=512` words (32,768 bits), `HI=389` words
3. Build the half-domain kernel from the existing primitives:
   - HQC-128: `bc_1_16384 + btfy_32768`
   - HQC-192/256: `bc_1_32768 + btfy_65536`
4. Use half-domain FAFFT for `f_lo * g_lo` and the Karatsuba middle term
   `(f_lo ^ f_hi) * (g_lo ^ g_hi)`.
5. Keep the residual `f_hi * g_hi` on scalar Karatsuba + `clmul`, because it is
   the intentionally tiny tail subproblem.
6. Preserve the two important deferred ideas in the documentation:
   - CRT iteration was not completed because it requires a same-domain
     `bmul2_4096_to_4096` path that the current `bc_1` / `btfy` infrastructure
     does not provide.
   - Key caching was deferred because it requires KEM-API surgery to expose
     FFT-transformed key state across keygen/encaps/decaps.

**Current Results & Observations:**

- `vect_mul` instruction counts:
  - HQC-128: **1,304,473**
  - HQC-192: **2,950,035**
  - HQC-256: **3,523,625**
- Relative to `polymul_zbc`:
  - HQC-128: **3.68x slower** (1,304,473 vs 354,187)
  - HQC-192: **2.73x slower** (2,950,035 vs 1,078,641)
  - HQC-256: **1.56x slower** (3,523,625 vs 2,257,476)
- Root-cause analysis:
  - The 2-way split improves over pure FAFFT at HQC-128 and HQC-192, but the
    underlying leaf economics still favor Karatsuba + `clmul`.
  - `clmul` reduces the Karatsuba leaf cost to essentially two instructions per
    64x64 partial product, while the half-domain FAFFT kernel still pays for
    basis conversion, butterfly passes, and constant-table traffic.
  - At HQC-256, the larger `HI` tail makes the scalar residual multiply costly
    enough that the 2-way split even loses slightly against pure FAFFT.
- Historical conclusion that must remain in the plan: on RISC-V with `Zbc`,
  the Karatsuba/FAFFT crossover shifts far to the right, beyond the HQC sizes
  studied here.

### `polymul_zvbc`

**Status:** BENCHMARKED.

**Goal:** Vectorize the winning Karatsuba path with `vclmul.vv` / `vclmulh.vv`
and high-LMUL XOR combines.

**Implementation steps and historical notes:**

1. Implement `src/poly_mul/karatsuba_zvbc/gf2x_vclmul.c` as the vector version
   of the scalar `polymul_zbc` path.
2. Vectorize the leaf multiply loop so each `vclmul.vv` processes multiple
   64-bit carry-less products simultaneously; at `VLEN=256`, each vector
   multiply handles four 64x64 products per instruction.
3. Use splat-and-sweep vector loops over `b[]` rather than attempting to alter
   the higher Karatsuba structure first.
4. Vectorize the combine XOR loops with `vxor.vv` at high LMUL so the Karatsuba
   assembly phase stops being a large scalar tax.
5. Preserve the same recursion depth and threshold structure as the scalar
   winner so the comparison isolates vectorization effects.
6. Benchmark on Spike under the Zvbc ISA string and compare directly against
   both `polymul_zbc` and the FAFFT-family vector experiments.

**Current Results & Observations:**

- `vect_mul` instruction counts and speedups vs `baseline_ref`:
  - HQC-128: **187.1K** (99.3x)
  - HQC-192: **575.5K** (98.1x)
  - HQC-256: **816.4K** (170.4x)
- Relative to `polymul_zbc`, the vector Karatsuba path adds another large gain
  over the scalar winner.
- Observation: this is already materially faster than scalar `polymul_zbc` in
  the micro-benchmarks and is the strongest current pure-software polymul path.
- Root-cause interpretation: the gain is strong but still below the theoretical
  4x leaf-level parallelism because recursion overhead, addressing logic, and
  non-vectorizable control flow remain in the total cost.

### `polymul_zvbc_v2`

**Status:** BENCHMARKED.

**Goal:** Preserve the alternate vector benchmark set and document the higher
micro-benchmark ceiling now visible in the report pipeline.

**Implementation steps and historical notes:**

1. Keep the alternate Zvbc benchmark set reproducible in the analysis and
   report pipeline rather than collapsing it into the main vector track.
2. Preserve the same broad vector-carry-less-multiply thesis question: how far
   can vectorized binary polynomial multiplication push the HQC bottleneck?
3. Compare this alternate benchmark set against both `polymul_zvbc` and the
   scalar `polymul_zbc` winner.
4. Use it to bound the realistic best-case micro-benchmark ceiling before final
   algorithm-level interpretation is frozen in the thesis tables.

**Current Results & Observations:**

- `vect_mul` instruction counts and speedups vs `baseline_ref`:
  - HQC-128: **154.5K** (120.3x)
  - HQC-192: **474.9K** (118.9x)
  - HQC-256: **663.8K** (209.5x)
- Observation: this is the best currently recorded vector polymul benchmark set
  in the report pipeline.
- Historical caution that should remain explicit: this benchmark set still needs
  final algorithm-level interpretation before it is declared the stable end
  state for the thesis narrative.

### `keccak_rvv`

**Status:** TODO / PLANNED.

**Goal:** Accelerate SHAKE / Keccak once post-polymul Amdahl analysis shows it
has become a co-dominant bottleneck.

**Implementation steps and planned work:**

1. Integrate the Zhang et al. RVV Keccak-f[1600] implementation under
   `src/keccak/rvv/`.
2. Wrap it behind HQC's `shake_absorb` / `shake_squeeze` interface so the KEM
   framework can switch implementations without semantic drift.
3. Configure the build with `HQC_KECCAK_IMPL=rvv`.
4. Re-run KAT against the active polymul winners, especially `polymul_zbc` and
   `polymul_zvbc`.
5. Measure the SHAKE subsystem directly and then re-measure full KEM behavior.
6. If drop-in RVV is not enough, implement multi-instance interleaving so
   multiple independent Keccak states occupy vector lanes simultaneously.

**Current Results & Observations:**

- This track is still **TODO / PLANNED**.
- Expected gain from the Amdahl model: **~3x SHAKE speedup**.
- Expected share reduction: **~36% -> ~16%** of post-polymul runtime.
- Expected end-to-end impact: necessary to move beyond the point where polymul
  alone dominates overall KEM runtime.
- Root-cause question still open: whether a drop-in RVV Keccak is enough, or
  whether multi-instance interleaving is required to realize the subsystem gain.

### `reed_muller_rvv`

**Status:** TODO / PLANNED.

**Goal:** Vectorize the Reed-Muller decoding half of the concatenated decoder,
especially the Hadamard-heavy path.

**Implementation steps and planned work:**

1. Implement `src/decode/rvv/reed_muller_rvv.c` using RVV intrinsics.
2. Build the 7-stage Fast Hadamard Transform over soft-decision `int16_t`
   values using `vadd.vv`, `vsub.vv`, and `vslidedown.vi`.
3. Keep stages 1-4 inside vector registers where stride permits and handle the
   larger-stride stages as cross-register butterflies.
4. Replace the scalar argmax with `vabs`, reduction, and equality-mask logic.
5. Batch multiple RM codewords so the RVV setup cost is amortized cleanly.
6. Validate the decoder in isolation and then in full KAT.

**Current Results & Observations:**

- This track is still **TODO / PLANNED**.
- It is one half of the combined decoder target.
- Expected combined decoder improvement with `reed_solomon_rvv`: **~3.5x**.
- Expected decoder share reduction, combined with RS work: **~15% -> ~6%** of
  post-polymul runtime.
- Planned next step is straightforward: land the RVV FHT path first, because it
  is the most structurally self-contained decoder acceleration.

### `reed_solomon_rvv`

**Status:** TODO / PLANNED.

**Goal:** Vectorize RS syndrome computation and table-driven GF operations, and
then fuse the RM->RS pipeline where beneficial.

**Implementation steps and planned work:**

1. Implement `src/decode/rvv/reed_solomon_rvv.c` using table-driven GF(2^8)
   arithmetic adapted from OptHQC.
2. Precompute the multiplication tables for the RS evaluation points and expose
   them in a layout suitable for RVV indexed loads.
3. Use `vluxei8.v` to perform many table lookups in parallel.
4. Accumulate syndrome contributions with `vxor.vv` instead of scalar loops.
5. Implement the fused RM->RS pipeline so decoded RM outputs are consumed before
   they leave the hot cache footprint.
6. Validate standalone RS behavior and then the full decoder pipeline in KAT.

**Current Results & Observations:**

- This track is still **TODO / PLANNED**.
- It works jointly with `reed_muller_rvv` in the Amdahl projection.
- Expected combined decoder speedup remains **~3.5x**, reducing the decoder
  share from **~15% to ~6%** after polymul acceleration.
- Historical implementation note that should stay visible: the table-driven RVV
  RS path is attractive precisely because it can replace roughly hundreds of
  scalar GF operations with a much shorter indexed-vector sequence.

### `custom_ise_upper_bound`

**Status:** TODO / PLANNED (OPTIONAL).

**Goal:** Characterize the performance ceiling beyond the pure-software thesis
path and document which custom instructions would matter most.

**Implementation steps and planned work:**

1. Keep this as an optional branch after the software stack is stable.
2. Model candidate fused instructions such as `BFY.GF2M`, `VBFY.GF2M`,
   `CLMUL.RED`, and `KEC.ROUND`.
3. Estimate their per-kernel cycle impact and project end-to-end savings.
4. If time permits, drive the model in gem5 or a comparable simulator-backed
   environment rather than relying on hand estimates alone.
5. Use the result as an upper-bound argument and future-work section, not as a
   required thesis contribution.

**Current Results & Observations:**

- This track is still **TODO / PLANNED**.
- It is not required for the core thesis claim.
- Expected role in the narrative: quantify how much headroom remains after the
  software stack (`polymul_zvbc` + Keccak + decoder tracks) is complete.
- Amdahl-guided expectation: the pure-software stack reaches about **15.7x**
  end-to-end, while the software-only upper-bound model reaches roughly
  **35.8x** over baseline before any custom hardware is added.

---

## Schedule

| Weeks | Focus | Deliverable |
|---|---|---|
| 1 | `baseline_ref` audit and reproducible measurement | clean reference benchmark set |
| 2-4 | `polymul_zbc` refinement and validation | stable scalar `Zbc` winner |
| 5-6 | `fafft_zbc` implementation and measurement | direct Karatsuba vs FAFFT comparison |
| 7 | `hybrid_zbc`, `hybrid_zbc_v2`, and `hybrid_2way_zbc` comparison | crossover analysis with scoreboard |
| 8 | `polymul_zvbc` and `polymul_zvbc_v2` | vector polymul result set |
| 9 | `keccak_rvv` integration | subsystem-level SHAKE speedup data |
| 10 | `reed_muller_rvv` | decoder micro-benchmark and KAT validation |
| 11 | `reed_solomon_rvv` | complete decoder track or bounded partial result |
| 12 | thesis tables, plots, write-up, and optional `custom_ise_upper_bound` framing | publication-ready figures and conclusions |
