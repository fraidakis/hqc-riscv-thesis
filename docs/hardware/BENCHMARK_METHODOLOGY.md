# Benchmarking Methodology for HQC on RISC-V (SpacemiT X60)

## Structured Recommendations for Diploma Thesis

---

## 1. Metrics to Measure and Report

### 1.1 Primary Metrics (Mandatory)

Based on established practice in CHES/TCHES publications and NIST PQC evaluation reports:

| Metric | Unit | Granularity | Justification |
|--------|------|-------------|---------------|
| **CPU cycles** | cycles | Per KEM operation (KeyGen, Encaps, Decaps) | Gold standard in all PQC papers; directly comparable across platforms at same frequency |
| **Instructions retired** | count | Per KEM operation | Shows algorithmic complexity independent of microarchitecture |
| **IPC** (Instructions Per Cycle) | ratio | Per KEM operation | Reveals pipeline utilization; critical for showing vectorization benefit |
| **Stack usage** | bytes | Per operation | Standard in pqm4/SUPERCOP; shows embedded suitability |
| **Code size** (.text) | bytes | Per variant | Standard in pqm4; shows ROM/flash footprint |

### 1.2 Secondary Metrics (Recommended for Thesis)

| Metric | Unit | Granularity | Justification |
|--------|------|-------------|---------------|
| **Cycles per polymul** | cycles | Isolated micro-benchmark | Your core contribution is polymul optimization; isolate it |
| **Speedup factor** | ×  | vs. reference implementation | Makes contribution immediately quantifiable |
| **Wall-clock time** | μs | Per KEM operation | Practical relevance for real deployments |
| **Cycles/byte** | cycles/byte | Per ciphertext byte | Normalizes across security levels (HQC-128/192/256) |

### 1.3 Tertiary Metrics (Nice-to-Have)

| Metric | Notes |
|--------|-------|
| **VLEN scaling** | How cycles change with VLEN=128 vs 256 (Spike can simulate both) |
| **Cache misses** | If `perf_event_open` exposes L1d/L1i miss counters on X60 |
| **Branch mispredictions** | Same caveat — check PMU availability |
| **Energy** | Not feasible without external power measurement equipment |

### 1.4 What Published Papers Report

The following is a synthesis of reporting conventions observed across major PQC implementation papers:

- **NIST submission documents** (HQC, BIKE, Kyber): Report cycles for KeyGen/Encaps/Decaps at each security level, measured on specific platforms (Intel Haswell/Skylake with `rdtsc`, ARM Cortex-M4 with DWT_CYCCNT).
- **pqm4** (Kannwischer et al., TCHES 2019): Reports cycle counts, stack usage, and .text/.data/.bss sizes. Uses DWT cycle counter on Cortex-M4. Single execution per measurement (deterministic bare-metal).
- **SUPERCOP** (Bernstein & Lange): Reports median of multiple runs. Uses `rdtsc`/`rdtscp` on x86. Tries multiple compiler flags and reports best.
- **ECO-CRYSTALS** (2024, RISC-V): Reports cycles for each KEM operation, speedup vs reference. Uses cycle counters on SiFive U74 (HiFive Unmatched).
- **OptHQC** (2024): Reports cycles and speedup for each HQC operation on x86 with `rdtsc`.
- **"All the Polynomial Multiplication You Need on RISC-V"** (Seo et al., 2021): Reports cycle counts for isolated polynomial multiplication kernels on RISC-V, using Zbc carry-less multiply instructions.
- **Fritzmann et al. (RISQ-V, TCHES 2020)**: Reports cycles on FPGA-based RISC-V with custom accelerators.

**Consensus**: Cycles per operation is the universal metric. Everything else is supplementary.

---

## 2. Measurement Methodology

### 2.1 Iteration Count and Statistical Treatment

**Established conventions from the literature:**

| Framework/Paper | Iterations | Statistic Reported | Warm-up |
|-----------------|-----------|-------------------|---------|
| SUPERCOP | Variable (auto-calibrated) | Median of best runs | Yes (discards initial) |
| pqm4 | 1 (bare-metal, deterministic) | Single value | N/A (no OS) |
| NIST reference benchmarks | 10,000+ | Average or median | Yes |
| Typical CHES paper (Linux) | 1,000–10,000 | Median | 100–1,000 warm-up |
| OptHQC (2024) | 1,000 | Average | Not specified |

