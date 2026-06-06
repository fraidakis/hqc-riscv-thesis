# Vector RVV Polynomial Multiplication — Performance Analysis

## Executive Summary

The `vector_rvv` variant is **41% slower** than `scalar_zb` despite executing **22% fewer instructions**. The root cause is a **scalar store → vector load forwarding failure** on the SpacemiT X60 microarchitecture, combined with frequent scalar↔vector pipeline transitions throughout the Karatsuba recursion tree.

## Benchmark Data (HQC-128, BPI-F3, 100 iterations)

| Metric | scalar_zb | vector_rvv | Delta |
|--------|-----------|------------|-------|
| Median cycles | 149,555 | 211,241 | +41.2% |
| Instructions (instret) | 196,096 | 152,770 | −22.1% |
| IPC | 1.31 | 0.72 | −45% |
| Backend stalls | 16,452 | **109,528** | +565% |
| VPU stalls | 0 | 25,842 | — |
| Vector instructions | 0 | 45,178 | — |
| L1D miss rate | 0.21% | 0.11% | better |
| Frontend stalls | 2,708 | 1,207 | better |

## Root Cause Analysis

### 1. Scalar Store → Vector Load Forwarding Failure (Primary Bottleneck)

**Measured on SpacemiT X60:**
- Vector load from L1 cache (cold): **9.4 ns**
- Vector load after vector store (same address): **16.1 ns** (forwarding works)
- Vector load after scalar store (same address): **27.0 ns** (forwarding FAILS)
- 16 scalar stores then vector load: **43.8 ns**

**Penalty per occurrence: ~17.6 ns (28 cycles at 1.6 GHz)**

The SpacemiT X60 has a **decoupled scalar/vector pipeline**. The scalar store buffer cannot forward data to the vector load unit. When a vector load hits an address that has pending scalar stores, the pipeline must drain the store buffer to L1 cache before the vector load can proceed.

**How this manifests in vector_rvv:**

The "direct-write discipline" in the RVV variant writes `z0` and `z2` directly to the output buffer `r[]` using `clmul_leaf` (scalar `sd` stores). Then the Loop 3 assembly phase reads from `r[]` using vector loads (`vle64`). This creates the forwarding failure at every recursion level.

### 2. Scalar↔Vector Pipeline Transition Cost

**Measured:**
- Pure scalar clmul pair: **5.0 ns**
- Scalar clmul then immediately vector: **20.7 ns** (transition penalty: ~6.3 ns / 10 cycles)

Each `kmul_N` function performs 3 transitions:
1. Scalar `clmul_leaf` → Vector ta/tb XOR preparation
2. Vector ta/tb write → Scalar `clmul_leaf` for zmid (reads vector-stored ta/tb)
3. Scalar `clmul_leaf` for zmid → Vector Loop 3

With 121 inner nodes in the recursion tree for HQC-128 (n=277), that's **363 transitions × 10 cycles = 3,630 cycles** of pure transition overhead.

### 3. VPU Stall Accumulation

The 25,842 VPU stall cycles come from:
- Vector load latency when reading recently-scalar-stored data
- Vector instruction chaining inefficiency at small element counts (8–18 elements per loop iteration at the bottom recursion levels)
- With VLEN=256 and LMUL=m2, each loop iteration processes only 8 elements — the loop executes 1–2 iterations for the smallest recursion levels

### 4. Recursion Tree Amplification

For HQC-128 (n=277, threshold=16):
```
Function call counts:
  kmul_277:  1
  kmul_139:  3
  kmul_70:   6
  kmul_69:   3
  kmul_35:   24
  kmul_34:   3
  kmul_18:   24
  kmul_17:   57
  Total inner nodes: 121
```

The 81 calls at the bottom two levels (kmul_17 + kmul_18) each process only 9–18 elements in their vector loops. At these sizes, the transition and forwarding penalties dominate the actual computation time.

## Why scalar_zb Doesn't Have This Problem

The scalar_zb variant uses a **separate tmp buffer** approach:
1. `clmul_leaf` writes z0, z2, zmid to separate regions in `tmp[]` (scalar stores)
2. Loop 1 copies `tmp[z0]` → `r[]` (scalar load from tmp, scalar store to r — forwarding works)
3. Loop 2 copies `tmp[z2]` → `r[]` (same)
4. Loop 3 reads from `tmp[zmid]`, `tmp[z0]`, `tmp[z2]` (all scalar loads — forwarding works)

**No scalar→vector transitions ever occur.** All data flows through the scalar pipeline exclusively, achieving IPC of 1.44 with perfect store-to-load forwarding.

## Optimization Strategies to Make vector_rvv Faster

### Strategy A: Hybrid Recursion Depth Threshold (Recommended)

Use **scalar assembly loops at the bottom 2 recursion levels** (kmul_17, kmul_18) and **vector assembly loops only at higher levels** (kmul_35+).

**Rationale:** At N≥35, the vector XOR3 loop processes 35+ elements, amortizing the transition cost. At N=9–18, the overhead dominates.

