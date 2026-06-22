# Hardware Benchmark Harness (BPI-F3 / SpacemiT X60)

## Overview

Linux userspace benchmark harness for HQC on real RISC-V hardware.
Uses `perf_event_open` with `exclude_kernel=1` for per-region cycle and
instruction counting.  Supports optional extended PMU counters (L1d misses,
DTLB misses, branch mispredictions, pipeline stalls, vector instruction
counts) via the SpacemiT X60 platform PMU.

## Files

- `perf_hw.h` — `perf_event_open` wrapper (header-only); basic (cycles + instret) and extended PMU APIs
- `bench_common.h` — Shared header with platform-conditional includes, helper functions (poly init, median, anti-DCE)
- `bench_kem.c` — Full KEM benchmark (KeyGen, Encaps, Decaps); dual-platform (sim + hw)
- `bench_mul.c` — Isolated polynomial multiplication (`vect_mul`) micro-benchmark
- `bench_keccak.c` — SHAKE-256/SHA-3 (Keccak) micro-benchmark replicating the full decaps symmetric call pattern
- `bench_decode.c` — Concatenated Reed-Muller + Reed-Solomon decode micro-benchmark
- `Makefile.hw` — Standalone Makefile for native compilation on the board (no CMake required)

## Quick Start (on the board)

```bash
# 1. Clone/rsync the repo to the board
rsync -avz --exclude='.venv' --exclude='build*' . bpi-f3:~/hqc-riscv/

# 2. SSH into the board
ssh bpi-f3

# 3. Build all targets for HQC-128 (default variant: karatsuba_zbc)
cd ~/hqc-riscv/tests/bench
make -f Makefile.hw LEVEL=128 all

# 4. Set performance governor
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 5. Run polymul micro-benchmark (all variants)
make -f Makefile.hw LEVEL=128 run_all_mul

# 6. Run full KEM benchmark (single variant)
make -f Makefile.hw LEVEL=128 POLYMUL=karatsuba_zbc run_kem

# 7. Run Keccak micro-benchmark
make -f Makefile.hw LEVEL=128 run_keccak

# 8. Run decode micro-benchmark
make -f Makefile.hw LEVEL=128 run_decode
```

## Build Targets

| Target | Binary | Description |
|--------|--------|-------------|
| `kem` | `bench_hw_kem_hqc<L>_<P>` | Full KEM (KeyGen + Encaps + Decaps) |
| `mul` | `bench_hw_mul_hqc<L>_<P>` | Isolated `vect_mul` micro-benchmark |
| `keccak` | `bench_hw_keccak_hqc<L>` | Decaps Keccak call-pattern micro-benchmark |
| `decode` | `bench_hw_decode_hqc<L>` | RM + RS decode micro-benchmark |
| `all` | all of the above | Build everything for current POLYMUL |
| `all_variants` | — | Build `mul`+`kem` for all three POLYMUL variants |

## Build Options

| Variable | Default | Description |
|----------|---------|-------------|
| `CC` | `gcc` | Compiler (GCC 14.2 on the board) |
| `LEVEL` | `128` | Security level: 128, 192, 256 |
| `POLYMUL` | `karatsuba_zbc` | Variant: ref_scalar, karatsuba_zbc, karatsuba_rvv |
| `MODE` | `dev` | `dev` (100 iter, fast) or `publish` (10000 iter, thesis-quality) |
| `N_ITER` | 100 (dev) / 10000 (publish) | Measured iterations |
| `N_WARMUP` | 10 (dev) / 100 (publish) | Warm-up iterations (discarded) |

## Output Format

CSV compatible with `scripts/report/extract_results.py`:

```
# level=128,polymul=karatsuba_zbc,n_iter=10000,n_warmup=100,platform=hw
op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret
keypair,10000,123456,124000,130000,124100.5,500.2,100000,100100,100200,100100.0,50.1
encaps,10000,...
decaps,10000,...
```

With `-DPERF_HW_EXTENDED=1`, additional columns are appended:

```
...,median_l1d_miss,median_l1d_access,l1d_miss_rate,median_dtlb_miss,median_br_mispred,median_stall_fe,median_stall_be,median_vec_inst,median_vpu_stall
```

## Measurement Methodology

- **Counter**: `perf_event_open` with `PERF_COUNT_HW_CPU_CYCLES` and `PERF_COUNT_HW_INSTRUCTIONS`
- **Extended PMU**: SpacemiT X60 raw events (type=4) for cache/TLB/stall/vector counters
- **Two-pass approach** (with `PERF_HW_EXTENDED`): basic counters in pass 1 for clean cycle/instret, extended counters in pass 2 to avoid multiplexing noise
- **Isolation**: `exclude_kernel=1`, `exclude_hv=1` — measures only userspace
- **Per-iteration**: Each iteration gets its own reset/enable/disable/read cycle
- **Statistics**: Reports min, median, max, mean, stddev
- **Warm-up**: N_WARMUP iterations run before measurement begins
- **Correctness**: One decaps verification pass before timing starts (KEM bench)
- **System check**: Warns if governor != performance, errors if counters return 0
- **Raw dump**: Pass `--raw-dir <dir>` to dump per-iteration cycle/instret pairs to text files

## Running with Proper Isolation

```bash
# Best: elevated priority + pinned to isolated core
sudo nice -n -20 taskset -c 7 ./bench_hw_kem_hqc128_karatsuba_zbc

# With raw dump for offline analysis
sudo nice -n -20 taskset -c 7 ./bench_hw_kem_hqc128_karatsuba_zbc --raw-dir ./results/

# Or via Makefile (already uses nice + taskset)
make -f Makefile.hw LEVEL=128 POLYMUL=karatsuba_zbc run_kem
```

## Verifying Instruction Emission

After building, verify the compiler emitted the expected instructions:

```bash
# Check for Zbc carry-less multiply
objdump -d bench_hw_mul_hqc128_karatsuba_zbc | grep -c "clmul"

# Check for RVV vector instructions
objdump -d bench_hw_mul_hqc128_karatsuba_rvv | grep -cE "vle|vse|vxor|vslide"
```

## Comparison with Spike Results

The hardware benchmark produces actual CPU cycles (with pipeline effects).
Compare against Spike instruction counts to compute IPC:

```
IPC = Spike_instret / HW_cycles
```

- IPC > 1.0 for RVV code → superscalar/vector benefit
- IPC < 1.0 for scalar code → pipeline stalls, cache misses
