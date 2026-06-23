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
| HQC-128 | 11,636,523 | 148,903 | ×78.1 | 141,716 | ×82.1 |
| HQC-192 | 35,380,368 | 448,455 | ×78.9 | 420,347 | ×84.2 |
| HQC-256 | 88,394,244 | 927,364 | ×95.3 | 849,017 | ×104.1 |

## Full KEM Operations

| Level | Op | scalar_ref (cycles) | scalar_zb (cycles) | Speedup | vector_rvv (cycles) | Speedup |
|-------|-----|---------------------|--------------------|---------|--------------------|---------|
| HQC-128 | keypair | 11,771,647 | 261,279 | ×45.1 | 234,993 | ×50.1 |
| HQC-128 | encaps | 23,501,394 | 506,708 | ×46.4 | 468,737 | ×50.1 |
| HQC-128 | decaps | 35,748,712 | 1,265,465 | ×28.2 | 970,694 | ×36.8 |
| HQC-128 | **Total** | **71,021,753** | **2,033,452** | **×34.9** | **1,674,424** | **×42.4** |
| HQC-192 | keypair | 35,575,827 | 645,194 | ×55.1 | 591,031 | ×60.2 |
| HQC-192 | encaps | 71,164,382 | 1,293,972 | ×55.0 | 1,188,147 | ×59.9 |
| HQC-192 | decaps | 107,483,876 | 2,692,077 | ×39.9 | 2,197,835 | ×48.9 |
| HQC-192 | **Total** | **214,224,085** | **4,631,243** | **×46.3** | **3,977,013** | **×53.9** |
| HQC-256 | keypair | 88,733,883 | 1,249,402 | ×71.0 | 1,105,258 | ×80.3 |
| HQC-256 | encaps | 177,531,846 | 2,545,929 | ×69.7 | 2,255,028 | ×78.7 |
| HQC-256 | decaps | 267,783,250 | 5,153,782 | ×52.0 | 4,196,643 | ×63.8 |
| HQC-256 | **Total** | **534,048,979** | **8,949,113** | **×59.7** | **7,556,929** | **×70.7** |

