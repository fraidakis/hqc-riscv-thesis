# Spec Prompt: Make the RVV Variant Faster Than Scalar Zbc on SpacemiT X60

## Problem Statement

Our `karatsuba_rvv` polynomial multiplication variant for HQC is **53% slower** than the scalar `karatsuba_zbc` variant on real hardware (SpacemiT X60, BPI-F3), despite executing **28% fewer instructions**. The root cause is low IPC (0.71 vs 1.35) due to the X60's in-order dual-issue pipeline penalizing vector instruction latency.

### Current Hardware Measurements (HQC-128, vect_mul, median of 10,000 runs)

| Variant | Cycles | Instructions | IPC |
|---------|--------|-------------|-----|
| karatsuba_zbc | 145,130 | 195,399 | 1.35 |
| karatsuba_rvv | 221,592 | 158,368 | 0.71 |

### Why RVV is Slow

1. **RVV only vectorizes the "glue" (copy/XOR), not the hot path (clmul leaves)**. The O(n²) schoolbook multiply at Karatsuba leaves uses identical scalar `clmul`/`clmulh` in both variants.
2. **Vector instruction startup cost**: Each `vle64.v`/`vse64.v`/`vxor.vv` with LMUL=m4 stalls the in-order pipeline for multiple cycles.
3. **vsetvl overhead**: Repeated CSR writes at each helper function entry.
4. **The scalar 4-way unrolled loops achieve IPC ~1.5** on the dual-issue core — hard to beat with vector ops that serialize.

### Hardware Constraints

- **ISA**: rv64imafdcv_zba_zbb_zbc_zbs (NO Zvbc — cannot vectorize clmul itself)
- **VLEN**: 256 bits (4 × uint64_t per vector register)
- **Microarchitecture**: SpacemiT X60, dual-issue in-order, 8 cores
- **Compiler**: GCC 14.2 (native on board)
- **Measurement**: `perf_event_open` with `exclude_kernel=1`

### Key Files

- `src/mult/karatsuba_rvv/gf2x_clmul.c` — current RVV implementation
- `src/mult/karatsuba_rvv/karatsuba_rvv_specialized_gen.h` — generated recursive tree
- `src/mult/karatsuba_zbc/gf2x_clmul.c` — scalar champion (target to beat)
- `src/mult/karatsuba_zbc/karatsuba_specialized_gen.h` — scalar recursive tree
- `tests/bench/Makefile.hw` — hardware benchmark build system
- `tests/bench/bench_hw_mul.c` — polymul micro-benchmark harness
- `analysis/benchmarks/hardware/` — results storage

### Benchmark Workflow

Build and test on the BPI-F3 board (see `.kiro/steering/banana-pi-hardware.md`):
```bash
# On host: push changes, then on board: pull + build + run
make -f Makefile.hw mul LEVEL=128 POLYMUL=karatsuba_rvv  # MODE=dev by default (N=100)
taskset -c 7 ./bench_hw_mul_hqc128_karatsuba_rvv
```

## Goal

Explore, implement, and benchmark optimizations to the `karatsuba_rvv` variant that make it **faster than `karatsuba_zbc` (145,130 cycles)** on the SpacemiT X60. If that's not achievable without Zvbc, determine the minimum gap and document why.

## Optimization Avenues to Explore

### 1. Reduce Vector Instruction Count and Overhead

- **Fuse copy+XOR operations**: Instead of separate `rvv_copy_u64` then `rvv_xor3_u64`, write directly to the output during the XOR accumulation phase (eliminate Loop 1 + Loop 2 entirely).
- **Eliminate vsetvl calls**: Use a single `vsetvl` at function entry and pass `vl` as a parameter to all helpers. Or use fixed LMUL with compile-time-known lengths.
- **Use LMUL=m8 for bulk operations**: Doubles throughput per instruction (8 × uint64_t = 512 bits per op at VLEN=256). Fewer loop iterations = fewer vsetvl + branch instructions.

### 2. Restructure the Karatsuba Assembly Phase

