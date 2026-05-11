# HQC-RISC-V Workflow Runbook (Single Consolidated Reference)

This file is the only execution runbook for build, correctness, benchmarking,
analysis, and reporting.

All commands assume:

```bash
cd ~/Thesis/hqc-riscv
source toolchain/toolchain.env
# exports RISCV, QEMU and prepends $RISCV/bin to PATH
```

For architectural rationale and toolchain decisions, see [docs/METHODOLOGY.md](METHODOLOGY.md).

**Canonical polymul variant labels used by build targets and reports:**
- `karatsuba_zbc`
- `karatsuba_zvbc`
- `fafft_zbc`
- `fafft_zvbc`
- `hybrid`

---

## 1. Build System & Compilation

### Overview: Two Benchmark Targets

Spike provides two distinct benchmark targets for each HQC security level (128/192/256):

1. **`bench_hqc<level>_<variant>`** — Full KEM benchmark
   - Measures end-to-end performance of keypair generation, encapsulation, and decapsulation
   - Use for: Overall performance analysis, thesis results, phase checkpoints
   - Example: `bench_hqc128_karatsuba_ref`

2. **`bench_polymul_hqc<level>_<variant>`** — Polynomial multiplication micro-benchmark
   - Isolates the `vect_mul` hotspot (dominant operation in HQC)
   - Use for: Iterative optimization of the core multiplication algorithm
   - Example: `bench_polymul_hqc128_karatsuba_ref`

### Quick Build After Code Changes

Use this after editing C or ASM code:

```bash
# Build individual Spike KEM and polymul targets
cmake --build build/spike \
  --target bench_hqc128_karatsuba_ref bench_polymul_hqc128_karatsuba_ref \
  -j$(nproc)

# Or build a QEMU KAT target (for validation only)
cmake --build build/qemu --target hqc128_karatsuba_ref -j$(nproc)
```

### CMake Reconfiguration (When Needed)

