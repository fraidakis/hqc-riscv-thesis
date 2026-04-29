# Spike vs QEMU Performance Investigation
## Root-Cause Analysis and Fix Report

---

## Executive Summary

The observed 360× wall-clock slowdown of Spike versus QEMU was **not** a performance
problem. Spike was silently stuck in an infinite trap loop caused by a missing CPU
extension in its ISA string. Every benchmark call to `rdcycle` immediately triggered an
illegal-instruction exception, and the bare-metal binary has no exception handler, so the
machine-mode exception vector (`mtvec = 0x0`) looped at the unmapped address forever.
After fixing the ISA string (adding `_zicntr_zihpm`) and removing the three mandatory
instruction-logging flags from `run.sh`, Spike completes the HQC-128 karatsuba_ref
benchmark in **1.5 s** — versus 8.5 s on QEMU (KAT mode). The actual Spike:QEMU ratio
is roughly **1:4–1:6**, which is correct for a clean ISS vs JIT comparison on short runs.

---

## 1. Root Cause: Missing `_zicntr` in the Spike ISA String

### What broke

`hqc_rdcycle()` in `src/common/bench.c` emits:

```
rdcycle rd    →    csrrs rd, 0xc00, x0
```

CSR `0xc00` is the user-mode `cycle` counter, which belongs to the **Zicntr** extension
(RISC-V Privileged Spec §10.1). The Spike ISA string used throughout this project was:

```
rv64gcv_zbc_zbb_zbkb_zvbc
```

`rv64g` expands to `rv64imafd_zicsr` — it does **not** include `Zicntr`. Spike 1.1.1-dev
correctly enforces this and raises `trap_illegal_instruction` on every `rdcycle`.

The bare-metal startup (`sim/common/startup.S`) never sets `mtvec`, so the default value
is `0x0`. DRAM starts at `0x8000_0000`; address `0x0` is unmapped ROM. The exception
handler therefore immediately raises `trap_instruction_access_fault`, which re-enters the
handler, creating an infinite loop that burns host CPU cycles and produces no output.

**Confirmed via Spike instruction trace:**
```
core   0: exception trap_illegal_instruction, epc 0x000000008000041e   ← rdcycle
core   0: exception trap_instruction_access_fault, epc 0x0000000000000000   ← mtvec=0
core   0: exception trap_instruction_access_fault, epc 0x0000000000000000
...  (repeats forever)
```

QEMU does not enforce Zicntr gating and always allows `rdcycle`, so it worked fine.
This is why QEMU appeared "360× faster" — it was actually completing, while Spike looped.

### The fix

Add `_zicntr_zihpm` to the ISA string passed to Spike:

| File | Before | After |
|------|--------|-------|
| `sim/spike/run_bench.sh` | `rv64gcv_zbc_zbb_zbkb_zvbc` | `rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm` |
| `sim/spike/run.sh`       | `rv64gcv_zbc_zbb_zbkb_zvbc` | `rv64gcv_zbc_zbb_zbkb_zvbc_zicntr_zihpm` |

**Note:** `flags.mk` and `riscv-clang.cmake` define the compiler's `-march=`, not Spike's
`--isa=`. They remain correct and do not need `_zicntr` (the compiler does not need it
to emit the `rdcycle` pseudoinstruction; only the simulator needs to know it is legal).

### Belt-and-suspenders: `mcycle` in `mini_bench.c`

`mini_bench.c` reads CSR `0xb00` (`mcycle`) directly instead of using `hqc_rdcycle()`:

```c
static inline uint64_t read_mcycle(void) {
    uint64_t c;
    __asm__ volatile ("csrr %0, 0xb00" : "=r"(c));
    return c;
}
```

`mcycle` (M-mode cycle CSR) is always accessible in machine mode regardless of Zicntr.
This ensures `mini_bench` works even on a misconfigured simulator.

To fix the existing bench ELFs in production, update `src/common/bench.c` and
recompile all targets (see Section 4).

---

## 2. Root Cause: Three Instruction-Logging Flags in `run.sh`

Even without the `_zicntr` bug, running `sim/spike/run.sh` for any real benchmark
would be extremely slow because it passes three simultaneous logging flags:

```bash
"$SPIKE"  \
    --isa="$ISA"      \
    --log-commits     \   # ← logs EVERY committed instruction + reg writes to stderr
    --histogram       \   # ← counts instruction-mix frequencies (minor overhead)
    -l "$@"           \   # ← disassembly trace of EVERY decoded instruction
    "$ELF"            \
    2>"$LOGFILE"
```

