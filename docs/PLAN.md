# HQC-RISC-V: Implementation Plan

Feature-based roadmap and execution plan for the HQC optimization thesis.

For day-to-day commands see `WORKFLOW.md`.
For toolchain rationale and measurement notes see `TOOLCHAIN.md`.

---

## Table of Contents

- [HQC-RISC-V: Implementation Plan](#hqc-risc-v-implementation-plan)
  - [Table of Contents](#table-of-contents)
  - [Research Context](#research-context)
  - [Optimization Tracks](#optimization-tracks)
    - [Track Intent](#track-intent)
  - [Execution Order](#execution-order)
  - [Track Checklist](#track-checklist)
  - [Twelve-Week Schedule](#twelve-week-schedule)
  - [Success Criteria](#success-criteria)
  - [Mapping Reference](#mapping-reference)

---

## Research Context

The thesis is driven by Amdahl's law. The current baseline is dominated by
polynomial multiplication, so the first work package focuses on polymul tracks.
Once polymul stops dominating total KEM cost, the next practical bottlenecks are
Keccak and decoding.

The repository therefore separates work into these families:

- `baseline_ref`: unmodified vendor reference used as the denominator for all comparisons.
- `polymul_*`: polynomial multiplication variants and experiments.
- `keccak_rvv`: RVV SHAKE/Keccak acceleration.
- `reed_muller_rvv` and `reed_solomon_rvv`: decoder acceleration tracks.
- `custom_ise_upper_bound`: optional upper-bound exploration beyond the main thesis path.

---

## Optimization Tracks

| Track | Primary artifact | Goal | Status |
|---|---|---|---|
| `baseline_ref` | `karatsuba_ref` | correctness baseline and measurement denominator | complete |
| `polymul_zbc` | `karatsuba_zbc` | scalar `clmul` base-case acceleration | complete |
| `fafft_zbc` | `fafft_zbc` | additive-FFT polymul with scalar `Zbc` butterflies | complete |
| `hybrid_zbc` | `hybrid_zbc` | Karatsuba outer levels with FAFFT inner kernel | complete |
| `hybrid_zbc_v2` | `hybrid_zbc` | second hybrid benchmark set for alternate crossover experiments | benchmarked |
| `hybrid_2way_zbc` | `hybrid_zbc` | two-way split hybrid based on the newer literature design | complete |
| `polymul_zvbc` | `karatsuba_zvbc` | vector carry-less multiply polymul path | complete |
| `polymul_zvbc_v2` | `fafft_zvbc` | vectorized FAFFT-oriented experiment / upper-bound comparison | benchmarked |
| `keccak_rvv` | `fips202_rvv` | RVV SHAKE acceleration for seed expansion | planned |
| `reed_muller_rvv` | `reed_muller_rvv` | RVV Hadamard / RM decoding | planned |
| `reed_solomon_rvv` | `reed_solomon_rvv` | RVV RS decoding path | planned |
| `custom_ise_upper_bound` | custom ISE notes | architecture upper bound outside the main baseline path | optional |

### Track Intent

`baseline_ref`

- Preserve a stable denominator for every speedup figure.
- Keep raw Spike data and full-KEM measurements reproducible.

`polymul_zbc`

- Prove the value of `clmul` and `clmulh` in the Karatsuba base case.
- Establish the first large drop in retired instructions.

`fafft_zbc`

- Test whether FAFFT remains competitive once scalar `Zbc` lowers the leaf cost.
- Measure the real padding and basis-conversion overhead under HQC sizes.

`hybrid_zbc`, `hybrid_zbc_v2`, `hybrid_2way_zbc`

- Explore crossover designs that combine Karatsuba and FAFFT.
- Preserve alternate experiments as first-class benchmark sets instead of folding them into a generic phase bucket.

`polymul_zvbc`, `polymul_zvbc_v2`

- Capture the vector carry-less multiply path separately from scalar polymul work.
- Keep Karatsuba- and FAFFT-oriented vector experiments distinguishable in reports.

`keccak_rvv`, `reed_muller_rvv`, `reed_solomon_rvv`

- Address the next bottlenecks after polymul optimization reduces total KEM share.
- Maintain one implementation track per subsystem so results and failure modes stay isolated.

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
- Reports, docs, and scripts refer to benchmark sets by descriptive names rather than numbered phases.
- `baseline_ref` remains the shared denominator for reported speedups.
- Every performance claim in the thesis is traceable to a concrete benchmark set directory.
- The workspace structure makes it obvious which files belong to scalar polymul, vector polymul, Keccak, decoder, and optional upper-bound work.

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