**Recommendation for your thesis:**

```
Warm-up:     100 iterations (discarded)
Measured:    10,000 iterations
Reported:    Median (primary), Mean, Min, Max, Std Dev
```

**Why median over mean**: Median is robust to outlier spikes caused by interrupts, context switches, and timer interrupts. This is the standard in SUPERCOP and most CHES papers running on Linux.

**Additionally report**: Interquartile range (IQR) or standard deviation to show measurement stability.

### 2.2 System Configuration for Reproducibility

**CPU Isolation and Pinning:**
```bash
# Pin benchmark process to a single core (e.g., core 7)
taskset -c 7 ./benchmark

# Better: boot with kernel parameter (if you can modify boot args)
# isolcpus=7 nohz_full=7 rcu_nocbs=7
```

**Frequency Scaling:**
```bash
# Check current governor
cat /sys/devices/system/cpu/cpu7/cpufreq/scaling_governor

# Set to performance (disable DVFS)
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Verify frequency is stable
cat /sys/devices/system/cpu/cpu7/cpufreq/scaling_cur_freq
```

**OS Noise Reduction:**
```bash
# Disable unnecessary services during benchmarking
# Run at elevated priority
sudo nice -n -20 taskset -c 7 ./benchmark

# Or use chrt for real-time scheduling
sudo chrt -f 99 taskset -c 7 ./benchmark
```

**Checklist before measurement:**
1. ☐ CPU governor set to `performance`
2. ☐ Process pinned to isolated core
3. ☐ No other user processes on that core
4. ☐ System idle (no background compilation, no network-heavy tasks)
5. ☐ Verify cycle counter is working (non-zero values)
6. ☐ Static binary (no dynamic linker overhead)
7. ☐ Warm-up iterations completed before measurement begins

### 2.3 Simulator vs. Real Hardware

**Report both Spike and real hardware measurements.** This is a strength, not a weakness:

| Aspect | Spike (Functional Simulator) | Real Hardware (BPI-F3) |
|--------|------------------------------|------------------------|
| What it measures | Instructions retired (deterministic) | Actual CPU cycles (with pipeline effects) |
| Reproducibility | Perfect (bit-exact) | Statistical (needs many runs) |
| IPC assumption | 1.0 (by definition) | Actual (reveals pipeline utilization) |
| Use case | Algorithmic comparison, instruction count | Real-world performance |
| OS effects | None | Interrupts, cache, TLB |

**How to present the difference:**
- Table: "Instruction Count (Spike)" alongside "Cycles (Hardware)" and "IPC (Hardware)"
- The IPC column directly shows how well the microarchitecture handles your code
- IPC > 1.0 for RVV code shows superscalar/vector benefit
- IPC < 1.0 for scalar code shows pipeline stalls

---

## 3. Tooling Approach

### 3.1 Primary: `perf_event_open` Harness (Recommended)

Your existing prototype in `.tmp/test_perf_event.c` is the correct approach. This is the gold standard for Linux userspace benchmarking when `rdcycle` is unavailable.

**Why this is acceptable in published work:**
- `perf_event_open` with `exclude_kernel=1` measures only userspace cycles — this is exactly what you want
- It uses the same hardware PMU counters that `rdcycle` would read, just accessed through the kernel interface
- The overhead of the ioctl enable/disable is amortized over thousands of iterations
- Multiple RISC-V papers running on Linux use this approach when direct CSR access is restricted

**Enhanced harness design:**

```c
// Pseudocode for benchmark harness structure
struct bench_result {
    uint64_t cycles;
    uint64_t instructions;
};

// 1. Open perf counters (once at startup)
// 2. Warm-up loop: N_WARMUP iterations (discard)
// 3. Measurement loop:
//    for i in 0..N_MEASURE:
//        reset counters
//        enable counters
//        operation_under_test()
//        disable counters
//        read counters → results[i]
// 4. Compute statistics: median, mean, stddev, min, max
// 5. Print results in machine-parseable format (CSV/JSON)
```

