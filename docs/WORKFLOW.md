# HQC-RISC-V: Workflow Runbook (Consolidated)

Step-by-step execution guide for building, testing, and benchmarking the HQC
implementation. For architectural rationale, toolchain rankings, and
methodology decisions, see [`docs/TOOLCHAIN.md`](TOOLCHAIN.md).

All commands assume `cd ~/Thesis/hqc-riscv` and the environment sourced:

```bash
source toolchain/toolchain.env
# exports $RISCV, $QEMU, prepends $RISCV/bin to PATH
```

---

## Table of Contents

**Part I — Daily Iteration Reference**
- [§0 Setup](#0-setup)
- [§1 Tier 0: Rebuild after code change](#1-tier-0--rebuild-after-any-code-change)
- [§2 Tier 1: QEMU KAT — correctness gate](#2-tier-1--qemu-kat-correctness-gate-10-s)
- [§3 Tier 1b: QEMU bench sanity](#3-tier-1b--qemu-bench-sanity-10-s)
- [§4 Tier 2: Spike polymul micro-bench](#4-tier-2--spike-polymul-micro-bench-1-s)
- [§5 Tier 3: Full Spike KEM bench](#5-tier-3--full-spike-kem-bench-1-5-s-per-level)
- [§6 Tier 4: llvm-mca static analysis](#6-tier-4--llvm-mca-static-analysis-seconds)
- [§7 Tier 5: Spike --histogram instruction mix](#7-tier-5--spike---histogram-instruction-mix-1-s)
- [§8 Quick-reference decision table](#8-quick-reference-decision-table)

**Part II — Batch & Orchestration Scripts**
- [§9 Batch KAT testing](#9-batch-kat-testing)
- [§10 Spike KAT (for Zvbc variants)](#10-spike-kat-for-zvbc-variants)
- [§11 Batch Spike benchmark jobs](#11-batch-spike-benchmark-jobs)
- [§12 Batch Spike histogram](#12-batch-spike-histogram)
- [§13 Parse and compare results](#13-parse-and-compare-results)
- [§14 Generate dated report](#14-generate-dated-report)

**Part III — Reference & Interpretation**
- [§15 Measured baseline results](#15-measured-baseline-results)
- [§16 Output file locations & interpretation](#16-output-file-locations--interpretation)
- [§17 Step-by-step build guide](#17-step-by-step-build-guide)

---

# PART I — Daily Iteration Reference

## 0. Setup

```bash
source toolchain/toolchain.env
# RISCV=$HOME/Thesis/installed-tools must be on PATH
```

---

## 1. Tier 0 — Rebuild after any code change

After modifying C source files, rebuild the relevant targets:

```bash
# QEMU KAT build (for correctness checks)
cmake --build build --target hqc128_karatsuba_ref -j$(nproc)

# Spike bench build (for instruction counts)
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

# Spike bench build (with N_ITER=1 for fast probing)
cmake -S . -B build-spike \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1
```

---

## 2. Tier 1 — QEMU KAT: correctness gate (~10 s)

**Run after every single change.** Validates against 100 NIST KAT vectors.
Never benchmark a broken implementation.

```bash
./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp 2>/dev/null
```

**Expected output:**
```
Running: build/hqc128_karatsuba_ref.elf under QEMU...
PASS [HQC-128]: 100/100 vectors matched
```

**For fafft_zvbc variant (requires Zvbc, unsupported by QEMU):**

```bash
scripts/bench/spike_kat.sh \
    build/hqc128_fafft_zvbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

**Quick test all variants + levels at once** (see §9 for batch script details):

```bash
scripts/bench/run_kat_all.sh   # 12 runs, ~2 min total
```

**RSP file locations:**

| Level | RSP file |
|-------|----------|
| 128 | `vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp` |
| 192 | `vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp` |
| 256 | `vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp` |

---

## 3. Tier 1b — QEMU bench sanity (~10 s)

**For development only.** QEMU's `rdcycle` is a software counter, not actual
instruction latency. Use only to confirm the bench harness CSV format works.

```bash
cmake --build build --target bench_hqc128_karatsuba_ref -j$(nproc)

CPU="rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true"
$QEMU -machine virt -cpu $CPU -m 256M -nographic -bios none \
      -kernel build/bench_hqc128_karatsuba_ref.elf \
      -serial mon:stdio -no-reboot 2>/dev/null
```

**Expected output (numbers are bogus):**
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,<bogus>,<bogus>,<bogus>
encaps,1,<bogus>,<bogus>,<bogus>
decaps,1,<bogus>,<bogus>,<bogus>
```

---

## 4. Tier 2 — Spike polymul micro-bench (~1 s)

**Benchmark only `vect_mul()` — the hottest loop.**
Spike `rdcycle` = real retired instruction count. `vect_mul` is ~99% of 
keypair runtime, so this micro-bench gives the best signal for optimization 
feedback.

```bash
OUT=analysis/benchmarks/phase0b
ISA=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm

$RISCV/bin/spike --isa=$ISA \
    build-spike/bench_polymul_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_karatsuba_ref_polymul_N1.csv 2>/dev/null

cat $OUT/hqc128_karatsuba_ref_polymul_N1.csv
```

**Expected output:**
```
# level=128,polymul=karatsuba_ref,n_iter=1,codesize_text=4612
op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
vect_mul,1,18578006,18578006,18578006,18578007,18578007,18578007
```

**Output files:**
- `analysis/benchmarks/phase0b/hqc128_karatsuba_ref_polymul_N1.csv` — polymul instruction counts
- Median cycles = retired instruction count (with Spike's in-order model, IPC=1)
- Use `median_instret` as the primary optimization target

**Batch variant:** See §11 for launching all polymul variants at once.

---

## 5. Tier 3 — Full Spike KEM bench (~1–5 s per level)

**Complete keypair + encaps + decaps benchmark.** Use at phase milestones only
(§11 runs these in batch with `--full` flag).

```bash
OUT=analysis/benchmarks/phase0b
ISA=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm

$RISCV/bin/spike --isa=$ISA \
    build-spike/bench_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_karatsuba_ref_N1.csv 2>/dev/null

cat $OUT/hqc128_karatsuba_ref_N1.csv
```

**Expected output:**
```
# level=128,polymul=karatsuba_ref,n_iter=1,codesize_text=43584
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,18816325,18816325,18816325
encaps,1,37636615,37636615,37636615
decaps,1,56962431,56962431,56962431
```

**Monitor progress:** (Spike is slow for 256; runs ~5–15 min)

```bash
ps aux | grep "[s]pike.*bench_hqc128"
wc -c analysis/benchmarks/phase0b/hqc128_karatsuba_ref_N1.csv  # grows as lines arrive
```

**Output files:**
- `analysis/benchmarks/phase0b/hqc128_karatsuba_ref_N1.csv` — full KEM instruction counts
- Three rows: keypair, encaps, decaps (each run once with N_ITER=1)

**Batch variant:** See §11 for launching multiple levels at once with `--full`.

---

## 6. Tier 4 — llvm-mca static analysis (seconds)

**Estimate throughput & latency without simulation.** Analyzes a hot function
to predict IPC (instructions per cycle) and resource bottlenecks.

```bash
./analysis/llvm_mca/run_mca.sh src/poly_mul/karatsuba_zbc/gf2x_clmul.c
# Report written to: analysis/llvm_mca/reports/gf2x_clmul_mca.txt
```

**Manual invocation** (for custom analysis):

```bash
RISCV=~/Thesis/installed-tools

# Step 1: Compile to assembly
$RISCV/bin/clang \
    --target=riscv64-unknown-elf \
    -march=rv64gcv_zbc_zbb_zbkb_zvbc -mabi=lp64d \
    -O3 -ffreestanding -fno-builtin \
    -I vendor/hqc-ref/src/ref \
    -I vendor/hqc-ref/src/ref/hqc-1 \
    -S -o /tmp/gf2x.s \
    vendor/hqc-ref/src/ref/gf2x.c

# Step 2: Run llvm-mca
$RISCV/bin/llvm-mca \
    -march=riscv64 -mcpu=sifive-u74 \
    --timeline --iterations=10 \
    /tmp/gf2x.s
```

**Output files:**
- `analysis/llvm_mca/reports/<source>_mca.txt` — detailed throughput analysis
- Key metrics: Throughput (IPC), Timeline (stalls), Resource Pressure

---

## 7. Tier 5 — Spike --histogram: instruction mix (~1 s)

**Which instructions dominate?** Run alongside a bench with negligible overhead.

```bash
ISA=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm

$RISCV/bin/spike --isa=$ISA --histogram \
    build-spike/bench_polymul_hqc128_karatsuba_ref.elf \
    > analysis/benchmarks/phase0b/hqc128_karatsuba_ref_polymul_N1.csv \
    2>analysis/benchmarks/phase0b/hqc128_karatsuba_ref_hist.txt

# View top-20 instructions by frequency:
sort -t$'\t' -k2 -rn \
    analysis/benchmarks/phase0b/hqc128_karatsuba_ref_hist.txt | head -20
```

**Output files:**
- CSV to stdout — same as Tier 2 (instruction counts)
- `analysis/benchmarks/phase0b/hqc128_karatsuba_ref_hist.txt` — histogram of instruction frequencies
  - Format: `<instruction_name>\t<count>` (tab-separated)
  - Sorted by Spike internally; use `sort -t$'\t' -k2 -rn` to rank by frequency

**Batch variant:** See §12 for launching all histograms at once.

---

## 8. Quick-reference decision table

| Question | Tool | Time | Output |
|----------|------|------|--------|
| Does it pass KAT? | `./sim/qemu/run_kat.sh <elf> <rsp>` | ~10 s | PASS/FAIL |
| Does fafft_zvbc pass? | `scripts/bench/spike_kat.sh` | ~30–90 s | PASS/FAIL |
| CSV format OK? | Tier 1b QEMU bench | ~10 s | CSV (bogus cycles) |
| Did `vect_mul` get faster? | Tier 2 Spike polymul | ~1 s | `*_polymul_N1.csv` |
| Full KEM instruction count? | Tier 3 Spike KEM bench | ~5 s (HQC-128) | `*_N1.csv` (keypair/encaps/decaps) |
| Throughput of hot loop? | Tier 4 llvm-mca | seconds | `*_mca.txt` (IPC, timeline) |
| Instruction mix? | Tier 5 Spike histogram | ~1 s | `*_hist.txt` (ranked by freq) |
| **All variants all levels?** | **§9–12 batch scripts** | — | — |

---

# PART II — Batch & Orchestration Scripts

These scripts automate repetitive testing and result aggregation. All use
environment detection and soft-fail patterns (one failure doesn't stop the rest).

## 9. Batch KAT testing

**Test all 4 QEMU-compatible variants × 3 security levels (12 runs, ~2 min).**
Delegates to `./sim/qemu/run_kat.sh`. Prints a 4×3 pass/fail table.

```bash
scripts/bench/run_kat_all.sh
```

**Expected output:**
```
KAT results  karatsuba_ref  karatsuba_zbc  fafft_zbc  hybrid_zbc
HQC-128      PASS           PASS           PASS        PASS
HQC-192      PASS           PASS           PASS        PASS
HQC-256      PASS           PASS           PASS        PASS
NOTE: fafft_zvbc skipped (Zvbc not supported by QEMU).
      Run: scripts/bench/spike_kat.sh build/hqc128_fafft_zvbc.elf <rsp>
```

**Output files:** None (output is printed to stdout/stderr; piped results can be redirected).

**Exit code:** Non-zero if any run fails (use in CI/CD).

---

## 10. Spike KAT (for Zvbc variants)

**Single KAT run under Spike.** Use for variants requiring Zvbc
(unsupported by QEMU's system-mode emulation).

```bash
scripts/bench/spike_kat.sh \
    build/hqc128_fafft_zvbc.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

**Timing:** HQC-128 ~10–30 s, HQC-256 ~60–120 s (depending on fafft_zvbc speedup).

**Expected output:**
```
[spike_kat] Running: build/hqc128_fafft_zvbc.elf ...
PASS 100/100
[spike_kat] Verified against PQCkemKAT_2321.rsp
```

**Output files:** None (printed to stdout).

---

## 11. Batch Spike benchmark jobs

**Launch benchmarks in the background for all variants of a phase.**
Uses `nohup` to background each job. Prints PIDs and monitoring hints.

### Polymul micro-bench only (default; fast feedback, ~20–150 min per variant)

```bash
scripts/bench/run_spike_bench_phase.sh phase1a
```

### Full KEM bench (use at phase milestones; runs overnight)

```bash
scripts/bench/run_spike_bench_phase.sh phase1a --full
```

### Polymul + full KEM

```bash
scripts/bench/run_spike_bench_phase.sh phase1a --full
```

### Filter to specific variant

```bash
scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc
scripts/bench/run_spike_bench_phase.sh phase1a --variant karatsuba_zbc --full
```

### Full KEM without polymul

```bash
scripts/bench/run_spike_bench_phase.sh phase1a --full --no-polymul
```

**Expected output:**
```
[bench] Launched 3 polymul jobs for phase1a:
  PID 12345  hqc128_karatsuba_zbc_polymul  → analysis/benchmarks/phase1a/hqc128_karatsuba_zbc_polymul_N1.csv
  PID 12346  hqc192_karatsuba_zbc_polymul  → analysis/benchmarks/phase1a/hqc192_karatsuba_zbc_polymul_N1.csv
  PID 12347  hqc256_karatsuba_zbc_polymul  → analysis/benchmarks/phase1a/hqc256_karatsuba_zbc_polymul_N1.csv

Monitor:
  watch -n 30 "wc -c analysis/benchmarks/phase1a/*.csv"
  ps aux | grep '[s]pike.*bench_polymul'
```

**Output files:**
- `analysis/benchmarks/<phase>/hqc{128,192,256}_{variant}_polymul_N1.csv` — polymul micro-bench results
- `analysis/benchmarks/<phase>/hqc{128,192,256}_{variant}_N1.csv` — full KEM results (if `--full`)
- `analysis/benchmarks/<phase>/pids.txt` — record of PIDs for each job

**Monitor progress:**
```bash
watch -n 30 "wc -c analysis/benchmarks/phase1a/*.csv"
ps aux | grep '[s]pike.*bench_hqc'
tail -f analysis/benchmarks/phase1a/*.csv
```

---

## 12. Batch Spike histogram

**Dual mode: `--launch` (default) or `--report`.**

### Launch histograms (generates CSV + histogram files)

```bash
scripts/bench/run_histogram_all.sh phase1a
scripts/bench/run_histogram_all.sh phase1a --variant karatsuba_zbc
```

**Output files:**
- CSV (stdout) → `analysis/benchmarks/phase1a/hqc{128,192,256}_{variant}_polymul_N1.csv`
- Histogram (stderr) → `analysis/benchmarks/phase1a/hqc{128,192,256}_{variant}_hist.txt`
  - Format: `<instruction>\t<count>` (tab-separated, sorted by Spike)

### Report mode (reads existing histograms, prints top-20)

```bash
scripts/bench/run_histogram_all.sh phase1a --report
scripts/bench/run_histogram_all.sh phase1a --report --variant karatsuba_zbc
```

**Example output:**
```
=== hqc128_karatsuba_zbc (phase1a) ===
clmul          1,234,567
clmulh         1,234,234
addi             456,789
xori             345,678
...
```

---

## 13. Parse and compare results

**Reads all CSV files, computes speedups, generates comparison tables.**

```bash
python3 scripts/report/extract_results.py
```

### Filter to one phase

```bash
python3 scripts/report/extract_results.py --phase phase1a
```

### LaTeX output (for thesis)

```bash
python3 scripts/report/extract_results.py --latex \
    > analysis/report/table_results.tex
```

### Include instruction mix from histograms

```bash
python3 scripts/report/extract_results.py --mix
```

### Write CSV summary files (for plotting)

```bash
python3 scripts/report/extract_results.py --write-summary-csv
```

**Output files:**
- `analysis/benchmarks/cycle_counts.csv` — summary table (impl, hqc128, hqc192, hqc256)
- `analysis/benchmarks/instruction_mix.csv` — instruction category breakdown
- LaTeX table written to stdout (redirect to `.tex` file)

**CSV parsing rules:**
- Reads `# level=NNN,polymul=VARIANT` comment header (source of truth, not filename)
- Uses `median_cycles` as primary metric
- Computes speedup vs. baseline (`karatsuba_ref`)

---

## 14. Generate dated report

**Orchestrates §13 + plot scripts + Amdahl's law → dated Markdown report.**

```bash
scripts/report/gen_report.sh
```

### Restrict to one phase

```bash
scripts/report/gen_report.sh --phase phase1a
```

### Custom vect_mul fraction for Amdahl (default: 0.90)

```bash
scripts/report/gen_report.sh --fraction 0.85
```

**Output files:**
- `analysis/report/report_YYYY-MM-DD.md` — dated report containing:
  - Date, git SHA, phases found
  - Instruction count comparison tables (from §13)
  - Paths to generated PDF figures
  - Amdahl's law projections
  - Next-steps checklist

**Soft-fail design:** If any sub-step fails, it's logged in the report as SKIPPED
(full report is still generated).

---

# PART III — Reference & Interpretation

## 15. Measured baseline results

### Mini-benchmark (`tests/bench/mini_bench.c`)

Three kernels: 2-limb GF(2^128) clmul, RVV 256-byte XOR, scalar XOR-rotate.
Each kernel runs 10,000 iterations.

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

**Takeaway:** Spike is **4× faster** in wall time. Cycle counts differ because
Spike's `mcycle` = retired instructions (IPC=1 in-order), while QEMU `rdcycle` 
tracks real host wall-clock time.

### Full HQC karatsuba_ref KEM benchmark (Phase 0b baseline)

Measured on Spike ISS 1.1.1-dev, `rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm`, N_ITER=1:

| Level | Operation | Retired instructions | Wall time |
|-------|-----------|---------------------|-----------|
| HQC-128 | keypair | 18,816,325 | 0.72 s total |
| HQC-128 | encaps | 37,636,615 | |
| HQC-128 | decaps | 56,962,431 | |
| HQC-192 | keypair | 56,986,059 | 2.16 s total |
| HQC-192 | encaps | 113,992,185 | |
| HQC-192 | decaps | 171,660,064 | |
| HQC-256 | keypair | 140,074,066 | 5.10 s total |
| HQC-256 | encaps | 279,984,371 | |
| HQC-256 | decaps | 421,305,306 | |

`vect_mul` polymul micro-bench (isolated `vect_mul`, same ISA):

| Level | `vect_mul` instructions | Wall time |
|-------|------------------------|-----------|
| HQC-128 | 18,578,006 | 0.14 s |
| HQC-192 | 56,469,319 | 0.33 s |
| HQC-256 | 139,077,901 | 0.79 s |

**Key insight:** `vect_mul` accounts for **~99%** of keypair instruction count.
It is the dominant cost and correct optimization target.

QEMU KAT (100 vectors, `karatsuba_ref`): **~8.6 s** (HQC-128)

---

## 16. Output file locations & interpretation

### CSV benchmark files (`*_N1.csv`)

Each file has a metadata comment header followed by CSV rows:

```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
vect_mul,1,18234567,18234889,18298123,18234567,18234889,18298123
```

**Columns:**
- `op`: benchmarked operation (vect_mul, keypair, encaps, decaps)
- `median_instret`: primary metric — **use this** (Spike retired instruction count)
- `median_cycles`: same as instret with Spike (IPC=1 in-order model)

**⚠️ WARNING:** Do NOT use CSV files from QEMU builds for thesis numbers.
QEMU `rdcycle` is a software counter unrelated to actual instruction count.

### Histogram files (`*_hist.txt`)

Tab-separated `<instruction>\t<count>` lines:

```
xori          234567890
addi          198234567
clmul         180000000
...
```

**To view top-20 by frequency:**
```bash
sort -t$'\t' -k2 -rn <file> | head -20
```

**Interpretation:** Which instructions consumed the most issue slots?
- Phase 0b baseline: `scalar_alu` dominates
- Phase 1a (karatsuba_zbc): `clmul`/`clmulh` dominate
- Phase 2+ (vector): `vclmul.vv`/`vclmulh.vv` dominate

### Summary table files (from extract_results.py)

**`cycle_counts.csv`** (columns: `impl, hqc128, hqc192, hqc256`)
```
karatsuba_ref,18816325,56986059,140074066
karatsuba_zbc,1500000,4500000,11000000
```

**`instruction_mix.csv`** (per-instruction-category breakdown)
```
impl,scalar_alu,clmul,vclmul,...
karatsuba_ref,99,1,0,...
karatsuba_zbc,50,50,0,...
```

### Report files

**`analysis/report/report_YYYY-MM-DD.md`**
- Dated markdown report with all results, figures, and Amdahl projections
- Generated by `scripts/report/gen_report.sh`

**`analysis/llvm_mca/reports/<source>_mca.txt`**
- llvm-mca throughput analysis for a single source file
- Key metrics: Throughput (IPC), Resource Pressure, Timeline (stalls)

---

## 17. Step-by-step build guide

### Prerequisites

```bash
source toolchain/toolchain.env
# RISCV=$HOME/Thesis/installed-tools must be on PATH
```

### Mini-benchmark

```bash
make -f tests/bench/mini_bench.mk
./sim/spike/run_bench.sh build-mini/mini_bench_spike.elf
```

### Full HQC-128 benchmark on Spike

```bash
cmake -S . -B build-spike \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=spike
cmake --build build-spike --target bench_hqc128_karatsuba_ref -j$(nproc)

./sim/spike/run_bench.sh build-spike/bench_hqc128_karatsuba_ref.elf
```

### KAT on QEMU

```bash
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=qemu
cmake --build build -j$(nproc)

./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf \
    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

### All five polymul variants (Spike)

```bash
for variant in karatsuba_ref karatsuba_zbc fafft_zbc hybrid_zbc fafft_zvbc; do
    echo "=== $variant ==="
    ./sim/spike/run_bench.sh \
        build-spike/bench_hqc128_${variant}.elf 2>/dev/null
done
```

### Rebuild Spike with optimized host compiler (optional, +15–30% wall-time)

```bash
bash toolchain/build-spike.sh
```

Rebuilds from `$HOME/Thesis/src/riscv-isa-sim` with `-O3 -march=native`,
strips debug symbols, reinstalls to `$RISCV/bin/spike`.

---

## Last updated

2026-04-30 — Consolidated WORKFLOW.md: merged Part III into Part I/II, removed duplicates,
integrated output file descriptions with each tool. All commands tested and verified to work.

