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

Local shell (custom LLVM 21) preflight:

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
scripts/test/run_kat.sh --sim spike \
  build/spike/hqc128_fafft_zvbc.elf \
  vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

### 2.2 Fallback / extra check: Single KAT on QEMU

```bash
./scripts/test/run_kat.sh --sim qemu build/qemu/hqc128_karatsuba_ref.elf tests/kat/PQCkemKAT_128.rsp
```

Expected output tail:
- many lines of `ss: <hex>`
- `PASS 100/100`

Notes:
- `sim/qemu/run.sh` takes only the ELF path.
- The older style that passed an RSP argument to this script is outdated.
- QEMU must not be used for Zvbc tracks; use Spike for those tracks.
- Reason: QEMU crashes on experimental Zvbc extension paths in this project setup.

Expected terminal snippet:

```text
ss: <hex>
ss: <hex>
...
PASS 100/100
```

### 2.3 Batch KAT across levels and implemented QEMU variants

```bash
ctest -R kat
```

What it does:
- runs the CTest KAT matrix defined in CMake
- executes all registered `kat_*` tests using the unified `scripts/test/run_kat.sh` runner
- preserves simulator selection and target wiring from the active CMake configuration

Current expected summary shape:

```text
Test project ...
  Start ...: kat_hqc128_...
  Start ...: kat_hqc192_...
  Start ...: kat_hqc256_...
100% tests passed, 0 tests failed out of ...
```

RSP files used:
- HQC-128: vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
- HQC-192: vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp
- HQC-256: vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp

### 2.4 Spike KAT for Zvbc-only variant

```bash
scripts/test/run_kat.sh --sim spike \
  build/spike/hqc128_fafft_zvbc.elf \
  vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

Notes:
- this can run long depending on host and variant
- use Spike natively for variants requiring Zvbc support

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
scripts/bench/run_benchmarks.sh --mode suite --iter 1
```

Outputs:
- `analysis/benchmarks/runs/<run_id>/raw/` (transient stdout/stderr, ignored)
- `analysis/benchmarks/runs/<run_id>/curated/` (clean CSV for reporting)
- `analysis/benchmarks/runs/<run_id>/summary.csv` (wall-time + medians)

Promote curated CSV to canonical versioned folder:

```bash
scripts/bench/run_benchmarks.sh --mode suite --iter 1 --promote phase1_spike_n1
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
./scripts/verify/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c 128
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
- use scripts/bench/run_profile.sh --histogram, which uses Spike `-g` PC histogram
  and resolves PCs to mnemonics.

### 7.1 Launch and wait for histogram generation

```bash
scripts/bench/run_profile.sh --histogram --bench-set polymul_zbc --variant karatsuba_ref --wait
```

### 7.2 Report top instructions from existing histogram files

```bash
scripts/bench/run_profile.sh --histogram --bench-set polymul_zbc --report --variant karatsuba_ref --top 20
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

### 7.3 Hardware Memory Profiling (Banana Pi)

Use this to validate Spike cache-simulator trends against physical silicon for
`zbc` variants.

Command on Banana Pi:

```bash
perf stat -e L1-dcache-loads,L1-dcache-load-misses ./dudect_cross_hqc128_fafft_zbc.elf
```

Notes:
- treat this as the authoritative measurement for native memory contention.
- use Spike cache profiling (`scripts/bench/run_profile.sh --cache`) as a
  controlled simulator baseline and compare relative deltas across variants.

---

## 8. Batch Spike benchmarking (phase orchestration)

Script:
- scripts/bench/run_benchmarks.sh --mode async

### 8.1 Polymul-only launch (default)

```bash
scripts/bench/run_benchmarks.sh --mode async polymul_zbc
```

### 8.2 Variant filter

```bash
scripts/bench/run_benchmarks.sh --mode async polymul_zbc --variant karatsuba_zbc
```

### 8.3 Full KEM launch

```bash
scripts/bench/run_benchmarks.sh --mode async polymul_zbc --full
```

### 8.4 Full KEM only (no polymul)

