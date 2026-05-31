# Benchmark & Scripts Restructuring Proposal

## Executive Summary

The benchmarking infrastructure has parallel scripts for every concern — one for simulation, one for hardware. This causes confusion, duplication, and maintenance burden.

**Core principle**: A single script per concern, with a `--platform sim|hw` flag to select the execution target. The platform is a parameter, not a reason to duplicate a file.

---

## 1. Current Problems

| Problem | Manifestation |
|---------|---------------|
| **Parallel scripts for sim vs hw** | `run_benchmarks.sh` + `run_hw_benchmarks.sh`, `generate_speedups.py` + `generate_hw_speedups.py`, `lib_bench.sh` + `lib_hw_bench.sh`, 4× `bench_*.c` + 4× `bench_hw_*.c` |
| **No single entry point** | User must know which of 6+ scripts to invoke |
| **Duplicated Python boilerplate** | `_VARIANT_SLUG_MAP`, CSV readers, formatting functions copied across 6 files |
| **Unclear naming** | `generate_comparison.py` — comparison of what? `run_hardware_bench.sh` vs `run_hw_benchmarks.sh` — which? |
| **Dead scripts** | `generate_pie_charts.py` absorbed into `generate_charts.py`; `plot_speedup_comparison.py` uses hardcoded data |
| **Deprecated modes** | `--mode suite` (just use `--variant all --level all`), `--mode publish` (should be a `--quality` flag) |

---

## 2. Proposed Consolidation

### 2.1 Shell Scripts: Merge with `--platform` flag

**Before (6 scripts):**
```
scripts/bench/lib_bench.sh
scripts/bench/lib_hw_bench.sh
scripts/bench/run_benchmarks.sh
scripts/bench/run_hw_benchmarks.sh
scripts/bench/run_hardware_bench.sh   ← redundant
scripts/bench/run_profile.sh
```

**After (4 scripts):**
```
scripts/bench/lib_bench.sh            ← unified (absorbs lib_hw_bench.sh)
scripts/bench/run_bench.sh            ← unified benchmark runner
scripts/bench/run_profile.sh          ← keep as-is (sim-only)
scripts/bench/run_sweep.sh            ← hw-only parameter sweeps (moved from modes)
```

#### `run_bench.sh` — Single unified runner

```bash
# Usage:
#   run_bench.sh --platform sim --mode kem --variant scalar_zb
#   run_bench.sh --platform sim --mode mul --variant vector_rvv
#   run_bench.sh --platform hw  --mode kem --variant vector_rvv --level all
#   run_bench.sh --platform hw  --mode mul --variant scalar_zb --quality publish
#   run_bench.sh --platform hw  --variant all --level all   # replaces old "suite"
#
# Platforms:
#   sim     Spike ISS (deterministic instret, cross-compiled via CMake)
#   hw      BPI-F3 hardware (perf_event_open cycles, native GCC or --remote)
#
# Modes:
#   kem       Full KEM (keypair/encaps/decaps) — DEFAULT
#   mul       Polynomial-multiply micro-benchmark
#   keccak    SHAKE-256 micro-benchmark
#   decode    Reed-Muller + Reed-Solomon decode micro-benchmark
#
# Options:
#   --platform sim|hw     Execution target (required)
#   --mode MODE           Benchmark mode (default: kem)
#   --variant VARIANT     Polymul variant (default: scalar_zb)
#   --level LEVEL         128|192|256|all (default: 128)
#   --quality dev|publish (hw only) Iteration count: dev=100, publish=10000 (default: dev)
#   --remote HOST         (hw only) Deploy and run via SSH
#   --local               (hw only) Run on this machine
#   --keccak IMPL         Keccak implementation override (ref|opt)
#   --decode IMPL         Decode implementation override (ref|rvv|zvbc)
```

**Key changes from current scripts:**
- `--mode` defaults to `kem` (not `mul`)
- `--variant` defaults to `scalar_zb` (not karatsuba_zbc — old naming)
- `--mode suite` removed: use `--variant all --level all` instead
- `--mode publish` removed: use `--quality publish` with any mode
- Sweep modes moved to separate `run_sweep.sh`

#### `run_sweep.sh` — Hardware-only parameter exploration

