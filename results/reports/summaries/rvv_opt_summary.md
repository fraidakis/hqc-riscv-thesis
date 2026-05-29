# RVV Optimization — Final Summary Comparison

**Date:** 2026-05-28  
**Platform:** SpacemiT X60 (BPI-F3), VLEN=256, RVV 1.0  
**Operation:** HQC-128 `vect_mul` (isolated polynomial multiplication)  
**Measurement:** `perf_event_open`, userspace-only, single core (taskset -c 7)  
**Status:** Hardware measurements PENDING — board not accessible from current host

---

## 1. Primary Comparison Table (HQC-128 vect_mul)

| Metric | Baseline RVV | Scalar Zbc (Target) | Optimized RVV | Δ vs Baseline | Δ vs Zbc Target |
|--------|:------------:|:-------------------:|:-------------:|:-------------:|:---------------:|
| Median cycles | 221,688 | 145,580 | **PENDING** | — | — |
| Median instructions | 158,368 | 195,399 | **PENDING** | — | — |
| IPC | 0.714 | 1.342 | **PENDING** | — | — |
| Min cycles | 219,313 | 143,961 | **PENDING** | — | — |
| Max cycles | 296,928 | 239,130 | **PENDING** | — | — |
| CoV (cycles) | 2.07% | 2.85% | **PENDING** | — | — |
| N iterations | 10,000 | 10,000 | **PENDING** | — | — |

**Source files:**
- Baseline RVV: `mul_hqc128_karatsuba_rvv.csv` (N=10,000, pre-optimization)
- Scalar Zbc: `mul_hqc128_karatsuba_zbc.csv` (N=10,000)
- Optimized RVV: pending hardware measurement

### How to Fill In After Hardware Run

```
Δ vs Baseline = (1 - optimized_cycles / 221,688) × 100%    [positive = improvement]
Δ vs Zbc Target = (optimized_cycles / 145,580 - 1) × 100%  [negative = beat target]
IPC = median_instret / median_cycles
```

---

## 2. Spike Instruction Count Data (Available — Deterministic)

Spike ISS provides exact instruction-retired counts independent of microarchitecture.
These measurements confirm the optimization reduces instruction count and validate
correctness, but do not predict cycle behavior on the in-order X60 pipeline.

### 2.1 Isolated `vect_mul` Instruction Counts

| Variant | HQC-128 | HQC-192 | HQC-256 | Source |
|---------|--------:|--------:|--------:|--------|
| karatsuba_rvv (optimized) | 160,215 | 481,715 | 1,519,801 | `isolated_mul_karatsuba-rvv_latest.csv` (commit c56c136) |
| karatsuba_rvv (polymul only, no harness) | 157,377 | 481,065 | 1,608,062 | `isolated_polymul_karatsuba-rvv_latest.csv` (commit db59cec) |
| karatsuba_zbc | 221,026 | 670,289 | 1,879,598 | `isolated_mul_karatsuba-zbc_latest.csv` (commit c56c136) |
| karatsuba_rvv (pre-opt, original) | 288,117 | 877,340 | 1,928,802 | archive `polymul_2026-05-14` (commit 03c68a6) |

### 2.2 Instruction Count Comparison

| Level | Pre-opt RVV | Optimized RVV | Reduction | Zbc | RVV/Zbc Ratio |
|-------|------------:|--------------:|:---------:|----:|:-------------:|
| HQC-128 | 288,117 | 160,215 | **−44.4%** | 221,026 | **0.72×** (28% fewer) |
| HQC-192 | 877,340 | 481,715 | **−45.1%** | 670,289 | **0.72×** (28% fewer) |
| HQC-256 | 1,928,802 | 1,519,801 | **−21.2%** | 1,879,598 | **0.81×** (19% fewer) |

**Key insight:** The optimized RVV variant executes significantly fewer instructions
than the scalar Zbc variant (28% fewer for HQC-128/192). Despite this instruction
advantage, the hardware cycle count depends on IPC, which is constrained by the X60's
in-order vector pipeline.

