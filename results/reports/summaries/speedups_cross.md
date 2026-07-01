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
| HQC-128 | 18,504,302 | 182,726 | ×101.27 | 11,635,670 | 122,503 | ×94.98 | 0.938 |
| HQC-192 | 56,292,809 | 570,293 | ×98.71 | 35,382,098 | 367,480 | ×96.28 | 0.975 |
| HQC-256 | 139,222,953 | 1,262,888 | ×110.24 | 88,385,859 | 790,047 | ×111.87 | 1.015 |

## Vector RVV (Vector Tier)

| Level | Spike Instret (ref) | Spike Instret (champion) | Spike Speedup | HW Cycles (ref) | HW Cycles (champion) | HW Speedup | HW/Spike Ratio |
|-------|--------------------:|-------------------------:|--------------:|----------------:|---------------------:|-----------:|---------------:|
| HQC-128 | 18,504,302 | 148,479 | ×124.63 | 11,635,670 | 122,085 | ×95.31 | 0.765 |
| HQC-192 | 56,292,809 | 449,808 | ×125.15 | 35,382,098 | 357,116 | ×99.08 | 0.792 |
| HQC-256 | 139,222,953 | 1,043,726 | ×133.39 | 88,385,859 | 730,657 | ×120.97 | 0.907 |

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
