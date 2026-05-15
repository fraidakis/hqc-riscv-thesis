# HQC-RISC-V Workflow Runbook

Single source of truth for the current build, correctness, benchmarking, and profiling workflow.

For rationale and measurement philosophy, see [docs/METHODOLOGY.md](METHODOLOGY.md).

## 0. Environment

```bash
cd ~/Thesis/hqc-riscv
source toolchain/toolchain.env
```

## 1. Variant Status (Current Ground Truth)

- Primary scalar baseline: karatsuba_zbc
- Primary vector baseline: karatsuba_zvbc
- Active comparison track: hybrid
- Placeholder tracks: fafft_zbc and fafft_zvbc are currently clean slates and not used as primary baselines.

Implemented runnable variants are listed in [scripts/implemented_variants.txt](../scripts/implemented_variants.txt).

## 2. Correctness Pipeline (KAT)

Use the unified KAT runner directly:

```bash
./scripts/test/run_kat.sh --variant <name> --level <128|192|256|all> [--spike]
```

Examples:

```bash
# Default simulator path (QEMU)
./scripts/test/run_kat.sh --variant karatsuba_zvbc --level 128

# Full matrix for one variant
./scripts/test/run_kat.sh --variant karatsuba_zbc --level all

# Optional Spike fallback
./scripts/test/run_kat.sh --variant karatsuba_zvbc --level 128 --spike
```

Notes:

- QEMU is now the default KAT simulator because it executes heavy Zvbc rejection-sampling paths much faster in this environment.
- Spike remains available as an explicit fallback with --spike.
- KAT references are read from tests/kat/PQCkemKAT_128.rsp, tests/kat/PQCkemKAT_192.rsp, and tests/kat/PQCkemKAT_256.rsp.

## 3. Benchmarking Pipeline (Stateless)

The canonical benchmark/profiling entrypoint is:

```bash
./scripts/report/run_full_analysis.sh --variant <name> [--level <128|192|256|all>] [--no-clean]
```

Examples:

```bash
./scripts/report/run_full_analysis.sh --variant karatsuba_zbc --level all
./scripts/report/run_full_analysis.sh --variant karatsuba_zvbc --level 128
```

Pipeline behavior:

- Uses native sequential isolated executions only.
- Calls run_benchmarks.sh with --isolated for polymul and --isolated --kem for full KEM.
- Runs cache, memory, trace, and histogram profiling through run_profile.sh.
- Aggregates results into a single Markdown report under analysis/reports/.
- Reads and writes through canonical isolated paths under analysis/benchmarks/isolated/<track>/<variant>/.

### VLEN Scaling Matrix Benchmarks

Use per-VLEN polymul micro-benchmark targets to evaluate RVV scaling for the same algorithm across fixed vector widths.

Build a specific VLEN target (example: HQC-128, karatsuba_zbc_rvv, VLEN=512):

```bash
cmake --build build/bench --target bench_polymul_hqc128_karatsuba_zbc_rvv_vlen512
```

When testing wider vectors on Spike, invoke Spike directly with ISA `zvl<VLEN>b` so the emulated machine width matches the benchmark target. Some Spike builds do not support `--varch`, so the ISA suffix is the portable method.

Example (VLEN=512):

```bash
$RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc_zvl512b build/bench/bench_polymul_hqc128_karatsuba_zbc_rvv_vlen512.elf
```

## 4. Direct Isolated Runs (Fast Iteration)

For quick local iteration outside the full wrapper:

```bash
# Polymul isolated
./scripts/bench/run_benchmarks.sh --isolated --variant karatsuba_zbc --level all

# Full KEM isolated
./scripts/bench/run_benchmarks.sh --isolated --kem --variant karatsuba_zbc --level all
```

## 5. Profiling Commands

```bash
# Function-level trace
./scripts/bench/run_profile.sh --trace --variant karatsuba_zbc --level 128

# Line-level trace
./scripts/bench/run_profile.sh --trace --lines --variant karatsuba_zbc --level 128

# Histogram
./scripts/bench/run_profile.sh --histogram --variant karatsuba_zbc --level 128

# Cache and memory profiling
./scripts/bench/run_profile.sh --cache --variant karatsuba_zbc --level all
./scripts/bench/run_profile.sh --memory --variant karatsuba_zbc --level all
```

## 6. Reporting

```bash
python3 scripts/report/extract_results.py
python3 scripts/report/extract_results.py --latex > analysis/report/table_results.tex
```

## 7. Quick Checks

| Question | Command |
|---|---|
| Did correctness break? | ./scripts/test/run_kat.sh --variant karatsuba_zbc --level 128 |
| Did all levels pass for a variant? | ./scripts/test/run_kat.sh --variant karatsuba_zbc --level all |
| Need faster vector-KAT loop? | ./scripts/test/run_kat.sh --variant karatsuba_zvbc --level 128 |
| Need deterministic ISS fallback? | ./scripts/test/run_kat.sh --variant karatsuba_zvbc --level 128 --spike |
| Run full stateless analysis? | ./scripts/report/run_full_analysis.sh --variant karatsuba_zbc --level all |

## 8. Notes

- Keep Spike for deterministic instruction-count benchmarking and profiling.
- Keep QEMU as default for KAT correctness throughput.
- Prefer the stateless full-analysis wrapper for reproducible thesis artifacts.

---

## 9. Running the Hardware-Accelerated Tracks

Two definitive execution pairings have been validated across all three HQC security levels.
Both pair a vector polynomial multiplier with a vector Keccak implementation.

### Track B — Base RVV (no Vector Crypto extension)

- **Polynomial multiply:** `src/poly_mul/vector/rvv_only/karatsuba_zbc/gf2x_clmul.c`
- **Keccak / SHAKE:** `src/keccak/fips202_rvv_only.c`
- **ISA:** `rv64gcv_zbc_zbb_zbkb_zicntr_zihpm` (no Zvbc/Zvbb required)

```bash
cmake -S . -B build/track_b \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
  -DRISCV=$RISCV \
  -DHQC_KECCAK_IMPL=rvv_only \
  -DHQC_SIM=spike
./scripts/report/run_full_analysis.sh --build-dir build/track_b --variant karatsuba_zbc_rvv
```

### Track C — Vector Crypto (Zvbc + Zvbb, winning configuration)

- **Polynomial multiply:** `src/poly_mul/vector/zvbc/karatsuba_zvbc/gf2x_vclmul.c`
- **Keccak / SHAKE:** `src/keccak/fips202_zvbb_gather.c`
- **ISA:** `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm`
- **Result (HQC-128 decaps):** 1,225,209 instructions — 2.5% fewer than Track B scalar Keccak
- **Note:** `zvl256b` is mandatory; LMUL=8/e64 requires VLEN≥256 to hold all 25 Keccak-f lanes

```bash
cmake -S . -B build/track_c \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
  -DRISCV=$RISCV \
  -DHQC_KECCAK_IMPL=zvbb_gather \
  -DHQC_SIM=spike
./scripts/report/run_full_analysis.sh --build-dir build/track_c --variant karatsuba_zvbc
```