```bash
scripts/bench/run_benchmarks.sh --mode async polymul_zbc --full --no-polymul
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

## 10. Static analysis / linting

Run the unified lint script locally before pushing to CI:

```bash
source toolchain/toolchain.env   # sets $RISCV for vector-file sysroot injection
bash scripts/ci/lint.sh
```

**What it does:**

- Reads `build/qemu/compile_commands.json` and collects every `src/**/*.c`
  file whose compile command does **not** contain `-mrvv-vector-bits`.
  VLEN-specific TUs (fafft_zbc, vector/, rvv/ variants) require the
  cross-compiler and are intentionally excluded from host linting.
- Runs `clang-tidy` (`bugprone-*`, `cert-*` checks) on all remaining files.
- `--warnings-as-errors` enabled — any hit exits non-zero.
- All files get `--extra-arg=-march=rv64gcv_zbc_zbb_zbkb` to strip `zvbc`
  and other extensions that host clang-tidy (LLVM 16) cannot parse.

**Suppressed checks (project-wide):**

| Check | Reason suppressed |
|---|---|
| `bugprone-easily-swappable-parameters` | High false-positive rate on fixed-signature crypto functions |

**Secure zeroization (`cert-MSC12-C` / `insecureAPI.bzero`):**

Secret key material must be erased with `memset_zero(ptr, len)` (from
`vendor/hqc-ref/src/common/crypto_memset.h`), never plain `memset`.  The macro
routes through a `volatile` function pointer that prevents dead-store
elimination.

**Configuration knobs:**

```bash
CLANG_TIDY=/path/to/clang-tidy bash scripts/ci/lint.sh
BUILD_DIR=build/spike bash scripts/ci/lint.sh
```

**Expected clean output:**

```text
(no warnings — script exits 0)
```

If clang-tidy is not installed, install it via your package manager:

```bash
sudo apt-get install clang-tidy    # Ubuntu / Debian
```

---

## 11. Quick decision table

| Question | Use | Typical output |
|---|---|---|
| Did correctness break? | `./scripts/test/run_kat.sh --sim qemu <elf> <rsp>` | `PASS 100/100` |
| All QEMU-compatible variants pass? | `scripts/test/run_kat.sh --sim qemu <elf> <level> <polymul>` | PASS/FAIL matrix |
| Zvbc variant correctness? | `scripts/test/run_kat.sh <elf> <level> <polymul>` | PASS/FAIL |
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
- tier 1 batch KAT (ctest -R kat)
- tier 1b QEMU bench sanity
- tier 2 and tier 3 Spike bench commands
- tier 4 helper and manual llvm-mca commands (on vendor/hqc-ref/src/ref/gf2x.c)
- tier 5 run_profile.sh histogram launch/report flow
- run_benchmarks.sh async/suite modes
- extract_results.py and gen_report.sh commands

Known caveats retained in this file:
- `spike --histogram` is not supported by current Spike binary; use `scripts/bench/run_profile.sh --histogram`.
- `scripts/test/run_kat.sh` can be long-running depending on host and variant. Use `--sim qemu` for faster execution on non-Zvbc variants.

---

## 12. KAT Seed Generation (one-time)

The headers under `tests/kat/generated/` are pre-committed and should not
need regeneration unless the upstream `.rsp` files change. To regenerate:

```bash
python3 tests/kat/gen_kat_seeds.py \
  --rsp   vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp \
  --level 128 \
  --out   tests/kat/generated/kat_seeds_128.h

python3 tests/kat/gen_kat_seeds.py \
  --rsp   vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp \
  --level 192 \
  --out   tests/kat/generated/kat_seeds_192.h

python3 tests/kat/gen_kat_seeds.py \
  --rsp   vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp \
  --level 256 \
  --out   tests/kat/generated/kat_seeds_256.h
```

Each header exports:
- `kat_seeds[100][48]` — entropy inputs for `prng_init()`
- `kat_ss[100][64]` — expected shared-secret values

> **Important:** The vendor KEM (`crypto_kem_keypair`, `_enc`, `_dec`) uses its
> own SHAKE-256 PRNG (`prng_get_bytes`), seeded via `prng_init(seed, NULL, 48, 0)`.
> It does **not** use `randombytes()` / AES-CTR DRBG. Always seed with
> `prng_init`, not `randombytes_init`.

---

## 13. Disassembly & Static Analysis

```bash
# Full disassembly with source interleave (if DWARF present)
llvm-objdump -d --source build/hqc128_karatsuba_ref.elf | less

# Show all defined symbols with sizes
llvm-nm --print-size --defined-only --size-sort build/hqc128_karatsuba_ref.elf | tail -20

# Check for specific instruction patterns
llvm-objdump -d build/hqc128_karatsuba_ref.elf | grep -E "clmul|packh|vclmul"

# Find functions that contain vector instructions
llvm-objdump -d build/hqc128_karatsuba_ref.elf | awk '/^[0-9a-f]+ </{fn=$0} /^\s+v/{print fn; fn=""}'
```

