# Dudect Constant-Time Verification Harnesses

Hardware-based constant-time verification of HQC RISC-V implementations
using the dudect methodology (Reparaz et al., 2016). Runs natively on the
Banana Pi BPI-F3 (SpacemiT K1, rv64gcv) using `perf_event_open` for
cycle-accurate timing.

## Prerequisites

- GCC 14.2+ (native on the board)
- Linux kernel with `perf_event_open` support
- CPU frequency governor set to `performance`
- Root access for process isolation (`nice -n -20`, `taskset`)

## Variant Matrix

| Variable   | Values                                          |
|------------|-------------------------------------------------|
| `TARGET`   | `polymul` (vect_mul) · `kem` (crypto_kem_dec)   |
| `LEVEL`    | `128` · `192` · `256`                           |
| `POLYMUL`  | `ref_scalar` · `karatsuba_zbc` · `karatsuba_rvv`|

ISA flags are set automatically per variant:

| Variant          | `-march`                          | Extra flags            |
|------------------|-----------------------------------|------------------------|
| `ref_scalar`     | `rv64gc_zbc_zbb_zbs`             | `-fno-tree-vectorize`  |
| `karatsuba_zbc`  | `rv64gc_zbc_zbb_zbs`             | `-fno-tree-vectorize`  |
| `karatsuba_rvv`  | `rv64gcv_zbc_zbb_zba_zbs`        | —                      |

## MODE Flag

The `MODE` variable controls run duration:

| Mode      | `N_MAX`     | `BATCH_SIZE` | Use case                          |
|-----------|-------------|--------------|-----------------------------------|
| `dev`     | 100,000     | 5,000        | Fast iteration during development |
| `publish` | 10,000,000  | 10,000       | Thesis-quality evidence           |

Default: `MODE=dev`.

## Build Commands

All commands are run from `tests/ct_dudect/`.

### Single-variant build

```bash
make -f Makefile.hw TARGET=polymul LEVEL=128 POLYMUL=karatsuba_zbc
```

Produces: `dudect_polymul_hqc128_karatsuba_zbc`

### Build with publish mode

```bash
make -f Makefile.hw TARGET=kem LEVEL=128 POLYMUL=ref_scalar MODE=publish
```

Produces: `dudect_kem_hqc128_ref_scalar` compiled with N_MAX=10000000 and
BATCH_SIZE=10000.

### Build all variants for a given target and level

```bash
make -f Makefile.hw all_variants TARGET=polymul LEVEL=128 MODE=dev
```

Builds `ref_scalar`, `karatsuba_zbc`, and `karatsuba_rvv` for the
specified target and level.

### Clean

```bash
make -f Makefile.hw clean
```

Removes all `dudect_polymul_hqc*` and `dudect_kem_hqc*` binaries.

### Show configuration

```bash
make -f Makefile.hw info
```

Prints the resolved values of all configuration variables.

## Running

### Basic run with process isolation

The `run` target executes the binary pinned to core 7 at highest priority:

```bash
make -f Makefile.hw run TARGET=polymul LEVEL=128 POLYMUL=karatsuba_zbc
```

Equivalent to:

```bash
sudo nice -n -20 taskset -c 7 ./dudect_polymul_hqc128_karatsuba_zbc
```

### Output directory override (`--outdir`)

By default, CSV results are written to `results/ct_verify/dudect/` (relative
to the repository root). Override with:

```bash
./dudect_polymul_hqc128_karatsuba_zbc --outdir /tmp/my_results
```

The directory is created automatically if it does not exist.

### Publish-mode run (thesis-quality)

```bash
make -f Makefile.hw TARGET=polymul LEVEL=128 POLYMUL=karatsuba_zbc MODE=publish
sudo nice -n -20 taskset -c 7 ./dudect_polymul_hqc128_karatsuba_zbc
```

This collects up to 10,000,000 measurements (≈ 1000 batches of 10,000).

## Reproducibility (`SEED` macro)

For deterministic, reproducible runs, pass a fixed PRNG seed at compile time:

```bash
make -f Makefile.hw TARGET=polymul LEVEL=128 POLYMUL=karatsuba_zbc SEED=0xDEADBEEF
```

When `SEED` is set, the harness uses the given value instead of reading
`/dev/urandom`. The seed is always printed to stderr at startup regardless
of source, so any run can be replayed:

```
[dudect] PRNG seed: fixed = 0x00000000deadbeef
```

Without `SEED`, the harness seeds from `/dev/urandom` and prints the
resulting 4×64-bit state:

```
[dudect] PRNG seed: a1b2c3d4e5f60718 091a2b3c4d5e6f70 1122334455667788 99aabbccddeeff00
```

## T-Threshold Override

Override the default t-statistic threshold (4.5) with `T_THRESH`:

```bash
make -f Makefile.hw TARGET=polymul LEVEL=128 POLYMUL=karatsuba_zbc T_THRESH=5.0
```

## Output Format

### CSV file

Written to `results/ct_verify/dudect/dudect_{target}_hqc{level}_{variant}.csv`:

```csv
batch,n_measurements,t_statistic_first_order,t_statistic_second_order
1,5000,0.123456,0.089012
2,10000,0.234567,0.156789
...
20,100000,1.234567,0.987654
```

Each row is flushed immediately so partial results survive interruption.

### Stderr progress

```
[dudect] target=vect_mul variant=karatsuba_zbc level=128
[dudect] PRNG seed: a1b2c3d4...
[dudect] CPU governor: performance ✓
[dudect] batch  1: n=  5000  t1= 0.123  t2= 0.089
[dudect] batch  2: n= 10000  t1= 0.234  t2= 0.156
...
```

### Final verdict

```
[dudect] RESULT: no leakage detected (n=100000, t1=1.23, t2=0.98)
```

or

```
[dudect] RESULT: LEAKAGE DETECTED (n=35000, t1=5.12, t2=4.87)
```

## Interpreting Results

| Verdict                  | Meaning                                                        |
|--------------------------|----------------------------------------------------------------|
| **no leakage detected**  | After N_MAX measurements, both t-statistics stayed below 4.5. The implementation shows no detectable timing dependence on secrets. |
| **LEAKAGE DETECTED**     | At least one t-statistic exceeded 4.5 before reaching N_MAX. The implementation has a statistically significant timing correlation with secret data. |

**Guidelines:**

- A **pass** does not guarantee constant-time behavior — it means no leakage
  was detected at the given measurement budget and confidence level.
- A **leakage** verdict is a strong signal. Check `t1` (first-order, raw) vs
  `t2` (second-order, percentile-cropped) to understand the leakage source:
  - High `t1` only → leakage visible in raw data (likely branch-level).
  - High `t2` only → leakage hidden by noise, revealed after cropping outliers.
  - Both high → strong, consistent leakage.
- Use `MODE=publish` for definitive conclusions (10M measurements).
- Use the CSV output to plot convergence curves of t-statistics over time.

## File Layout

```
tests/ct_dudect/
├── Makefile.hw              # Standalone build system
├── dudect_polymul.c         # Polymul harness (vect_mul)
├── dudect_kem.c             # KEM harness (crypto_kem_dec)
├── dudect_ttest.h           # Streaming Welch's t-test (header-only)
├── dudect_percentile.h      # Percentile cropping (header-only)
├── dudect_prng.h            # xoshiro256** PRNG (header-only)
└── README.md                # This file
```
