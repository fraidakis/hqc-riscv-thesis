# Hardware Speedup Report — BPI-F3

| Parameter | Value |
|-----------|-------|
| Board | Banana Pi BPI-F3 (SpacemiT K1) |
| Core | SpacemiT X60 |
| VLEN | 256 |
| Compiler | GCC (native) |
| Isolation | taskset (single-core pinning) |
| Iterations | — |

Speedup = baseline_median_cycles / champion_median_cycles (higher is better).

## Isolated Polynomial Multiplication (`vect_mul`)

| Level | scalar_ref (cycles) | karatsuba_zbc (cycles) | Speedup | karatsuba_rvv (cycles) | Speedup |
|-------|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | — | — | — | — | — |
| HQC-192 | — | — | — | — | — |
| HQC-256 | — | — | — | — | — |

## Full KEM Operations

| Level | Op | scalar_ref (cycles) | karatsuba_zbc (cycles) | Speedup | karatsuba_rvv (cycles) | Speedup |
|-------|-----|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | keypair | — | — | — | — | — |
| HQC-128 | encaps | — | — | — | — | — |
| HQC-128 | decaps | — | — | — | — | — |
| HQC-128 | **Total** | **—** | **—** | **—** | **—** | **—** |
| HQC-192 | keypair | — | — | — | — | — |
| HQC-192 | encaps | — | — | — | — | — |
| HQC-192 | decaps | — | — | — | — | — |
| HQC-192 | **Total** | **—** | **—** | **—** | **—** | **—** |
| HQC-256 | keypair | — | — | — | — | — |
| HQC-256 | encaps | — | — | — | — | — |
| HQC-256 | decaps | — | — | — | — | — |
| HQC-256 | **Total** | **—** | **—** | **—** | **—** | **—** |

## Notes

⚠ **Missing data** — the following `_latest.csv` files were not found; corresponding comparisons are skipped (shown as —):

- ref_scalar (polymul): no data found
- karatsuba_zbc (polymul): no data found
- karatsuba_rvv (polymul): no data found
- ref_scalar (KEM): no data found
- karatsuba_zbc (KEM): no data found
- karatsuba_rvv (KEM): no data found

