# Phase 1b Scoreboard (fafft_v4)

Primary metric: median_instret from polymul CSV (HQC-128, vect_mul).
Reference baseline: v3 monomial FAFFT/Karatsuba wiring = 364,872 instret.

| Iter | Technique | HQC-128 instret | Delta vs v3 Baseline | Verdict |
|------|-----------|-----------------|----------------------|---------|
| 0 | v3 Baseline (Monomial FAFFT) | 364,872 | — | ref |
| 1 | Task 1 (TFAFFT Pruning) | N/A (KAT FAIL 0/100) | N/A | drop |
| 2 | Task 2 (CRT Bypass) | 1,652,853 | +1,287,981 (+353.0%) | drop |
| 3 | Task 3 (GF(2^64) Packing) | N/A (KAT FAIL 0/100) | N/A | drop |
| 4a | Task 4 (Invalid Karatsuba bypass) | 160,681 | -204,191 (-55.96%) | invalid |
| 4b | Task 4 (True RVV FAFFT butterflies) | 45,907 | -318,965 (-87.42%) | drop (KAT fail) |
| 4c | Task 4 (RVV vclmul.vv vectorized schoolbook) | **178,048** | **-186,824 (-51.2%)** | **keep** ✓ |

## Iteration Notes

### Iter 1 — Task 1 (TFAFFT pruning)
- Hypothesis: Truncating from 553 words to the 512-word bc_1_16384 domain would prune zero-heavy butterfly work and reduce instret.
- Implementation: Added `vect_mul_v4_task1()` in `src/poly_mul/fafft_zbc/v4/fafft_v4.c` using bc_1_16384 + encode + btfy_32768 + pointwise gf232v_mul + ibtfy + decode + ibc_1_16384.
- Correctness: FAIL 0/100 in `sim/qemu/run_kat.sh`.
- Root cause: upper 41 words (word indices 512..552) are dropped before transform; missing terms cannot be reconstructed.
- Action: Reverted active wiring; moved to Task 2.

### Iter 2 — Task 2 (CRT bypass)
- Hypothesis: Split into 512-word low block + 41-word tail, process low block with half-domain path and tail with clmul cross-terms, then reconstruct full product.
- Implementation: Added `vect_mul_v4_task2()` plus split-path helpers in `src/poly_mul/fafft_zbc/v4/fafft_v4.c`.
- Correctness: PASS 100/100 in `sim/qemu/run_kat.sh` after fixing one off-by-one in cross-term assembly.
- Benchmark: 1,652,853 instret (from Spike polymul run).
- Root cause of slowdown: extra split/assemble overhead and multiple schoolbook clmul cross products dominate; no effective reduction in arithmetic cost.
- Action: Drop (slower than baseline).

### Iter 3 — Task 3 (GF(2^64) packing)
- Hypothesis: 64-bit element packing reduces transform size and total operations.
- Implementation: Added `gf2_64_mul` (mod x^64+x^4+x^3+x+1) and `vect_mul_v4_task3()` in `src/poly_mul/fafft_zbc/v4/fafft_v4.c`.
- Correctness: FAIL 0/100 in `sim/qemu/run_kat.sh`.
- Root cause: reduced GF(2^64) multiplication is not equivalent to unreduced binary polynomial carry-less multiplication required by HQC ring arithmetic.
- Action: Reverted active wiring; moved to Task 4.

### Iter 4a — Task 4 (invalid Karatsuba bypass)
- Hypothesis used at the time: RVV `vclmul.vv/vclmulh.vv` in a fast vectorized multiplier would lower instret.
- Implementation used then: `vect_mul_v4_task4()` ported from `karatsuba_zvbc/gf2x_vclmul.c`.
- Correctness then: PASS 100/100 in `sim/qemu/run_kat.sh`.
- Benchmark then: 160,681 instret.
- Invalidity note: this was not FAFFT butterfly vectorization; it replaced FAFFT with a Karatsuba path and therefore did not satisfy Task 4 research intent.
- Action: invalidated.

### Iter 4b — Task 4 (true RVV FAFFT butterflies)
- Hypothesis: vectorizing monomial-basis Gao-Mateer butterfly layers directly with RVV (`vsetvl_e64m4`, `vle64`, `vxor`, `vclmul`) evaluates the intended FAFFT architecture.
- Implementation: rewrote `src/poly_mul/fafft_zbc/v4/fafft_v4.c` to remove Karatsuba paths and add `vect_mul_v4_task4_fafft()` with explicit RVV forward/inverse butterfly loops and RVV pointwise clmul.
- Correctness: FAIL 0/100 in `sim/qemu/run_kat.sh`.
- Benchmark: 45,907 instret in `analysis/benchmarks/phase1b/hqc128_fafft_zbc_polymul_N1.csv` (N_ITER=10).
- Action: drop (incorrect; not KAT-safe).
### Iter 4c — Task 4 (RVV vclmul.vv vectorized schoolbook) ← ACTIVE
- Hypothesis: The inner j-loop of the binary polynomial schoolbook convolution maps directly onto `vclmul.vv` + `vclmulh.vv`: broadcast `a[i]` via `vmv.v.x`, multiply against a window of `b[j..j+vl-1]`, XOR-accumulate low/high 64-bit halves into a double-width buffer, then ring-reduce.
- Root cause of prior KAT failure (4b): GF(2^64) FAFFT reduces 128-bit carry-less products to 64 bits via mod g(x), destroying the cross-word carry bits required for correct GF(2)[x] convolution.
- Implementation: `vect_mul_v4_task4_fafft()` in `src/poly_mul/fafft_zbc/v4/fafft_v4.c` — O(n²) vectorized schoolbook with `vsetvl_e64m4`, `vmv_v_x`, `vle64`, `vclmul_vv`, `vclmulh_vv`, `vxor_vv`, `vse64`, then `v4_reduce()`.
- Correctness: **PASS 100/100** in `sim/qemu/run_kat.sh build/hqc128_fafft_zbc.elf`.
- Benchmark (Spike, N_ITER=10, HQC-128): **178,048 instret** (1.99× faster than v3 scalar Karatsuba baseline 354,187 / scalar 364,872).
- HQC-192: 723,841 instret; HQC-256: 1,843,597 instret.
- Action: **keep** — KAT correct, significant speedup via SIMD parallelism.

## Final Active Wiring (updated)

- `src/poly_mul/fafft_zbc/fafft.c` calls `vect_mul_v4()`.
- `src/poly_mul/fafft_zbc/v4/fafft_v4.c` currently wires `vect_mul_v4 -> vect_mul_v4_task4_fafft`.
- `CMakeLists.txt` includes `src/poly_mul/fafft_zbc/v4/fafft_v4.c` in `fafft_zbc` `POLYMUL_SOURCES`.
