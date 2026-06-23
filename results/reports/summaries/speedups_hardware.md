# Hardware Speedup Report — BPI-F3

| Parameter | Value |
|-----------|-------|
| Board | Banana Pi BPI-F3 (SpacemiT K1) |
| Core | SpacemiT X60 |
| VLEN | 256 |
| Compiler | GCC 14.2.0 (native) / Clang 20.1.2 |
| Isolation | taskset (single-core pinning) |
| Iterations | 100 |

Speedup = baseline_median_cycles / champion_median_cycles (higher is better).

## Isolated Polynomial Multiplication (`vect_mul`)

| Level | scalar_ref (cycles) | scalar_zb (cycles) | Speedup | vector_rvv (cycles) | Speedup |
|-------|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | 11,636,523 | 148,903 | ×78.1 | 125,921 | ×92.4 |
| HQC-192 | 35,380,368 | 448,455 | ×78.9 | 364,036 | ×97.2 |
| HQC-256 | 88,394,244 | 927,364 | ×95.3 | 847,899 | ×104.3 |

## Full KEM Operations

| Level | Op | scalar_ref (cycles) | scalar_zb (cycles) | Speedup | vector_rvv (cycles) | Speedup |
|-------|-----|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | keypair | 11,771,647 | 261,279 | ×45.1 | 218,959 | ×53.8 |
| HQC-128 | encaps | 23,501,394 | 506,708 | ×46.4 | 432,120 | ×54.4 |
| HQC-128 | decaps | 35,748,712 | 1,265,465 | ×28.2 | 916,067 | ×39.0 |
| HQC-128 | **Total** | **71,021,753** | **2,033,452** | **×34.9** | **1,567,146** | **×45.3** |
| HQC-192 | keypair | 35,575,827 | 645,194 | ×55.1 | 534,796 | ×66.5 |
| HQC-192 | encaps | 71,164,382 | 1,293,972 | ×55.0 | 1,067,041 | ×66.7 |
| HQC-192 | decaps | 107,483,876 | 2,692,077 | ×39.9 | 2,027,725 | ×53.0 |
| HQC-192 | **Total** | **214,224,085** | **4,631,243** | **×46.3** | **3,629,562** | **×59.0** |
| HQC-256 | keypair | 88,733,883 | 1,249,402 | ×71.0 | 1,104,689 | ×80.3 |
| HQC-256 | encaps | 177,531,846 | 2,545,929 | ×69.7 | 2,255,193 | ×78.7 |
| HQC-256 | decaps | 267,783,250 | 5,153,782 | ×52.0 | 4,195,538 | ×63.8 |
| HQC-256 | **Total** | **534,048,979** | **8,949,113** | **×59.7** | **7,555,420** | **×70.7** |