Reconfigure **only** if:
- CMake files were edited (CMakeLists.txt, hqc_core.cmake, etc.)
- Source file lists changed (src/*/CMakeLists.txt)
- Build options changed (-DHQC_SIM, -DBENCH_N_ITER, etc.)

**For Spike benchmarking:**

```bash
cmake -S . -B build/spike \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
  -DHQC_SIM=spike
```

**For QEMU correctness validation:**

```bash
cmake -S . -B build/qemu \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
  -DHQC_SIM=qemu
```

---

## 2. Correctness & Testing (KATs)

Run correctness checks **after every code change and before benchmarking**.

### Single KAT on Spike (Recommended)

Test a single security level with a specific variant:

```bash
scripts/test/run_kat.sh --sim spike build/spike/hqc128_karatsuba_ref.elf tests/kat/PQCkemKAT_128.rsp
```

Expected output tail:
```
PASS 100/100
```

### Batch KAT Matrix (All Levels & Variants)

Run all registered KAT tests across security levels and implemented variants:

```bash
ctest --test-dir build/spike -R kat
```

What it does:
- Executes all registered `kat_*` tests using the unified `scripts/test/run_kat.sh` runner
- Preserves simulator selection and target wiring from the active CMake configuration
- Tests all configured levels (128, 192, 256) and implemented variants

Expected output summary:

```text
Test project /home/.../hqc-riscv/build/spike
  Start   1: kat_hqc128_...
  Start   2: kat_hqc192_...
  Start   3: kat_hqc256_...
  ...
100% tests passed, 0 tests failed out of NN
```

**RSP files used:**
- HQC-128: `tests/kat/PQCkemKAT_128.rsp`
- HQC-192: `tests/kat/PQCkemKAT_192.rsp`
- HQC-256: `tests/kat/PQCkemKAT_256.rsp`

(These are symlinks to `vendor/hqc-ref/kats/ref/` for clean path references.)

---

## 3. Benchmarking Suite

### Spike Benchmark Framework

Benchmarks run on Spike with a standardized ISA:
```
rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm
```

All benchmarks output CSV format: `op,iterations,min,max,median_cycles,median_instret,...`

### Quick Run: Polymul Micro-Benchmark (Single Variant)

Launch a polymul micro-benchmark for a specific variant in the background:

```bash
scripts/bench/run_benchmarks.sh --mode async polymul_zbc --variant karatsuba_ref
```

Output: CSV files written to `analysis/benchmarks/isolated/scalar/polymul_zbc/`

Monitoring:
```bash
watch -n 10 "wc -c analysis/benchmarks/isolated/scalar/polymul_zbc/*_polymul_N1.csv"
ps aux | grep '[s]pike.*bench'
```

### Full KEM Bench Launch (All Security Levels)

Launch full KEM benchmarks (keypair, encaps, decaps) for all configured security levels:

```bash
scripts/bench/run_benchmarks.sh --mode async polymul_zbc --full
```

Options:
- `--variant <regex>` — Filter variants by regex (default: canonical variant for bench_set)
- `--iter N` — Iterations per operation (default: 1)
- `--no-polymul` — Omit polymul benchmarks and measure KEM only (requires `--full`)

### Suite Mode: Synchronized Benchmarking with Promotion

For reproducible, atomic benchmark runs with optional promotion to canonical storage:

```bash
scripts/bench/run_benchmarks.sh --mode suite --iter 1 --promote phase1_spike_n1
```

**What it does:**
- Reconfigures CMake with `-DBENCH_N_ITER=<iter>`
- Runs all levels and implemented variants sequentially
- Organizes outputs in `analysis/benchmarks/suites/<run_id>/` (run-id auto-generated or via `--run-id`)
- Extracts clean curated CSV and wall-time metrics into `summary.csv`
- Optionally promotes curated results to `analysis/benchmarks/canonical/<name>/`

**Output structure:**
```
analysis/benchmarks/suites/<run_id>/
  raw/              — transient stdout/stderr (ignored by git)
  curated/          — clean CSV per target (suitable for promotion)
  summary.csv       — aggregated wall-time + median metrics
```

**Example with custom run-id:**

```bash
scripts/bench/run_benchmarks.sh --mode suite --run-id my_optimization_attempt_v1 --iter 1
```

### Interpreting Benchmark Output

**Polymul CSV (`*_polymul_N1.csv`):**
- Primary metric: `median_instret` (instruction count)
- Secondary: `median_cycles` (closely tracks instret on Spike)
- Focus: `vect_mul` row (the dominant hotspot)

**Full KEM CSV (`*_N1.csv`):**
- Rows: `keypair`, `encaps`, `decaps`
- Metric: `median_instret` for comparison
- Use for: End-to-end phase checkpoints

---

## 4. Profiling & Tracing

### Static Microarchitectural Analysis (llvm-mca)

**Quick run on hotspot source:**

```bash
./scripts/verify/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c 128
```

Output: `analysis/llvm_mca/reports/gf2x_mca.txt`

**Notes:**
- Helper script includes `--skip-unsupported-instructions=lack-sched` to suppress unsupported marker warnings
- Only analyze known-hot sources (e.g., `gf2x.c`, `polymul.c`); some sources produce low-value output
- Output shows predicted throughput bottlenecks and instruction latencies

**Example interpretation:**
- "Throughput: 2.5 cycles" indicates the loop body takes ~2.5 cycles to retire
- "Critical resource: LSUnit" suggests memory bandwidth is the bottleneck
- "Instruction latency" table shows latencies for individual instructions

### Instruction Mix & Histogram

**Generate histogram (launch and wait):**

```bash
scripts/bench/run_profile.sh --histogram --bench-set polymul_zbc --variant karatsuba_ref --wait
```

**Report top instructions from existing histograms:**

```bash
scripts/bench/run_profile.sh --histogram --bench-set polymul_zbc --report --variant karatsuba_ref --top 20
```

Output files:
- `analysis/profiles/histograms/<track>/<bench_set>/hqc{128,192,256}_<variant>_polymul_N1.csv` (cycle data)
- `analysis/profiles/histograms/<track>/<bench_set>/hqc{128,192,256}_<variant>_hist.txt` (instruction frequency)

**Report output example:**

```text
=== hqc128_karatsuba_zbc (polymul_zbc) ===
  mnemonic                            count      %
  --------                            -----      -
  clmul                              123456   22.1%
  clmulh                             98765   17.7%
  vclmul.vv                          54321    9.8%
  ...
```

**Use cases:**
- Validate scalar → CLMUL → vclmul transition
- Identify new instruction mix after optimization
- Profile different security levels and variants for comparison

### Cache Profiling (Simulation)

Analyze cache behavior on Spike's simulated cache hierarchy:

```bash
scripts/bench/run_profile.sh --cache --bench-set polymul_zbc --variant karatsuba_ref --report
```

Output: Per-level cache hit rates, miss counts, and memory stall estimates.

**Note:** For authoritative hardware cache behavior, run on Banana Pi with `perf`:
```bash
perf stat -e L1-dcache-loads,L1-dcache-load-misses ./dudect_cross_hqc128_fafft_zbc.elf
```

---

## 5. Reporting & Aggregation

### Extract Results Tables

**Extract all benchmark results to CSV tables:**

```bash
python3 scripts/report/extract_results.py
```

**For specific benchmark set:**

```bash
python3 scripts/report/extract_results.py --bench-set polymul_zbc
```

### Generate LaTeX Tables (for Thesis)

```bash
python3 scripts/report/extract_results.py --latex \
  > analysis/report/table_results.tex
```

### Include Instruction Mix in Summary

```bash
python3 scripts/report/extract_results.py --mix --bench-set polymul_zbc
python3 scripts/report/extract_results.py --write-summary-csv
```

Generated summary files:
- `analysis/benchmarks/cycle_counts.csv`
- `analysis/profiles/traces/instruction_mix.csv`

### Generate Dated Report (Markdown + LaTeX)

```bash
scripts/report/gen_report.sh
scripts/report/gen_report.sh --bench-set polymul_zbc
scripts/report/gen_report.sh --fraction 0.85
```

Generated artifacts:
- `analysis/report/report_YYYY-MM-DD.md`
- `analysis/report/table_results_YYYY-MM-DD.tex`

**Notes:**
- `gen_report.sh` is soft-fail by design (missing data will not halt execution)
- `plot.py` step is skipped if plotting dependencies or data are missing

---

## 6. Static Analysis & Verification

### Linting & Static Checks

Run unified linting locally before pushing to CI:

```bash
bash scripts/ci/lint.sh
```

**What it does:**

1. Reads `build/qemu/compile_commands.json` to find all compiled C files
2. Excludes VLEN-specific compilation units (`-mrvv-vector-bits` variants)
3. Runs `clang-tidy` with `bugprone-*` and `cert-*` checks
4. Treats all warnings as errors (`--warnings-as-errors`)
5. Injects `-march=rv64gcv_zbc_zbb_zbkb` to strip unsupported extensions (zvbc, etc.) for host clang-tidy

**Suppressed checks (project-wide):**

| Check | Reason |
|---|---|
| `bugprone-easily-swappable-parameters` | False positives on fixed-signature crypto functions |

**Secure zeroization rule (CERT-MSC12-C):**

Secret key material must use `memset_zero(ptr, len)` (from `vendor/hqc-ref/src/common/crypto_memset.h`),
never plain `memset()`. The macro routes through a `volatile` function pointer to prevent dead-store elimination.

**Configuration knobs:**

```bash
CLANG_TIDY=/path/to/clang-tidy bash scripts/ci/lint.sh
BUILD_DIR=build/spike bash scripts/ci/lint.sh
```

**Expected clean output:**

```text
(no warnings — script exits 0)
```

If clang-tidy is not installed:

```bash
sudo apt-get install clang-tidy    # Ubuntu / Debian
```

---

## 7. Debugging & Development Utilities

### Disassembly & Symbol Inspection

**Full disassembly with source interleave (requires DWARF debug info):**

```bash
llvm-objdump -d --source build/spike/bench_hqc128_karatsuba_ref.elf | less
```

**List all defined symbols sorted by size:**

```bash
llvm-nm --print-size --defined-only --size-sort build/spike/bench_hqc128_karatsuba_ref.elf | tail -20
```

**Find instruction patterns (e.g., CLMUL variants):**

```bash
llvm-objdump -d build/spike/bench_hqc128_karatsuba_ref.elf | grep -E "clmul|packh|vclmul"
```

**Identify functions containing vector instructions:**

```bash
llvm-objdump -d build/spike/bench_hqc128_karatsuba_ref.elf | \
  awk '/^[0-9a-f]+ </{fn=$0} /^\s+v/{print fn; fn=""}'
```

---

## Appendix: QEMU Fallback & Legacy Notes

### QEMU-Only Benchmark Sanity Check

**Purpose:** Validate bench harness CSV format on QEMU (not for thesis performance results).

```bash
cmake --build build/qemu --target bench_hqc128_karatsuba_ref -j$(nproc)

CPU="rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true"
$QEMU -machine virt -cpu "$CPU" -m 256M -nographic -bios none \
  -kernel build/qemu/bench_hqc128_karatsuba_ref.elf \
  -serial mon:stdio -no-reboot 2>/dev/null
```

**Expected output:**
- CSV header: `op,iterations,min,max,median_cycles,median_instret,...`
- Data rows: `keypair,...`, `encaps,...`, `decaps,...`
- Note: QEMU cycle counts are not accurate architectural instruction counts (use Spike for thesis results)

### QEMU KAT Fallback

```bash
./scripts/test/run_kat.sh --sim qemu build/qemu/hqc128_karatsuba_ref.elf tests/kat/PQCkemKAT_128.rsp
```

Expected: `PASS 100/100`

### Why Spike is Primary, QEMU is Secondary

- **Spike:** Accurate architectural instruction count and cycle accounting. Primary tool for thesis benchmarks.
- **QEMU:** Approximate cycle simulation, higher overhead. Use only for fallback validation and correctness checks.

---

## KAT Seed Generation (One-Time Setup)

The headers under `tests/kat/generated/` are pre-committed and normally need no regeneration.
Regenerate **only** if upstream `.rsp` files change:

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

**Important:** The vendor KEM uses SHAKE-256 PRNG (`prng_get_bytes`), seeded via `prng_init(seed, NULL, 48, 0)`.
It does **not** use `randombytes()` / AES-CTR DRBG. Always seed with `prng_init`, never `randombytes_init`.

---

## Quick Decision Table

| Question | Command | Output |
|---|---|---|
| Did correctness break? | `scripts/test/run_kat.sh --sim spike build/spike/hqc128_karatsuba_ref.elf tests/kat/PQCkemKAT_128.rsp` | `PASS 100/100` |
| All variants pass KAT? | `ctest --test-dir build/spike -R kat` | All tests pass |
| Did polymul instret improve? | `scripts/bench/run_benchmarks.sh --mode async polymul_zbc` then check `*_polymul_N1.csv` | `median_instret` value |
| Full KEM end-to-end? | `scripts/bench/run_benchmarks.sh --mode async polymul_zbc --full` then check `*_N1.csv` | `keypair, encaps, decaps` rows |
| Static throughput hints? | `./scripts/verify/run_mca.sh vendor/hqc-ref/src/ref/gf2x.c 128` | `*_mca.txt` report |
| Instruction mix by mnemonic? | `scripts/bench/run_profile.sh --histogram --bench-set polymul_zbc --report --top 20` | `*_hist.txt` + terminal report |
| Aggregate thesis tables? | `python3 scripts/report/extract_results.py --latex` | `table_results.tex` |

---

## Verified on This Repository State

**Date validated:** May 11, 2026

**Validated working commands:**
- Prerequisites & setup (`source toolchain/toolchain.env`, command checks)
- Build system: `cmake --build build/spike`, `cmake --build build/qemu`
- Correctness gates: single KAT with standardized `tests/kat/` path, batch `ctest --test-dir build/spike -R kat`
- Benchmarking suite: `scripts/bench/run_benchmarks.sh --mode async` and `--mode suite`
- Profiling & tracing: `scripts/verify/run_mca.sh`, `scripts/bench/run_profile.sh --histogram`
- Reporting: `extract_results.py`, `gen_report.sh`
- Static analysis: `scripts/ci/lint.sh`
- Debugging utilities: `llvm-objdump`, `llvm-nm` examples

**New features validated:**
- `run_benchmarks.sh` `--mode async` and `--mode suite` (replaces raw Spike bash blocks)
- `ctest --test-dir build/spike -R kat` (fixes "No tests were found!!!" bug)
- Standardized RSP paths `tests/kat/PQCkemKAT_{128,192,256}.rsp` (verified as symlinks)
- Two benchmark target explanation and use cases
- Clarification of CMake reconfiguration conditions

**Known caveats:**
- `spike --histogram` is unsupported; use `scripts/bench/run_profile.sh --histogram`
- `scripts/test/run_kat.sh --sim spike` can be long-running on complex variants; use `--sim qemu` for faster non-Zvbc checks
- QEMU cycle counts are approximate; use Spike for thesis results

---

## Architecture & Optimization History

For architectural decisions, design rationale, and optimization phases, see [docs/METHODOLOGY.md](METHODOLOGY.md).
For detailed project structure and phase planning, see [docs/PLAN.md](PLAN.md).