```bash
# Usage:
#   run_sweep.sh --type threshold --variant vector_rvv --values "8 16 32"
#   run_sweep.sh --type lmul --variant vector_rvv
#
# These are one-off experiments for tuning. Results are baked into the code.
# Kept as a separate utility to avoid polluting the main runner.
```

#### `lib_bench.sh` — Absorb `lib_hw_bench.sh`

`lib_hw_bench.sh` only adds 3 functions:
- `hw_variant_slug()` — literally delegates to `bench_variant_slug()` (pointless)
- `hw_csv_header()` — generates metadata comments for HW CSVs
- `hw_store_result()` — routes to the correct output directory

Merge directly into `lib_bench.sh`. One file, one source.

---

### 2.2 Python Report Scripts: Merge with `--platform` flag

**Before (7 scripts):**
```
scripts/report/generate_speedups.py           ← sim speedups
scripts/report/generate_hw_speedups.py        ← hw speedups
scripts/report/generate_comparison.py         ← sim vs hw (unclear name)
scripts/report/generate_compiler_compare.py   ← clang vs gcc
scripts/report/generate_pie_charts.py         ← dead (absorbed)
scripts/report/generate_charts.py             ← all PDF charts
scripts/report/plot_speedup_comparison.py     ← dead (hardcoded data)
```

**After (4 scripts + 1 shared package):**
```
scripts/report/hqc_bench/                     ← shared package
│   ├── __init__.py
│   ├── variants.py                           ← slug maps, track detection
│   ├── csv_io.py                             ← read_csv(), parse_metadata()
│   └── formatting.py                         ← fmt_int(), fmt_speedup(), etc.
│
scripts/report/generate_speedups.py           ← MERGED (--platform sim|hw|cross)
scripts/report/generate_compiler_compare.py   ← keep (unique purpose)
scripts/report/generate_charts.py             ← renamed from generate_thesis_charts.py
scripts/report/map_pcs.py                     ← keep (utility)
scripts/report/parse_histogram.py             ← keep (utility)
```

#### `generate_speedups.py --platform sim|hw|cross`

```bash
python3 scripts/report/generate_speedups.py --platform sim     # Spike instret speedups
python3 scripts/report/generate_speedups.py --platform hw      # BPI-F3 cycle speedups
python3 scripts/report/generate_speedups.py --platform cross   # Spike vs HW correlation
```

Replaces:
- `generate_speedups.py` → `--platform sim`
- `generate_hw_speedups.py` → `--platform hw`
- `generate_comparison.py` → `--platform cross` (fixes the unclear naming)

---

### 2.3 C Benchmarks: Unify via `#ifdef`

**Before (8 files):**
```
tests/bench/bench_mul.c        tests/bench/bench_hw_mul.c
tests/bench/bench_kem.c        tests/bench/bench_hw_kem.c
tests/bench/bench_keccak.c     tests/bench/bench_hw_keccak.c
tests/bench/bench_decode.c     tests/bench/bench_hw_decode.c
```

**After (4 files + 1 shared header):**
```
tests/bench/bench_common.h     ← shared: poly init, sort, median, anti-DCE
tests/bench/bench_mul.c        ← unified (compiles for both platforms)
tests/bench/bench_kem.c        ← unified
tests/bench/bench_keccak.c     ← unified
tests/bench/bench_decode.c     ← unified
tests/bench/perf_hw.h          ← keep (hw-only helper)
tests/bench/Makefile.hw        ← keep (native build)
```

Platform dispatch via `#ifdef HQC_PLATFORM_HW`:

```c
#ifdef HQC_PLATFORM_HW
  #include "perf_hw.h"           // perf_event_open
  #define N_ITER_DEFAULT 10000
#else
  #include "bench.h"             // bare-metal rdcycle/HTIF
  #define N_ITER_DEFAULT 1
#endif
```

The benchmark kernel is identical on both platforms. What differs:
1. Timing mechanism (rdcycle vs perf_event_open)
2. Output method (HTIF vs printf)
3. Iteration count (1 vs 10000)
4. Memory strategy (static vs heap)

CMake already controls target platform — add `-DHQC_PLATFORM_HW=1` for hardware.

---

### 2.4 Delete Dead Code