- **Inline the RVV operations into the generated code**: Instead of calling helper functions (`rvv_xor_u64`, `rvv_copy_u64`), emit the vector intrinsics directly in `karatsuba_rvv_specialized_gen.h`. This eliminates function call overhead and allows the compiler to schedule vector ops between scalar clmul pairs.
- **Interleave scalar clmul with vector XOR**: While the scalar pipeline computes `clmul_leaf`, the vector unit could be doing the XOR accumulation from the previous level. This requires restructuring the recursion to overlap computation.

### 3. Vectorize the Reduction Phase

The `reduce()` function does a shift+XOR loop over `VEC_N_SIZE_64` elements. This is a perfect candidate for RVV:
```c
// Current: scalar loop
for (i = 0; i < VEC_N_SIZE_64; i++)
    o[i] = a[i] ^ (a[i + VEC_N_SIZE_64 - 1] >> shift) ^ (a[i + VEC_N_SIZE_64] << inv);
```
Vectorize with `vsrl.vx` + `vsll.vx` + `vxor.vv`. This is pure data-parallel — ideal for RVV.

### 4. Explore Alternative Algorithms

- **Lower the Karatsuba threshold**: Currently T=16 (leaves are 8–16 words). A lower threshold (T=8) means more recursion levels but smaller leaves — more opportunity for RVV to amortize the copy/XOR cost over fewer clmul operations per level.
- **Raise the threshold**: T=32 means fewer recursion levels, fewer copy/XOR operations total, but larger leaves. Profile which direction helps.

### 5. Memory Access Pattern Optimization

- **Avoid tmp_buf entirely**: Compute z0, z2, zmid directly into the output buffer with careful offset management. Eliminates all copy operations.
- **Align buffers for vector access**: Ensure all arrays are 32-byte aligned (VLEN/8) to avoid misaligned vector load penalties.
- **Prefetch**: If the X60 supports hardware prefetch hints, use them before large vector loads.

### 6. Compiler Tuning

- **Try different LMUL values**: m1, m2, m4, m8 — profile each to find the sweet spot on X60.
- **Inspect generated assembly**: `objdump -d` the hot functions to verify the compiler isn't inserting unnecessary spills or redundant vsetvl instructions.

## Success Criteria

1. `karatsuba_rvv` median cycles < 145,130 (beats scalar Zbc) on HQC-128 polymul
2. KAT tests still pass (correctness preserved)
3. Improvement holds across HQC-192 and HQC-256
4. Document the optimization that worked and why

## Testing Each Optimization (Step-by-Step)

Follow the `.kiro/steering/banana-pi-hardware.md` guide. For each optimization attempt:

### 1. Implement the change locally

Edit `src/mult/karatsuba_rvv/gf2x_clmul.c` (or the code generator) on the host.

### 2. Verify correctness on Spike first

```bash
# Build and run KAT on Spike (fast, deterministic)
cmake --build build-spike --target hqc128_karatsuba_rvv
scripts/test/run_kat.sh build-spike/hqc128_karatsuba_rvv.elf 128 karatsuba_rvv
```

If KAT fails, the optimization is incorrect — fix before proceeding.

### 3. Measure instruction count on Spike (optional but useful)

```bash
cmake --build build-spike --target bench_polymul_hqc128_karatsuba_rvv
sim/spike/run.sh build-spike/bench_polymul_hqc128_karatsuba_rvv.elf
```

Compare instret against the baseline (160,215 for current karatsuba_rvv).

### 4. Push to GitHub and pull on the board

```bash
# Host
git add src/mult/karatsuba_rvv/
git commit -m "perf(rvv): <description of optimization>"
git push

# Board (via SSH from host)
sshpass -p 'bianbu' ssh -o StrictHostKeyChecking=no root@192.168.1.50 \
  "su - banana -c 'cd /home/banana/hqc-riscv && git pull'"
```

### 5. Build on the board (dev mode — fast iteration)

