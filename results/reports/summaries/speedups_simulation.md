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

| Level | vector_ref (instret) | vector_rvv (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,527,409 | 157,473 | ×117.7 |
| HQC-192 | 56,315,139 | 473,083 | ×119.0 |
| HQC-256 | 139,115,660 | 1,502,171 | ×92.6 |

### KEM operations

| Level | Op | vector_ref (instret) | vector_rvv (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,653,676 | 286,255 | ×65.2 |
| HQC-128 | encaps | 37,325,173 | 585,996 | ×63.7 |
| HQC-128 | decaps | 56,394,693 | 1,345,522 | ×41.9 |
| HQC-128 | **Total KEM** | **112,373,542** | **2,217,773** | **×50.7** |
| HQC-192 | keypair | 56,543,388 | 704,882 | ×80.2 |
| HQC-192 | encaps | 113,126,035 | 1,454,566 | ×77.8 |
| HQC-192 | decaps | 170,291,912 | 2,918,831 | ×58.3 |
| HQC-192 | **Total KEM** | **339,961,335** | **5,078,279** | **×66.9** |
| HQC-256 | keypair | 139,476,234 | 1,869,816 | ×74.6 |
| HQC-256 | encaps | 279,034,551 | 3,811,335 | ×73.2 |
| HQC-256 | decaps | 419,679,222 | 7,069,403 | ×59.4 |
| HQC-256 | **Total KEM** | **838,190,007** | **12,750,554** | **×65.7** |

## Table 3 — Tier 3 (High-End Zvbc)

> rv64gcv + Zvbc/Zvbb/Zvl256b

### `vect_mul` (polynomial multiply)

| Level | vector_ref (instret) | vector_zvk (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,527,409 | 62,530 | ×296.3 |
| HQC-192 | 56,315,139 | 185,120 | ×304.2 |
| HQC-256 | 139,115,660 | 271,006 | ×513.3 |

### KEM operations

| Level | Op | vector_ref (instret) | vector_zvk (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,653,676 | 191,312 | ×97.5 |
| HQC-128 | encaps | 37,325,173 | 396,110 | ×94.2 |
| HQC-128 | decaps | 56,394,693 | 1,060,606 | ×53.2 |
| HQC-128 | **Total KEM** | **112,373,542** | **1,648,028** | **×68.2** |
| HQC-192 | keypair | 56,543,388 | 416,919 | ×135.6 |
| HQC-192 | encaps | 113,126,035 | 878,640 | ×128.8 |
| HQC-192 | decaps | 170,291,912 | 2,054,818 | ×82.9 |
| HQC-192 | **Total KEM** | **339,961,335** | **3,350,377** | **×101.5** |
| HQC-256 | keypair | 139,476,234 | 638,651 | ×218.4 |
| HQC-256 | encaps | 279,034,551 | 1,349,005 | ×206.8 |
| HQC-256 | decaps | 419,679,222 | 3,375,849 | ×124.3 |
| HQC-256 | **Total KEM** | **838,190,007** | **5,363,505** | **×156.3** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 463,069 | 131,944 | ×3.5 |
| HQC-192 | — | 249,581 | — |
| HQC-256 | — | 383,277 | — |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 384,986 | 56,515 | ×6.8 |
| HQC-192 | 562,125 | 72,237 | ×7.8 |
| HQC-256 | 1,059,605 | 147,077 | ×7.2 |

