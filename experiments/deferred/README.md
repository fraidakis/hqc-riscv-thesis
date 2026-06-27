# Deferred Two-Phase Karatsuba (Rejected Experiment)

**Status:** Rejected and removed from the build system (2026-06-01).

## Concept

A two-phase approach to Karatsuba polynomial multiplication that cleanly
separates scalar and vector execution:

1. **Phase 1 (scalar only):** Run the entire Karatsuba recursion tree doing
   only the leaf `clmul` operations, writing all intermediate products (z0, z2,
   zmid) into a large pre-allocated flat buffer (~87 KB for HQC-128). No vector
   instructions at all.

2. **Phase 2 (vector only):** A single RVV pass iterates a static descriptor
   table (121 entries, bottom-up) and does all the XOR assembly/recomposition
   using `rvv_copy_u64` and `rvv_xor3_u64`. Zero scalar `clmul` in this phase.

The goal was to eliminate all scalar↔vector transitions by separating the two
ISA domains into non-overlapping phases.

## Results — Hardware (BPI-F3, SpacemiT X60, HQC-128)

### Full KEM (median cycles)

| Operation | vector_rvv | vector_rvv_deferred | Delta |
|-----------|----------:|--------------------:|------:|
| keypair   |   302,181 |             356,912 | +18.1% |
| encaps    |   600,692 |             705,525 | +17.5% |
| decaps    |  1,147,999|           1,498,574 | +30.5% |
| **total** |**2,050,872**|       **2,561,011**| **+24.9%** |

### Isolated poly_mul (median)

| Metric          | vector_rvv | vector_rvv_deferred | Delta |
|-----------------|----------:|--------------------:|------:|
| Cycles          |   210,287 |             266,123 | +26.6% |
| L1D miss rate   |     0.10% |               0.94% | 9.4× worse |
| L1D misses      |        83 |                 863 | 10.4× |
| Frontend stalls |     1,148 |              11,897 | 10.4× |
| VPU stalls      |    25,438 |              49,804 | +95.8% |

## Root Cause

Three microarchitectural effects make it **25% slower**:

1. **L1D cache thrashing** — The flat buffer (87 KB) far exceeds the X60's
   32 KB L1D. The temporal gap between Phase 1 writes and Phase 2 reads
   guarantees cache eviction.

2. **VPU pipeline stalls** — Phase 2 reads non-sequential buffer locations,
   stalling on cache-line fills. VPU stalls nearly doubled (+96%).

3. **I-cache pressure** — The fully unrolled scalar phase = 149 KB `.text`
   (3.6× larger), causing 10× more frontend stalls.

## Architectural Lesson

The scalar↔vector transition penalty (~28 cycles × 363 transitions ≈ 10,164
cycles) represents only ~4.8% of execution time. Trading it for 87 KB memory
pressure + 3.6× code bloat is a net loss on the X60's memory hierarchy.

The interleaved approach (current `--mode rvv`) keeps the working set small
(each recursion level's buffers fit in L1D) and benefits from immediate reuse
of freshly-written products.

## Viability conditions

Would only be viable on a microarchitecture with:
- Very large L1D (≥128 KB), or
- Extremely expensive scalar↔vector transitions (≥100 cycles), or
- Hardware prefetch capable of hiding the Phase 1→Phase 2 latency gap

## Files

- `gen_karatsuba_deferred.py` — Standalone generator (extracted from
  `scripts/gen/gen_karatsuba_specialized.py`)
- `README.md` — This file

## Usage (for reference only)

```bash
python3 experiments/deferred/gen_karatsuba_deferred.py --threshold 16
```

Generates `karatsuba_rvv_deferred_specialized_gen.h` in the current directory.