```bash
sshpass -p 'bianbu' ssh -o StrictHostKeyChecking=no root@192.168.1.50 \
  "su - banana -c 'cd /home/banana/hqc-riscv/tests/bench && \
   rm -f bench_hw_mul_hqc128_karatsuba_rvv && \
   make -f Makefile.hw mul LEVEL=128 POLYMUL=karatsuba_rvv'"
```

Note: `rm -f` is needed because N_ITER/N_WARMUP are baked into the binary at compile time. The Makefile won't rebuild if only source files changed upstream (the Makefile tracks local deps only).

### 6. Run and compare (dev mode — N=100, ~1s)

```bash
sshpass -p 'bianbu' ssh -o StrictHostKeyChecking=no root@192.168.1.50 \
  "cd /home/banana/hqc-riscv/tests/bench && \
   taskset -c 7 ./bench_hw_mul_hqc128_karatsuba_rvv"
```

**Target to beat**: median_cycles < 145,130 (karatsuba_zbc baseline).

Compare against the scalar baseline (already built):
```bash
sshpass -p 'bianbu' ssh -o StrictHostKeyChecking=no root@192.168.1.50 \
  "cd /home/banana/hqc-riscv/tests/bench && \
   taskset -c 7 ./bench_hw_mul_hqc128_karatsuba_zbc"
```

### 7. If promising, run publish-quality measurement

```bash
sshpass -p 'bianbu' ssh -o StrictHostKeyChecking=no root@192.168.1.50 \
  "su - banana -c 'cd /home/banana/hqc-riscv/tests/bench && \
   rm -f bench_hw_mul_hqc128_karatsuba_rvv && \
   make -f Makefile.hw mul LEVEL=128 POLYMUL=karatsuba_rvv MODE=publish' && \
   cd /home/banana/hqc-riscv/tests/bench && \
   nice -n -20 taskset -c 7 ./bench_hw_mul_hqc128_karatsuba_rvv"
```

### 8. Save results and iterate

```bash
# Save to results directory on board
sshpass -p 'bianbu' ssh -o StrictHostKeyChecking=no root@192.168.1.50 \
  "cd /home/banana/hqc-riscv/tests/bench && \
   nice -n -20 taskset -c 7 ./bench_hw_mul_hqc128_karatsuba_rvv \
   > /home/banana/hqc-riscv/analysis/benchmarks/hardware/mul_hqc128_karatsuba_rvv.csv"

# Pull to host
sshpass -p 'bianbu' scp -o StrictHostKeyChecking=no \
  root@192.168.1.50:/home/banana/hqc-riscv/analysis/benchmarks/hardware/mul_hqc128_karatsuba_rvv.csv \
  analysis/benchmarks/hardware/
```

### Quick A/B Comparison Template

For rapid iteration, run both variants back-to-back:
```bash
sshpass -p 'bianbu' ssh -o StrictHostKeyChecking=no root@192.168.1.50 \
  "cd /home/banana/hqc-riscv/tests/bench && \
   echo '=== ZBC (baseline) ===' && taskset -c 7 ./bench_hw_mul_hqc128_karatsuba_zbc && \
   echo '=== RVV (candidate) ===' && taskset -c 7 ./bench_hw_mul_hqc128_karatsuba_rvv"
```

### Important Notes

- Always `rm -f` the binary before `make` when testing a new optimization — the Makefile dependency tracking doesn't see upstream git changes.
- Dev mode (N=100) gives median within 1–2% of publish mode — sufficient for A/B comparisons.
- If an optimization reduces instret on Spike but doesn't improve cycles on hardware, the bottleneck is vector pipeline latency, not instruction count.
- Check IPC: `median_instret / median_cycles`. Target IPC > 1.0 for the RVV variant to have a chance of winning.

## Deliverables

- Modified `src/mult/karatsuba_rvv/gf2x_clmul.c` with the winning optimization
- Updated `karatsuba_rvv_specialized_gen.h` if the code generator needs changes
- Hardware benchmark results in `analysis/benchmarks/hardware/`
- Analysis document explaining what worked, what didn't, and the microarchitectural reasoning
