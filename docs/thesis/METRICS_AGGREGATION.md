# Final Metrics Aggregation

This document records the thesis-facing HQC-128 instruction-count table after the final architecture and sterility sweep.

## Master Table (HQC-128)

| Tier | ISA profile | Baseline variant | Champion variant | `vect_mul` instret (baseline) | `vect_mul` instret (champion) | `decaps` instret (baseline) | `decaps` instret (champion) |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: |
| Tier 1: Ultra-Constrained IoT | `rv64gc_zbc` (strict scalar) | `ref_scalar` | `karatsuba_zbc` | 19,213,918 | 211,841 | 59,252,696 | 2,099,075 |
| Tier 2: Mid-Range Edge | `rv64gcv_zbc` (base vector) | `ref_vector` | `karatsuba_rvv` (AP-2) | 18,535,529 | 157,377 | 56,669,986 | 1,370,788 |
| Tier 3: High-End Silicon | `rv64gcv_zbc_zvbc` (vector crypto) | `ref_vector` | `karatsuba_zvbc` (AP-2) | 18,535,529 | 72,644 | 56,669,986 | 901,150 |

## Measurement Sources

- `ref_scalar`: `analysis/benchmarks/isolated/scalar/ref_scalar/polymul_latest.csv`, `analysis/benchmarks/isolated/scalar/ref_scalar/kem_latest.csv`
- `ref_vector`: `analysis/benchmarks/isolated/scalar/ref_vector/polymul_latest.csv`, `analysis/benchmarks/isolated/scalar/ref_vector/kem_latest.csv`
- `karatsuba_zbc`: `analysis/benchmarks/isolated/scalar/karatsuba_zbc/polymul_latest.csv`, `analysis/benchmarks/isolated/scalar/karatsuba_zbc/kem_latest.csv`
- `karatsuba_rvv`: `analysis/benchmarks/isolated/vector/karatsuba_rvv/polymul_latest.csv`, `analysis/benchmarks/isolated/vector/karatsuba_rvv/kem_latest.csv`
- `karatsuba_zvbc`: `analysis/benchmarks/isolated/vector/karatsuba_zvbc/polymul_latest.csv`, `analysis/benchmarks/isolated/vector/karatsuba_zvbc/kem_latest.csv`

## AP-2 Threshold Check

- `karatsuba_rvv` AP-2 `vect_mul` at HQC-128: **157,377** instret.
- Target threshold: **158,928** instret.
- Result: **PASS** (improvement of 1,551 instret versus the threshold).