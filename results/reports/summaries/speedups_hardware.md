# Hardware Speedup Report — BPI-F3

| Parameter | Value |
|-----------|-------|
| Board | Banana Pi BPI-F3 (SpacemiT K1) |
| Core | SpacemiT X60 |
| VLEN | 256 |
| Compiler | GCC (native) |
| Isolation | taskset (single-core pinning) |
| Iterations | 100 |

Speedup = baseline_median_cycles / champion_median_cycles (higher is better).

## Isolated Polynomial Multiplication (`vect_mul`)

| Level | scalar_ref (cycles) | scalar_zb (cycles) | Speedup | vector_rvv (cycles) | Speedup |
|-------|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | 11,635,670 | 122,503 | ×95.0 | 122,085 | ×95.3 |
| HQC-192 | 35,382,098 | 367,480 | ×96.3 | 357,116 | ×99.1 |
| HQC-256 | 88,385,859 | 790,047 | ×111.9 | 730,657 | ×121.0 |

## Full KEM Operations

| Level | Op | scalar_ref (cycles) | scalar_zb (cycles) | Speedup | vector_rvv (cycles) | Speedup |
|-------|-----|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | keypair | 11,764,946 | 232,836 | ×50.5 | 212,671 | ×55.3 |
| HQC-128 | encaps | 23,488,696 | 461,278 | ×50.9 | 425,274 | ×55.2 |
| HQC-128 | decaps | 35,741,592 | 1,197,341 | ×29.9 | 910,108 | ×39.3 |
| HQC-128 | **Total** | **70,995,234** | **1,891,455** | **×37.5** | **1,548,053** | **×45.9** |
| HQC-192 | keypair | 35,708,046 | 578,051 | ×61.8 | 528,765 | ×67.5 |
| HQC-192 | encaps | 71,454,941 | 1,157,009 | ×61.8 | 1,062,094 | ×67.3 |
| HQC-192 | decaps | 107,873,933 | 2,493,891 | ×43.3 | 2,021,272 | ×53.4 |
| HQC-192 | **Total** | **215,036,920** | **4,228,951** | **×50.8** | **3,612,131** | **×59.5** |
| HQC-256 | keypair | 89,354,458 | 1,111,921 | ×80.4 | 982,499 | ×90.9 |
| HQC-256 | encaps | 178,512,358 | 2,275,266 | ×78.5 | 2,019,880 | ×88.4 |
| HQC-256 | decaps | 269,051,042 | 4,746,244 | ×56.7 | 3,845,521 | ×70.0 |
| HQC-256 | **Total** | **536,917,858** | **8,133,431** | **×66.0** | **6,847,900** | **×78.4** |

