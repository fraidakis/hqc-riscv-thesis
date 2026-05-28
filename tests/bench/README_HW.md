# Hardware Benchmark Harness (BPI-F3 / SpacemiT X60)

## Overview

Linux userspace benchmark harness for HQC on real RISC-V hardware.
Uses `perf_event_open` with `exclude_kernel=1` for per-region cycle and
instruction counting.

## Files

- `perf_hw.h` — `perf_event_open` wrapper (header-only)
- `bench_hw_kem.c` — Full KEM benchmark (KeyGen, Encaps, Decaps)
- `bench_hw_mul.c` — Isolated polynomial multiplication micro-benchmark
- `Makefile.hw` — Standalone Makefile for native compilation on the board

## Quick Start (on the board)

```bash
# 1. Clone/rsync the repo to the board
rsync -avz --exclude='.venv' --exclude='build*' . bpi-f3:~/hqc-riscv/

# 2. SSH into the board
ssh bpi-f3

# 3. Build all variants for HQC-128
cd ~/hqc-riscv/tests/bench
make -f Makefile.hw LEVEL=128 all_variants

# 4. Set performance governor
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 5. Run polymul micro-benchmark (all variants)
make -f Makefile.hw LEVEL=128 run_all_mul

# 6. Run full KEM benchmark (single variant)
make -f Makefile.hw LEVEL=128 POLYMUL=karatsuba_zbc run_kem
```

## Build Options

| Variable | Default | Description |
|----------|---------|-------------|
| `CC` | `gcc` | Compiler (GCC 14.2 on the board) |
| `LEVEL` | `128` | Security level: 128, 192, 256 |
| `POLYMUL` | `karatsuba_zbc` | Variant: ref_scalar, karatsuba_zbc, karatsuba_rvv |
| `N_ITER` | `10000` | Measured iterations |
| `N_WARMUP` | `100` | Warm-up iterations (discarded) |

## Output Format

CSV compatible with `scripts/report/extract_results.py`:

```
# level=128,polymul=karatsuba_zbc,n_iter=10000,n_warmup=100,platform=hw
op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret
keypair,10000,123456,124000,130000,124100.5,500.2,100000,100100,100200,100100.0,50.1
encaps,10000,...
decaps,10000,...
```

## Measurement Methodology

- **Counter**: `perf_event_open` with `PERF_COUNT_HW_CPU_CYCLES` and `PERF_COUNT_HW_INSTRUCTIONS`
- **Isolation**: `exclude_kernel=1`, `exclude_hv=1` — measures only userspace
- **Per-iteration**: Each iteration gets its own reset/enable/disable/read cycle
- **Statistics**: Reports min, median, max, mean, stddev
- **Warm-up**: N_WARMUP iterations run before measurement begins
- **Correctness**: One decaps verification pass before timing starts
- **System check**: Warns if governor != performance, errors if counters return 0

## Running with Proper Isolation

```bash
# Best: pin to isolated core with real-time priority
sudo chrt -f 99 taskset -c 7 ./bench_hw_kem_hqc128_karatsuba_zbc

# Good: elevated priority + pinned
sudo nice -n -20 taskset -c 7 ./bench_hw_kem_hqc128_karatsuba_zbc

# Minimum: just pinned
taskset -c 7 ./bench_hw_kem_hqc128_karatsuba_zbc
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
