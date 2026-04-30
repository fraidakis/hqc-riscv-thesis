# HQC-RISC-V: Implementation Plan

Phase-by-phase roadmap and 12-week execution schedule for the HQC
post-quantum KEM optimisation thesis.

For day-to-day build/run commands see [`WORKFLOW.md`](WORKFLOW.md).
For toolchain and methodology rationale see [`TOOLCHAIN.md`](TOOLCHAIN.md).

---

## Table of Contents

- [Phase Overview](#phase-overview)
- [Phase 0b — Spike Baseline Benchmarks (Week 1)](#phase-0b--spike-baseline-benchmarks-week-1)
- [Phase 1a — Scalar Zbc Karatsuba (Weeks 2–4)](#phase-1a--scalar-zbc-karatsuba-weeks-24)
- [Phase 1b — FAFFT with Zbc (Weeks 5–7)](#phase-1b--fafft-with-zbc-weeks-57)
- [Phase 1c — Hybrid Zbc (Week 8)](#phase-1c--hybrid-zbc-week-8)
- [Phase 1+ — Vectorised FAFFT with Zvbc (Weeks 9–10)](#phase-1--vectorised-fafft-with-zvbc-weeks-910)
- [Phase 2 — RVV Keccak (optional)](#phase-2--rvv-keccak-optional)
- [Phase 3 — RVV RM/RS Decoder (optional)](#phase-3--rvv-rmrs-decoder-optional)
- [Weeks 11–12: Thesis Tables and Paper](#weeks-1112-thesis-tables-and-paper)
- [Measurement Checklist (per phase)](#measurement-checklist-per-phase)

---

## Phase Overview

| Phase | Variant | Target speedup | Status |
|-------|---------|---------------|--------|
| 0a | `karatsuba_ref` | baseline KAT | ✅ complete |
| 0b | `karatsuba_ref` | Spike instruction counts | 🔄 in progress |
| 1a | `karatsuba_zbc` | ≥ 2× on `vect_mul` | planned |
| 1b | `fafft_zbc` | ≥ 3× over 1a | planned |
| 1c | `hybrid_zbc` | crossover optimum | planned |
| 1+ | `fafft_zvbc` | ≥ 4× over 1b | planned |
| 2  | `*_rvv_keccak` | > 5× Keccak throughput | optional |
| 3  | `*_rvv_decoder` | FHT vectorisation | optional |

> **Iteration rule:** QEMU KAT after every change → Spike polymul micro-bench
> after QEMU passes → Spike full KEM bench only at phase milestones.
> See [WORKFLOW.md §8](WORKFLOW.md) for the three-tier summary table.

---

## Phase 0b — Spike Baseline Benchmarks (Week 1)

**Goal:** Establish the instruction-count denominator for all speedup comparisons.

```bash
cd ~/Thesis/hqc-riscv
source toolchain/toolchain.env

# Configure Spike bench build
cmake -S . -B build-spike \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1
cmake --build build-spike -j$(nproc)

# Launch all three levels in background (polymul micro-bench first)
scripts/bench/run_spike_bench_phase.sh phase0b --variant karatsuba_ref
scripts/bench/run_histogram_all.sh phase0b --variant karatsuba_ref

# While Spike runs, get the static baseline with llvm-mca
./analysis/llvm_mca/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c
# Output: analysis/llvm_mca/reports/gf2x_mca.txt

# When polymul micro-bench finishes, commit baseline numbers
git add analysis/benchmarks/phase0b/ analysis/llvm_mca/reports/
git commit -m "feat(phase0b): baseline instruction counts (Spike, karatsuba_ref)"
```

**Deliverables:**
- `analysis/benchmarks/phase0b/hqc{128,192,256}_karatsuba_ref_polymul_N1.csv` — `vect_mul` instruction counts
- `analysis/benchmarks/phase0b/hqc{128,192,256}_karatsuba_ref_hist.txt` — instruction frequency histograms
- `analysis/benchmarks/phase0b/hqc{128,192,256}_karatsuba_ref_N1.csv` — full KEM instruction counts (run overnight)
- `analysis/llvm_mca/reports/gf2x_mca.txt` — MCA baseline

**Key check:** `gf2x_mul` Karatsuba should account for ≥ 95% of total KEM instruction count.

---

## Phase 1a — Scalar Zbc Karatsuba (Weeks 2–4)

**Goal:** Replace the inner Karatsuba base-case multiplication with `clmul`/`clmulh`
scalar instructions from the Zbc extension.

**Implementation steps:**

1. Implement `src/poly_mul/karatsuba_zbc/gf2x_clmul.c`:
   - Leaf multiplications: single `clmul` + `clmulh` per 64-bit word pair
   - Keep the Karatsuba recursion structure from vendor `gf2x.c`
   - Add lazy reduction with `clmulr` where applicable
2. Run KAT to verify correctness against all three levels
3. Benchmark on Spike vs. `karatsuba_ref` — target ≥ 2× speedup on `gf2x_mul`
4. Profile instruction mix: `clmul` fraction in `gf2x_mul` should approach 100%

**Per-change workflow:**

```bash
cmake --build build --target hqc128_karatsuba_zbc -j$(nproc)

./tests/kat/run_kat.sh build/hqc128_karatsuba_zbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp 2>/dev/null
# Expected: PASS 100/100

scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc
./analysis/llvm_mca/run_mca.sh src/poly_mul/karatsuba_zbc/gf2x_clmul.c
```

**Key metric:** In histogram output, `clmul` + `clmulh` should dominate;
`xor`, `slli`, `srli` counts should drop by > 90%.

---

## Phase 1b — FAFFT with Zbc (Weeks 5–7)

**Goal:** Replace Karatsuba with a Frobenius Additive FFT polynomial multiplication
using `clmul`-accelerated GF(2^m) arithmetic.

**Implementation steps:**

1. Implement `src/poly_mul/fafft_zbc/fafft.c`:
   - Additive NTT over GF(2^{16}) with `clmul`-based butterfly operations
   - Match HQC-128 parameter: degree-16384 polynomials over GF(2)
2. Implement RVV XOR accumulation for the linear-complexity layers
3. KAT + benchmark comparison with Phase 1a

**Reference:** Ras et al. (NIST 2025) achieve 3.52× over Karatsuba on CV32E40P
using this approach.  Target ≥ 3× to match or exceed their scalar result.

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

**Goal:** Karatsuba outer levels descending into FAFFT inner levels at a
crossover depth chosen by profiling.

1. Implement `src/poly_mul/hybrid_zbc/hybrid.c`
2. Benchmark crossover depths to find optimum (likely 2–4 Karatsuba levels)
3. Compare against Phase 1a and 1b

---

## Phase 1+ — Vectorised FAFFT with Zvbc (Weeks 9–10)

**Goal:** Replace scalar `clmul` butterflies with `vclmul.vv` (Zvbc extension),
targeting ≥ 4× over `fafft_zbc` (≥ 14× over baseline).

**Implementation steps:**

1. Implement `src/poly_mul/fafft_zvbc/fafft_vclmul.c` using RVV intrinsics
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

| Phase | `vect_mul` insns (HQC-128) | vs baseline | vs `karatsuba_zbc` |
|-------|---------------------------|-------------|---------------------|
| 0 (ref) | XXXM | 1.0× | — |
| 1a (zbc) | XXXM | ~12× | 1.0× |
| 1b (fafft) | XXXM | ~25× | ~2× |
| 1+ (zvbc) | XXXM | ~50× | ~4× |

*(Numbers are illustrative — actual values from Spike runs.)*

**Throughput argument:** `vclmul.vv` processes VLEN/SEW = 256/64 = 4 carry-less
products simultaneously.  FAFFT butterfly operations are data-independent, so
the loop is throughput-bound.  Under throughput-bound execution, even if
`vclmul.vv` has latency L > 1, the 4× parallelism more than compensates for
any N ≤ 4 issue cycles.

---

## Phase 2 — RVV Keccak (optional)

**Goal:** Replace the scalar Keccak-f\[1600\] with a vectorized implementation.

1. Integrate Zhang et al. RVV Keccak (or similar) under `src/keccak/rvv/`
2. Configure with `-DHQC_KECCAK_IMPL=rvv`
3. KAT across all levels and all polymul variants
4. Benchmark: target > 5× Keccak throughput improvement

**Priority:** Only if `vect_mul` speedup leaves Keccak as a significant fraction
of total runtime.  Profile with Spike `--histogram` first.

---

## Phase 3 — RVV RM/RS Decoder (optional)

**Goal:** Vectorise the RM/RS decoding path (FHT + syndrome computation).

1. Implement `src/decode/rvv/reed_muller_rvv.c` using `vxor.vv` + FHT butterfly
2. Implement `src/decode/rvv/reed_solomon_rvv.c` with `vluxei8.v` gather-scatter
3. Configure with `-DHQC_DECODE_IMPL=rvv`
4. KAT + benchmark across all levels

---

## Weeks 11–12: Thesis Tables and Paper

**Mandatory results:**
1. Spike instruction counts for `vect_mul` and full KEM — all 3 levels × all variants.
2. Instruction mix histograms showing the scalar → `clmul` → `vclmul` transition.
3. llvm-mca IPC estimates for each hot loop.
4. Speedup table vs. NIST reference (`karatsuba_ref`) and vs. Ras et al. (RISC-V FAFFT baseline).

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
  Phase 0  XXXM           XXXM           XXXM
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

*Last updated: 2026-04-30. Phase 0a complete, Phase 0b in progress.*
