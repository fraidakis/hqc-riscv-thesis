# HQC-RISC-V: Development Workflow (HQC-128 cheatsheet)

Quick-reference for the iteration loop after making a code change.
All commands assume `cd ~/Thesis/hqc-riscv` and the env sourced:

```bash
source toolchain/toolchain.env
# exports $RISCV, $QEMU, $SPIKE, prepends $RISCV/bin to PATH
```

---

## 0. After any code change — rebuild first

```bash
# KAT build (QEMU backend — for correctness checks)
cmake --build build --target hqc128_karatsuba_ref -j$(nproc)

# Spike bench build (Spike backend — for instruction counts)
cmake --build build-spike-probe \
      --target bench_hqc128_karatsuba_ref \
                bench_polymul_hqc128_karatsuba_ref \
      -j$(nproc)
```

If you modified `CMakeLists.txt` or added source files, reconfigure first:

```bash
# KAT build
cmake -S . -B build \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=qemu

# Spike bench build
cmake -S . -B build-spike-probe \
      -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
      -DHQC_SIM=spike \
      -DBENCH_N_ITER=1
```

---

## 1. TIER 1 — Correctness: QEMU KAT (~10 s)

**Use after every single change.** Runs 100 NIST KAT vectors.

```bash
CPU="rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true"

$QEMU -machine virt -cpu $CPU -m 256M -nographic -bios none \
      -kernel build/hqc128_karatsuba_ref.elf \
      -serial mon:stdio -no-reboot 2>/dev/null | tee /tmp/kat128.log

grep -c "^ss:" /tmp/kat128.log     # should print 100
tail -1 /tmp/kat128.log            # should print PASS 100/100
```

Or use the helper script (identical flags):

```bash
./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf 2>/dev/null
```

**Expected output:**
```
ss: <64 hex bytes>
...  (×100)
PASS 100/100
```

---

## 2. TIER 1b — QEMU instruction count (approximate, ~10 s)

**Sanity check only — not for thesis numbers.**
QEMU uses a DBT/JIT; its `rdcycle` is a software counter unrelated to
actual instruction latency. Use only to confirm the bench harness works
and that the CSV format is correct.

```bash
# Build the QEMU-backend bench ELF (once)
cmake --build build --target bench_hqc128_karatsuba_ref -j$(nproc)

# Run it — produces CSV but numbers are meaningless
$QEMU -machine virt -cpu $CPU -m 256M -nographic -bios none \
      -kernel build/bench_hqc128_karatsuba_ref.elf \
      -serial mon:stdio -no-reboot 2>/dev/null
```

**Expected output (numbers are NOT real):**
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,<bogus>,<bogus>,<bogus>
encaps,1,<bogus>,<bogus>,<bogus>
decaps,1,<bogus>,<bogus>,<bogus>
```

---

## 3. TIER 2 — Quick instruction count: Spike polymul micro-bench (~20–40 min)

**Use after a QEMU-confirmed change to check if `vect_mul` got faster.**
Benchmarks only `vect_mul()` — no keypair/encaps/decaps, no Keccak.
`vect_mul` is >90% of HQC runtime so this is the number that matters.

```bash
OUT=analysis/benchmarks/phase0b

nohup $SPIKE --isa=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_vect_mul_N1.csv 2>$OUT/hqc128_polymul.log &

echo "PID=$!  — monitor with: cat $OUT/hqc128_vect_mul_N1.csv"
```

**Expected output when done:**
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
vect_mul,1,<count>,<count>,<count>
```

---

## 4. TIER 3 — Full baseline: Spike KEM bench (~2–5 h for HQC-128)

**Use only at phase milestones.** Full keypair + encaps + decaps.

```bash
OUT=analysis/benchmarks/phase0b

nohup $SPIKE --isa=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm \
    build-spike-probe/bench_hqc128_karatsuba_ref.elf \
    > $OUT/hqc128_karatsuba_ref_N1.csv 2>$OUT/hqc128_spike.log &

echo "PID=$!  — monitor with: cat $OUT/hqc128_karatsuba_ref_N1.csv"
```

Check if still running:
```bash
ps aux | grep "[s]pike.*bench_hqc128"
wc -c $OUT/hqc128_karatsuba_ref_N1.csv   # grows as CSV lines arrive
```

**Expected output when done:**
```
# level=128,polymul=karatsuba_ref,n_iter=1
op,iterations,min_cycles,median_cycles,max_cycles
keypair,1,<count>,<count>,<count>
encaps,1,<count>,<count>,<count>
decaps,1,<count>,<count>,<count>
```

---

## 5. Static analysis: llvm-mca

**Use to estimate throughput/latency of a specific function without running Spike.**
llvm-mca works on assembly — no simulator needed, runs in seconds.

```bash
# Analyse a single source file (e.g. the new polymul implementation)
./analysis/llvm_mca/run_mca.sh src/poly_mul/karatsuba_zbc/gf2x_clmul.c
# Report written to: analysis/llvm_mca/reports/gf2x_clmul_mca.txt
```

Or manually — extract a hot function from the disassembly:

```bash
RISCV=~/Thesis/installed-tools

# Step 1: disassemble
$RISCV/bin/llvm-objdump -d build-spike-probe/bench_hqc128_karatsuba_ref.elf \
    > /tmp/hqc128.s

# Step 2: compile a single C file to assembly for MCA input
$RISCV/bin/clang \
    --target=riscv64-unknown-elf \
    -march=rv64gcv_zbc_zbb_zbkb_zvbc -mabi=lp64d \
    -O3 -ffreestanding -fno-builtin \
    -I vendor/hqc-ref/src/ref/hqc-1 \
    -I vendor/hqc-ref/src/ref \
    -S -o /tmp/gf2x.s \
    vendor/hqc-ref/src/ref/gf2x.c

# Step 3: run MCA
$RISCV/bin/llvm-mca \
    -march=riscv64 -mcpu=generic-rv64 \
    --timeline --iterations=10 \
    /tmp/gf2x.s
```

