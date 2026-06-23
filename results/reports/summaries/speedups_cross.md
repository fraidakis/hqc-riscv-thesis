# HQC-RISC-V Cross-Platform Comparison: Spike ISS vs BPI-F3 Hardware

Compares Spike instruction-retired speedups against real hardware cycle speedups
for polynomial multiplication (`vect_mul`).

- **Spike speedup** = ref_instret / champion_instret (deterministic)
- **HW speedup** = ref_median_cycles / champion_median_cycles (statistical)
- **HW/Spike ratio** = HW_speedup / Spike_speedup
  - ratio > 1 → hardware **amplifies** the Spike-predicted advantage
  - ratio < 1 → hardware **attenuates** it (microarchitectural overhead)

## Scalar Zb (Scalar Tier)

| Level | Spike Instret (ref) | Spike Instret (champion) | Spike Speedup | HW Cycles (ref) | HW Cycles (champion) | HW Speedup | HW/Spike Ratio |
|-------|--------------------:|-------------------------:|--------------:|----------------:|---------------------:|-----------:|---------------:|
| HQC-128 | 18,639,752 | 186,629 | ×99.88 | 11,636,523 | 148,903 | ×78.15 | 0.782 |
| HQC-192 | 56,702,057 | 591,557 | ×95.85 | 35,380,368 | 448,455 | ×78.89 | 0.823 |
| HQC-256 | 139,877,019 | 1,306,068 | ×107.10 | 88,394,244 | 927,364 | ×95.32 | 0.890 |

## Vector RVV (Vector Tier)

| Level | Spike Instret (ref) | Spike Instret (champion) | Spike Speedup | HW Cycles (ref) | HW Cycles (champion) | HW Speedup | HW/Spike Ratio |
|-------|--------------------:|-------------------------:|--------------:|----------------:|---------------------:|-----------:|---------------:|
| HQC-128 | 18,639,752 | 140,487 | ×132.68 | 11,636,523 | 125,921 | ×92.41 | 0.696 |
| HQC-192 | 56,702,057 | 439,516 | ×129.01 | 35,380,368 | 364,036 | ×97.19 | 0.753 |
| HQC-256 | 139,877,019 | 1,014,499 | ×137.88 | 88,394,244 | 847,899 | ×104.25 | 0.756 |

## Interpretation

- A **HW/Spike ratio > 1** indicates that the real hardware amplifies the
  speedup predicted by Spike's instruction-retired model. This can happen when
  the champion variant benefits from microarchitectural features (caches, OoO
  execution, prefetching) more than the baseline.

- A **HW/Spike ratio < 1** indicates that hardware attenuates the predicted
  speedup. This typically occurs when the champion variant incurs
  microarchitectural penalties (cache misses, pipeline stalls, memory bandwidth
  limits) not captured by the instruction-count model.

- A **ratio ≈ 1** suggests the Spike instret model is a good predictor of
  real-world performance for that variant/level combination.
