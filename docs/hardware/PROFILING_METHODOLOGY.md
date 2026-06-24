# Hardware Profiling Methodology — BPI-F3 (SpacemiT X60)

## Overview

This document describes the hardware profiling methodology for the HQC-RISC-V
implementation on the BPI-F3 board (SpacemiT K1 SoC, 8× X60 cores, RV64GCV(B),
RVV 1.0, VLEN=256, 1.6 GHz fixed frequency).

The profiling produces three complementary data sets:
1. **Counters** — per-operation microarchitectural metrics via `perf_event_open`
2. **Hotspots** — function-level cycle attribution via `perf record -e task-clock`
3. **ISA mix** — static instruction composition via `objdump`

## Metric Split: Benchmarks vs Profiling

**`run_bench.sh --platform hw`** = performance results (lean, fast):
- Builds with `EXTENDED=0` (basic pass only)
- Emits: cycles (median, min), instructions, IPC
- Purpose: thesis headline numbers, cross-validation with Spike instret

**`run_profile.sh --platform hw`** = diagnostic / microarchitectural (EXTENDED):
- Builds with `EXTENDED=1` (two-pass: basic then extended counters)
- Emits: all counters + derived metrics (miss rates, MPKI, stall %, vector %)
- Purpose: explain WHY performance differs between variants

This split keeps bench runs fast and their cycle numbers clean (no extended-pass
multiplexing overhead), while profiling provides full diagnostic detail.

## Tooling Audit (verified 2026-06-24)

| Tool | Found? | Path/Version | Decision |
|------|--------|-------------|----------|
| `perf` (counting) | ✅ | `/usr/bin/perf` v6.6.63 | SBI PMU: type=4, 18 HW counters |
| `perf record -e task-clock` | ✅ | Software-event sampling, hrtimer-driven | **Primary hotspot method** |
| `perf record -e cycles` | ❌ | 0 samples (PMI non-functional) | Not used |
| `python3` | ✅ | Python 3.13.3 | Derived metrics, parsing |
| `objdump` / `nm` / `addr2line` | ✅ | GNU Binutils 2.44 | ISA mix, symbol resolution |
| `gprof` | ✅ | GNU gprof 2.44 | **Not used** (superseded by task-clock) |
| FlameGraph scripts | ✅ | `~/FlameGraph/` (Perl) | SVG flame graphs |
| `taskset` | ✅ | `/usr/bin/taskset` | Core pinning |
| Governor sysfs | ✅ | Set to `performance` (1.6 GHz fixed) | Used |
| `perf_event_paranoid` | ✅ | Set to 1 | Sufficient for userspace counters |
| GCC | ✅ | 14.2.0 | Native compilation on board |
| Kernel | ✅ | 6.6.63 riscv64 | Bianbu 3.0 |

### PMU Nuance: `sscofpmf` Advertised but Non-Functional

The `/proc/cpuinfo` ISA string includes `sscofpmf` (Supervisor Counter Overflow
PMU Filtering). However, `perf record -e cycles` consistently yields **0 samples**
— the counter-overflow interrupt (PMI) path is non-functional on this SoC/kernel
combination. This means hardware-event sampling is unavailable.

**Workaround:** Software-event sampling via `task-clock` (hrtimer-driven) works
correctly and produces thousands of samples per profiling run. This is our hotspot
method — it provides cycle-proportional function attribution without requiring
hardware PMI support.

## Profiling Modes

### 1. Counters (`--counters`)

Uses the binary's own `perf_event_open` harness (see `perf_hw.h`) to measure
per-operation medians across N iterations. Two-pass approach:
- Pass 1: cycles + instructions (basic, `PERF_TYPE_HARDWARE`)
- Pass 2: L1D miss, L1D access, dTLB miss, branch mispred, stall FE/BE,
  vector inst, VPU stall (raw PMU events, type=4)

**Derived metrics computed:**
| Metric | Formula |
|--------|---------|
| IPC | instructions / cycles |
| L1D miss rate | L1D_load_miss / L1D_access |
| L1D MPKI | (L1D_miss / instructions) × 1000 |
| dTLB MPKI | (dTLB_miss / instructions) × 1000 |
| Branch mispred PKI | (br_mispred / instructions) × 1000 |
| Stall frontend % | stall_FE / cycles |
| Stall backend % | stall_BE / cycles |
| Vector inst % | vector_inst / instructions (vector_rvv only) |
| VPU stall % | VPU_stall / cycles (vector_rvv only) |
| DRAM bytes est. | L1D_miss × 64 (upper bound) |
| Arithmetic intensity | instructions / DRAM_bytes |

```bash
scripts/bench/run_profile.sh --platform hw --counters --variant scalar_zb --level all --component mul
```

### 2. Hotspots (`--hotspots`)

Statistical function-level attribution via software-event sampling on a
**harness-free profiling binary** (`bench_profile_mul.c`, `bench_profile_kem.c`):

- Compiled at `-O3 -fno-omit-frame-pointer -g` — preserves inlining (no `-fno-inline`)
- **NO** `perf_event_open` harness, NO median/sort overhead
- Just a tight loop calling the target function (50K iterations for mul, 2K for kem)
- Profiled externally: `perf record -e task-clock -F 4999 --call-graph fp`

This eliminates the measurement harness contamination that plagued the old gprof
approach (where `ioctl` and `perf_hw_sort_u64` consumed 8–20% of the profile).