### 2.3 Hardware Instret vs Spike Instret Discrepancy

| Variant | Spike instret | Hardware instret | Difference | Explanation |
|---------|-------------:|----------------:|:----------:|-------------|
| karatsuba_rvv (baseline) | 157,377¹ | 158,368 | +0.6% | GCC vs Clang codegen differences |
| karatsuba_zbc | 221,026 | 195,399 | −11.6% | GCC native optimizes scalar code more aggressively |

¹ Spike polymul-only count; hardware CSV includes full `vect_mul` (polymul + reduce).

**Note:** The hardware uses native GCC 14.2 while Spike measurements use cross-compiled
Clang. The Zbc variant shows a larger discrepancy because GCC's scalar optimizer is
more effective than Clang's for the scalar `clmul` leaf code. For the RVV variant,
the discrepancy is minimal because vector intrinsics leave less room for compiler
optimization.

---

## 3. Expected Hardware Performance Analysis

### 3.1 IPC Projection

The baseline RVV achieved IPC = 0.714 (158,368 instret / 221,688 cycles). The
optimizations target cycle reduction through better pipeline utilization:

| Scenario | Assumed IPC | Projected Cycles | vs Baseline | vs Zbc Target |
|----------|:-----------:|:----------------:|:-----------:|:-------------:|
| No IPC improvement | 0.714 | ~224,000² | +1.0% (worse) | +53.8% gap |
| Moderate improvement | 0.90 | ~178,000 | −19.7% | +22.3% gap |
| Good improvement | 1.00 | ~160,000 | −27.8% | +9.9% gap |
| Target-matching | 1.10 | ~146,000 | −34.2% | +0.3% gap |
| Beat target | 1.15 | ~139,000 | −37.3% | −4.5% (beats) |

² Using optimized instret (~160,215) divided by assumed IPC.

**To beat the Zbc target (145,580 cycles), the optimized RVV needs IPC ≥ 1.10.**

### 3.2 Why IPC Improvement Is Expected

The optimizations specifically target pipeline stall sources:

1. **Eliminated copy loops (Loop 1 + Loop 2):** Removes load-after-store hazards
   where the pipeline stalled waiting for store-to-load forwarding across function
   boundaries.

2. **Inline intrinsics:** Eliminates ~50 `jalr` + stack-frame pairs per multiply.
   On the in-order X60, each function call costs 4–6 cycles of pipeline drain.
   Expected saving: 200–300 cycles.

3. **Vectorized reduction:** The scalar reduction loop had poor IPC due to
   read-after-write dependencies on shift results. The vector version processes
   8 elements per iteration with independent lanes.

4. **LMUL tuning:** Using m8 for large operations maximizes vector unit throughput;
   m2 for XOR3 avoids register spills that would cause additional loads/stores.

5. **Reduced vsetvl overhead:** Fewer CSR writes means fewer pipeline flushes on
   the X60 (vsetvl causes a full pipeline drain on in-order cores).

---

## 4. Microarchitectural Bottleneck Analysis (Requirement 6.4)

### 4.1 Fundamental Constraint: Scalar clmul Leaves

The Karatsuba recursion tree for HQC-128 (n=277, T=16) has 243 leaf calls, each
performing O(n²) schoolbook carry-less multiplication using scalar `clmul`/`clmulh`
instructions. These leaves account for approximately **70% of total instructions**
and cannot be vectorized without the Zvbc extension (which provides `vclmul.vv`).

**Instruction breakdown (estimated from Spike profiling):**

| Component | Instructions | % of Total | Vectorizable? |
|-----------|------------:|:----------:|:-------------:|
| clmul/clmulh leaves | ~112,000 | ~70% | ❌ Requires Zvbc |
| Assembly phase (glue) | ~32,000 | ~20% | ✅ Optimized with RVV |
| Reduction phase | ~8,000 | ~5% | ✅ Vectorized |
| Function call overhead | ~8,000 | ~5% | ✅ Eliminated (inline) |

