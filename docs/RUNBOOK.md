# HQC-RISC-V Workflow Runbook (Single Consolidated Reference)

This file is the only execution runbook for build, correctness, benchmarking,
analysis, and reporting.

All commands assume:

```bash
cd ~/Thesis/hqc-riscv
source toolchain/toolchain.env
# exports RISCV, QEMU and prepends $RISCV/bin to PATH
```

For architectural rationale and toolchain decisions, see docs/METHODOLOGY.md.

Canonical polymul variant labels used by build targets and reports:
- `karatsuba_zbc`
- `karatsuba_zvbc`
- `fafft_zbc`
- `fafft_zvbc`
- `hybrid`

---

## 0. Setup and preflight

```bash
source toolchain/toolchain.env
command -v cmake
command -v "$RISCV/bin/spike"
command -v "$QEMU"
```

Expected:
- all commands resolve to valid paths

If a command is missing, fix toolchain setup before continuing.

Expected terminal snippet:

```text
$ command -v cmake
/usr/bin/cmake
$ command -v "$RISCV/bin/spike"
/home/.../installed-tools/bin/spike
$ command -v "$QEMU"
/home/.../installed-tools/bin/qemu-system-riscv64
```

---

## 1. Tier 0 - Rebuild after code change

Use this after editing C or ASM code.

```bash
# QEMU/KAT ELF
cmake --build build/qemu --target hqc128_karatsuba_ref -j$(nproc)

# Spike benches
cmake --build build/spike \
  --target bench_hqc128_karatsuba_ref bench_polymul_hqc128_karatsuba_ref \
  -j$(nproc)
```

If CMake files or source lists changed, reconfigure first:

```bash
cmake -S . -B build/qemu \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
  -DHQC_SIM=qemu

cmake -S . -B build/spike \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
  -DHQC_SIM=spike \
  -DBENCH_N_ITER=1
```

---

## 2. Tier 1 - Correctness gate (Spike-first, QEMU fallback)

Run this after every change before benchmarking.

### 2.1 Primary: Single KAT on Spike

```bash
scripts/bench/spike_kat.sh \
  build/spike/hqc128_karatsuba_ref.elf \
  vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

### 2.2 Fallback / extra check: Single KAT on QEMU

```bash
./sim/qemu/run_kat.sh build/qemu/hqc128_karatsuba_ref.elf
```

Expected output tail:
- many lines of `ss: <hex>`
- `PASS 100/100`

Notes:
- `sim/qemu/run_kat.sh` takes only the ELF path.
- The older style that passed an RSP argument to this script is outdated.

Expected terminal snippet:

```text
ss: <hex>
ss: <hex>
...
PASS 100/100
```

### 2.3 Batch fallback KAT across levels and implemented QEMU variants

```bash
scripts/bench/run_kat_all.sh
```

What it does:
- reads implemented variants from scripts/implemented_variants.txt
- skips `fafft_zvbc` (QEMU does not support Zvbc)
- runs each ELF with sim/qemu/run_kat.sh
- validates against RSP files via tests/kat/kat_check.py

Current expected summary shape:

```text
KAT results   <variant columns>
HQC-128       PASS ...
HQC-192       PASS ...
HQC-256       PASS ...
OVERALL: PASS
```

RSP files used:
- HQC-128: vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
- HQC-192: vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp
- HQC-256: vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp

### 2.4 Spike KAT for Zvbc-only variant

```bash
scripts/bench/spike_kat.sh \
  build/spike/hqc128_fafft_zvbc.elf \
  vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

Notes:
- this can run long depending on host and variant
- use only for variants requiring Zvbc support

---

## 3. Tier 1b - QEMU bench sanity (format check only)

Purpose:
- confirm bench harness CSV format
- not for thesis performance numbers

```bash
cmake --build build/qemu --target bench_hqc128_karatsuba_ref -j$(nproc)

CPU="rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true"
$QEMU -machine virt -cpu "$CPU" -m 256M -nographic -bios none \
  -kernel build/qemu/bench_hqc128_karatsuba_ref.elf \
  -serial mon:stdio -no-reboot 2>/dev/null
```

Expected:
- CSV header and keypair/encaps/decaps rows
- values are not architectural instruction counts

---

## 4. Tier 2 - Spike polymul micro-bench (primary optimization signal)

This targets `vect_mul`, the dominant hotspot.

```bash
OUT=analysis/benchmarks/baseline_ref
mkdir -p "$OUT"
ISA=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm

$RISCV/bin/spike --isa=$ISA \
  build/spike/bench_polymul_hqc128_karatsuba_ref.elf \
  > "$OUT/hqc128_karatsuba_ref_polymul_N1.csv" 2>/dev/null

cat "$OUT/hqc128_karatsuba_ref_polymul_N1.csv"
```

Output file:
- analysis/benchmarks/baseline_ref/hqc128_karatsuba_ref_polymul_N1.csv