Key metrics in the output:
- **Throughput** (IPC): instructions per cycle at steady state
- **Timeline**: shows stalls and dependency chains
- **Resource pressure**: which functional units are bottlenecked

---

## 6. Spike instruction histogram (cheap profiling, ~same time as full bench)

Which instructions dominate? Run with `--histogram`:

```bash
$SPIKE --isa=rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm --histogram \
    build-spike-probe/bench_polymul_hqc128_karatsuba_ref.elf \
    2>analysis/benchmarks/phase0b/hqc128_polymul_hist.txt &

# When done — top instructions by frequency:
sort -t$'\t' -k2 -rn \
    analysis/benchmarks/phase0b/hqc128_polymul_hist.txt | head -20
```

---

## 7. Measured Results After Fix

### Mini-benchmark (`tests/bench/mini_bench.c`)

Three kernels: 2-limb GF(2^128) clmul, RVV 256-byte XOR, scalar XOR-rotate.
Each kernel runs 10 000 iterations.

| Kernel | Simulator | Wall time | Simulated cycles | Cycles/iter |
|--------|-----------|-----------|-----------------|-------------|
| `clmul128` | **Spike (fixed)** | — | 110 001 | 11 |
| `clmul128` | QEMU | — | 9 005 766 | 900 |
| `rvv_xor256` | **Spike (fixed)** | — | 150 007 | 15 |
| `rvv_xor256` | QEMU | — | 215 441 721 | 21 544 |
| `scalar_xor` | **Spike (fixed)** | — | 2 560 003 | 8 |
| `scalar_xor` | QEMU | — | 3 579 642 | 11 |

| Total wall time | Spike (fixed) | QEMU |
|-----------------|--------------|------|
| All 3 kernels   | **45 ms**    | 180 ms |

Spike is **4× faster** in wall time for the mini-benchmark. Simulated cycle counts differ
because Spike's `mcycle` increments once per committed instruction (roughly an IPC=1
in-order model), while QEMU's cycle counter tracks real host wall-clock cycles.

### Full HQC-128 karatsuba_ref KEM benchmark

| Operation | Spike cycles | Wall time |
|-----------|-------------|-----------|
| KeyGen | 18 816 324 | — |
| Encaps | 37 636 614 | — |
| Decaps | 56 962 430 | — |
| **Total** | — | **1.54 s** |

QEMU KAT (100 vectors): **8.56 s**

Spike is **5.6× faster** than QEMU for this workload after the fix. The previous apparent
"360× slowdown" was entirely due to the infinite trap loop — Spike was never computing.


## Summary: which tool for what

| Question | Tool | Time |
|----------|------|------|
| "Does it still produce correct output?" | QEMU KAT | ~10 s |
| "Does the bench harness/CSV work?" | QEMU bench | ~10 s |
| "Did `vect_mul` get faster?" | Spike polymul micro-bench | ~20–40 min |
| "What are the full KEM instruction counts?" | Spike KEM bench | ~2–5 h |
| "What is the static throughput of this loop?" | llvm-mca | seconds |
| "Which instructions dominate?" | Spike --histogram | ~20–40 min |

**Never use QEMU rdcycle values in the thesis.** Only Spike `rdcycle` = real instruction count.

---

## 8. How to Run the Benchmarks

### Prerequisites

```bash
source toolchain/toolchain.env
# RISCV=$HOME/Thesis/installed-tools must be on PATH
```

### Build and run the mini-benchmark

```bash
# Build both backends
make -f tests/bench/mini_bench.mk

# Run on Spike (should finish in < 1 s)
./sim/spike/run_bench.sh build-mini/mini_bench_spike.elf

# Run on QEMU (should finish in < 1 s)
$RISCV/bin/qemu-system-riscv64 \
    -machine virt \
    -cpu rv64,v=true,vlen=256,elen=64,vext_spec=v1.0,zbkb=true,zbc=true,zvbc=true \
    -m 256M -nographic -bios none \
    -kernel build-mini/mini_bench_qemu.elf \
    -serial mon:stdio -no-reboot 2>/dev/null
```

### Run the full HQC-128 benchmark on Spike (fast path)

```bash
# Build Spike probe targets (N_ITER=1)
cmake -S . -B build-spike-probe \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=spike
cmake --build build-spike-probe --target bench_hqc128_karatsuba_ref -j$(nproc)

# Run (should complete in ~2 s)
./sim/spike/run_bench.sh build-spike-probe/bench_hqc128_karatsuba_ref.elf
```

### Run the KAT on QEMU

```bash
# Build QEMU targets
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake \
    -DHQC_SIM=qemu
cmake --build build -j$(nproc)

./sim/qemu/run_kat.sh build/hqc128_karatsuba_ref.elf
```

### Run all five polynomial-mul variants

```bash
for variant in karatsuba_ref karatsuba_zbc fafft_zbc hybrid_zbc fafft_zvbc; do
    echo "=== $variant ==="
    ./sim/spike/run_bench.sh \
        build-spike-probe/bench_hqc128_${variant}.elf 2>/dev/null
done
```

### Rebuild Spike with optimized host compiler (optional, +15–30% throughput)

```bash
bash toolchain/build-spike.sh
```

This rebuilds from `$HOME/Thesis/src/riscv-isa-sim` with `-O3 -march=native`, strips
debug symbols, and reinstalls to `$RISCV/bin/spike`.
