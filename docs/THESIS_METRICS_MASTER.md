# Thesis Metrics Master

Security level: HQC-128

Speedup definition: `Baseline / Champion`.

## Table 1: The Scalar Optimization Path (IoT Tier)

| Stage | Variant | `vect_mul` instret | Speedup vs previous (`vect_mul`) | `decaps` instret | Speedup vs previous (`decaps`) |
|---|---|---:|---:|---:|---:|
| Level 1 (Pure C) | `ref_pure` | 19,213,918 | — | 59,252,696 | — |
| Level 2 (Hardware Assisted) | `ref_scalar` | 19,214,281 | x1.00 | 59,051,432 | x1.00 |
| Level 3 (Algorithmic Champion) | `karatsuba_zbc` | 211,841 | x90.70 | 2,099,075 | x28.13 |

## Table 4: The Vector Optimization Path (Edge/Banana Pi Tier)

| Stage | Variant | `vect_mul` instret | Speedup vs previous (`vect_mul`) | `decaps` instret | Speedup vs previous (`decaps`) |
|---|---|---:|---:|---:|---:|
| Level 1 (Compiler Auto-Vectorized) | `ref_vector` | 18,527,412 | — | 56,442,467 | — |
| Level 2 (Hybrid Champion) | `karatsuba_rvv` | 157,377 | x117.73 | 1,186,497 | x47.57 |

## Table 5: The Vector Crypto Path (High-End Tier)

| Stage | Variant | `vect_mul` instret | Speedup vs previous (`vect_mul`) | `decaps` instret | Speedup vs previous (`decaps`) |
|---|---|---:|---:|---:|---:|
| Level 1 (Compiler Barrier) | `ref_vector` | 18,527,412 | — | 56,442,467 | — |
| Level 2 (Full Hardware Champion) | `karatsuba_zvbc` | 72,644 | x255.04 | 901,150 | x62.63 |