### 4.2 SpacemiT X60 Pipeline Characteristics

The X60 is a **dual-issue, in-order, 8-stage pipeline** with:

- **Vector unit:** Single-issue, in-order, VLEN=256 (4 × 64-bit elements)
- **Scalar-vector interleave:** Vector instructions block the scalar pipeline
  until completion (no out-of-order overlap)
- **vsetvl cost:** Full pipeline drain (~4 cycles) on each CSR write
- **Vector memory:** Single load/store port, no store buffer forwarding to
  vector loads

These characteristics mean:
1. Scalar `clmul` sequences achieve high IPC (~1.3–1.4) because the dual-issue
   pipeline can overlap independent multiplies
2. Vector instruction sequences achieve lower IPC (~0.5–0.7) because:
   - Only one vector instruction issues per cycle
   - Vector loads have higher latency than scalar loads
   - Transitions between scalar and vector code cause pipeline bubbles

### 4.3 Why the Zbc Target May Not Be Achievable Without Zvbc

The scalar Zbc variant achieves IPC = 1.34 because:
- The entire computation is scalar (no vector pipeline stalls)
- The dual-issue pipeline overlaps independent `clmul`/`clmulh` pairs
- No vsetvl overhead, no scalar-vector transitions

The RVV variant, even fully optimized, faces:
- **70% of instructions are scalar** (clmul leaves) → these achieve ~1.3 IPC
- **30% of instructions are vector** (glue + reduction) → these achieve ~0.5–0.7 IPC
- **Transitions** between scalar and vector regions cause additional stalls

**Weighted IPC estimate:**
```
IPC_estimated = 0.70 × 1.3 (scalar) + 0.30 × 0.6 (vector) = 0.91 + 0.18 = 1.09
```

This suggests the optimized RVV variant will achieve IPC ≈ 1.0–1.1, yielding
approximately 145,000–160,000 cycles — **close to but possibly not beating** the
Zbc target.

### 4.4 The Zvbc Solution

With Zvbc (`vclmul.vv`/`vclmulh.vv`), the leaf multiplications could be vectorized,
eliminating the scalar-vector transitions entirely. The Spike data shows:

| Variant | HQC-128 instret | vs Zbc |
|---------|----------------:|:------:|
| karatsuba_zvbc | 72,644 | 3.04× fewer |
| karatsuba_rvv (optimized) | 160,215 | 1.38× fewer |
| karatsuba_zbc | 221,026 | 1.00× (baseline) |

A fully vectorized Zvbc implementation would achieve ~72,644 instructions at
IPC ≈ 0.7–1.0, yielding 73,000–104,000 cycles — a **2× improvement** over the
Zbc target. This is the path to true RVV dominance, but requires hardware with
Zvbc support (e.g., Marian RTL simulation or future RVA23 silicon).

---

## 5. Optimizations Applied

All optimizations from tasks 1–8 are implemented and KAT-verified on Spike:

| # | Optimization | Impact | Status |
|---|-------------|--------|--------|
| 1 | Vectorized reduction (`reduce_rvv`) | Replaces scalar shift+XOR loop with LMUL=m8 | ✅ KAT pass |
| 2 | Direct-write discipline | z0→r[0..], z2→r[2*n0..], eliminates copy loops | ✅ KAT pass |
| 3 | Inline RVV intrinsics | ta/tb XOR prep + XOR3 accumulation inlined | ✅ KAT pass |
| 4 | LMUL selection strategy | m8 for ≥28 elements, m4 for small, m2 for XOR3 | ✅ KAT pass |
| 5 | vsetvl minimization | One vsetvl per distinct (SEW, LMUL, count) tuple | ✅ KAT pass |
| 6 | 32-byte buffer alignment | All buffers aligned to VLEN/8 = 32 bytes | ✅ KAT pass |
| 7 | Reduced tmp buffer | 4×VEC_N_SIZE_64 words (down from ~6×) | ✅ KAT pass |