**Key design decisions:**
- Reset + enable/disable around EACH iteration (not just start/stop around the whole loop)
- This gives you per-iteration cycle counts for statistical analysis
- The ioctl overhead (~100 cycles) is negligible for operations taking 100k+ cycles

### 3.2 Secondary: `perf stat` (Whole-Process Validation)

Use `perf stat` as a cross-check:
```bash
perf stat -e cycles,instructions,cache-misses,branch-misses \
    taskset -c 7 ./hqc_bench_static
```

This validates your harness numbers against the kernel's own accounting.

### 3.3 Regarding `rdcycle` / `scounteren`

**Options to enable `rdcycle` from userspace:**

1. **Kernel recompilation**: Set `scounteren.CY = 1` in the kernel's RISC-V initialization code. The relevant code is in `arch/riscv/kernel/perf_event.c` or the SBI initialization path. On Linux 6.6, look for where `scounteren` is written.

2. **Kernel module**: Write a small module that sets `scounteren` bits. This is fragile and non-portable.

3. **Pragmatic recommendation**: Don't bother. `perf_event_open` gives you the same data with better portability and is accepted in published work. The overhead difference is negligible for operations in the 100k+ cycle range.

**What other RISC-V papers do:**
- Papers on bare-metal (FPGA prototypes, SiFive boards with custom firmware): Use `rdcycle` directly
- Papers on Linux: Use `perf_event_open` or `perf stat`, or note that they patched the kernel
- The ECO-CRYSTALS paper (2024) on SiFive U74 used direct CSR access because that board's kernel enables it

### 3.4 Measurement Overhead Analysis

| Method | Overhead per measurement | Acceptable for |
|--------|------------------------|----------------|
| `perf_event_open` ioctl | ~100–200 cycles | Operations > 10k cycles |
| `perf stat` (whole process) | ~1ms startup | Macro-benchmarks only |
| `rdcycle` (if available) | 1 instruction | Everything |

For HQC operations (100k–10M cycles), the `perf_event_open` overhead is < 0.2%. Negligible.

---

## 4. Comparison Table Formats for Thesis

### 4.1 Main Results Table (Per-Operation Cycles)

```
Table X: HQC-128 Performance on SpacemiT X60 (VLEN=256, 1.6 GHz)

| Variant          | KeyGen    | Encaps    | Decaps    | Total     | Speedup |
|                  | (cycles)  | (cycles)  | (cycles)  | (cycles)  | vs ref  |
|------------------|-----------|-----------|-----------|-----------|---------|
| ref_scalar       | xxx,xxx   | xxx,xxx   | xxx,xxx   | x,xxx,xxx | 1.00×   |
| karatsuba_zbc    | xxx,xxx   | xxx,xxx   | xxx,xxx   | x,xxx,xxx | X.XX×   |
| karatsuba_rvv    | xxx,xxx   | xxx,xxx   | xxx,xxx   | x,xxx,xxx | X.XX×   |
```

Repeat for HQC-192 and HQC-256.

### 4.2 Micro-Benchmark Table (Isolated Polynomial Multiplication)

```
Table Y: Polynomial Multiplication Cycles (median of 10,000 runs)

| Variant          | n=17,669  | n=35,851  | n=57,637  | IPC   |
|                  | (HQC-128) | (HQC-192) | (HQC-256) |       |
|------------------|-----------|-----------|-----------|-------|
| ref_scalar       | xxx,xxx   | xxx,xxx   | xxx,xxx   | 0.XX  |
| karatsuba_zbc    | xxx,xxx   | xxx,xxx   | xxx,xxx   | 0.XX  |
| karatsuba_rvv    | xxx,xxx   | xxx,xxx   | xxx,xxx   | X.XX  |
```

### 4.3 Simulator vs. Hardware Comparison

