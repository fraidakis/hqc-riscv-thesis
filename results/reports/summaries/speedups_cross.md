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
| HQC-128 | 19,214,277 | 221,030 | ×86.93 | 11,664,579 | 148,489 | ×78.56 | 0.904 |
| HQC-192 | 58,433,896 | 670,293 | ×87.18 | — | — | — | — |
| HQC-256 | 142,641,043 | 1,879,602 | ×75.89 | — | — | — | — |

## Vector RVV (Vector Tier)

| Level | Spike Instret (ref) | Spike Instret (champion) | Spike Speedup | HW Cycles (ref) | HW Cycles (champion) | HW Speedup | HW/Spike Ratio |
|-------|--------------------:|-------------------------:|--------------:|----------------:|---------------------:|-----------:|---------------:|
| HQC-128 | 19,214,277 | 157,473 | ×122.02 | 11,664,579 | 211,846 | ×55.06 | 0.451 |
| HQC-192 | 58,433,896 | 473,083 | ×123.52 | — | — | — | — |
| HQC-256 | 142,641,043 | 1,502,171 | ×94.96 | — | — | — | — |

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
