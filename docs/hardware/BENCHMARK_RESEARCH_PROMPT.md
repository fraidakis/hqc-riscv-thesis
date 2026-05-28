# Research Prompt: Professional Benchmarking Methodology for Post-Quantum Cryptography on Real RISC-V Hardware

## Context

I am writing a diploma thesis on optimizing HQC (Hamming Quasi-Cyclic) — a NIST post-quantum KEM — for RISC-V, specifically targeting the SpacemiT X60 core (Banana Pi BPI-F3). The ISA is rv64imafdcv_zba_zbb_zbc_zbs_zicntr_zihpm with RVV 1.0.

I have multiple polynomial multiplication variants (scalar with Zbc carry-less multiply, RVV vectorized) and need to benchmark them on real hardware running Linux (kernel 6.6.63, Bianbu distro). Note: the board does NOT have Zvbc, so only scalar Zbc and RVV (base V extension) variants will be tested on hardware.

**Hardware constraints discovered:**
- `rdcycle`/`rdinstret` CSRs return 0 from userspace (kernel doesn't enable scounteren)
- `perf_event_open` syscall works and gives per-region cycle + instruction counts
- `perf stat` works for whole-process measurement
- Native GCC 14.2 is available on the board
- 8-core SpacemiT X60, 16 GB RAM, Linux 6.6.63

## Research Questions

Search the literature and current best practices for the following:

### 1. What metrics do published PQC papers report for hardware benchmarks?

Look at papers from:
- NIST PQC competition submissions and evaluation reports
- CHES (Cryptographic Hardware and Embedded Systems) conference papers
- TCHES (Transactions on Cryptographic Hardware and Embedded Systems)
- Papers benchmarking lattice/code-based KEM on ARM, RISC-V, or embedded platforms
- Specifically: HQC implementation papers, BIKE papers, Classic McEliece papers (code-based)
- PQC on RISC-V papers (e.g., Fritzmann et al., Stoffelen, Alkim et al.)

What do they measure?
- CPU cycles (total per operation: KeyGen, Encaps, Decaps)?
- Instructions retired?
- Wall-clock time (ms/μs)?
- Throughput (ops/sec)?
- Cycles per byte of ciphertext/shared secret?
- Stack/memory usage?
- Code size (.text section)?
- Cache misses / branch mispredictions?
- Energy consumption?

### 2. What is the gold-standard measurement methodology?

- How many iterations/repetitions are standard? (median of N runs? trimmed mean?)
- How do papers handle warm-up passes vs cold-start?
- Do they pin to a single core? Disable frequency scaling? Isolate from OS noise?
- What statistical treatment is applied? (min/median/max? confidence intervals? standard deviation?)
- Is there a standard benchmarking framework for PQC? (SUPERCOP? PQM4? pqcrypto-riscv?)
- How do papers handle the difference between cycle-accurate simulators (like Spike) vs real hardware measurements?

### 3. How should I handle the Linux userspace measurement problem?

Given that `rdcycle` doesn't work but `perf_event_open` does:
- Is `perf_event_open` with `exclude_kernel=1` considered acceptable in published work?
- Should I use `perf stat` wrapping the whole binary instead?
- Is there a way to get the kernel to enable `scounteren` (recompile kernel? kernel module?)?
- What do other RISC-V papers do when running on Linux vs bare-metal?
- Is there a standard RISC-V PQC benchmarking harness for Linux targets?

### 4. What should my benchmark report contain for a thesis?

For a diploma/master thesis on PQC optimization:
- What tables/figures are expected? (cycle counts per operation? speedup ratios? comparison with reference?)
- Should I report both Spike (deterministic instruction count) AND real hardware (actual cycles with pipeline effects)?
- How do I present the IPC difference between simulator and real hardware?
- What's the standard way to compare against other published implementations?
- Should I include micro-benchmarks (just polymul) AND macro-benchmarks (full KEM)?

### 5. Cross-compilation and deployment best practices

- Should I cross-compile on the host (Clang targeting riscv64-linux-gnu) or compile natively on the board (GCC 14.2)?
- What optimization flags are appropriate? (-O3? -Os? LTO? PGO?)
- How do I ensure the compiler actually emits the Zbc instructions I expect? (objdump verification?)
- Should I use static linking to avoid shared library overhead in measurements?

### 6. Specific metrics to prioritize for my thesis

Given that I'm comparing:
- `ref_scalar` (vendor baseline, no extensions)
- `karatsuba_zbc` (scalar optimized with Zbc carry-less multiply)
- `karatsuba_rvv` (RVV vectorized hybrid)


What metrics best demonstrate the value of each optimization?
- Absolute cycles for each KEM operation?
- Speedup factor vs reference?
- Cycles per polynomial multiplication (isolated micro-benchmark)?
- IPC (instructions per cycle) to show pipeline utilization?
- VLEN scaling (how performance changes with vector register width — X60 has VLEN=256)?

## Deliverable

Produce a structured recommendation document covering:
1. The exact metrics to measure and report
2. The measurement methodology (iterations, warm-up, pinning, statistics)
3. The tooling approach (perf_event_open harness vs perf stat vs other)
4. A comparison table format suitable for a thesis
5. References to published papers that use similar methodology
6. Any pitfalls or common mistakes to avoid

Ground this in what CHES/TCHES papers actually do, not just theoretical best practices.