```
Table Z: Spike (instret) vs. Hardware (cycles) — HQC-128 Encaps

| Variant          | Spike instret | HW cycles | IPC  | Pipeline efficiency |
|------------------|---------------|-----------|------|---------------------|
| ref_scalar       | xxx,xxx       | xxx,xxx   | 0.XX | XX%                 |
| karatsuba_zbc    | xxx,xxx       | xxx,xxx   | 0.XX | XX%                 |
| karatsuba_rvv    | xxx,xxx       | xxx,xxx   | X.XX | XX%                 |
```

### 4.4 Comparison with Published Results

```
Table W: Comparison with Published HQC Implementations

| Implementation       | Platform           | HQC-128 Encaps | Notes          |
|                      |                    | (cycles)       |                |
|----------------------|--------------------|----------------|----------------|
| HQC reference        | Intel Skylake      | ~XXX,XXX       | PCLMULQDQ      |
| OptHQC (2024)        | Intel x86          | ~XXX,XXX       | AVX2+PCLMUL   |
| This work (Zbc)      | SpacemiT X60       | XXX,XXX        | Scalar Zbc     |
| This work (RVV)      | SpacemiT X60       | XXX,XXX        | RVV VLEN=256   |
| HQC ref (ARM)        | Cortex-M4 (pqm4)  | ~X,XXX,XXX     | No CLMUL       |
```

**Important caveat**: When comparing across platforms, note the frequency and microarchitecture differences. Cycles are not directly comparable across different CPUs — but they show relative efficiency.

### 4.5 VLEN Scaling Table (Spike Only)

```
Table V: RVV Variant Scaling with VLEN (Spike, HQC-128 polymul)

| VLEN  | Instructions | Speedup vs VLEN=128 |
|-------|-------------|---------------------|
| 128   | xxx,xxx     | 1.00×               |
| 256   | xxx,xxx     | X.XX×               |
| 512   | xxx,xxx     | X.XX×               |
| 1024  | xxx,xxx     | X.XX×               |
```

---

## 5. Compilation and Deployment

### 5.1 Compiler Choice

**Recommendation: Native GCC 14.2 on the board.**

Rationale:
- GCC 14.2 has mature RISC-V V extension support
- Native compilation avoids cross-compilation sysroot issues
- You can verify generated code immediately with `objdump`
- Published RISC-V papers commonly use GCC (versions 10–14)

**If cross-compiling is needed** (for faster iteration):
- Use the same GCC version (14.x) with matching target triple
- Verify identical codegen with `objdump -d` comparison

### 5.2 Optimization Flags

```bash
# Primary benchmark build
CFLAGS="-O3 -march=rv64imafdcv_zba_zbb_zbc_zbs -static"

# For code size measurement
CFLAGS_SIZE="-Os -march=rv64imafdcv_zba_zbb_zbc_zbs -static"
```

**Flag justification:**
- `-O3`: Standard for performance benchmarks in PQC papers
- `-march=rv64imafdcv_zba_zbb_zbc_zbs`: Enable all available extensions
- `-static`: Eliminates dynamic linker overhead and PLT indirection; makes measurements deterministic
- Do NOT use `-flto` unless you also report non-LTO numbers (LTO can obscure what the compiler does with your hand-optimized code)
- Do NOT use PGO (Profile-Guided Optimization) — it's not standard practice in PQC benchmarking and makes reproduction harder

### 5.3 Verification of Generated Code

**Critical**: Verify that Zbc instructions are actually emitted:

```bash
# After compilation
riscv64-linux-gnu-objdump -d hqc_bench | grep -E "clmul|clmulh|clmulr"

# Count occurrences
riscv64-linux-gnu-objdump -d hqc_bench | grep -c "clmul"

# For RVV instructions
riscv64-linux-gnu-objdump -d hqc_bench | grep -E "vle|vse|vxor|vslide|vmul"
```

Report the instruction mix in your thesis — this proves the compiler is using the extensions you claim.

### 5.4 Static Linking

**Always use static linking for benchmarks.** Rationale:
- Eliminates PLT/GOT overhead on function calls
- Removes dynamic linker startup cost
- Makes binary self-contained and reproducible
- Standard practice in embedded/PQC benchmarking

