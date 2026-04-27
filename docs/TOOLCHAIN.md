# TOOLCHAIN.md — Benchmarking Methodology for HQC on RISC-V

Comprehensive analysis, tool ranking, and workflow for benchmarking a
pure-software, RISC-V-extended implementation of HQC within a 3-month
thesis deadline.

---

## Table of Contents

1. [Defining the Benchmarking Goal](#1-defining-the-benchmarking-goal)
2. [Addressing the CPI / Vector Dilemma](#2-addressing-the-cpi--vector-dilemma)
   - [2.1 The Core Problem](#21-the-core-problem)
   - [2.2 The Accepted Argument Structure](#22-the-accepted-argument-structure)
   - [2.3 Instruction Mix Breakdown — Our Primary Defense](#23-instruction-mix-breakdown--our-primary-defense)
   - [2.4 The Throughput-Bound Argument for vclmul](#24-the-throughput-bound-argument-for-vclmul)
   - [2.5 How Published Papers Handle This](#25-how-published-papers-handle-this)
3. [Toolchain Analysis & Ranking](#3-toolchain-analysis--ranking)
   - [3.1 Tier 1 — Recommended (use now)](#31-tier-1--recommended-use-now)
   - [3.2 Tier 2 — Supporting (supplementary evidence)](#32-tier-2--supporting-supplementary-evidence)
   - [3.3 Tier 3 — Avoid / Not Applicable](#33-tier-3--avoid--not-applicable)
4. [Actionable Workflow — Start Today](#4-actionable-workflow--start-today)
   - [4.1 Week 1: Establish the Baseline](#41-week-1-establish-the-baseline)
   - [4.2 Weeks 2–4: Phase 1a (scalar Zbc Karatsuba)](#42-weeks-24-phase-1a-scalar-zbc-karatsuba)
   - [4.3 Weeks 5–8: Phase 1b/1c (FAFFT + hybrid)](#43-weeks-58-phase-1b1c-fafft--hybrid)
   - [4.4 Weeks 9–10: Phase 1+ (Zvbc vectorisation)](#44-weeks-910-phase-1-zvbc-vectorisation)
   - [4.5 Weeks 11–12: Thesis results and paper tables](#45-weeks-1112-thesis-results-and-paper-tables)
5. [Prior Art Reference Points](#5-prior-art-reference-points)

---

## 1. Defining the Benchmarking Goal

This is a **pure software optimization paper**. We are not designing
hardware, writing RTL, or modifying the microarchitecture. Our
contributions are algorithmic and ISA-level:

1. Replacing scalar GF(2) polynomial multiplication with `clmul`/`clmulh`
   (Zbc) and `vclmul.vv`/`vclmulh.vv` (Zvbc).
2. Applying FAFFT-based multiplication to reduce the polynomial
   multiplication complexity.
3. Optionally vectorizing Keccak (Phase 2) and the RM/RS decoder (Phase 3).

### Primary metric: retired instruction count

Since we run on **Spike ISS** (the only available RISC-V simulator that
provides a meaningful `rdcycle`), our primary metric is **retired
instruction count** — not wall-clock time, not pipeline cycles.

This is explicitly acceptable in the PQC community. The pqm4 project
documentation explicitly contrasts its real-hardware measurements with
`pqriscv`'s ISS-based approach, noting that ISS results are presented as
instruction counts. Spike's own README documents that `rdcycle = retired
instruction count` (no pipeline model, no cache, no branch predictor).

> **Claim to reviewers:** "We reduce the retired instruction count of
> `vect_mul` by X× for HQC-128/192/256, which represents ≥90% of total
> KEM runtime (verified by profiling). Under any throughput-bound
> execution model, this translates directly to a proportional cycle-time
> reduction."

### Secondary metric: instruction mix / operation count

A reduction in instruction count is only meaningful if the replacement
instructions are not pathologically expensive. We will demonstrate this
via instruction mix breakdowns (see §2.3) and llvm-mca static throughput
analysis (see §3.1).

### What we do NOT claim

- Wall-clock time on real hardware (no RISC-V Linux board with Zvbc in lab).
- Exact cycle counts with pipeline effects (no RTL model with Zvbc timing).
- Better performance than ARM Cortex-M4/A72 implementations (different
  ISA; we compare against other RISC-V baselines).

---

## 2. Addressing the CPI / Vector Dilemma

### 2.1 The Core Problem

When a reviewer sees:

> "We replaced 8 scalar `clmul` instructions (each CPI ≈ 1) with 2
> `vclmul.vv` instructions (CPI unknown, likely ≥ 4 per element on most
> pipelines)."

...they will immediately ask: *"How do you know the net cycle count
decreased? The vector instruction may have higher CPI than the scalar one."*

This is the **CPI dilemma**: instruction count decreased, but cycles may
not have, if the replacement instructions are proportionally more expensive.

### 2.2 The Accepted Argument Structure

The PQC benchmarking community (TCHES, CHES, NIST PQC conferences)
accepts the following chain of evidence, in decreasing strength:

| Evidence type | Strength | Available to us? |
|---|---|---|
| Direct cycle measurement on real silicon (perf PMU) | Gold standard | No — no Zvbc board in lab |
| Cycle measurement on FPGA-synthesized core (CSR counter) | Strong | Possible (Vivado, see §3.2) |
| Instruction count on Spike ISS + throughput argument + llvm-mca | Accepted | **Yes — our primary approach** |
| Asymptotic complexity argument only | Not accepted alone | N/A |

Zhang et al. (TCHES 2025, C908 RISC-V board) show exactly how to execute
the third approach: they provide (a) an instruction count table, (b) a CPI
table measured via microbenchmarks, and (c) a theoretical IPC prediction
that they validate against measured cycles. We replicate this structure
with the tools we have.

### 2.3 Instruction Mix Breakdown — Our Primary Defense

The key insight: a reviewer cannot object to a speedup claim if the
replacement instruction is shown to be **throughput-equivalent or better**
per unit of work.

For GF(2) polynomial multiplication, the unit of work is one
**64×64→128-bit carry-less product** (a GF(2^64) multiplication).

| Implementation | Instructions per 64-bit clmul | Mix |
|---|---|---|
| Reference (`gf2x.c`) | ~64 scalar XOR + shifts per word pair | 100% scalar ALU |
| `karatsuba_ref` | ~8–16 scalar ops per Karatsuba leaf | ~100% scalar ALU/logic |
| `karatsuba_zbc` | 2 (`clmul` + `clmulh`) per leaf | ~100% Zbc multiply |
| `fafft_zbc` | 2 (`clmul` + `clmulh`) per butterfly | ~100% Zbc multiply |
| `fafft_zvbc` (VLEN=256, SEW=64) | 2 vector (`vclmul.vv` + `vclmulh.vv`) per 4 butterflies | 4× parallel Zbc multiply |

The instruction mix breakdown shows that:
1. We are not adding overhead — we are replacing like-for-like (one
   64-bit carry-less multiply per leaf/butterfly), not stacking vector
   overhead on top of scalar work.
2. The reduction is structural, not micro-architectural: `clmul` expresses
   the operation the hardware was meant to accelerate, where the baseline
   implements it as a loop of scalar shifts and XORs.

**How to generate this data in our workflow:**

```bash
# Run Spike with --histogram to get instruction frequency table
$SPIKE --isa=rv64gcv_zbc_zbb_zbkb_zvbc --histogram \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    2> analysis/benchmarks/phase0b/hqc128_polymul_hist.txt

# Compare histograms: baseline vs karatsuba_zbc vs fafft_zvbc
# Key columns to show in paper:
#   - "clmul / clmulh" count (phase 1a+) vs "xor / slli / srli" count (phase 0)
#   - "vclmul.vv / vclmulh.vv" count (phase 1+) vs scalar clmul count (phase 1a)
```

The paper table should show something like:

| Phase | `xor`/`slli`/`srli` | `clmul`/`clmulh` | `vclmul.vv`/`vclmulh.vv` | Total insns | Speedup |
|---|---|---|---|---|---|
| 0 (ref) | 2,400 M | 0 | 0 | 2,600 M | 1.0× |
| 1a (zbc) | ~0 | 180 M | 0 | 210 M | ~12× |
| 1+ (zvbc) | ~0 | ~0 | 45 M | 55 M | ~47× |

*(Numbers are illustrative — actual values from Spike runs.)*

This table is the **single most powerful piece of evidence** for the
claim. A reviewer looking at this cannot argue CPI alone explains away the
speedup: the scalar XOR/shift flood is completely eliminated.

### 2.4 The Throughput-Bound Argument for vclmul

For the Zvbc vectorization step specifically (Phase 1+), we make an
explicit throughput-bound argument:

**Preconditions (verifiable in our code):**
1. `vclmul.vv` processes VLEN/SEW = 256/64 = **4 carry-less products
   simultaneously**.
2. The FAFFT butterfly operations at each level are **data-independent**
   (each butterfly reads different element pairs). There are no loop-carried
   RAW dependencies.
3. Therefore the loop is **throughput-bound**, not latency-bound.

**Throughput argument:**
Under throughput-bound execution, even if `vclmul.vv` has latency L > 1,
the processor can issue subsequent independent `vclmul.vv` instructions
every N cycles (N = throughput CPI). For any reasonable value of N ≤ L,
and given that we execute 4 operations per instruction vs. 1 for scalar,
the throughput ratio is at least 4/N.

For N ≤ 4 (which holds on any pipelined vector unit), the throughput
ratio is ≥ 1 — i.e., we never lose performance vs. scalar. In practice,
published ARM NEON data for `PMULL.P64` (the ARM equivalent) shows
throughput CPI of 1 on Cortex-A72 and Cortex-A55 (source: Arm Cortex-A72
Software Optimization Guide, Table 15), giving a 4× throughput gain at
VLEN=256.

**Supporting evidence chain:**
1. Spike instruction count shows 4× reduction in carry-less multiply
   instructions (measured).
2. llvm-mca on the hot loop shows predicted IPC ≥ 1 for the vclmul block
   (static analysis, minutes to run).
3. ARM NEON `PMULL.P64` analogue shows CPI = 1 on Cortex-A72 (published
   silicon data, conservative bound for any pipelined vector unit).
4. If FPGA flow is completed (see §3.2), measured CSR cycle count on
   CV32E40P-RV32V would provide direct validation.

### 2.5 How Published Papers Handle This

| Paper | Platform | Method | Accepted by reviewers? |
|---|---|---|---|
| Ras et al. 2025 (HQC FAFFT on RISC-V SoC) | CV32E40P FPGA | CSR cycle counter (single run, deterministic FPGA) | Yes — NIST PQC conference |
| Zhang et al. TCHES 2025 (Keccak/NTT on C908 RVV) | Real silicon Linux board | Linux perf events + CPI microbenchmarks + IPC prediction | Yes — TCHES |
| Chen-Chou-Krausz TCHES 2021 (BIKE on Cortex-M4) | STM32F407 at 24 MHz | pqm4 SysTick counter (zero wait-state flash) | Yes — TCHES |
| Becker et al. TCHES 2022 (Neon NTT) | Raspberry Pi 4 (A72) + Apple M1 | PMU cycle counter, 10k iterations | Yes — TCHES |

All papers that use real silicon or FPGA simply measure and report. Papers
working with ISS (like our Spike flow) explicitly label results as
"instruction counts" and supplement with throughput analysis. Zhang et al.
is the gold standard template: provide CPI tables from microbenchmarks,
predict IPC, report both cycles and instructions simultaneously.

---

## 3. Toolchain Analysis & Ranking

All tools are ranked against our three hard constraints:

- **C1: Pure software** — no RTL/hardware modification
- **C2: Ratified extensions** — Zbc, V, Zbb, Zbkb, Zvbc only
- **C3: 3-month deadline** — tooling overhead must be near-zero

### 3.1 Tier 1 — Recommended (use now)

#### Spike ISS — Primary Benchmarking Tool

**Rank: #1 for instruction counting.**

| Attribute | Value |
|---|---|
| Available in lab | Yes (`~/Thesis/installed-tools/bin/spike`) |
| `rdcycle` semantics | Exact retired instruction count (deterministic) |
| Zvbc support | Yes: `--isa=rv64gcv_zbc_zbb_zbkb_zvbc --varch=vlen:256,elen:64` |
| Throughput | ~1–5 M insn/s (very slow — use micro-benchmarks) |
| Deadline risk | Low — already integrated, ISA string confirmed working |

**Why #1:** Spike is the only tool we have that provides a meaningful,
deterministic, reproducible instruction count. `rdcycle` on Spike is
*exactly* the number of retired instructions — not an approximation, not
a DBT artifact. This is the value we report as our primary metric.

**Limitation and mitigation:**
Spike does not model pipeline latency, caches, or branch prediction.
`rdcycle` is not wall-clock cycles. We mitigate this by: (a) labeling
results as instruction counts throughout the paper, (b) supplementing
with llvm-mca throughput analysis, and (c) using the ARM NEON analogue
data as a conservative lower bound for real-hardware cycle performance.

**Usage in workflow:**

```bash
# Tier 2: polymul micro-bench (20–40 min for HQC-128)
$SPIKE --isa=rv64gcv_zbc_zbb_zbkb_zvbc \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_polymul.csv &

# Tier 3: full KEM bench (2–5 h for HQC-128, once per phase milestone)
$SPIKE --isa=rv64gcv_zbc_zbb_zbkb_zvbc \
    build-spike-probe/bench_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_full.csv &

# Instruction histogram (same time as bench, run in parallel)
$SPIKE --isa=rv64gcv_zbc_zbb_zbkb_zvbc --histogram \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    2> analysis/benchmarks/phase0b/hqc128_polymul_hist.txt &
```

---

#### llvm-mca — Static Throughput Analysis

**Rank: #2 for understanding bottlenecks (seconds to run, no hardware).**

| Attribute | Value |
|---|---|
| Available in lab | Yes (`~/Thesis/installed-tools/bin/llvm-mca`, LLVM 21) |
| Purpose | Estimates IPC, resource pressure, dependency stalls |
| Zvbc support | `--mattr=+v,+zbc,+zbkb,+zvbc` |
| Throughput | Runs in seconds on any hot-loop assembly snippet |
| Deadline risk | Zero — no silicon, no simulation |

**Why #2:** llvm-mca answers "is this loop throughput-bound or
latency-bound?" in seconds. It is the standard tool used in academic
crypto papers to annotate assembly listings with IPC predictions. TCHES
reviewers recognize and accept llvm-mca output as supplementary evidence.

**Critical caveat:** llvm-mca uses LLVM's internal machine model. For
`generic-rv64`, the vector and Zvbc extension instructions may not have
accurate latency/throughput data (the generic model often defaults to
latency=1, throughput=1). Interpret the output as a **theoretical
lower-bound IPC** (assuming ideal scheduling), not a hardware measurement.
Always note this limitation in the paper.

**Usage for the Zvbc hot loop:**

```bash
# Step 1: compile the hot function to assembly
$RISCV/bin/clang \
    --target=riscv64-unknown-elf \
    -march=rv64gcv_zbc_zbb_zbkb_zvbc -mabi=lp64d \
    -O3 -ffreestanding \
    -S -o analysis/llvm_mca/reports/fafft_zvbc.s \
    src/poly_mul/fafft_zvbc/fafft_vclmul.c

# Step 2: run MCA on the loop
$RISCV/bin/llvm-mca \
    -march=riscv64 -mcpu=generic-rv64 \
    --mattr=+v,+zbc,+zbkb,+zvbc \
    --timeline --iterations=100 \
    analysis/llvm_mca/reports/fafft_zvbc.s \
    > analysis/llvm_mca/reports/fafft_zvbc_mca.txt
```

Report key metrics: IPC, resource pressure on the multiply unit, and any
throughput stalls flagged in the timeline view.

---

#### QEMU — Correctness Verification

**Rank: #3 for correctness only (not for benchmarking).**

| Attribute | Value |
|---|---|
| Available in lab | Yes (`~/Thesis/installed-tools/bin/qemu-system-riscv64`, v10.2.2) |
| `rdcycle` semantics | **Meaningless** — DBT software counter, not instruction count |
| Zvbc support | Yes: `-cpu rv64,v=true,vlen=256,elen=64,zbkb=true,zbc=true,zvbc=true` |
| Speed | Near-native (~10 s for 100 HQC-128 KAT vectors) |
| Deadline risk | Zero — already working, KAT passes 100/100 all three levels |

**QEMU is our correctness gatekeeper, not our benchmarking tool.**
Run QEMU KAT after every code change. Never use QEMU `rdcycle` values in
the thesis. See GUIDE.md §5.5 for the detailed rationale.

```bash
# After every change — the first command to run
./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf 2>/dev/null
# Expected: PASS 100/100
```

---

### 3.2 Tier 2 — Supporting (supplementary evidence)

#### Xilinx Vivado / Vitis + CV32E40P — Optional FPGA Validation

**Rank: #4 — powerful if time allows, not on the critical path.**

| Attribute | Value |
|---|---|
| Available in lab | Yes (Vivado/Vitis installed) |
| Approach | Synthesize CV32E40P (OpenHW RV32IMCV) onto FPGA; run bare-metal ELF; read CSR cycle counter |
| `rdcycle` semantics | Real pipeline cycles (in-order 4-stage, no branch prediction) |
| Zvbc support | Depends on CV32E40P variant — check if `Zvbc` is included in the core you synthesize |
| Effort | High — requires RTL integration, synthesis, bitstream programming |
| Deadline risk | High — do NOT block on this; treat as bonus if completed in weeks 10–12 |

**Why it matters:** Ras et al. (NIST 2025) use exactly this approach
(CV32E40P on Kintex-7). Their cycle counts are accepted as strong evidence
because FPGA execution is deterministic. If we synthesize the same core
and run our implementation on it, we have a directly comparable, hardware-
validated result that elevates the paper from "ISS instruction count" to
"measured silicon-equivalent cycles."

**Critical blocker to investigate first:** Does the CV32E40P configuration
you synthesize support the `Zvbc` extension? The base OpenHW CV32E40P is
RV32IMCV (with the OpenHW-specific `X-RI5CY` multiply extension). Zvbc
support may require a custom extension or a different core (e.g., CVA6,
which is RV64GC-capable). Spend no more than 2 days investigating before
moving on.

**If Zvbc is unavailable on CV32E40P:** You can still benchmark the
`karatsuba_ref` (scalar) and `karatsuba_zbc` (Zbc) variants on FPGA,
since `Zbc` (`clmul`) is a simpler extension to integrate. This gives you
real cycle counts for the scalar→Zbc transition, with the Zvbc results
remaining at the ISS+llvm-mca level.

---

#### gem5 — Detailed Microarchitecture Simulation

**Rank: #5 — not recommended for this deadline.**

| Attribute | Value |
|---|---|
| Available in lab | Yes |
| Zvbc support | Experimental — gem5's RISC-V model includes basic RVV support (PRs merged 2023–2024), but Zvbc-specific instruction timing is likely absent or generic |
| Effort | Very high — configuring an accurate RISC-V pipeline model with correct Zvbc CPI tables requires expert knowledge of gem5 internals |
| Speed | Orders of magnitude slower than Spike for full-program simulation |
| Deadline risk | **Severe** — do not use gem5 as a primary tool on a 3-month timeline |

**Why gem5 is ranked below Spike here:**
gem5's value is *detailed microarchitecture simulation* — out-of-order
execution, branch prediction, cache hierarchy, DRAM timing. These are
valuable when studying performance on a specific hardware design. But:
1. Our paper is about software optimization, not hardware design. We need
   instruction counts, not microarchitectural timing.
2. gem5's RISC-V support is still maturing; Zvbc pipeline models are not
   validated against any production silicon.
3. Configuration, validation, and running a single gem5 experiment for a
   complex PQC workload can take weeks even for experienced users.

**The only scenario where gem5 is useful here:** If you want to model a
hypothetical in-order RISC-V pipeline (similar to CV32E40P) with a specific
CPI for `vclmul.vv`, gem5 could provide that. But even then, the CPI model
is unvalidated, so the result is no more reliable than a hand-calculation.

**Bottom line:** Skip gem5. Use Spike for instruction counts and FPGA for
cycle counts if time allows.

---

### 3.3 Tier 3 — Avoid / Not Applicable

#### Verilator — RTL Simulation

**Not applicable.** We are not writing RTL. Verilator simulates hardware
described in SystemVerilog/Verilog. Our work is purely C + intrinsics.

Verilator would become relevant only if we designed a custom accelerator
(the ANDROMEDA approach of Ras et al.) — which is explicitly out of scope.

#### Xilinx Vivado HLS / Vitis HLS

**Not applicable.** HLS synthesizes C into RTL. Our optimizations target
the software ISA (RISC-V extensions), not custom hardware datapaths.

#### RISC-V Formal Verification Tools (RVFI, riscv-formal)

**Not applicable.** Formal verification proves hardware correctness. Our
correctness verification is done via NIST KAT vectors (functional testing).

---

## 4. Actionable Workflow — Start Today

### 4.1 Week 1: Establish the Baseline

**Goal:** Have the Phase 0b Spike instruction counts committed before
writing a single line of Phase 1 code.

```bash
# Step 1: Build all three levels for Spike (bench + polymul micro-bench)
cd ~/Thesis/hqc-riscv
source toolchain/toolchain.env

cmake -S . -B build-spike-probe \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1

cmake --build build-spike-probe -j$(nproc)

# Step 2: Launch all three levels in parallel (uses 3 CPU cores)
SPIKE=$RISCV/bin/spike
ISA=rv64gcv_zbc_zbb_zbkb_zvbc
OUT=analysis/benchmarks/phase0b
mkdir -p $OUT

# Full KEM benchmarks (background — 2–20 h per level)
nohup $SPIKE --isa=$ISA \
    build-spike-probe/bench_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_full.csv 2>$OUT/hqc128_full.log &

nohup $SPIKE --isa=$ISA \
    build-spike-probe/bench_hqc192_karatsuba_ref.elf \
    > $OUT/hqc192_full.csv 2>$OUT/hqc192_full.log &

nohup $SPIKE --isa=$ISA \
    build-spike-probe/bench_hqc256_karatsuba_ref.elf \
    > $OUT/hqc256_full.csv 2>$OUT/hqc256_full.log &

# Polymul micro-benchmarks (finish in 20–150 min — results available sooner)
nohup $SPIKE --isa=$ISA --histogram \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_polymul.csv 2>$OUT/hqc128_polymul_hist.txt &

# Step 3: While Spike runs, run llvm-mca on the reference gf2x.c
./analysis/llvm_mca/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c
# Output: analysis/llvm_mca/reports/gf2x_ref_mca.txt

# Step 4: When polymul micro-bench finishes, commit baseline numbers
# (Full KEM bench can continue in background)
git add analysis/benchmarks/phase0b/ analysis/llvm_mca/reports/
git commit -m "feat(phase0b): baseline instruction counts (Spike, karatsuba_ref)"
```

**Deliverable:** A committed `analysis/benchmarks/phase0b/` directory
containing:
- `hqc{128,192,256}_polymul.csv` — `vect_mul` instruction counts
- `hqc{128,192,256}_polymul_hist.txt` — instruction frequency histograms
- `hqc{128,192,256}_full.csv` — full KEM instruction counts (when ready)
- `analysis/llvm_mca/reports/gf2x_ref_mca.txt` — MCA baseline

---

### 4.2 Weeks 2–4: Phase 1a (scalar Zbc Karatsuba)

**Goal:** Replace Karatsuba leaf multiplications with `clmul`/`clmulh`.

```bash
# Edit: src/poly_mul/karatsuba_zbc/gf2x_clmul.c
# Build and verify correctness first (30 s)
cmake --build build --target hqc128_karatsuba_zbc -j$(nproc)
./sim/qemu/run_kat.sh build/hqc128_karatsuba_zbc.elf 2>/dev/null
# Expected: PASS 100/100

# Quick instruction count (20–40 min)
cmake --build build-spike-probe --target bench_polymul_hqc128_karatsuba_zbc
nohup $SPIKE --isa=$ISA --histogram \
    build-spike-probe/bench_polymul_hqc128_karatsuba_zbc.elf \
    > analysis/benchmarks/phase1a/hqc128_polymul.csv \
    2> analysis/benchmarks/phase1a/hqc128_polymul_hist.txt &

# Run llvm-mca on the new implementation
./analysis/llvm_mca/run_mca.sh src/poly_mul/karatsuba_zbc/gf2x_clmul.c
```

**Key metric to verify:** In the histogram output, `clmul` + `clmulh`
should now dominate, while `xor`, `slli`, `srli` counts should drop by
>90%.

---

### 4.3 Weeks 5–8: Phase 1b/1c (FAFFT + hybrid)

**Goal:** FAFFT-based multiplication for sub-quadratic complexity.

Per-change workflow:
1. `cmake --build` → QEMU KAT (30 s) — correctness gate
2. Spike polymul micro-bench (20–40 min background) — instruction count gate
3. llvm-mca on hot FAFFT butterfly loop (seconds) — throughput analysis
4. Commit to `analysis/benchmarks/phase1b/` and `analysis/llvm_mca/reports/`

Full KEM Spike benchmarks only at phase completion (runs overnight).

---

### 4.4 Weeks 9–10: Phase 1+ (Zvbc vectorisation)

**Goal:** Replace scalar `clmul` butterflies with `vclmul.vv`.

```bash
# Build and KAT first (correctness always before benchmarking)
cmake --build build --target hqc128_fafft_zvbc -j$(nproc)
./sim/qemu/run_kat.sh build/hqc128_fafft_zvbc.elf 2>/dev/null

# Run Spike with --histogram to produce the instruction mix table
nohup $SPIKE --isa=$ISA --varch=vlen:256,elen:64 --histogram \
    build-spike-probe/bench_polymul_hqc128_fafft_zvbc.elf \
    > analysis/benchmarks/phase1plus/hqc128_polymul.csv \
    2> analysis/benchmarks/phase1plus/hqc128_polymul_hist.txt &

# Run llvm-mca on the vclmul loop
./analysis/llvm_mca/run_mca.sh src/poly_mul/fafft_zvbc/fafft_vclmul.c
```

**Paper table target at this point:**

```
Phase        | vect_mul insns (HQC-128) | vs baseline | vs karatsuba_zbc
-------------|--------------------------|-------------|------------------
0 (ref)      | XXXM                     | 1.0×        | —
1a (zbc)     | XXXM                     | ~12×        | 1.0×
1b (fafft)   | XXXM                     | ~25×        | ~2×
1+ (zvbc)    | XXXM                     | ~50×        | ~4×
```

*(Target speedup ratios are estimates; actual values depend on Spike runs.)*

---

### 4.5 Weeks 11–12: Thesis results and paper tables

**Goal:** Assemble final tables for the paper.

Mandatory results:
1. Spike instruction counts for `vect_mul` and full KEM — all 3 levels ×
   all implemented variants.
2. Instruction mix histograms showing the scalar→clmul→vclmul transition.
3. llvm-mca IPC estimates for each hot loop.
4. Speedup table vs. NIST reference and vs. Ras et al. (RISC-V FAFFT
   baseline, CV32E40P, no SIMD).

Optional (if FPGA flow completed):
5. CV32E40P FPGA cycle counts for `karatsuba_ref` and `karatsuba_zbc` —
   validates that the instruction count reduction translates to cycle reduction
   on a real in-order pipeline.

**Format for thesis tables:**

```
Table N: HQC instruction counts on RISC-V (Spike ISS, VLEN=256)
          Keypair      Encaps       Decaps
HQC-128
  Phase 0  XXXM         XXXM         XXXM
  Phase 1a XXXM (X.Xx)  XXXM (X.Xx)  XXXM (X.Xx)
  Phase 1b XXXM (X.Xx)  XXXM (X.Xx)  XXXM (X.Xx)
  Phase 1+ XXXM (X.Xx)  XXXM (X.Xx)  XXXM (X.Xx)
...
Note: "instructions" = retired instruction count (Spike ISS rdcycle).
      Speedup in parentheses vs. Phase 0 (karatsuba_ref) baseline.
      Spike ISS models no pipeline; see Table N+1 for throughput analysis.
```

---

## 5. Prior Art Reference Points

The following published results serve as comparison baselines.

### RISC-V (direct comparison)

**Ras, Loiseau, et al. — NIST 6th PQC Conf., 2025**
"Optimizing HQC using Frobenius Additive FFT on a RISC-V-based SoC"
- Platform: CV32E40P (RV32IMC, in-order, no SIMD), Xilinx Kintex-7 FPGA
- Metric: Real CSR cycles (deterministic FPGA)
- HQC-128 Encaps (software FAFFT): **24,882,981 cycles** (3.52× over Karatsuba)
- HQC-128 Encaps (Karatsuba ref on same core): ~87,574,395 cycles
- **Our target:** Exceed the 3.52× FAFFT speedup with Zvbc vectorization
  (targeting ≥ 4× beyond their FAFFT baseline = ≥ 14× over Karatsuba,
  on a comparable instruction-count basis).

### ARM (cross-platform reference)

**Chen, Chiu, Peng, Yang — TCHES 2026(2)**
"Accelerating HQC with Additive FFT"
- Platform: Cortex-A72 (Raspberry Pi 4), Apple M1, x86 Haswell
- Metric: PMU cycle counter, median of 10,000 iterations
- Uses `PMULL.P64` (ARM NEON carry-less multiply = ARM equivalent of `clmul`)

**Chen, Chou, Krausz — TCHES 2021(3)**
"Optimizing BIKE for the Intel Haswell and ARM Cortex-M4"
- Platform: STM32F407 (Cortex-M4 at 24 MHz), pqm4 SysTick counter
- Basis for FAFFT and Karatsuba techniques we reuse for HQC

### RISC-V RVV methodology reference (NTT, not GF(2))

**Zhang, Yan, Huang, Koç — TCHES 2025(1)**
"Optimized Software Implementation of Keccak, Kyber, and Dilithium on RV{32,64}IM{B}{V}"
- Platform: T-Head C908 (VLEN=128, real silicon), Linux perf events
- Key methodology: CPI microbenchmarks + IPC prediction + instruction count table
- Replication of their table format is the template for our §2.3 tables

---

*Last updated: Phase 0b in progress. QEMU KAT clean (100/100 all three
levels). Spike Phase 0b baseline runs queued (background, N_ITER=1).*
