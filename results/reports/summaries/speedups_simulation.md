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
| HQC-128 | encaps | 37,325,173 | 586,400 | ×63.7 |
| HQC-128 | decaps | 56,394,693 | 1,084,132 | ×52.0 |
| HQC-128 | **Total KEM** | **112,373,542** | **1,956,787** | **×57.4** |
| HQC-192 | keypair | 56,543,388 | 704,882 | ×80.2 |
| HQC-192 | encaps | 113,126,035 | 1,455,147 | ×77.7 |
| HQC-192 | decaps | 170,291,912 | 2,515,258 | ×67.7 |
| HQC-192 | **Total KEM** | **339,961,335** | **4,675,287** | **×72.7** |
| HQC-256 | keypair | 139,476,234 | 1,869,816 | ×74.6 |
| HQC-256 | encaps | 279,034,551 | 3,809,926 | ×73.2 |
| HQC-256 | decaps | 419,679,222 | 6,341,725 | ×66.2 |
| HQC-256 | **Total KEM** | **838,190,007** | **12,021,467** | **×69.7** |

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
| HQC-128 | keypair | 18,653,676 | 107,408 | ×173.7 |
| HQC-128 | encaps | 37,325,173 | 221,511 | ×168.5 |
| HQC-128 | decaps | 56,394,693 | 400,478 | ×140.8 |
| HQC-128 | **Total KEM** | **112,373,542** | **729,397** | **×154.1** |
| HQC-192 | keypair | 56,543,388 | 262,108 | ×215.7 |
| HQC-192 | encaps | 113,126,035 | 542,410 | ×208.6 |
| HQC-192 | decaps | 170,291,912 | 914,520 | ×186.2 |
| HQC-192 | **Total KEM** | **339,961,335** | **1,719,038** | **×197.8** |
| HQC-256 | keypair | 139,476,234 | 389,753 | ×357.9 |
| HQC-256 | encaps | 279,034,551 | 827,937 | ×337.0 |
| HQC-256 | decaps | 419,679,222 | 1,430,020 | ×293.5 |
| HQC-256 | **Total KEM** | **838,190,007** | **2,647,710** | **×316.6** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 463,069 | 131,944 | ×3.5 |
| HQC-192 | 902,926 | 249,581 | ×3.6 |
| HQC-256 | 1,405,641 | 383,277 | ×3.7 |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 384,986 | 56,515 | ×6.8 |
| HQC-192 | 562,125 | 72,237 | ×7.8 |
| HQC-256 | 1,059,605 | 147,077 | ×7.2 |

