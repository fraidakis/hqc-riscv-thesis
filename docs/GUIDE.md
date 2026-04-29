# HQC-RISC-V Development Guide

End-to-end reference for configuring, building, simulating, and benchmarking
the HQC post-quantum KEM on bare-metal RISC-V.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Repository Layout](#2-repository-layout)
3. [Toolchain Setup](#3-toolchain-setup)
4. [Configuration & CMake Build](#4-configuration--cmake-build)
5. [Running KAT Tests (QEMU)](#5-running-kat-tests-qemu)
   - [5.5 QEMU vs Spike — choosing the right tool](#55-qemu-vs-spike--choosing-the-right-tool)
6. [KAT Seed Generation (one-time)](#6-kat-seed-generation-one-time)
7. [Running Benchmarks (Spike)](#7-running-benchmarks-spike)
8. [Extracting Results](#8-extracting-results)
9. [Disassembly & Static Analysis](#9-disassembly--static-analysis)
10. [Common Pitfalls](#10-common-pitfalls)
11. [Next Steps (Phase Roadmap)](#11-next-steps-phase-roadmap)

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
| `spike` | current | ISS for cycle-accurate benchmarks |
| Python 3 | ≥ 3.9 | KAT seed generation and result checking |
| CMake | ≥ 3.21 | Build system |
| Ninja | any | Recommended generator |

---

## 2. Repository Layout

```
hqc-riscv/
├── vendor/hqc-ref/          # HQC v5.0.0 upstream (git submodule)
├── src/
│   ├── baseline/            # Phase 0: vendor gf2x (unmodified)
│   ├── scalar_zbc/          # Phase 1: clmul-based poly-mul
│   └── vector_zvbc/         # Phase 1+: vclmul.vv vectorised
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
│       ├── run.sh           # Helper: run any ELF under Spike (+commit log)
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
spike --version
```

---

## 4. Configuration & CMake Build

### 4.1 First-time configure (Clang, out-of-source build)

```bash
cd ~/Thesis/hqc-riscv

cmake -S . -B build \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=qemu
```

Key CMake options:

| Option | Values | Default | Effect |
|--------|--------|---------|--------|
| `HQC_SIM` | `qemu` / `spike` | `qemu` | Selects the UART I/O backend compiled into the ELF |
| `HQC_KECCAK_IMPL` | `ref` / `rvv` | `ref` | Scalar vendor Keccak or RVV Keccak (Phase 2) |
| `HQC_DECODE_IMPL` | `ref` / `rvv` | `ref` | Scalar vendor RM/RS decoder or RVV decoder (Phase 3) |
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

Examples: `hqc128_karatsuba_ref.elf`, `hqc192_karatsuba_zbc.elf`, `hqc256_fafft_zvbc.elf`

### 4.3 Reconfigure for Spike benchmarks

```bash
cmake -S . -B build-spike-bench \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike

cmake --build build-spike-bench --target hqc128_karatsuba_ref -j$(nproc)
```

---

## 5. Running KAT Tests (QEMU)

QEMU runs the bare-metal ELF, which prints `ss: <hex>` for each of the 100 vectors and then `PASS 100/100` or `FAIL at vector N`. The program exits via the QEMU test-finisher device.

### 5.1 Quick run (using helper script)

```bash
./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf
```

> **Note:** The helper uses `-serial mon:stdio`. For automated/CI use, prefer
> capturing to a file (see below) so the log can be diff-checked.

### 5.2 Automated run with file capture

```bash
LEVEL=128
ELF=build/hqc${LEVEL}_karatsuba_ref.elf
LOG=/tmp/hqc${LEVEL}_kat.log
TIMEOUT=30   # seconds (use 90 for HQC-192, 180 for HQC-256)

timeout $TIMEOUT \
  qemu-system-riscv64 \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,zbkb=true,zbc=true,zvbc=true \
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

These are short enough to run after every code change. See §5.5 for guidance
on when to use QEMU versus Spike.

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

### 5.5 QEMU vs Spike — choosing the right tool

| Task | Tool | Why |
|------|------|-----|
| Verify correctness after every change | **QEMU KAT** | Fast (~10–68 s for all three levels combined). Checks all 100 NIST vectors. |
| Measure instruction counts for thesis results | **Spike bench** | Spike ISS `rdcycle` = exact retired-instruction count. QEMU `rdcycle` is **not** instruction count — it is a software counter incremented by a fixed amount per basic block, unrelated to pipeline latency. |
| Debug output / trace a specific operation | **QEMU** | Fast iteration. Use `--log-commits` on Spike only when you need per-instruction data. |

**Workflow for iterative development:**

```
[edit code] → cmake --build → QEMU KAT (30–180 s) → correctness OK
                                                      ↓ (only at phase milestones)
                                              Spike bench overnight → instruction counts
```

**Full QEMU KAT command (run from `hqc-riscv/`):**

```bash
RISCV=~/Thesis/installed-tools
QEMU=$RISCV/bin/qemu-system-riscv64
CPU="rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true"

for LEVEL in 128 192 256; do
  echo -n "HQC-$LEVEL KAT: "
  time timeout 180 $QEMU -machine virt -cpu $CPU -m 256M \
    -nographic -bios none -serial mon:stdio -no-reboot \
    -kernel build/hqc${LEVEL}_karatsuba_ref.elf 2>/dev/null \
    | grep -c "^ss:"
done
```

Expected output:
```
HQC-128 KAT: 100    real 0m10s
HQC-192 KAT: 100    real 0m29s
HQC-256 KAT: 100    real 1m08s
```

**Why QEMU rdcycle is useless for benchmarking:**
QEMU is a dynamic binary translator (DBT). It translates guest RISC-V basic
blocks into host x86 micro-ops and executes them natively. There is no pipeline
model, no cache simulation, and no cycle counter. The guest `rdcycle` CSR is
implemented by incrementing a host counter by a fixed constant per translated
block — the value is arbitrary and changes between QEMU versions. Use Spike for
all instruction-count measurements.

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

Spike provides cycle-accurate simulation with `rdcycle` CSR support and can
emit per-instruction commit logs for profiling.

### 7.1 Quick run

```bash
# Build the bench target against the Spike backend
cmake --build build-spike-bench --target hqc128_karatsuba_ref -j$(nproc)

# Run (commit log written next to the ELF)
./sim/spike/run.sh build-spike-bench/hqc128_karatsuba_ref.elf
```

The `run.sh` script:
- Sets `--isa=rv64gcv_zbc_zbb_zbkb_zvbc`
- Enables `--log-commits` (per-instruction trace → `<elf>.commitlog`)
- Enables `--histogram` (instruction frequency table on stderr)

### 7.2 Manual Spike invocation

```bash
SPIKE=$RISCV/bin/spike
ISA=rv64gcv_zbc_zbb_zbkb_zvbc
ELF=build-spike-bench/hqc128_karatsuba_ref.elf

# Minimal run (HTIF I/O only, no logging)
$SPIKE --isa=$ISA $ELF

# With instruction histogram
$SPIKE --isa=$ISA --histogram $ELF 2>hist.txt

# With full commit log (WARNING: produces very large files)
$SPIKE --isa=$ISA --log-commits -l $ELF 2>commits.log

# With vlen=256 for V-extension (required for zvbc/rvv targets)
$SPIKE --isa=$ISA --varch=vlen:256,elen:64 $ELF
```

### 7.3 N_ITER and rdcycle on Spike ISS

Spike is a pure software ISS (no JIT, no pipeline model).  Two important
consequences:

- **`rdcycle` = instruction count**, not wall-clock CPU cycles.  There is no
  branch misprediction, cache miss, or pipeline stall modelled.  The value is
  the total number of retired instructions since reset.  Use it to compare
  *instruction counts*, not latencies.

- **Deterministic execution** — running the same benchmark twice on Spike
  always produces exactly the same `rdcycle` delta.  Therefore `N_ITER=1` is
  sufficient: `min == median == max` in every run.  Use
  `-DBENCH_N_ITER=1` when configuring the spike bench build.

### 7.4 Spike is slow — three-tier iteration strategy

Spike interprets ~1–5 M RISC-V instructions per second.  QEMU (DBT/JIT)
executes the same code at near-native x86 speed (~500–3000 M eff. insn/s).
The observed ratio for HQC workloads is **10,000–100,000×** — not just 100×:

| Simulator | HQC-128 one KEM op | Why |
|-----------|-------------------|-----|
| QEMU (DBT) | ~0.1 s | Translates RISC-V to host x86 JIT; runs near-native |
| Spike (ISS) | ~2–5 h | Interprets every instruction in C++; no JIT |

This is well-documented: the Spike README and Chipyard docs explicitly state
Spike is intended as an ISA reference, not a fast simulator.

**Do not wait 5 hours for every change.** Use this three-tier strategy:

| Tier | Tool | Time | When to use |
|------|------|------|-------------|
| 1 — Correctness | QEMU KAT | 10–68 s | After every single change |
| 2 — Quick instruction count | Spike polymul micro-bench | ~20–60 min | After a QEMU-confirmed change, to verify instruction count improved |
| 3 — Full baseline | Spike KEM bench | 2–20 h | Once per phase milestone only |

**Tier 2: polymul micro-benchmark** — benchmarks only `vect_mul()` (the
dominant cost, >90% of HQC runtime).  No keypair/encaps/decaps, no Keccak:

```bash
SPIKE=~/Thesis/installed-tools/bin/spike
ISA=rv64gcv_zbc_zbb_zbkb_zvbc
OUT=analysis/benchmarks/phase0b

# Build the micro-bench (build-spike already configured for Spike)
cmake --build build-spike \
      --target bench_polymul_hqc128_karatsuba_ref \
      -j$(nproc)

# Run in background
nohup $SPIKE --isa=$ISA \
    build-spike/bench_polymul_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_vect_mul_N1.csv 2>$OUT/hqc128_polymul.log &

# Monitor
cat $OUT/hqc128_vect_mul_N1.csv
```

Expected output:
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
vect_mul,1,<count>,<count>,<count>
```

### 7.5 Can Spike use multiple CPU cores?

**No.** Spike is a single-threaded interpreter.  It simulates one RISC-V
hart sequentially — there is no JIT, no parallel dispatch, no way to split
one simulation across cores.

What you *can* do is run multiple independent Spike instances simultaneously,
one per security level.  Since each level is a separate ELF they use separate
cores transparently:

```bash
# Launch all three levels at once — uses 3 of your CPU cores in parallel
nohup $SPIKE --isa=$ISA \
    build-spike/bench_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_karatsuba_ref_N1.csv 2>$OUT/hqc128_spike.log &
nohup $SPIKE --isa=$ISA build-spike/bench_hqc192_karatsuba_ref.elf \
    > $OUT/hqc192_karatsuba_ref_N1.csv 2>$OUT/hqc192_spike.log &
nohup $SPIKE --isa=$ISA build-spike/bench_hqc256_karatsuba_ref.elf \
    > $OUT/hqc256_karatsuba_ref_N1.csv 2>$OUT/hqc256_spike.log &

echo "Monitor: wc -c $OUT/hqc{128,192,256}_karatsuba_ref_N1.csv"
```

Running all three in parallel does **not** speed up any individual run — it
just avoids serial waiting when you need results from all three levels.

Build the bench ELFs (also rebuilds polymul micro-bench targets):

```bash
cmake -S . -B build-spike \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1

cmake --build build-spike --target \
      bench_hqc128_karatsuba_ref \
      bench_hqc192_karatsuba_ref \
      bench_hqc256_karatsuba_ref \
      bench_polymul_hqc128_karatsuba_ref \
      bench_polymul_hqc192_karatsuba_ref \
      bench_polymul_hqc256_karatsuba_ref \
      -j$(nproc)
```

Approximate wall-clock times (4-core laptop, Spike single-threaded):

| Level | Full KEM bench | `vect_mul` micro-bench |
|-------|---------------|------------------------|
| HQC-128 | **2–5 h** | **~20–40 min** |
| HQC-192 | **5–12 h** | **~40–90 min** |
| HQC-256 | **10–20 h** | **~60–150 min** |

> **Use Spike only for final phase baselines.** For everyday correctness
> checks use QEMU KAT (§5.5).  For quick iteration on a new polymul
> implementation use the Tier 2 micro-bench.  Only run the full KEM bench
> once per phase to record the numbers for the thesis tables.

`bench_kem.c` uses the `rdcycle` CSR and emits CSV to the HTIF console:

```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,<count>,<count>,<count>
encaps,1,<count>,<count>,<count>
decaps,1,<count>,<count>,<count>
```

With `N_ITER=1` the three columns are identical (ISS is deterministic).

---

## 8. Extracting Results

### 8.1 Cycle counts from Spike

Capture Spike's HTIF stdout to a file and parse the CSV:

```bash
$SPIKE --isa=$ISA build-spike-bench/hqc128_karatsuba_ref.elf > /tmp/bench128.csv 2>/dev/null

# Inspect
cat /tmp/bench128.csv
```

Example output:
```
op,iterations,min_cycles,median_cycles,max_cycles
keypair,10,14823450,14851002,15001234
encaps,10,16234561,16278900,16512345
decaps,10,17123456,17198723,17321098
```

### 8.2 Instruction histogram from Spike

```bash
$SPIKE --isa=$ISA --histogram build-spike-bench/hqc128_karatsuba_ref.elf 2>hist.txt
# Top instructions by frequency:
sort -t',' -k2 -rn hist.txt | head -20
```

### 8.3 Instruction mix with llvm-mca (static analysis)

Isolate a hot function from the disassembly and feed it to `llvm-mca`:

```bash
# Disassemble the ELF
llvm-objdump -d build/hqc128_karatsuba_ref.elf > hqc128.s

# Extract function of interest (e.g. vect_mul / KeccakF1600_StatePermute)
# Then run llvm-mca on that section:
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

## 10. Common Pitfalls

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| QEMU at 99.9% CPU, zero serial output | Missing `zbkb=true` CPU flag; `packh` raises illegal-instruction trap | Add `zbkb=true,zbc=true,zvbc=true` to `-cpu` string |
| All 100 KAT ss values WRONG | `randombytes_init` called instead of `prng_init` | Use `prng_init(seed, NULL, 48, 0)` in `kat_gen.c` |
| QEMU timeout (exit 124) for 192/256 | Insufficient timeout | Use 90 s for HQC-192, 180 s for HQC-256 (measured: ~29 s and ~68 s respectively) |
| Link error: `undefined reference to memcpy` | Building without libc and `sim/common/mem.c` not included | Add `${CMAKE_SOURCE_DIR}/sim/common/mem.c` to source list |
| `packh` in disassembly without `-zbkb` march | Clang -O3 auto-generates Zbkb instructions from Keccak bitmanip | march must include `zbkb` and QEMU CPU must too |
| Spike exits immediately with `tohost` error | Wrong simulator backend compiled in (QEMU UART code hits Spike) | Rebuild with `-DHQC_SIM=spike` |
| `Property 'rv64-riscv-cpu.x-zbkb' not found` | Wrong QEMU CPU property syntax | Use `zbkb=true` not `x-zbkb=true` |
| QEMU error `Some ROM regions are overlapping` | `opensbi-riscv64-generic-fw_dynamic.bin` exists and also loads at 0x80000000 | Always pass `-bios none`; auto-detect via `[[ -f "$BIOS" ]]` is **not** safe here |
| Spike bench produces no output after > 10 min | Normal — Spike ISS interprets ~1–5 M insn/s; HQC-128 > 10⁹ instructions | Use `nohup ... &` to background; do not wait interactively |

---

## 11. Next Steps (Phase Roadmap)

The thesis follows four progressive optimization phases. Phase 0a (baseline KAT) is complete.

### Phase 0b — Spike Baseline Benchmarks

**Goal:** Establish the cycle-count denominator for all speedup comparisons.

1. Build all three levels with `-DHQC_SIM=spike`
2. Run `bench_kem.c` on Spike for HQC-128/192/256 with `karatsuba_ref`
3. Record `min_cycles` and `median_cycles` for keypair / encaps / decaps
4. Commit the raw CSV results under `analysis/benchmarks/phase0b/`
5. Identify the dominant cost (expected: `gf2x_mul` Karatsuba ≫ 95%)

### Phase 1a — Clmul Karatsuba (`karatsuba_zbc`)

**Goal:** Replace the inner Karatsuba base-case multiplication with `clmul`/`clmulh` scalar instructions from the Zbc extension.

1. Implement `src/poly_mul/karatsuba_zbc/gf2x_clmul.c`:
   - Leaf multiplications: single `clmul` + `clmulh` per 64-bit word pair
   - Keep the Karatsuba recursion structure from vendor `gf2x.c`
   - Add lazy reduction with `clmulr` where applicable
2. Run KAT to verify correctness against all three levels
3. Benchmark on Spike vs. `karatsuba_ref` — target ≥ 2× speedup on `gf2x_mul`
4. Profile instruction mix: clmul fraction in `gf2x_mul` should approach 100%

### Phase 1b — FAFFT with Zbc (`fafft_zbc`)

**Goal:** Replace Karatsuba with a Fast Walsh-Hadamard / Additive FFT polynomial multiplication using `clmul`-accelerated GF(2^m) arithmetic.

1. Implement `src/poly_mul/fafft_zbc/fafft.c`:
   - Additive NTT over GF(2^{16}) with `clmul`-based butterfly operations
   - Match HQC-128 parameter: degree-16384 polynomials over GF(2)
2. Implement RVV XOR accumulation for the linear-complexity layers
3. KAT + benchmark comparison with Phase 1a

### Phase 1c — Hybrid (`hybrid_zbc`)

**Goal:** Karatsuba outer levels descending into FAFFT inner levels at a crossover depth chosen by profiling.

1. Implement `src/poly_mul/hybrid_zbc/hybrid.c`
2. Benchmark crossover depths to find optimum (likely 2–4 Karatsuba levels)
3. Compare against 1a and 1b

### Phase 1+ — Vectorised FAFFT (`fafft_zvbc`)

**Goal:** Replace scalar `clmul` butterflies with `vclmul.vv` (Zvbc extension).

1. Implement `src/poly_mul/fafft_zvbc/fafft_vclmul.c` using RVV intrinsics
2. Validate on Spike (`--isa=rv64gcv_zbc_zbb_zbkb_zvbc --varch=vlen:256,elen:64`)
3. Target ≥ 10× over baseline, quantify vclmul IPC

### Phase 2 — RVV Keccak

**Goal:** Replace the scalar Keccak-f\[1600\] with a vectorized implementation, reducing its contribution to the end-to-end runtime.

1. Integrate Zhang et al. RVV Keccak (or similar) under `src/keccak/rvv/`
2. Configure with `-DHQC_KECCAK_IMPL=rvv`
3. KAT across all levels and all polymul variants
4. Benchmark: target >5× Keccak throughput improvement

### Phase 3 — RVV Reed-Muller / Reed-Solomon Decoder

**Goal:** Vectorise the RM/RS decoding path (FHT + syndrome computation).

1. Implement `src/decode/rvv/reed_muller_rvv.c` using `vxor.vv` + FHT butterfly
2. Implement `src/decode/rvv/reed_solomon_rvv.c` with `vluxei8.v` gather-scatter
3. Configure with `-DHQC_DECODE_IMPL=rvv`
4. KAT + benchmark across all levels

### Measurement & Thesis Writing

After each phase:
- Run KAT on QEMU for correctness
- Run benchmarks on Spike for cycle counts
- Compute speedup ratios relative to Phase 0b baseline
- Update `analysis/benchmarks/` with raw CSV + processed tables
- Update `docs/REPORT.md` with results and discussion

---

*Last updated: Phase 0b in progress — QEMU KAT clean (100/100 all levels), Spike background runs started (N_ITER=1, deterministic ISS).*