```bash
gcc -O3 -march=rv64imafdcv_zba_zbb_zbc_zbs -static -o hqc_bench hqc_bench.c
```

---

## 6. Metrics That Best Demonstrate Each Optimization

### 6.1 `ref_scalar` → `karatsuba_zbc` (Algorithmic + ISA Extension)

**Best metrics:**
- **Absolute cycle reduction** in polymul (micro-benchmark)
- **Instruction count reduction** (Spike) — shows algorithmic improvement from Karatsuba
- **Speedup factor** for full KEM operations
- **Instruction mix**: Show that `clmul`/`clmulh` replace multi-instruction GF(2) multiply sequences

### 6.2 `karatsuba_zbc` → `karatsuba_rvv` (Vectorization)

**Best metrics:**
- **IPC improvement** — this is the killer metric for vectorization. Scalar IPC ~0.5–0.8, vector IPC can exceed 1.0
- **Cycle reduction** despite potentially similar instruction count (each vector instruction does more work)
- **VLEN scaling** (Spike): Shows how the algorithm scales with wider vectors
- **Throughput** (ops/sec): Practical metric for real deployment

### 6.3 Overall Contribution

Present a **stacked bar chart** or **waterfall chart** showing:
```
ref_scalar [████████████████████████] 100%
karatsuba_zbc [████████████████] XX%  (Karatsuba + Zbc contribution)
karatsuba_rvv [████████████] XX%      (+ RVV contribution)
```

This visually demonstrates the cumulative optimization impact.

---

## 7. Pitfalls and Common Mistakes

### 7.1 Measurement Pitfalls

| Pitfall | Consequence | Mitigation |
|---------|-------------|------------|
| Not disabling frequency scaling | Inconsistent cycle counts | Set governor to `performance` |
| Measuring with debug build | 2–10× slower, misleading | Always use `-O3` for benchmarks |
| Not warming up caches | First iterations are outliers | 100+ warm-up iterations |
| Reporting mean instead of median | Skewed by interrupt spikes | Report median (primary) + mean |
| Timer interrupt during measurement | ~10k cycle spike every ~4ms | Use median; pin to isolated core |
| Measuring too few iterations | High variance | ≥1,000 iterations minimum |
| Including I/O in measurement | Dominates actual computation | Separate I/O from measured region |

### 7.2 Compilation Pitfalls

| Pitfall | Consequence | Mitigation |
|---------|-------------|------------|
| Compiler optimizes away dead code | Measures nothing | Use `volatile` sink or `__attribute__((noinline))` |
| LTO inlines your hand-optimized asm | Breaks intrinsics/asm | Compile asm files separately, or use `-fno-lto` for those TUs |
| Wrong `-march` flag | Zbc/V instructions not emitted | Verify with `objdump` |
| Dynamic linking | PLT overhead in hot path | Use `-static` |
| Different compiler versions | Results not comparable | Report exact compiler version |

### 7.3 Reporting Pitfalls

| Pitfall | Consequence | Mitigation |
|---------|-------------|------------|
| Comparing cycles across different CPUs | Misleading (different μarch) | Note frequency and μarch; compare speedup ratios |
| Not reporting measurement conditions | Not reproducible | Include: compiler, flags, OS, kernel, governor, pinning |
| Claiming "X times faster" without confidence | Statistically unsound | Report median ± IQR or stddev |
| Omitting security level | Incomplete | Always specify HQC-128/192/256 |
| Not verifying correctness | Optimized code might be wrong | Run KAT (Known Answer Tests) before benchmarking |

---

## 8. Recommended Benchmark Report Structure for Thesis

### Chapter/Section Outline:

1. **Experimental Setup** (1 page)
   - Hardware: SpacemiT X60, 8-core, VLEN=256, 1.6 GHz, 16 GB RAM
   - Software: Linux 6.6.63 (Bianbu), GCC 14.2, `-O3 -static`
   - Measurement: `perf_event_open` with `exclude_kernel=1`
   - Protocol: 100 warm-up + 10,000 measured iterations, median reported
   - CPU pinned to core 7, governor=performance