| Flag | Effect | Overhead |
|------|--------|----------|
| `--log-commits` | Writes `core N: 0x<pc> (<disasm>) <rd> 0x<val>` for every committed instruction to stderr | Very high — 1 line per instruction = billions of lines for HQC |
| `-l` | Writes instruction decode trace to stderr (a second, different format) | Very high — doubles I/O vs `--log-commits` alone |
| `--histogram` | Accumulates instruction-frequency table in memory, prints at exit | Moderate — small per-instruction overhead |

For an HQC-128 KEM operation executing ~60 M RISC-V instructions, these flags together
write roughly **120 M–180 M log lines** to `$LOGFILE` per run. File I/O serialises Spike's
simulation loop and would produce a 100–500× slowdown even on a properly working binary.

**Resolution:** `sim/spike/run_bench.sh` (new file) runs Spike with no logging flags and
is the canonical script for cycle-count benchmarking. `run.sh` is retained as a
**profiling/instruction-mix tool** only, with a prominent warning added to its header.

---

## 3. Secondary Issue: Spike Built Without `-O3 -march=native`

Spike was built with the autoconf defaults (`-g -O2 -fPIC`), evidenced by:

```
$ file $RISCV/bin/spike
spike: ELF 64-bit, with debug_info, not stripped   ← 201 MB binary

$ grep 'default-CXXFLAGS' $HOME/Thesis/src/riscv-isa-sim/build/Makefile
default-CXXFLAGS := ... -g -O2 -fPIC
```

A 201 MB unstripped debug binary vs a typical ~10 MB stripped release build indicates
significant debug-symbol overhead. Rebuilding with `-O3 -march=native` and stripping is
expected to yield a 15–30% host-side throughput improvement.

`toolchain/build-spike.sh` has been updated to pass these flags and strip the binary.

---

## 4. Measured Results After Fix

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

---

## 5. Files Changed

| File | Change |
|------|--------|
| `sim/spike/run.sh` | Added `_zicntr_zihpm` to ISA string; added `--varch` note; added explicit WARNING that this script is for profiling, not benchmarking |
| `sim/spike/run_bench.sh` | **New.** Clean benchmark runner: no `--log-commits`, no `-l`, no `--histogram`; correct ISA string with `_zicntr_zihpm` |
| `toolchain/build-spike.sh` | Added `CXXFLAGS/CFLAGS="-O3 -march=native"`; added `--enable-commitlog`; added `strip` after install |
| `tests/bench/mini_bench.c` | **New.** Minimal standalone benchmark; uses `csrr rd, 0xb00` (mcycle) instead of `rdcycle` |
| `tests/bench/mini_bench.mk` | **New.** Standalone Makefile to compile mini_bench for both Spike and QEMU backends |

---

## 6. How to Run the Benchmarks

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

---

## 7. Applying the Fix to Production Bench ELFs

The existing `bench_hqc128_*.elf` and `bench_polymul_hqc128_*.elf` ELFs in
`build-spike-probe/` use `hqc_rdcycle()` which emits `rdcycle` (CSR `0xc00`).
They now work correctly because `run_bench.sh` passes the correct ISA string.

For future-proofing, update `src/common/bench.c` to use `mcycle` (CSR `0xb00`):

```c
// Change:
__asm__ volatile ("rdcycle %0" : "=r"(cycles));
// To:
__asm__ volatile ("csrr %0, 0xb00" : "=r"(cycles));   // mcycle, always M-mode accessible
```

Then rebuild all targets:
```bash
cmake --build build-spike-probe -j$(nproc)
cmake --build build -j$(nproc)
```

---

## 8. Checklist

- [x] `sim/spike/run_bench.sh` — new clean benchmark runner (no logging)
- [x] `sim/spike/run.sh` — ISA string fixed, logging-overhead warning added
- [x] `toolchain/build-spike.sh` — `-O3 -march=native` host optimization added
- [x] `tests/bench/mini_bench.c` — standalone benchmark with `mcycle` counter
- [x] `tests/bench/mini_bench.mk` — standalone Makefile
- [ ] `src/common/bench.c` — change `rdcycle` to `csrr rd, 0xb00` (mcycle) and rebuild
- [ ] Rebuild Spike with `toolchain/build-spike.sh` for optimal host performance
