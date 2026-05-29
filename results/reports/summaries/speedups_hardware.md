# Hardware Speedup Report — BPI-F3

| Parameter | Value |
|-----------|-------|
| Board | Banana Pi BPI-F3 (SpacemiT K1) |
| Core | SpacemiT X60 |
| VLEN | 256 |
| Compiler | GCC (native) |
| Isolation | taskset (single-core pinning) |
| Iterations | 10000 |

Speedup = baseline_median_cycles / champion_median_cycles (higher is better).

## Isolated Polynomial Multiplication (`vect_mul`)

| Level | ref_scalar (cycles) | karatsuba_zbc (cycles) | Speedup | karatsuba_rvv (cycles) | Speedup |
|-------|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | 11,635,114 | 145,580 | ×79.9 | 221,688 | ×52.5 |
| HQC-192 | — | — | — | — | — |
| HQC-256 | — | — | — | — | — |

## Full KEM Operations

| Level | Op | ref_scalar (cycles) | karatsuba_zbc (cycles) | Speedup | karatsuba_rvv (cycles) | Speedup |
|-------|-----|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | keypair | 11,755,276 | 255,499 | ×46.0 | 317,021 | ×37.1 |
| HQC-128 | encaps | 23,488,661 | 507,729 | ×46.3 | 628,510 | ×37.4 |
| HQC-128 | decaps | 35,749,883 | 1,278,607 | ×28.0 | 1,387,386 | ×25.8 |
| HQC-128 | **Total** | **70,993,820** | **2,041,835** | **×34.8** | **2,332,917** | **×30.4** |
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

- ref_scalar (polymul): missing levels [192, 256]
- karatsuba_zbc (polymul): missing levels [192, 256]
- karatsuba_rvv (polymul): missing levels [192, 256]
- ref_scalar (KEM): missing levels [192, 256]
- karatsuba_zbc (KEM): missing levels [192, 256]
- karatsuba_rvv (KEM): missing levels [192, 256]

