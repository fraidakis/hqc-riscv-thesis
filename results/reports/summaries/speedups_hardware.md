# Hardware Speedup Report — BPI-F3

| Parameter | Value |
|-----------|-------|
| Board | Banana Pi BPI-F3 (SpacemiT K1) |
| Core | SpacemiT X60 |
| VLEN | 256 |
| Compiler | GCC (native) |
| Isolation | taskset (single-core pinning) |
| Iterations | 10000 (+100 warm-up) |

Speedup = baseline_median_cycles / champion_median_cycles (higher is better).

## Isolated Polynomial Multiplication (`vect_mul`)

| Level | scalar_ref (cycles) | scalar_zb (cycles) | Speedup | vector_rvv (cycles) | Speedup |
|-------|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | 11,664,579 | 148,489 | ×78.6 | 211,846 | ×55.1 |
| HQC-192 | — | — | — | — | — |
| HQC-256 | — | — | — | — | — |

## Full KEM Operations

| Level | Op | scalar_ref (cycles) | scalar_zb (cycles) | Speedup | vector_rvv (cycles) | Speedup |
|-------|-----|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | keypair | 11,756,872 | 262,100 | ×44.9 | 302,181 | ×38.9 |
| HQC-128 | encaps | 23,495,960 | 519,221 | ×45.3 | 600,692 | ×39.1 |
| HQC-128 | decaps | 35,753,541 | 1,286,829 | ×27.8 | 1,147,999 | ×31.1 |
| HQC-128 | **Total** | **71,006,373** | **2,068,150** | **×34.3** | **2,050,872** | **×34.6** |
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

- scalar_ref (polymul): missing levels [192, 256]
- scalar_zb (polymul): missing levels [192, 256]
- vector_rvv (polymul): missing levels [192, 256]
- scalar_ref (KEM): missing levels [192, 256]
- scalar_zb (KEM): missing levels [192, 256]
- vector_rvv (KEM): missing levels [192, 256]