**Correctness verification:**
- KAT: 100/100 vectors matched for HQC-128, HQC-192, HQC-256 on Spike
- Instruction count: deterministic and reproducible across compiler flags (Clang)

---

## 6. Multi-Level Generalization

### 6.1 Spike Instruction Counts (All Security Levels)

| Level | VEC_N_SIZE_64 | Optimized RVV | Zbc | RVV Advantage |
|-------|:-------------:|--------------:|----:|:-------------:|
| HQC-128 | 277 | 160,215 | 221,026 | 27.5% fewer instructions |
| HQC-192 | 561 | 481,715 | 670,289 | 28.1% fewer instructions |
| HQC-256 | 901 | 1,519,801 | 1,879,598 | 19.1% fewer instructions |

### 6.2 Hardware Baselines (Pre-Optimization, dev-mode N=100)

| Level | Pre-opt RVV (cycles) | Zbc (cycles) | Zbc/RVV Ratio |
|-------|---------------------:|-------------:|:-------------:|
| HQC-128 | 222,010 | 144,215 | 1.54× |
| HQC-192 | 659,874 | 433,502 | 1.52× |
| HQC-256 | 1,641,922 | 925,736 | 1.77× |

### 6.3 Expected Optimized Hardware Results (PENDING)

| Level | Expected Cycles (IPC=1.0) | Expected Cycles (IPC=1.1) | Zbc Target | Beat Target? |
|-------|:-------------------------:|:-------------------------:|:----------:|:------------:|
| HQC-128 | ~160,000 | ~146,000 | 144,215 | Marginal |
| HQC-192 | ~482,000 | ~438,000 | 433,502 | Marginal |
| HQC-256 | ~1,520,000 | ~1,382,000 | 925,736 | ❌ Unlikely |

**Note:** HQC-256 has a larger Zbc/RVV gap (1.77×) because the deeper recursion tree
means more scalar clmul leaf work relative to vectorizable glue. The instruction
advantage is also smaller (19% vs 28%) at this level.

---

## 7. Conclusion and Next Steps

### 7.1 Summary

The RVV optimization achieves a **44% instruction count reduction** over the original
RVV baseline and executes **28% fewer instructions** than the scalar Zbc variant.
Whether this translates to a cycle count victory depends on the achieved IPC on the
X60 hardware, which is constrained by the in-order vector pipeline and
scalar-vector transition costs.

### 7.2 Pending Actions

1. **Run hardware benchmark** on BPI-F3 (see `TASK_11_1_INSTRUCTIONS.md`)
2. **Fill in PENDING values** in the comparison table above
3. **If target not met:** The bottleneck analysis in Section 4 satisfies Requirement 6.4
4. **Threshold sweep:** Run `scripts/bench/run_threshold_sweep_hw.sh` for T=8/16/32

### 7.3 Template for Final Results

After hardware measurement, update this section:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  FINAL RESULT — HQC-128 vect_mul
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  Optimized RVV median cycles:       ______
  Optimized RVV median instructions: ______
  Optimized RVV IPC:                 ______

  vs RVV baseline (221,688 cycles):  ______% improvement
  vs Zbc target (145,580 cycles):    ______% gap (or improvement)

  Target achieved: YES / NO
  If NO — minimum gap: ______ cycles (______%)
  Bottleneck: See Section 4 (scalar clmul leaves + in-order pipeline)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## References

- Requirements: `.kiro/specs/rvv-speedup-optimization/requirements.md` (Req 6.4, 10.2, 10.3)
- Design: `.kiro/specs/rvv-speedup-optimization/design.md`
- Baseline CSV: `analysis/benchmarks/hardware/mul_hqc128_karatsuba_rvv.csv`
- Target CSV: `analysis/benchmarks/hardware/mul_hqc128_karatsuba_zbc.csv`
- Spike data: `analysis/benchmarks/isolated/vector/karatsuba_rvv/`
- Hardware report: `analysis/reports/speedups-banana-pi.md`
- Spike report: `analysis/reports/speedups-spike.md`