**Expected improvement:** Eliminates ~81 × 3 = 243 transitions at the bottom levels. Estimated savings: ~2,400 cycles (from transitions) + reduced store forwarding failures.

**Implementation:** Generate two versions of the specialized headers:
- `karatsuba_rvv_specialized_gen.h` uses scalar loops for kmul_17/kmul_18
- Uses RVV loops for kmul_34+ where the data is large enough

### Strategy B: Eliminate Direct-Write at Bottom Levels

At kmul_17/kmul_18, revert to the scalar_zb pattern (write z0/z2 to tmp, then copy). This avoids the scalar store → vector load forwarding failure entirely at the most-called levels.

**Trade-off:** Increases instruction count slightly but eliminates the 28-cycle forwarding penalty per occurrence.

### Strategy C: Use `vl.whole` Register Loads (Advanced)

If the SpacemiT X60 supports whole-register loads that bypass the store buffer check, use `vl1re64.v` / `vl2re64.v` for the Loop 3 reads. These may have different forwarding behavior.

### Strategy D: Vectorize the Leaf Multiply (Requires Zvbc)

The fundamental limitation is that `clmul`/`clmulh` are scalar instructions. If Zvbc (`vclmul`/`vclmulh`) were available, the entire computation could stay in the vector pipeline, eliminating all transitions. **Not available on BPI-F3 (SpacemiT X60).**

### Strategy E: Increase Karatsuba Threshold for RVV Variant

Increase the threshold from 16 to 32 or 64 for the RVV variant. This reduces the number of recursion levels and inner nodes, reducing the total number of transitions. Trade-off: more clmul operations at the leaf level.

### Strategy F: Batch Vector Operations (Deferred Assembly)

Instead of interleaving scalar clmul_leaf calls with vector assembly at each recursion level, defer all vector assembly work to a post-processing pass after all scalar leaf multiplications complete. This creates a single scalar→vector transition for the entire computation.

**Implementation complexity:** High — requires restructuring the recursion to separate computation phases.

## Micro-benchmark Validation

### Pure XOR3 Performance (no scalar→vector transition)

| N | scalar_4way | rvv_m2 | rvv_m4 | Speedup |
|---|-------------|--------|--------|---------|
| 9 | 62.5 ns | 39.0 ns | 34.4 ns | 1.8× |
| 14 | 95.5 ns | 38.8 ns | 34.0 ns | 2.8× |
| 17 | 93.5 ns | 57.0 ns | 69.7 ns | 1.6× |
| 35 | 157.3 ns | 98.2 ns | 107.0 ns | 1.6× |
| 70 | 318.7 ns | 177.9 ns | 182.2 ns | 1.8× |
| 139 | 572.3 ns | 359.4 ns | 333.7 ns | 1.7× |

**Key insight:** When there's no scalar→vector transition, RVV is 1.6–2.8× faster than scalar for the XOR3 pattern at ALL sizes. The vector hardware is not the bottleneck — the pipeline transitions are.

### XOR3 After Scalar Stores (with forwarding penalty)

| N | scalar_4way | rvv_m2 | rvv_m4 | Winner |
|---|-------------|--------|--------|--------|
| 9 | 76 ns | 74 ns | 71 ns | RVV (barely) |
| 17 | 132 ns | 116 ns | 130 ns | RVV m2 |
| 35 | 243 ns | 205 ns | 215 ns | RVV m2 |
| 70 | 511 ns | 391 ns | 397 ns | RVV m2 |
| 139 | 968 ns | 775 ns | 750 ns | RVV m4 |

Even with the forwarding penalty, RVV is still faster for the XOR3 loop itself. The total slowdown comes from the **cumulative effect** of 121 inner nodes each paying the transition + forwarding penalty, plus the VPU stall cycles from the vector load latency.

## Recommended Next Steps

1. **Implement Strategy A** (hybrid threshold): Generate scalar assembly for kmul_17/kmul_18, vector for kmul_34+. This is the lowest-risk, highest-impact change.

2. **Measure with Strategy A**: Expected to reduce backend stalls by ~40-50%, bringing vector_rvv close to or below scalar_zb cycles.

3. **If still slower**: Combine with Strategy B (separate tmp buffers at bottom levels) to eliminate forwarding failures entirely at the most-called functions.

4. **Long-term**: Strategy F (deferred assembly) could achieve the theoretical 1.6–2.8× speedup shown in the pure-vector benchmarks, but requires significant restructuring.

## Hardware Context

- **Processor:** SpacemiT X60 (Spacemit K1 SoC)
- **ISA:** rv64imafdcv_zicbom_zicboz_zicntr_zicond_zba_zbb_zbc_zbs_zkt_zve64d_zvfh_zvkt
- **VLEN:** 256 bits (4 × uint64 per m1, 8 per m2, 16 per m4, 32 per m8)
- **Clock:** 1.6 GHz
- **No Zvbc/Zvbb** — cannot vectorize carry-less multiply
- **Decoupled scalar/vector pipeline** — scalar store buffer does NOT forward to vector loads
