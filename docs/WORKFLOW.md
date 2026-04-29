# HQC-RISC-V: Workflow Runbook

Step-by-step execution guide for building, testing, and benchmarking the HQC
implementation.  For architectural rationale, toolchain rankings, and
methodology decisions, see [`docs/TOOLCHAIN.md`](TOOLCHAIN.md).

All commands assume `cd ~/Thesis/hqc-riscv` and the environment sourced:

```bash
source toolchain/toolchain.env
# exports $RISCV, $QEMU, $SPIKE, prepends $RISCV/bin to PATH
```

---

## Table of Contents

**Part I — Daily Iteration Reference**
- [§0 Setup](#0-setup)
- [§1 Tier 0: Rebuild](#1-tier-0--rebuild-after-any-code-change)
- [§2 Tier 1: QEMU KAT — correctness gate](#2-tier-1--qemu-kat-correctness-gate-10-s)
- [§3 Tier 1b: QEMU bench sanity](#3-tier-1b--qemu-bench-sanity-10-s)
- [§4 Tier 2: Spike polymul micro-bench](#4-tier-2--spike-polymul-micro-bench-2040-min)
- [§5 Tier 3: Full Spike KEM bench](#5-tier-3--full-spike-kem-bench-25-h)
- [§6 Tier 4: llvm-mca static analysis](#6-tier-4--llvm-mca-static-analysis-seconds)
- [§7 Tier 5: Spike --histogram instruction mix](#7-tier-5--spike---histogram-instruction-mix)
- [§8 Quick-reference summary](#8-quick-reference-summary)
- [§9 Measured baseline results](#9-measured-baseline-results)
- [§10 Step-by-step build guide](#10-step-by-step-build-guide)

**Part II — Batch/Orchestration Scripts**
- [§11–16 Batch scripts](#11-batchorchestration-scripts)

**Part III — 12-Week Phase Plan**
- [§17–21 Phase plan](#17-week-1-phase-0b--establish-the-baseline)

**Part IV — Output Files and Results**
- [§22 Output locations](#22-output-file-locations)
- [§23 Reading and interpreting results](#23-reading-and-interpreting-results)

---

# PART I — Daily Iteration Reference

## 0. Setup

```bash
source toolchain/toolchain.env
# RISCV=$HOME/Thesis/installed-tools must be on PATH
```

---

## 1. Tier 0 — Rebuild after any code change

```bash
# KAT build (QEMU backend — for correctness checks)
cmake --build build --target hqc128_karatsuba_ref -j$(nproc)

# Spike bench build (Spike backend — for instruction counts)
cmake --build build-spike \
      --target bench_hqc128_karatsuba_ref \
                bench_polymul_hqc128_karatsuba_ref \
      -j$(nproc)
```

If you modified `CMakeLists.txt` or added source files, reconfigure first:

```bash
# KAT build (QEMU backend)
cmake -S . -B build \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=qemu

# Spike bench build (N_ITER=1 probe)
cmake -S . -B build-spike \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1
```

---

## 2. Tier 1 — QEMU KAT: correctness gate (~10 s)

**Run after every single change.** Runs 100 NIST KAT vectors. Pass before any
benchmarking — never benchmark a broken implementation.

```bash
./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp 2>/dev/null
```

Or use the batch script to check all variants × all levels at once:

```bash
scripts/bench/run_kat_all.sh   # 12 runs, ~2 min total, prints pass/fail table
```

**Expected output:**
```
Running: build/hqc128_karatsuba_ref.elf under QEMU...
PASS [HQC-128]: 100/100 vectors matched
```

**For `fafft_zvbc` (QEMU does not support Zvbc):**

```bash
scripts/bench/spike_kat.sh \
    build/hqc128_fafft_zvbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

RSP file locations:

| Level | RSP file |
|-------|----------|
| 128 | `vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp` |
| 192 | `vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp` |
| 256 | `vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp` |

---

## 3. Tier 1b — QEMU bench sanity (~10 s)

**Sanity check only — not for thesis numbers.**
QEMU uses a DBT/JIT; its `rdcycle` is a software counter unrelated to actual
instruction latency. Use only to confirm the bench harness works and that the
CSV format is correct.

```bash
cmake --build build --target bench_hqc128_karatsuba_ref -j$(nproc)

CPU="rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true"
$QEMU -machine virt -cpu $CPU -m 256M -nographic -bios none \
      -kernel build/bench_hqc128_karatsuba_ref.elf \
      -serial mon:stdio -no-reboot 2>/dev/null
```

**Expected output (numbers are NOT real):**
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,<bogus>,<bogus>,<bogus>
encaps,1,<bogus>,<bogus>,<bogus>
decaps,1,<bogus>,<bogus>,<bogus>
```

---

## 4. Tier 2 — Spike polymul micro-bench (~20–40 min)

**Use after a QEMU-confirmed change to check if `vect_mul` got faster.**
Benchmarks only `vect_mul()` — no keypair/encaps/decaps overhead, no Keccak.
`vect_mul` is >90% of HQC runtime so this is the number that matters most.

```bash
OUT=analysis/benchmarks/phase0b

nohup $RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc \
    build-spike/bench_polymul_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_karatsuba_ref_polymul_N1.csv \
    2>$OUT/hqc128_karatsuba_ref_polymul.log &

echo "PID=$!  — monitor with: cat $OUT/hqc128_karatsuba_ref_polymul_N1.csv"
```

Or use the batch script to launch all variants for a phase:

```bash
scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc
```

**Expected output when done:**
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
vect_mul,1,<count>,<count>,<count>
```

---

## 5. Tier 3 — Full Spike KEM bench (~2–5 h for HQC-128)

**Use only at phase milestones.** Full keypair + encaps + decaps.
This run takes 2–20 h depending on level — launch before leaving for the day.

```bash
OUT=analysis/benchmarks/phase0b

nohup $RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc \
    build-spike/bench_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_karatsuba_ref_N1.csv \
    2>$OUT/hqc128_karatsuba_ref_spike.log &

echo "PID=$!  — monitor with: cat $OUT/hqc128_karatsuba_ref_N1.csv"
```

Or use the batch script:

```bash
scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc --full
```

Check if still running:
```bash
ps aux | grep "[s]pike.*bench_hqc128"
wc -c analysis/benchmarks/phase0b/hqc128_karatsuba_ref_N1.csv  # grows as lines arrive
```

**Expected output when done:**
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,<count>,<count>,<count>
encaps,1,<count>,<count>,<count>
decaps,1,<count>,<count>,<count>
```

---

## 6. Tier 4 — llvm-mca static analysis (seconds)

**Use to estimate throughput/latency without running Spike.**
Runs in seconds on any hot-loop assembly snippet.

```bash
# Analyse a single source file (e.g. the new polymul implementation)
./analysis/llvm_mca/run_mca.sh src/poly_mul/karatsuba_zbc/gf2x_clmul.c
# Report written to: analysis/llvm_mca/reports/gf2x_clmul_mca.txt
```

Key metrics in the output:
- **Throughput** (IPC): instructions per cycle at steady state
- **Timeline**: shows stalls and dependency chains
- **Resource pressure**: which functional units are bottlenecked

Manual invocation (extract and analyse a hot function):

```bash
RISCV=~/Thesis/installed-tools

# Step 1: compile to assembly
$RISCV/bin/clang \
    --target=riscv64-unknown-elf \
    -march=rv64gcv_zbc_zbb_zbkb_zvbc -mabi=lp64d \
    -O3 -ffreestanding -fno-builtin \
    -I vendor/hqc-ref/src/ref \
    -I vendor/hqc-ref/src/ref/hqc-1 \
    -S -o /tmp/gf2x.s \
    vendor/hqc-ref/src/ref/gf2x.c

# Step 2: run MCA
$RISCV/bin/llvm-mca \
    -march=riscv64 -mcpu=sifive-u74 \
    --timeline --iterations=10 \
    /tmp/gf2x.s
```

---

## 7. Tier 5 — Spike `--histogram`: instruction mix

**Which instructions dominate?** Run alongside a bench for free —
`--histogram` adds negligible overhead compared to `--log-commits`.

```bash
nohup $RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc --histogram \
    build-spike/bench_polymul_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_karatsuba_ref_polymul_N1.csv \
    2>analysis/benchmarks/phase0b/hqc128_karatsuba_ref_hist.txt &

# When done — top instructions by frequency:
sort -t$'\t' -k2 -rn \
    analysis/benchmarks/phase0b/hqc128_karatsuba_ref_hist.txt | head -20
```

Or use the batch script:

```bash
scripts/bench/run_histogram_all.sh phase1a           # launch all
scripts/bench/run_histogram_all.sh phase1a --report  # print top-20 per variant
```

---

## 8. Quick-reference summary

| Question | Tool | Time |
|----------|------|------|
| "Does it still produce correct output?" | QEMU KAT (`run_kat_all.sh`) | ~10 s/run |
| "Does fafft_zvbc produce correct output?" | Spike KAT (`spike_kat.sh`) | ~30–90 s |
| "Does the bench harness/CSV work?" | QEMU bench | ~10 s |
| "Did `vect_mul` get faster?" | Spike polymul micro-bench | ~20–40 min |
| "What are the full KEM instruction counts?" | Spike KEM bench | ~2–5 h |
| "What is the static throughput of this loop?" | llvm-mca | seconds |
| "Which instructions dominate?" | Spike --histogram | ~20–40 min |
| "What is the speedup across all variants?" | `extract_results.py` | seconds |
| "Generate a dated report" | `gen_report.sh` | ~1 min |

> **Never use QEMU `rdcycle` values in the thesis.** Only Spike `rdcycle` =
> real instruction count.

---

## 9. Measured baseline results

### Mini-benchmark (`tests/bench/mini_bench.c`)

Three kernels: 2-limb GF(2^128) clmul, RVV 256-byte XOR, scalar XOR-rotate.
Each kernel runs 10 000 iterations.

| Kernel | Simulator | Simulated cycles | Cycles/iter |
|--------|-----------|-----------------|-------------|
| `clmul128` | **Spike** | 110 001 | 11 |
| `clmul128` | QEMU | 9 005 766 | 900 |
| `rvv_xor256` | **Spike** | 150 007 | 15 |
| `rvv_xor256` | QEMU | 215 441 721 | 21 544 |
| `scalar_xor` | **Spike** | 2 560 003 | 8 |
| `scalar_xor` | QEMU | 3 579 642 | 11 |

| Total wall time | Spike | QEMU |
|-----------------|-------|------|
| All 3 kernels | **45 ms** | 180 ms |

Spike is **4× faster** in wall time. Simulated cycle counts differ because
Spike's `mcycle` increments once per committed instruction (IPC=1 in-order
model), while QEMU's cycle counter tracks real host wall-clock cycles.

### Full HQC-128 karatsuba_ref KEM benchmark

| Operation | Spike cycles | Wall time |
|-----------|-------------|-----------|
| KeyGen | 18 816 324 | — |
| Encaps | 37 636 614 | — |
| Decaps | 56 962 430 | — |
| **Total** | — | **1.54 s** |

QEMU KAT (100 vectors): **8.56 s**

Spike is **5.6× faster** than QEMU for this workload. The previous apparent
"360× slowdown" was caused by a missing `_zicntr` extension in the ISA string,
which caused an infinite trap loop in Spike — Spike was never computing.

---

## 10. Step-by-step build guide

### Prerequisites

```bash
source toolchain/toolchain.env
# RISCV=$HOME/Thesis/installed-tools must be on PATH
```

### Build and run the mini-benchmark

```bash
make -f tests/bench/mini_bench.mk

./sim/spike/run_bench.sh build-mini/mini_bench_spike.elf

$RISCV/bin/qemu-system-riscv64 \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
    -m 256M -nographic -bios none \
    -kernel build-mini/mini_bench_qemu.elf \
    -serial mon:stdio -no-reboot 2>/dev/null
```

### Run the full HQC-128 benchmark on Spike

```bash
cmake -S . -B build-spike \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=spike
cmake --build build-spike --target bench_hqc128_karatsuba_ref -j$(nproc)

./sim/spike/run_bench.sh build-spike/bench_hqc128_karatsuba_ref.elf
```

### Run the KAT on QEMU

```bash
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=qemu
cmake --build build -j$(nproc)

./tests/kat/run_kat.sh build/hqc128_karatsuba_ref.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

### Run all five polymul variants

```bash
for variant in karatsuba_ref karatsuba_zbc fafft_zbc hybrid_zbc fafft_zvbc; do
    echo "=== $variant ==="
    ./sim/spike/run_bench.sh \
        build-spike/bench_hqc128_${variant}.elf 2>/dev/null
done
```

### Rebuild Spike with optimized host compiler (optional, +15–30% throughput)

```bash
bash toolchain/build-spike.sh
```

Rebuilds from `$HOME/Thesis/src/riscv-isa-sim` with `-O3 -march=native`,
strips debug symbols, and reinstalls to `$RISCV/bin/spike`.

---

# PART II — Batch/Orchestration Scripts

All batch scripts use the same conventions as the codebase:
`#!/usr/bin/env bash`, `set -euo pipefail`, `RISCV="${RISCV:-$HOME/Thesis/installed-tools}"`,
`SCRIPT_DIR`/`REPO_ROOT` from `BASH_SOURCE[0]`.

## 11. `scripts/bench/run_kat_all.sh` — batch QEMU KAT

Runs KAT for all 4 QEMU-compatible variants × 3 security levels (12 runs).
Delegates to `tests/kat/run_kat.sh`. Prints a 4×3 pass/fail table.

```bash
# Check all variants / levels at once
scripts/bench/run_kat_all.sh

# Expected output:
# KAT results  karatsuba_ref  karatsuba_zbc  fafft_zbc  hybrid_zbc
# HQC-128      PASS           PASS           PASS        PASS
# HQC-192      PASS           PASS           PASS        PASS
# HQC-256      PASS           PASS           PASS        PASS
# NOTE: fafft_zvbc skipped (Zvbc not supported by QEMU).
#       Run: scripts/bench/spike_kat.sh build/hqc128_fafft_zvbc.elf <rsp>
```

Exits non-zero if any run failed. Does NOT abort on first failure —
all 12 runs complete and the full table is always printed.

---

## 12. `scripts/bench/spike_kat.sh` — KAT under Spike (for fafft_zvbc)

Runs a single KAT ELF under Spike and checks its output against the reference
.rsp. Use for `fafft_zvbc` which requires Zvbc (unsupported by QEMU's
system-mode emulation).

```bash
scripts/bench/spike_kat.sh \
    build/hqc128_fafft_zvbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp

# HQC-192:
scripts/bench/spike_kat.sh \
    build/hqc192_fafft_zvbc.elf \
    vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp
```

Timing: HQC-128 ~10–30 s, HQC-256 ~60–120 s (depending on fafft_zvbc speedup).

---

## 13. `scripts/bench/run_spike_bench_phase.sh` — launch all bench jobs for a phase

Launches Spike benchmark jobs in the background (`nohup`) for all variants of
a given phase. Prints PIDs and monitoring hints.

```bash
# Polymul micro-bench only (default, ~20–150 min each, fast feedback)
scripts/bench/run_spike_bench_phase.sh phase1a

# Filter to specific variant(s)
scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc

# Also launch full KEM bench (use at phase milestones, runs overnight)
scripts/bench/run_spike_bench_phase.sh phase1a --full

# Full KEM bench only (skip polymul)
scripts/bench/run_spike_bench_phase.sh phase1a --full --no-polymul
```

Output files: `analysis/benchmarks/<phase>/hqc{level}_{variant}_polymul_N1.csv`
and (with `--full`) `analysis/benchmarks/<phase>/hqc{level}_{variant}_N1.csv`.

After launch, the script prints:
```
[bench] Launched 3 polymul jobs for phase1a:
  PID 12345  hqc128_karatsuba_zbc_polymul  → ...
  PID 12346  hqc192_karatsuba_zbc_polymul  → ...

Monitor:
  watch -n 30 "wc -c analysis/benchmarks/phase1a/*.csv"
  ps aux | grep '[s]pike.*bench_polymul'
```

PIDs recorded to `analysis/benchmarks/<phase>/pids.txt`.

---

## 14. `scripts/bench/run_histogram_all.sh` — batch Spike histogram

**`--launch` mode (default):** Runs `spike --histogram` for all matching
polymul ELFs. Both the CSV (stdout) and histogram (stderr) are captured in
a single Spike run, so you get instruction counts and instruction mix
simultaneously.

```bash
# Launch all polymul histograms for a phase
scripts/bench/run_histogram_all.sh phase1a

# Filter to one variant
scripts/bench/run_histogram_all.sh phase1a --variant karatsuba_zbc
```

**`--report` mode:** Reads existing `*_hist.txt` files and prints sorted
top-20 instruction frequency blocks. No Spike needed — safe to run any time.

```bash
scripts/bench/run_histogram_all.sh phase1a --report

# Example output:
# === hqc128_karatsuba_zbc (phase1a) ===
#   clmul          1,234,567
#   clmulh         1,234,234
#   addi             456,789
#   ...
```

---

## 15. `scripts/report/extract_results.py` — parse all CSVs, generate tables

Reads all `analysis/benchmarks/phase*/` CSV files, computes speedups vs
baseline, and prints comparison tables.

```bash
# Markdown table for all phases and levels
python3 scripts/report/extract_results.py

# Filter to one phase
python3 scripts/report/extract_results.py --phase phase1a

# LaTeX tabular (redirect to .tex file)
python3 scripts/report/extract_results.py --latex \
    > analysis/report/table_results.tex

# Include instruction mix from histogram files
python3 scripts/report/extract_results.py --mix

# Write cycle_counts.csv + instruction_mix.csv for plot.py
python3 scripts/report/extract_results.py --write-summary-csv

# Full options
python3 scripts/report/extract_results.py --help
```

CSV parsing reads the `# level=NNN,polymul=VARIANT` comment header as source
of truth (not the filename). Uses `median_cycles` as primary metric.

`--write-summary-csv` writes `analysis/benchmarks/cycle_counts.csv` (columns:
`impl,hqc128,hqc192,hqc256`) and `analysis/benchmarks/instruction_mix.csv`
in the exact format expected by `analysis/benchmarks/plot.py` — no changes to
`plot.py` needed.

---

## 16. `scripts/report/gen_report.sh` — master report generator

Orchestrates `extract_results.py` → `plot.py` → `amdahl.py` and assembles a
dated Markdown report.

```bash
scripts/report/gen_report.sh

# Restrict to one phase
scripts/report/gen_report.sh --phase phase1a

# Custom vect_mul fraction for Amdahl (default: 0.90)
scripts/report/gen_report.sh --fraction 0.85
```

**Output:** `analysis/report/report_YYYY-MM-DD.md` containing:
- Date, git SHA, phases found
- Instruction count comparison tables
- Paths to generated PDF figures in `docs/figures/`
- Amdahl's law projections
- Next-steps checklist stub

Soft-fail design: each sub-command is optional; the report is always written
even if individual steps fail (they appear in the report as SKIPPED with the
error message).

---

# PART III — 12-Week Phase Plan

## 17. Week 1 — Phase 0b: Establish the baseline

**Goal:** Commit Phase 0b Spike instruction counts before writing Phase 1 code.

```bash
cd ~/Thesis/hqc-riscv
source toolchain/toolchain.env

cmake -S . -B build-spike \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1
cmake --build build-spike -j$(nproc)

# Launch all three levels in background
scripts/bench/run_spike_bench_phase.sh phase0b --variant karatsuba_ref
scripts/bench/run_histogram_all.sh phase0b --variant karatsuba_ref

# While Spike runs, run llvm-mca on the reference
./analysis/llvm_mca/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c
# Output: analysis/llvm_mca/reports/gf2x_mca.txt

# When polymul micro-bench finishes, commit baseline numbers
git add analysis/benchmarks/phase0b/ analysis/llvm_mca/reports/
git commit -m "feat(phase0b): baseline instruction counts (Spike, karatsuba_ref)"
```

**Deliverable:** `analysis/benchmarks/phase0b/` with:
- `hqc{128,192,256}_karatsuba_ref_polymul_N1.csv` — `vect_mul` instruction counts
- `hqc{128,192,256}_karatsuba_ref_hist.txt` — instruction frequency histograms
- `hqc{128,192,256}_karatsuba_ref_N1.csv` — full KEM instruction counts (when ready)
- `analysis/llvm_mca/reports/gf2x_mca.txt` — MCA baseline

---

## 18. Weeks 2–4 — Phase 1a: scalar Zbc Karatsuba

**Goal:** Replace Karatsuba leaf multiplications with `clmul`/`clmulh`.

```bash
# Edit: src/poly_mul/karatsuba_zbc/gf2x_clmul.c
cmake --build build --target hqc128_karatsuba_zbc -j$(nproc)
./tests/kat/run_kat.sh build/hqc128_karatsuba_zbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp 2>/dev/null
# Expected: PASS 100/100

scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc
./analysis/llvm_mca/run_mca.sh src/poly_mul/karatsuba_zbc/gf2x_clmul.c
```

**Key metric:** In histogram output, `clmul` + `clmulh` should dominate, while
`xor`, `slli`, `srli` counts should drop by >90%.

---

## 19. Weeks 5–8 — Phase 1b/1c: FAFFT + hybrid

**Goal:** FAFFT-based multiplication for sub-quadratic complexity.

Per-change workflow:
1. `cmake --build` → QEMU KAT (30 s) — correctness gate
2. `scripts/bench/run_spike_bench_phase.sh phase1b --variant fafft_zbc` — instruction count gate
3. `./analysis/llvm_mca/run_mca.sh src/poly_mul/fafft_zbc/fafft.c` — throughput analysis
4. Commit to `analysis/benchmarks/phase1b/` and `analysis/llvm_mca/reports/`

Full KEM Spike benchmarks only at phase completion (run overnight with `--full`).

---

## 20. Weeks 9–10 — Phase 1+: Zvbc vectorisation

**Goal:** Replace scalar `clmul` butterflies with `vclmul.vv`.

```bash
cmake --build build --target hqc128_fafft_zvbc -j$(nproc)
scripts/bench/spike_kat.sh build/hqc128_fafft_zvbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp

scripts/bench/run_histogram_all.sh phase1plus --variant fafft_zvbc
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

---

## 21. Weeks 11–12: Thesis tables and paper

**Mandatory results:**
1. Spike instruction counts for `vect_mul` and full KEM — all 3 levels × all variants.
2. Instruction mix histograms showing the scalar→clmul→vclmul transition.
3. llvm-mca IPC estimates for each hot loop.
4. Speedup table vs. NIST reference and vs. Ras et al. (RISC-V FAFFT baseline).

**Generate all tables:**

```bash
python3 scripts/report/extract_results.py --latex \
    > analysis/report/table_results.tex
scripts/report/gen_report.sh
```

**Table format for thesis:**

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

---

# PART IV — Output Files and Results

## 22. Output file locations

### Build artifacts

| Build directory | Content | Used for |
|---|---|---|
| `build/` | KAT ELFs (`hqc{128,192,256}_<variant>.elf`) | QEMU correctness gate |
| `build-spike/` | Bench ELFs (`bench_*.elf`, `bench_polymul_*.elf`) | Spike instruction counts |
| `build-mini/` | Mini-bench ELFs (built via `make -f tests/bench/mini_bench.mk`) | Quick smoke test |

### Benchmark results

| Path | Content | Generated by |
|---|---|---|
| `analysis/benchmarks/phase0b/` | Phase 0b (baseline) CSV + histogram files | `run_spike_bench_phase.sh` |
| `analysis/benchmarks/phase1a/` | Phase 1a (karatsuba_zbc) results | `run_spike_bench_phase.sh` |
| `analysis/benchmarks/<phase>/*_polymul_N1.csv` | `vect_mul` instruction counts | Spike |
| `analysis/benchmarks/<phase>/*_N1.csv` | Full KEM instruction counts | Spike |
| `analysis/benchmarks/<phase>/*_hist.txt` | Instruction frequency histogram | Spike `--histogram` |
| `analysis/benchmarks/cycle_counts.csv` | Summary table for `plot.py` | `extract_results.py --write-summary-csv` |
| `analysis/benchmarks/instruction_mix.csv` | Instruction mix table for `plot.py` | `extract_results.py --write-summary-csv` |

### Reports and graphs

| Path | Content | Generated by |
|---|---|---|
| `analysis/report/report_YYYY-MM-DD.md` | Dated Markdown report | `gen_report.sh` |
| `analysis/report/table_results_YYYY-MM-DD.tex` | LaTeX tables for thesis | `extract_results.py --latex` |
| `docs/figures/cycle_counts.pdf` | Bar chart: cycles per implementation | `plot.py` |
| `docs/figures/instruction_mix.pdf` | Stacked bar chart: instruction categories | `plot.py` |
| `analysis/llvm_mca/reports/<source>_mca.txt` | llvm-mca throughput report | `run_mca.sh` |

---

## 23. Reading and interpreting results

### CSV benchmark files (`*_N1.csv`)

Each file has a metadata comment header followed by CSV rows:

```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
vect_mul,1,18234567,18234889,18298123
```

- **`op`**: the benchmarked operation (`vect_mul`, `keypair`, `encaps`, `decaps`)
- **`median_cycles`**: the primary metric — Spike retired instruction count
  (n_iter=1 so min/median/max are effectively identical unless multiple runs)
- **Do NOT use these numbers from QEMU builds** — QEMU `rdcycle` is a software
  counter unrelated to instruction count

### Histogram files (`*_hist.txt`)

Tab-separated `<instruction>\t<count>` lines, sorted by Spike internally:

```
xori    234567890
addi    198234567
clmul   180000000
```

Use `sort -t$'\t' -k2 -rn <file> | head -20` to see the top instructions.
Interpret the histogram as: which instructions consumed the most issue slots?
The goal is to see `clmul`/`clmulh` dominate in Phase 1a, and
`vclmul.vv`/`vclmulh.vv` dominate in Phase 1+.

### Speedup tables (from `extract_results.py`)

```
## `vect_mul` instruction counts — HQC-128

| Phase    |   karatsuba_ref |   karatsuba_zbc |      fafft_zbc |
|----------|-----------------|-----------------|----------------|
| phase0b  |  18.2 M (1.0×) |             —   |             —  |
| phase1a  |             —   |   1.5 M (12.1×) |             —  |
```

- Values in parentheses are speedup vs. the baseline (`karatsuba_ref`)
- `—` means no data exists for that (phase, variant) combination
- `M` = millions of retired instructions

### PDF figures

- **`cycle_counts.pdf`**: grouped bar chart showing instruction counts (y-axis)
  for each implementation (x-axis grouped), with one cluster per security level
  (HQC-128/192/256). Lower is faster.
- **`instruction_mix.pdf`**: grouped bar chart showing the percentage breakdown
  of instruction categories per implementation. Look for the shift from
  `scalar_alu` to `clmul/clmulh` to `vclmul/vclmulh` across phases.

### Amdahl's law projections

```bash
python3 analysis/benchmarks/amdahl.py --fraction 0.90 --speedup 12.0
# Parallel fraction : 90.0%
# Section speedup   : 12.0x
# Overall speedup   : 7.895x
# Theoretical limit : 10.000x
```

Interpret as: if `vect_mul` is 90% of total KEM runtime and we speed it up
12×, the full KEM speedup is ~7.9×. The theoretical limit (all of `vect_mul`
infinitely fast) is 10×.

---

*Last updated: 2026-04-29. Methodology split to docs/TOOLCHAIN.md.*
