# HQC-RISC-V: Toolchain & Benchmarking Methodology

Architectural rationale, toolchain rankings, and methodology decisions for the
HQC thesis project.  For day-to-day commands and build steps, see
[`docs/Workflow.md`](Workflow.md).

---

## Table of Contents

- [§11 Primary metric — retired instruction count](#11-primary-metric-retired-instruction-count)
- [§12 The CPI / Vector Dilemma](#12-the-cpi--vector-dilemma)
- [§13 Tool ranking](#13-tool-ranking)
- [§25 Prior art reference points](#25-prior-art-reference-points)

---

# PART II — Benchmarking Methodology

## 11. Primary metric: retired instruction count

This is a **pure software optimization paper**. Our contributions are
algorithmic and ISA-level: replacing scalar GF(2) polynomial multiplication
with `clmul`/`clmulh` (Zbc) and `vclmul.vv`/`vclmulh.vv` (Zvbc).

Since we run on **Spike ISS** (the only available RISC-V simulator that
provides a meaningful `rdcycle`), our primary metric is **retired instruction
count** — not wall-clock time, not pipeline cycles.

This is explicitly acceptable in the PQC community. The pqm4 project
contrasts real-hardware measurements with `pqriscv`'s ISS-based approach,
noting ISS results are presented as instruction counts. Spike's `rdcycle =
retired instruction count` (no pipeline model, no cache, no branch predictor).

> **Claim to reviewers:** "We reduce the retired instruction count of
> `vect_mul` by X× for HQC-128/192/256, which represents ≥90% of total
> KEM runtime (verified by profiling). Under any throughput-bound execution
> model, this translates directly to a proportional cycle-time reduction."

**Secondary metric:** instruction mix breakdown — demonstrates the replacement
instructions are not pathologically expensive (see §12.3).

**What we do NOT claim:**
- Wall-clock time on real hardware (no RISC-V Linux board with Zvbc in lab).
- Exact cycle counts with pipeline effects (no RTL model with Zvbc timing).
- Better performance than ARM Cortex-M4/A72 (different ISA).

---

## 12. The CPI / Vector Dilemma

### 12.1 The core problem

When a reviewer sees:

> "We replaced 8 scalar `clmul` instructions (each CPI ≈ 1) with 2
> `vclmul.vv` instructions (CPI unknown, likely ≥ 4 per element on most
> pipelines)."

...they will ask: *"How do you know the net cycle count decreased? The vector
instruction may have higher CPI."*

This is the **CPI dilemma**: instruction count decreased, but cycles may not
have if the replacement instructions are proportionally more expensive.

### 12.2 The accepted argument structure

The PQC benchmarking community (TCHES, CHES, NIST PQC conferences) accepts the
following chain of evidence, in decreasing strength:

| Evidence type | Strength | Available to us? |
|---|---|---|
| Direct cycle measurement on real silicon (perf PMU) | Gold standard | No — no Zvbc board in lab |
| Cycle measurement on FPGA-synthesized core (CSR counter) | Strong | Possible (Vivado, see §13.4) |
| Instruction count on Spike ISS + throughput argument + llvm-mca | Accepted | **Yes — our primary approach** |
| Asymptotic complexity argument only | Not accepted alone | N/A |

Zhang et al. (TCHES 2025, C908 RISC-V board) show exactly how to execute the
third approach: (a) an instruction count table, (b) a CPI table from
microbenchmarks, and (c) a theoretical IPC prediction validated against
measured cycles. We replicate this structure with the tools we have.

### 12.3 Instruction mix breakdown — our primary defense

A reviewer cannot object to a speedup claim if the replacement instruction is
shown to be **throughput-equivalent or better** per unit of work.

For GF(2) polynomial multiplication, the unit of work is one
**64×64→128-bit carry-less product** (a GF(2^64) multiplication).

| Implementation | Instructions per 64-bit clmul | Mix |
|---|---|---|
| `karatsuba_ref` | ~64 scalar XOR + shifts per word pair | ~100% scalar ALU |
| `karatsuba_zbc` | 2 (`clmul` + `clmulh`) per Karatsuba leaf | ~100% Zbc multiply |
| `fafft_zbc` | 2 (`clmul` + `clmulh`) per FAFFT butterfly | ~100% Zbc multiply |
| `fafft_zvbc` (VLEN=256, SEW=64) | 2 vector ops per 4 butterflies | 4× parallel Zbc multiply |

This table shows we are not adding overhead — we are replacing like-for-like.
The reduction is structural: `clmul` expresses the operation the hardware was
meant to accelerate, where the baseline implements it as a loop of shifts and XORs.

**How to generate this data:**

```bash
# Spike --histogram gives instruction frequency table
$RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc --histogram \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_karatsuba_ref_polymul_N1.csv \
    2> analysis/benchmarks/phase0b/hqc128_karatsuba_ref_hist.txt

# Or batch:
scripts/bench/run_histogram_all.sh phase0b
scripts/bench/run_histogram_all.sh phase0b --report
```

**Paper table target:**

| Phase | `xor`/`slli`/`srli` | `clmul`/`clmulh` | `vclmul.vv`/`vclmulh.vv` | Total insns | Speedup |
|---|---|---|---|---|---|
| 0 (ref) | 2,400 M | 0 | 0 | 2,600 M | 1.0× |
| 1a (zbc) | ~0 | 180 M | 0 | 210 M | ~12× |
| 1+ (zvbc) | ~0 | ~0 | 45 M | 55 M | ~47× |

*(Numbers are illustrative — actual values from Spike runs.)*

This table is the **single most powerful piece of evidence** in the paper.

### 12.4 The throughput-bound argument for vclmul

For Phase 1+ specifically, we make an explicit throughput-bound argument:

**Preconditions (verifiable in code):**
1. `vclmul.vv` processes VLEN/SEW = 256/64 = **4 carry-less products simultaneously**.
2. FAFFT butterfly operations are **data-independent** — no loop-carried RAW dependencies.
3. Therefore the loop is **throughput-bound**, not latency-bound.

**Argument:** Under throughput-bound execution, even if `vclmul.vv` has latency
L > 1, the processor can issue subsequent independent `vclmul.vv` instructions
every N cycles. For any N ≤ 4, the throughput ratio is at least 4/N ≥ 1 —
i.e., we never lose performance vs. scalar.

**Supporting evidence chain:**
1. Spike instruction count shows 4× reduction in carry-less multiply instructions.
2. llvm-mca on the hot loop shows predicted IPC ≥ 1 for the vclmul block.
3. ARM NEON `PMULL.P64` analogue shows CPI = 1 on Cortex-A72 (Arm Software
   Optimization Guide, Table 15) — conservative bound for any pipelined vector unit.
4. If FPGA flow is completed (see §13.4), measured CSR cycle count on CV32E40P
   would provide direct validation.

### 12.5 How published papers handle this

| Paper | Platform | Method | Accepted? |
|---|---|---|---|
| Ras et al. 2025 (HQC FAFFT on RISC-V SoC) | CV32E40P FPGA | CSR cycle counter (deterministic FPGA) | Yes — NIST PQC |
| Zhang et al. TCHES 2025 (Keccak/NTT on C908 RVV) | Real silicon Linux board | Linux perf + CPI microbenchmarks + IPC prediction | Yes — TCHES |
| Chen-Chou-Krausz TCHES 2021 (BIKE on Cortex-M4) | STM32F407 at 24 MHz | pqm4 SysTick counter | Yes — TCHES |
| Becker et al. TCHES 2022 (Neon NTT) | Raspberry Pi 4 (A72) + Apple M1 | PMU cycle counter, 10k iterations | Yes — TCHES |

Papers working with ISS explicitly label results as "instruction counts" and
supplement with throughput analysis. Zhang et al. is the gold standard template.

---

## 13. Tool ranking

All tools ranked against our three constraints:
- **C1: Pure software** — no RTL/hardware modification
- **C2: Ratified extensions** — Zbc, V, Zbb, Zbkb, Zvbc only
- **C3: 3-month deadline** — tooling overhead must be near-zero

### 13.1 Spike — rank 1 (primary instruction counting)

| Attribute | Value |
|---|---|
| Available | Yes (`~/Thesis/installed-tools/bin/spike`) |
| `rdcycle` semantics | Exact retired instruction count (deterministic) |
| Zvbc support | Yes: `--isa=rv64gcv_zbc_zbb_zbkb_zvbc --varch=vlen:256,elen:64` |
| Throughput | ~1–5 M insn/s (use micro-benchmarks to keep runs short) |

**Why #1:** Spike is the only tool providing a meaningful, deterministic,
reproducible instruction count. `rdcycle = retired instructions` — exactly the
value we report as our primary metric.

**Limitation:** Spike does not model pipeline latency, caches, or branch
prediction. We mitigate by: (a) labeling results as instruction counts
throughout the paper, (b) supplementing with llvm-mca, (c) using ARM NEON
analogue data as a conservative lower bound.

```bash
# Polymul micro-bench (20–40 min for HQC-128)
nohup $RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_karatsuba_ref_polymul_N1.csv &

# Full KEM bench (2–5 h, run at phase milestones)
nohup $RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc \
    build-spike-probe/bench_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_karatsuba_ref_N1.csv &

# Instruction histogram (same run as bench, free with --histogram)
nohup $RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc --histogram \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_karatsuba_ref_polymul_N1.csv \
    2> analysis/benchmarks/phase0b/hqc128_karatsuba_ref_hist.txt &
```

### 13.2 llvm-mca — rank 2 (static throughput)

| Attribute | Value |
|---|---|
| Available | Yes (`~/Thesis/installed-tools/bin/llvm-mca`, LLVM 21) |
| Purpose | Estimates IPC, resource pressure, dependency stalls |
| Zvbc support | `--mattr=+v,+zbc,+zbkb,+zvbc` |
| Speed | Seconds on any hot-loop assembly |

**Why #2:** llvm-mca answers "is this loop throughput-bound or latency-bound?"
in seconds. TCHES reviewers recognize and accept llvm-mca output as
supplementary evidence.

**Caveat:** For `generic-rv64`, vector and Zvbc instructions may not have
accurate latency/throughput data (generic model often defaults to
latency=1, throughput=1). Interpret output as a **theoretical lower-bound IPC**
(ideal scheduling), not a hardware measurement. Note this limitation in the paper.

```bash
# Analyse the Zvbc hot loop
./analysis/llvm_mca/run_mca.sh src/poly_mul/fafft_zvbc/fafft_vclmul.c 128
# Report: analysis/llvm_mca/reports/fafft_vclmul_mca.txt
```

### 13.3 QEMU — rank 3 (correctness only, NOT for benchmarking)

| Attribute | Value |
|---|---|
| Available | Yes (`~/Thesis/installed-tools/bin/qemu-system-riscv64`, v10.2.2) |
| `rdcycle` semantics | **Meaningless** — DBT software counter, not instruction count |
| Zvbc support | Yes: `-cpu rv64,v=true,vlen=256,elen=64,zbkb=true,zbc=true,zvbc=true` |
| Speed | Near-native (~10 s for 100 HQC-128 KAT vectors) |

**QEMU is our correctness gatekeeper, not our benchmarking tool.**
Run QEMU KAT after every code change. **Never use QEMU `rdcycle` values in
the thesis.**

```bash
./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp 2>/dev/null
# Expected: PASS [HQC-128]: 100/100 vectors matched
```

### 13.4 FPGA — optional rank 4 (supplementary evidence)

| Attribute | Value |
|---|---|
| Available | Yes (Vivado/Vitis installed) |
| Approach | Synthesize CV32E40P (OpenHW RV32IMCV) onto FPGA; read CSR cycle counter |
| `rdcycle` semantics | Real pipeline cycles (in-order 4-stage) |
| Effort | High — RTL integration, synthesis, bitstream programming |
| Deadline risk | High — **do NOT block on this** |

Ras et al. (NIST 2025) use exactly this approach (CV32E40P on Kintex-7). If
completed, measured CSR cycle count elevates the paper from "ISS instruction
count" to "hardware-validated cycles."

**Critical blocker:** Does your CV32E40P configuration support `Zvbc`? The base
OpenHW CV32E40P is RV32IMCV. Zvbc may require a custom extension or a
different core. Spend no more than 2 days investigating before moving on.

If Zvbc is unavailable: benchmark `karatsuba_ref` (scalar) and `karatsuba_zbc`
(Zbc) on FPGA, giving real cycle counts for the scalar→Zbc transition.

### 13.5 gem5 / Verilator / HLS — skip

**gem5 (rank 5 — not recommended):** gem5's RISC-V support is still maturing;
Zvbc pipeline models are not validated against any production silicon.
Configuration and validation for a complex PQC workload can take weeks.
Our paper is about software optimization, not hardware design — we need
instruction counts, not microarchitectural timing. Skip gem5.

**Verilator, Vivado HLS, RISC-V formal verification tools:** Not applicable.
We are not writing RTL or custom hardware. Correctness verification is done
via NIST KAT vectors. These tools would become relevant only if we designed a
custom accelerator — which is explicitly out of scope.

---

# PART V — Prior Art Reference Points

## 25. Prior art reference points

### RISC-V (direct comparison)

**Ras, Loiseau, et al. — NIST 6th PQC Conf., 2025**
"Optimizing HQC using Frobenius Additive FFT on a RISC-V-based SoC"
- Platform: CV32E40P (RV32IMC, in-order, no SIMD), Xilinx Kintex-7 FPGA
- Metric: Real CSR cycles (deterministic FPGA)
- HQC-128 Encaps (software FAFFT): **24,882,981 cycles** (3.52× over Karatsuba)
- HQC-128 Encaps (Karatsuba ref on same core): ~87,574,395 cycles
- **Our target:** Exceed the 3.52× FAFFT speedup with Zvbc vectorization
  (targeting ≥ 4× beyond their FAFFT baseline = ≥ 14× over Karatsuba,
  on a comparable instruction-count basis)

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
- Replication of their table format is the template for our §12.3 tables

---

*Last updated: 2026-04-29. Split from docs/Workflow.md (previously merged 2026-04-27).*