2. **Micro-Benchmarks** (1–2 pages)
   - Isolated polynomial multiplication for each variant
   - Table: cycles, instructions, IPC per variant per security level
   - Bar chart: speedup comparison

3. **Macro-Benchmarks** (1–2 pages)
   - Full KEM operations (KeyGen, Encaps, Decaps)
   - Table: cycles per operation per variant per security level
   - Pie chart: time breakdown by component (polymul, Keccak, sampling, etc.)

4. **Simulator vs. Hardware** (0.5–1 page)
   - Table comparing Spike instret vs. hardware cycles
   - IPC analysis showing pipeline utilization

5. **Comparison with State of the Art** (0.5–1 page)
   - Table comparing with published HQC implementations
   - Caveat about cross-platform comparison limitations

6. **VLEN Scaling Analysis** (0.5 page, Spike only)
   - Table/chart showing instruction count vs. VLEN

---

## 9. Key References

### Directly Relevant Papers

1. **Seo et al.** "All the Polynomial Multiplication You Need on RISC-V" (2021) — Zbc carry-less multiply for PQC on RISC-V. [ePrint 2021/1117](https://eprint.iacr.org/2021/1117)

2. **OptHQC** "Optimize HQC for High-Performance Post-Quantum Cryptography" (2024) — HQC optimization with detailed cycle benchmarks. [arXiv 2512.12904](https://arxiv.org/abs/2512.12904)

3. **"Accelerating HQC with Additive FFT"** (2026) — FFT-based HQC polymul, benchmarked with PCLMULQDQ. [ePrint 2026/014](https://eprint.iacr.org/2026/014)

4. **"Efficient Polynomial Multiplication for HQC on ARM Cortex-M4"** (2025) — FAFFT-CRT method, 33% polymul reduction. [ePrint 2025/1939](https://eprint.iacr.org/2025/1939)

5. **ECO-CRYSTALS** "Efficient Cryptography CRYSTALS on Standard RISC-V ISA" (2024) — Kyber/Dilithium on RISC-V with cycle benchmarks. [ePrint 2024/1198](https://eprint.iacr.org/2024/1198)

6. **RISQ-V** "Tightly Coupled RISC-V Accelerators for Post-Quantum Cryptography" (TCHES 2020) — PQC accelerators on RISC-V. [TCHES 2020](https://tosc.iacr.org/index.php/TCHES/article/view/8683)

### Benchmarking Frameworks

7. **pqm4** "Testing and Benchmarking NIST PQC on ARM Cortex-M4" (2019) — Defines the standard for PQC benchmarking methodology. [ePrint 2019/844](https://eprint.iacr.org/2019/844)

8. **SUPERCOP** — System for Unified Performance Evaluation Related to Cryptographic Operations and Primitives. [bench.cr.yp.to](https://bench.cr.yp.to/supercop.html)

### NIST Reports

9. **NIST IR 8545** "Status Report on the Fourth Round of the NIST PQC Standardization Process" (2025) — HQC selected for standardization. [NIST](https://csrc.nist.gov/pubs/ir/8545/final)

### RISC-V Performance Measurement

10. **Linux kernel RISC-V PMU documentation** — [kernel.org](https://www.kernel.org/doc/html/v5.9/riscv/pmu.html)

---

## 10. Summary: Your Benchmark Checklist

```
□ Build: GCC 14.2, -O3, -static, correct -march
□ Verify: objdump confirms Zbc/RVV instructions present
□ Correctness: KAT tests pass for all variants
□ Setup: governor=performance, taskset to isolated core
□ Measure: perf_event_open harness, exclude_kernel=1
□ Protocol: 100 warm-up + 10,000 measured iterations
□ Statistics: Report median, mean, stddev, min, max
□ Micro: Isolated polymul benchmarks
□ Macro: Full KEM (KeyGen, Encaps, Decaps)
□ Simulator: Spike instruction counts for all variants
□ Compare: IPC = HW_cycles / Spike_instret
□ Tables: Per-variant, per-security-level, with speedup
□ Cross-ref: Compare with published HQC numbers (with caveats)
□ Reproduce: Document exact versions, flags, kernel, board
```