| File | Reason |
|------|--------|
| `scripts/bench/run_hardware_bench.sh` | Superseded by `run_hw_benchmarks.sh` |
| `scripts/report/generate_pie_charts.py` | 100% duplicated in `generate_charts.py` |
| `scripts/report/plot_speedup_comparison.py` | Hardcoded data, not reproducible |
| `scripts/report/repo_structure.sh` | Not a report script; move to `scripts/util/` |

---

## 3. Final Target Layout

```
scripts/
├── bench/
│   ├── lib_bench.sh              # Unified library (sim + hw helpers)
│   ├── run_bench.sh              # Unified runner (--platform sim|hw)
│   ├── run_profile.sh            # Profiling (sim-only, already unified)
│   ├── run_sweep.sh              # Parameter sweeps (hw-only, one-off)
│   └── run_full_analysis.sh      # Full pipeline orchestrator
│
├── report/
│   ├── hqc_bench/                # Shared Python package
│   │   ├── __init__.py
│   │   ├── variants.py
│   │   ├── csv_io.py
│   │   └── formatting.py
│   ├── generate_speedups.py      # --platform sim|hw|cross
│   ├── generate_compiler_compare.py
│   ├── generate_charts.py        # All PDF figures
│   ├── map_pcs.py
│   └── parse_histogram.py
│
tests/bench/
├── bench_common.h                # Shared C helpers
├── perf_hw.h                     # HW perf wrapper
├── Makefile.hw                   # Native build for BPI-F3
├── README.md                     # Unified documentation
├── bench_mul.c                   # Unified (#ifdef for platform)
├── bench_kem.c
├── bench_keccak.c
└── bench_decode.c
```

**File count reduction**: 22 files → 15 files (32% fewer), zero loss of functionality.

---

## 4. Variant Naming Reference

Current canonical names (from `scripts/implemented_variants.txt`):
```
scalar_ref
scalar_zb
vector_ref
vector_rvv
vector_zvk
```

All scripts use these names. The slug mapping (`scalar_zb` → `scalar-zb`) is internal to CSV filenames only.

---

## 5. Implementation Priority

| # | Action | Effort | Risk |
|---|--------|--------|------|
| 1 | Delete `run_hardware_bench.sh`, `generate_pie_charts.py`, `plot_speedup_comparison.py` | 5 min | None |
| 2 | Merge `lib_hw_bench.sh` into `lib_bench.sh` | 30 min | Low |
| 3 | Create `hqc_bench/` shared Python package | 1-2 hrs | Low |
| 4 | Merge 3 speedup scripts → `generate_speedups.py --platform` | 2-3 hrs | Medium |
| 5 | Merge 2 runners → `run_bench.sh --platform` (remove suite/publish modes, add `--quality`) | 3-4 hrs | Medium |
| 6 | Extract sweeps → `run_sweep.sh` | 30 min | Low |
| 7 | Create `bench_common.h` + unify C benchmarks | 2 hrs | Low |
| 8 | Rename `generate_thesis_charts.py` → `generate_charts.py` | 5 min | Low |

---

## 6. Summary of Mode/Flag Changes

| Old | New | Reason |
|-----|-----|--------|
| `run_benchmarks.sh --mode mul` | `run_bench.sh --platform sim --mode mul` | Unified script |
| `run_hw_benchmarks.sh --mode mul` | `run_bench.sh --platform hw --mode mul` | Unified script |
| `run_hw_benchmarks.sh --mode suite` | `run_bench.sh --platform hw --variant all --level all` | Suite is just "run everything" — not a mode |
| `run_hw_benchmarks.sh --mode publish` | `run_bench.sh --platform hw --quality publish` | Quality is orthogonal to mode |
| `run_hw_benchmarks.sh --mode sweep-*` | `run_sweep.sh --type threshold\|lmul` | Separate utility |
| `--variant karatsuba_zbc` | `--variant scalar_zb` | New naming convention |
| Default mode: `mul` | Default mode: `kem` | KEM is the primary benchmark |
| Default variant: `karatsuba_zbc` | Default variant: `scalar_zb` | Updated naming |
| `generate_comparison.py` | `generate_speedups.py --platform cross` | Clear purpose |
