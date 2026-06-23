# HQC-RISC-V Thesis Speedup Summary

Speedup = Baseline instret / Champion instret (higher is better).
All measurements are deterministic instruction-retired counts from Spike ISS.

## Table 1 — Tier 1 (IoT Scalar)

> rv64gc_zbc_zbb, no vector

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | scalar_zb (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 19,214,277 | 221,030 | ×86.9 |
| HQC-192 | 58,433,896 | 670,293 | ×87.2 |
| HQC-256 | 142,641,043 | 1,879,602 | ×75.9 |

### KEM operations

| Level | Op | scalar_ref (instret) | scalar_zb (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 19,454,626 | 462,794 | ×42.0 |
| HQC-128 | encaps | 38,875,925 | 898,649 | ×43.3 |
| HQC-128 | decaps | 59,013,743 | 2,088,352 | ×28.3 |
| HQC-128 | **Total KEM** | **117,344,294** | **3,449,795** | **×34.0** |
| HQC-192 | keypair | 58,895,392 | 1,132,934 | ×52.0 |
| HQC-192 | encaps | 117,744,966 | 2,235,711 | ×52.7 |
| HQC-192 | decaps | 177,662,495 | 4,442,411 | ×40.0 |
| HQC-192 | **Total KEM** | **354,302,853** | **7,811,056** | **×45.4** |
| HQC-256 | keypair | 143,382,635 | 2,621,830 | ×54.7 |
| HQC-256 | encaps | 286,701,285 | 5,204,511 | ×55.1 |
| HQC-256 | decaps | 431,890,526 | 9,725,450 | ×44.4 |
| HQC-256 | **Total KEM** | **861,974,446** | **17,551,791** | **×49.1** |

## Table 2 — Tier 2 (Edge RVV)

> rv64gcv + Zbc/Zbb; AP-2 fixed-tree polymul, scalar Keccak

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | vector_rvv (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 19,214,277 | 157,473 | ×122.0 |
| HQC-192 | 58,433,896 | 473,083 | ×123.5 |
| HQC-256 | 142,641,043 | 1,502,171 | ×95.0 |

### KEM operations

| Level | Op | scalar_ref (instret) | vector_rvv (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 19,454,626 | 295,804 | ×65.8 |
| HQC-128 | encaps | 38,875,925 | 593,089 | ×65.5 |
| HQC-128 | decaps | 59,013,743 | 1,129,719 | ×52.2 |
| HQC-128 | **Total KEM** | **117,344,294** | **2,018,612** | **×58.1** |
| HQC-192 | keypair | 58,895,392 | 720,307 | ×81.8 |
| HQC-192 | encaps | 117,744,966 | 1,458,667 | ×80.7 |
| HQC-192 | decaps | 177,662,495 | 2,594,207 | ×68.5 |
| HQC-192 | **Total KEM** | **354,302,853** | **4,773,181** | **×74.2** |
| HQC-256 | keypair | 143,382,635 | 1,897,102 | ×75.6 |
| HQC-256 | encaps | 286,701,285 | 3,838,620 | ×74.7 |
| HQC-256 | decaps | 431,890,526 | 6,505,347 | ×66.4 |
| HQC-256 | **Total KEM** | **861,974,446** | **12,241,069** | **×70.4** |

## Table 3 — Tier 3 (High-End Zvbc)

> rv64gcv + Zvbc/Zvbb/Zvl256b

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 19,214,277 | 60,380 | ×318.2 |
| HQC-192 | 58,433,896 | 178,624 | ×327.1 |
| HQC-256 | 142,641,043 | 260,624 | ×547.3 |

### KEM operations

| Level | Op | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 19,454,626 | 103,129 | ×188.6 |
| HQC-128 | encaps | 38,875,925 | 213,179 | ×182.4 |
| HQC-128 | decaps | 59,013,743 | 386,447 | ×152.7 |
| HQC-128 | **Total KEM** | **117,344,294** | **702,755** | **×167.0** |
| HQC-192 | keypair | 58,895,392 | 251,755 | ×233.9 |
| HQC-192 | encaps | 117,744,966 | 521,642 | ×225.7 |
| HQC-192 | decaps | 177,662,495 | 880,407 | ×201.8 |
| HQC-192 | **Total KEM** | **354,302,853** | **1,653,804** | **×214.2** |
| HQC-256 | keypair | 143,382,635 | 373,210 | ×384.2 |
| HQC-256 | encaps | 286,701,285 | 795,269 | ×360.5 |
| HQC-256 | decaps | 431,890,526 | 1,375,654 | ×314.0 |
| HQC-256 | **Total KEM** | **861,974,446** | **2,544,133** | **×338.8** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 463,069 | 124,648 | ×3.7 |
| HQC-192 | 902,926 | 235,181 | ×3.8 |
| HQC-256 | 1,405,641 | 360,909 | ×3.9 |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 384,986 | 56,326 | ×6.8 |
| HQC-192 | 562,125 | 72,012 | ×7.8 |
| HQC-256 | 1,059,605 | 146,513 | ×7.2 |