Interpretation:
- use `median_instret` as primary metric
- on this Spike model, `median_cycles` and `median_instret` track closely

---

## 5. Tier 3 - Full Spike KEM bench

Use at phase checkpoints to measure keypair/encaps/decaps end-to-end.

```bash
OUT=analysis/benchmarks/baseline_ref
mkdir -p "$OUT"
ISA=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm

$RISCV/bin/spike --isa=$ISA \
  build/spike/bench_hqc128_karatsuba_ref.elf \
  > "$OUT/hqc128_karatsuba_ref_N1.csv" 2>/dev/null

cat "$OUT/hqc128_karatsuba_ref_N1.csv"
```

Output file:
- analysis/benchmarks/baseline_ref/hqc128_karatsuba_ref_N1.csv

Interpretation:
- rows are keypair, encaps, decaps
- use `median_instret` for comparisons

### 5.1 Reproducible all-variant run (run-id folders, raw+curated split)

```bash
scripts/bench/run_spike_suite_sync.sh --iter 1
```

Outputs:
- `analysis/benchmarks/runs/<run_id>/raw/` (transient stdout/stderr, ignored)
- `analysis/benchmarks/runs/<run_id>/curated/` (clean CSV for reporting)
- `analysis/benchmarks/runs/<run_id>/summary.csv` (wall-time + medians)

Promote curated CSV to canonical versioned folder:

```bash
scripts/bench/run_spike_suite_sync.sh --iter 1 --promote phase1_spike_n1
```

Expected terminal snippet:

```text
[suite] run-id: all_suite_YYYYMMDD_HHMM
[suite] build dir: /home/.../hqc-riscv/build/spike
[suite] output: /home/.../hqc-riscv/analysis/benchmarks/runs/all_suite_YYYYMMDD_HHMM
[suite] polymul L128 karatsuba_ref: 0.123s
[suite] kem L128 karatsuba_ref: 0.456s
...
[suite] done: /home/.../summary.csv
```

### 5.2 Benchmark folder structure (new and old)

- `analysis/benchmarks/runs/<run_id>/raw/`
  - transient per-run stdout/stderr and intermediate artifacts
  - ignored by git
- `analysis/benchmarks/runs/<run_id>/curated/`
  - clean per-target CSV extracted from raw output
  - suitable for promotion
- `analysis/benchmarks/runs/<run_id>/summary.csv`
  - single table with wall-time and median metrics

- `analysis/benchmarks/canonical/<name>/`
  - promoted curated CSV sets intended to be versioned in git
  - stable input for reports and plotting

Legacy benchmark-set folders still coexist for historical continuity:
- `analysis/benchmarks/<bench_set>/`
  - older phase-based outputs (for example `baseline_ref`, `phase1c`)
  - can be kept read-only and gradually superseded by canonical run-id promotions

---

## 6. Tier 4 - llvm-mca static throughput/latency analysis

### 6.1 Recommended helper script (validated)

```bash
./analysis/llvm_mca/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c 128
```

Output file:
- analysis/llvm_mca/reports/gf2x_mca.txt

Notes:
- helper script now includes `--skip-unsupported-instructions=lack-sched`
- some sources can still produce low-value output (for example when emitted asm
  is mostly unsupported markers). Prefer known hot sources such as
  vendor/hqc-ref/src/ref/gf2x.c.

### 6.2 Manual invocation (validated)

```bash
RISCV=~/Thesis/installed-tools

$RISCV/bin/clang \
  --target=riscv64-unknown-elf \
  -march=rv64gcv_zbc_zbb_zbkb_zvbc -mabi=lp64d \
  -O3 -ffreestanding -fno-builtin \
  -I vendor/hqc-ref/src/ref \
  -I vendor/hqc-ref/src/ref/hqc-1 \
  -I vendor/hqc-ref/src/common/hqc-1 \
  -S -o /tmp/gf2x.s \
  vendor/hqc-ref/src/ref/gf2x.c

$RISCV/bin/llvm-mca \
  -march=riscv64 -mcpu=sifive-u74 \
  --timeline --iterations=10 \
  --skip-unsupported-instructions=lack-sched \
  /tmp/gf2x.s
```

---

## 7. Tier 5 - Instruction mix (histogram)

Important update:
- direct `spike --histogram` is not supported by the current Spike binary in
  this environment.
- use scripts/bench/run_histogram_all.sh, which uses Spike `-g` PC histogram
  and resolves PCs to mnemonics.

### 7.1 Launch and wait for histogram generation

```bash
scripts/bench/run_histogram_all.sh polymul_zbc --variant karatsuba_ref --wait
```

### 7.2 Report top instructions from existing histogram files

```bash
scripts/bench/run_histogram_all.sh polymul_zbc --report --variant karatsuba_ref --top 20
```

