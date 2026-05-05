# HQC-RISC-V Development Guide

End-to-end reference for configuring, building, simulating, and benchmarking
the HQC post-quantum KEM on bare-metal RISC-V.

For methodology rationale, tool rankings, and benchmarking decisions see
[`docs/METHODOLOGY.md`](METHODOLOGY.md).  For day-to-day build/run commands see
[`docs/RUNBOOK.md`](RUNBOOK.md).  For the phase-by-phase implementation plan
see [`docs/PLAN.md`](PLAN.md).

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Repository Layout](#2-repository-layout)
3. [Toolchain Setup](#3-toolchain-setup)
4. [Configuration & CMake Build](#4-configuration--cmake-build)
5. [Running KAT Tests (QEMU)](#5-running-kat-tests-qemu)
6. [KAT Seed Generation (one-time)](#6-kat-seed-generation-one-time)
7. [Running Benchmarks (Spike)](#7-running-benchmarks-spike)
8. [Extracting Results](#8-extracting-results)
9. [Disassembly & Static Analysis](#9-disassembly--static-analysis)
10. [Common Pitfalls](#10-common-pitfalls)

---

## 1. Prerequisites

All pre-built tools live under `~/Thesis/installed-tools/`:

| Tool | Version | Purpose |
|------|---------|---------|
| `clang` / `clang++` | 21 | Cross-compiler for `riscv64-unknown-elf` |
| `lld` | 21 | Linker (`-fuse-ld=lld`) |
| `llvm-objdump`, `llvm-nm` | 21 | Disassembly and symbol inspection |
| `llvm-mca` | 21 | Static machine-code analysis |
| `qemu-system-riscv64` | 10.2.2 | System-mode simulator for KAT correctness |
| `spike` | 1.1.1-dev | ISS for instruction-count benchmarks |
| Python 3 | ≥ 3.9 | KAT seed generation and result checking |
| CMake | ≥ 3.21 | Build system |
| Ninja | any | Recommended generator |

---

## 2. Repository Layout

```
hqc-riscv/
├── vendor/hqc-ref/          # HQC v5.0.0 upstream (git submodule)
├── src/
│   ├── baseline/            # baseline_ref support / vendor gf2x (unmodified)
│   ├── scalar_zbc/          # legacy helper area for polymul_zbc-era scalar work
│   └── vector_zvbc/         # legacy helper area for polymul_zvbc-era vector work
├── tests/
│   ├── kat/
│   │   ├── kat_gen.c        # Bare-metal KAT runner (main entry point)
│   │   ├── kat_check.py     # Post-run checker against .rsp reference
│   │   ├── gen_kat_seeds.py # One-time seed header generator
│   │   └── generated/       # Pre-built kat_seeds_{128,192,256}.h
│   └── bench/
│       ├── bench_kem.c      # rdcycle KEM benchmark harness
│       └── bench_poly_mul.c # Isolated poly-mul micro-benchmark
├── sim/
│   ├── common/
│   │   ├── startup.S        # CRT0: zero BSS, set sp, jump to main
│   │   ├── link.ld          # Linker script: DRAM at 0x80000000, 512 KB stack
│   │   └── mem.c            # memcpy/memset/memcmp without libc
│   ├── qemu/
│   │   ├── run_kat.sh       # Helper: run any ELF under QEMU
│   │   └── run_rave.sh      # Helper: run RAVE (RTL sim)
│   └── spike/
│       ├── run_bench.sh     # Helper: run any ELF under Spike (bench mode)
│       ├── run.sh           # Helper: run with --log-commits + --histogram (SLOW — profiling only)
│       └── parse_commits.py # Post-process Spike commit log
├── toolchain/
│   ├── riscv-clang.cmake    # CMake toolchain file
│   ├── toolchain.env        # Shell environment (source once per session)
│   └── flags.mk             # Shared flags for legacy Makefiles
├── include/                 # Thin wrappers / stubs (immintrin.h etc.)
└── docs/                    # Documentation
```

---

## 3. Toolchain Setup

Source the environment once per shell session (or add to `~/.bashrc`):

```bash
source toolchain/toolchain.env
# exports RISCV, CC, CXX, CLANG, SPIKE, QEMU, and prepends $RISCV/bin to PATH
```

Verify the cross-compiler works:

```bash
clang --version        # should print clang version 21...
qemu-system-riscv64 --version
spike --help 2>&1 | head -1   # prints "Spike RISC-V ISA Simulator <version>"
```

> **Note:** `spike --version` is not a valid flag. Use `spike --help` or the
> first line of `spike --help` to confirm the installed version.

---

## 4. Configuration & CMake Build

### 4.1 First-time configure (Clang, out-of-source build)

```bash
cd ~/Thesis/hqc-riscv

# KAT / correctness build (QEMU backend)
cmake -S . -B build \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=qemu

# Spike benchmark build (N_ITER=1 for deterministic ISS)
cmake -S . -B build/spike \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1
```

Key CMake options:

| Option | Values | Default | Effect |
|--------|--------|---------|--------|
| `HQC_SIM` | `qemu` / `spike` | `qemu` | Selects the UART I/O backend compiled into the ELF |
| `HQC_KECCAK_IMPL` | `ref` / `rvv` | `ref` | Scalar vendor Keccak or the `keccak_rvv` track |
| `HQC_DECODE_IMPL` | `ref` / `rvv` | `ref` | Scalar vendor RM/RS decoder or the RVV decoder tracks |
| `BENCH_N_ITER` | integer | `10` | Number of benchmark iterations (`1` for Spike ISS — deterministic) |
| `RISCV` | path | `~/Thesis/installed-tools` | Override toolchain root |

### 4.2 Building individual targets

Each `<level>_<polymul>` combination produces one self-contained ELF:

```bash
# Build everything (all 3 levels × all 5 polymul variants = 15 ELFs)
cmake --build build

# Build one specific target
cmake --build build --target hqc128_karatsuba_ref

# Parallel build
cmake --build build -j$(nproc)
```

ELF names follow the pattern:

```
build/hqc<LEVEL>_<POLYMUL>.elf
```

Examples: `build/hqc128_karatsuba_ref.elf`, `build/spike/bench_hqc192_karatsuba_zbc.elf`

---

## 5. Running KAT Tests (QEMU)

QEMU runs the bare-metal ELF, which prints `ss: <hex>` for each of the 100 vectors and then `PASS 100/100` or `FAIL at vector N`. The program exits via the QEMU test-finisher device.

### 5.1 Quick run (using helper script)

```bash
./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf
```

### 5.2 Automated run with file capture

```bash
LEVEL=128
ELF=build/hqc${LEVEL}_karatsuba_ref.elf
LOG=/tmp/hqc${LEVEL}_kat.log
TIMEOUT=30   # seconds (use 90 for HQC-192, 180 for HQC-256)

timeout $TIMEOUT \
  qemu-system-riscv64 \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
    -m 256M -nographic -bios none -serial mon:stdio -no-reboot \
    -kernel "$ELF" 2>/dev/null | tee "$LOG"

echo "exit: $? | lines: $(wc -l < $LOG)"
tail -1 "$LOG"
```

**Critical CPU flags:** the compiled ELF contains `packh` (Zbkb) instructions.
Running without `zbkb=true` causes an illegal-instruction trap → infinite
exception loop at 99.9% CPU with zero serial output. Always include:
```
-cpu rv64,v=true,vlen=256,elen=64,zbkb=true,zbc=true,zvbc=true
```
(or `-cpu max,vlen=256,elen=64` for unrestricted development runs.)

**Always use `-bios none`:** `opensbi-riscv64-generic-fw_dynamic.bin` exists in
the installed-tools tree but loads at 0x80000000, colliding with the bare-metal
ELF which also starts at 0x80000000. QEMU will refuse to run with both.

### 5.3 Measured runtimes under QEMU (vlen=256, `karatsuba_ref`)

These are wall-clock times measured on an IdeaPad 5 Pro (AMD Ryzen 7 5600H):

| Level | Time for 100 KAT vectors | Suggested `timeout` |
|-------|--------------------------|---------------------|
| HQC-128 | **~10 s** | 30 s |
| HQC-192 | **~29 s** | 90 s |
| HQC-256 | **~68 s** | 180 s |

### 5.4 Verify against NIST reference vectors

```bash
python3 tests/kat/kat_check.py \
  --actual "$LOG" \
  --rsp    vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp
```

Reference `.rsp` file map:

| Level | File |
|-------|------|
| HQC-128 | `vendor/hqc-ref/kats/ref/hqc-1/PQCkemKAT_2321.rsp` |
| HQC-192 | `vendor/hqc-ref/kats/ref/hqc-3/PQCkemKAT_4602.rsp` |
| HQC-256 | `vendor/hqc-ref/kats/ref/hqc-5/PQCkemKAT_7333.rsp` |

Exit code 0 means `PASS [HQC]: 100/100 vectors matched`.

> **QEMU vs Spike for benchmarking:** QEMU is the correctness gatekeeper.
> Never use QEMU `rdcycle` values in thesis tables — QEMU is a DBT/JIT that
> increments a software counter per translated block, not per instruction.
> See [METHODOLOGY.md §13](METHODOLOGY.md) for tool rankings and methodology.

---

## 6. KAT Seed Generation (one-time)

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

## 7. Running Benchmarks (Spike)

Spike provides a cycle-accurate ISS where `rdcycle` = exact retired instruction
count.  Use `sim/spike/run_bench.sh` for benchmarking — it omits `--log-commits`
and `--histogram` to avoid the 10–50 GB log overhead that causes the apparent
360× slowdown.  Use `sim/spike/run.sh` only for profiling/commit-log analysis.

### 7.1 Quick run

```bash
# Build the bench target against the Spike backend
cmake --build build/spike --target bench_hqc128_karatsuba_ref -j$(nproc)

# Run (benchmark mode — no logging)
./sim/spike/run_bench.sh build/spike/bench_hqc128_karatsuba_ref.elf
```

### 7.2 Manual Spike invocation

```bash
SPIKE=$RISCV/bin/spike
ISA=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm
ELF=build/spike/bench_hqc128_karatsuba_ref.elf

# Minimal run (HTIF I/O only, no logging)
$SPIKE --isa=$ISA $ELF

# With instruction histogram
$SPIKE --isa=$ISA --histogram $ELF 2>hist.txt

# With full commit log (WARNING: produces very large files)
$SPIKE --isa=$ISA --log-commits -l $ELF 2>commits.log
```

> **ISA string must include `_zicntr_zihpm`.**  Without these extensions the
> `rdcycle` pseudoinstruction (CSR 0xc00) traps to mtvec=0, causing an infinite
> loop that looks like a ~360× slowdown.  The bench harness reads `mcycle`
> (CSR 0xb00) directly, so this is a belt-and-suspenders fix for any ELF that
> still uses the pseudoinstruction.

### 7.3 Build all bench targets

```bash
cmake --build build/spike --target \
      bench_hqc128_karatsuba_ref \
      bench_hqc192_karatsuba_ref \
      bench_hqc256_karatsuba_ref \
      bench_polymul_hqc128_karatsuba_ref \
      bench_polymul_hqc192_karatsuba_ref \
      bench_polymul_hqc256_karatsuba_ref \
      -j$(nproc)
```

### 7.4 Spike wall-clock times (measured, N_ITER=1)

With `BENCH_N_ITER=1` and no logging flags, Spike finishes quickly because
N_ITER=1 is a single KEM operation (ISS is deterministic — no need for more).
These are actual measured times on an AMD Ryzen 7 5600H:

| Level | Full KEM bench | `vect_mul` micro-bench |
|-------|---------------|------------------------|
| HQC-128 | **~0.7 s** | **~0.1 s** |
| HQC-192 | **~2.2 s** | **~0.3 s** |
| HQC-256 | **~5.1 s** | **~0.8 s** |

> **Note:** The old estimates of "2–5 h" and "20–40 min" referred to
> `--log-commits -l` mode (profiling), not benchmark mode.  Always use
> `sim/spike/run_bench.sh` or the bare `spike --isa=...` invocation without
> logging flags.

Instruction counts (`baseline_ref`, `karatsuba_ref`, N_ITER=1):

| Level | Operation | Retired instructions |
|-------|-----------|---------------------|
| HQC-128 | `vect_mul` | 18 578 006 |
| HQC-128 | keypair | 18 816 325 |
| HQC-128 | encaps | 37 636 615 |
| HQC-128 | decaps | 56 962 431 |
| HQC-192 | `vect_mul` | 56 469 319 |
| HQC-192 | keypair | 56 986 059 |
| HQC-192 | encaps | 113 992 185 |
| HQC-192 | decaps | 171 660 064 |
| HQC-256 | `vect_mul` | 139 077 901 |
| HQC-256 | keypair | 140 074 066 |
| HQC-256 | encaps | 279 984 371 |
| HQC-256 | decaps | 421 305 306 |

`vect_mul` accounts for **~99%** of keypair instruction count (HQC-128: 18.6 M / 18.8 M).
This confirms `vect_mul` is the dominant cost and the right target for optimization.

Run commands:

```bash
OUT=analysis/benchmarks/baseline_ref
ISA=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm

# Full KEM bench (~0.7–5 s per level)
for LEVEL in 128 192 256; do
    $RISCV/bin/spike --isa=$ISA \
        build/spike/bench_hqc${LEVEL}_karatsuba_ref.elf \
        > $OUT/hqc${LEVEL}_karatsuba_ref_N1.csv 2>/dev/null
done

# Polymul micro-bench (~0.1–0.8 s per level)
for LEVEL in 128 192 256; do
    $RISCV/bin/spike --isa=$ISA \
        build/spike/bench_polymul_hqc${LEVEL}_karatsuba_ref.elf \
        > $OUT/hqc${LEVEL}_karatsuba_ref_polymul_N1.csv 2>/dev/null
done
```

---

## 8. Extracting Results

### 8.1 Cycle counts from Spike

Capture Spike's HTIF stdout to a file and parse the CSV:

```bash
$RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm \
    build/spike/bench_hqc128_karatsuba_ref.elf > /tmp/bench128.csv 2>/dev/null

cat /tmp/bench128.csv
```

CSV format:
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,18816324,18816324,18816324
encaps,1,37636614,37636614,37636614
decaps,1,56962430,56962430,56962430
```

With `N_ITER=1` all three columns are identical (ISS is deterministic).

### 8.2 Instruction histogram from Spike

```bash
$RISCV/bin/spike --isa=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm \
    --histogram build/spike/bench_hqc128_karatsuba_ref.elf 2>hist.txt

# Top instructions by frequency:
sort -t$'\t' -k2 -rn hist.txt | head -20
```

### 8.3 Instruction mix with llvm-mca (static analysis)

Isolate a hot function from the disassembly and feed it to `llvm-mca`:

```bash
# Disassemble the ELF
llvm-objdump -d build/hqc128_karatsuba_ref.elf > hqc128.s

# Run llvm-mca on an extracted function:
llvm-mca --march=riscv64 --mcpu=generic-rv64 \
         --mattr=+v,+zbc,+zbkb,+zvbc \
         --timeline \
         extracted_function.s
```

### 8.4 Commit log analysis

Parse per-instruction information from Spike's `--log-commits` output:

```bash
# Count clmul/clmulh instructions in a KEM run:
grep -c "clmul" hqc128.commitlog

# Count V-extension instructions:
grep -cE "^\s+v[a-z]" hqc128.commitlog

# Use the provided parser (work in progress):
python3 sim/spike/parse_commits.py hqc128.commitlog
```

### 8.5 ELF size and section breakdown

```bash
llvm-size build/hqc128_karatsuba_ref.elf
llvm-objdump -h build/hqc128_karatsuba_ref.elf
```

---

## 9. Disassembly & Static Analysis

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

---

*Last updated: 2026-04-30. QEMU KAT clean (100/100 all levels), Spike bench builds verified.*