**Output:**
- CSV: `function,self_pct,samples` (filtered: no kernel, no libc, no system calls)
- SVG: FlameGraph with call-graph visualization

```bash
scripts/bench/run_profile.sh --platform hw --hotspots --variant scalar_zb --level all --component mul
```

### 3. ISA Mix (`--isa-mix`)

Static instruction-mix analysis from `objdump -d` on the real `-O3` binary.
Counts instructions by ISA extension category (Zbc, Zbb, Zba, Zbs, RVV, Zvbc)
and by memory/branch/arithmetic type.

Maps to the simulation PC-histogram analysis but provides the **static**
instruction composition rather than dynamic execution counts.

```bash
scripts/bench/run_profile.sh --platform hw --isa-mix --variant scalar_zb --level all --component mul
```

## Conceptual Mapping: Spike ↔ Hardware

| Spike (Simulation) | Hardware (BPI-F3) | Notes |
|--------------------|-------------------|-------|
| PC histogram → function attribution | task-clock hotspots | Spike = exact per-PC instret; HW = cycle-proportional statistical sampling |
| Total instret per operation | `instructions` from counters | Cross-validation bridge (should match closely) |
| N/A (no pipeline model) | cycles → **IPC** | Headline metric: IPC < 1 = stalls; IPC > 1 = superscalar/vector benefit |
| N/A | L1D/dTLB miss rates, MPKI | Memory-bound identification |
| N/A | stall_FE% / stall_BE% | Top-down-lite classification |
| N/A | vector_inst%, VPU_stall% | Vector utilization efficiency |
| objdump instruction count | ISA mix | Same binary, same static analysis |

## Key Findings (from real board data, dev quality, 2026-06-24)

### Polynomial Multiply (vect_mul) — Cycle Performance
| Variant | L128 | L192 | L256 |
|---------|------|------|------|
| scalar_ref | 11,640,420 | 35,374,299 | 88,403,322 |
| scalar_zb | 148,995 | 436,789 | 929,289 |
| vector_rvv | 125,570 | 363,358 | 849,470 |
| **scalar_zb speedup vs ref** | **78×** | **81×** | **95×** |
| **vector_rvv speedup vs scalar_zb** | **1.19×** | **1.20×** | **1.09×** |

### Hotspot Attribution (mul, L128, task-clock, harness-free)
| Function | scalar_zb | vector_rvv |
|----------|-----------|------------|
| clmul_leaf | 58.7% | — |
| kmul_18 | 6.1% | 44.0% |
| kmul_17 | 11.2% | 34.7% |
| kmul_35 | 6.0% | 7.7% |
| vect_mul | 10.4% | 3.4% |
| kmul_69 | 4.7% | 4.4% |

**Key insight:** In `scalar_zb`, the scalar `clmul` instruction is the bottleneck
(59% of cycles in `clmul_leaf`). In `vector_rvv`, the vector `vclmul` instructions
are fast enough that the Karatsuba overhead (data movement, XOR, slicing in
`kmul_17`/`kmul_18`) becomes the dominant cost (79% combined). The optimization
target shifts from "make clmul faster" to "reduce Karatsuba data movement."

### ISA Mix (mul binary, L128, static)
| Category | scalar_zb | vector_rvv |
|----------|-----------|------------|
| Zbc (clmul/clmulh) | 200 | 938 |
| RVV arithmetic | 0 | 69 |
| RVV load/store | 0 | 158 |
| RVV config (vsetvli) | 0 | 36 |
| Total instructions | 108,060 | 107,564 |

## Output Layout

```
results/hardware/profiling/
├── counters/<variant_dir>/
│   └── counters_<component>_L<level>_<slug>_latest.csv
├── hotspots/<variant_dir>/
│   ├── hotspots_<component>_L<level>_<slug>_latest.csv
│   └── hotspots_<component>_L<level>_<slug>_latest.svg
└── isa_mix/<variant_dir>/
    └── isa_mix_<component>_L<level>_<slug>_latest.csv
```

## Provenance

Every CSV includes comment headers documenting: date, git commit, variant, level,
component, quality preset, platform details, methodology notes, and sample counts.

## Known Limitations

1. **No per-instruction cycle annotation:** Hardware-event sampling is broken
   (`sscofpmf` non-functional). Function-level granularity from task-clock is the
   best available on this board.

2. **task-clock is wall-clock proportional, not cycle-exact:** It measures
   time-on-CPU including stalls. For compute-bound code (like polymul) this
   closely approximates cycle attribution. For I/O-bound code it would not.

3. **Frame-pointer call graphs vs DWARF:** DWARF unwinding fails on this riscv64
   build ("Fail to find IP/SP register"). Frame-pointer unwinding (`--call-graph fp`)
   works with `-fno-omit-frame-pointer`. This is the sole binary deviation from the
   pure benchmark build.

4. **Static ISA mix ≠ dynamic execution:** The objdump count reflects the binary's
   instruction composition, not execution frequency. Hot loops execute their
   instructions many times; cold initialization code runs once. Use Spike traces
   for dynamic PC-level attribution.

5. **gprof not used:** The old gprof approach (`-pg -fno-inline`) destroyed
   inlining and the `perf_event_open` ioctl harness leaked 8–20% into the profile.
   Software-event sampling on the real `-O3` binary is strictly superior. gprof
   remains installed as a theoretical fallback if task-clock ever breaks.