Output files:
- analysis/benchmarks/<bench_set>/hqc{128,192,256}_<variant>_polymul_N1.csv
- analysis/benchmarks/<bench_set>/hqc{128,192,256}_<variant>_hist.txt

Interpretation:
- report mode prints mnemonic frequency and percentage
- use this for scalar->clmul->vclmul transition analysis

Expected terminal snippet:

```text
=== hqc128_karatsuba_zbc (polymul_zbc) ===
  mnemonic                            count      %
  --------                            -----      -
  clmul                              123456   22.1%
  clmulh                             98765   17.7%
```

---

## 8. Batch Spike benchmarking (phase orchestration)

Script:
- scripts/bench/run_spike_bench_variant.sh

### 8.1 Polymul-only launch (default)

```bash
scripts/bench/run_spike_bench_variant.sh polymul_zbc
```

### 8.2 Variant filter

```bash
scripts/bench/run_spike_bench_variant.sh polymul_zbc --variant karatsuba_zbc
```

### 8.3 Full KEM launch

```bash
scripts/bench/run_spike_bench_variant.sh polymul_zbc --full
```

### 8.4 Full KEM only (no polymul)

```bash
scripts/bench/run_spike_bench_variant.sh polymul_zbc --full --no-polymul
```

Outputs written under:
- analysis/benchmarks/<bench_set>/

Key files:
- hqc{128,192,256}_{variant}_polymul_N1.csv
- hqc{128,192,256}_{variant}_N1.csv (when `--full`)
- run_events.log (start/end status)
- pids.txt

Monitoring:

```bash
watch -n 30 "wc -c analysis/benchmarks/polymul_zbc/*.csv"
ps aux | grep '[s]pike.*bench'
tail -f analysis/benchmarks/polymul_zbc/run_events.log
```

---

## 9. Results extraction, summaries, and report generation

### 9.1 Extract tables from benchmark CSV files

```bash
/usr/bin/python3 scripts/report/extract_results.py
/usr/bin/python3 scripts/report/extract_results.py --bench-set polymul_zbc
```

### 9.2 LaTeX table for thesis

```bash
/usr/bin/python3 scripts/report/extract_results.py --latex \
  > analysis/report/table_results.tex
```

### 9.3 Include instruction mix and write summary CSVs

```bash
/usr/bin/python3 scripts/report/extract_results.py --mix --bench-set polymul_zbc
/usr/bin/python3 scripts/report/extract_results.py --write-summary-csv
```

Generated summary files:
- analysis/benchmarks/cycle_counts.csv
- analysis/benchmarks/instruction_mix.csv

### 9.4 Generate dated report

```bash
scripts/report/gen_report.sh
scripts/report/gen_report.sh --bench-set polymul_zbc
scripts/report/gen_report.sh --fraction 0.85
```

Generated report artifacts:
- analysis/report/report_YYYY-MM-DD.md
- analysis/report/table_results_YYYY-MM-DD.tex

Notes:
- gen_report is soft-fail by design
- `plot.py` step is skipped automatically when plotting dependencies/data are missing

---

## 10. Quick decision table

| Question | Use | Typical output |
|---|---|---|
| Did correctness break? | `./sim/qemu/run_kat.sh <elf>` | `PASS 100/100` |
| All QEMU-compatible variants pass? | `scripts/bench/run_kat_all.sh` | PASS/FAIL matrix |
| Zvbc variant correctness? | `scripts/bench/spike_kat.sh <elf> <rsp>` | PASS/FAIL |
| Is bench CSV plumbing healthy? | Tier 1b QEMU bench | CSV rows |
| Did `vect_mul` improve? | Tier 2 Spike polymul | `*_polymul_N1.csv` |
| Full KEM instruction counts? | Tier 3 Spike KEM | `*_N1.csv` |
| Static throughput/stall hints? | Tier 4 llvm-mca | `*_mca.txt` |
| Instruction mix by mnemonic? | Tier 5 histogram script | `*_hist.txt` + report |
| Aggregate tables and report? | extract_results + gen_report | markdown/latex/csv |

---

## 11. Verified on this repository state

Date validated: 2026-04-30

Validated working commands in this runbook:
- setup/preflight
- tier 0 builds
- tier 1 single QEMU KAT
- tier 1 batch KAT (run_kat_all.sh)
- tier 1b QEMU bench sanity
- tier 2 and tier 3 Spike bench commands
- tier 4 helper and manual llvm-mca commands (on vendor/hqc-ref/src/ref/gf2x.c)
- tier 5 run_histogram_all.sh launch/report flow
- run_spike_bench_variant.sh polymul/full modes
- extract_results.py and gen_report.sh commands

Known caveats retained in this file:
- `spike --histogram` is not supported by current Spike binary; use run_histogram_all.sh.
- `scripts/bench/spike_kat.sh` can be long-running depending on host and variant.

