# HQC-RISC-V Thesis Speedup Summary

Speedup = Baseline instret / Champion instret (higher is better).
All measurements are deterministic instruction-retired counts from Spike ISS.

## Table 1 — Tier 1 (IoT Scalar)

> rv64gc_zbc_zbb_zbkb, no vector

### `vect_mul` (polynomial multiply)

| Level | ref_scalar (instret) | karatsuba_zbc (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 19,214,281 | 211,841 | x90.70 |
| HQC-192 | 58,433,901 | 650,635 | x89.81 |
| HQC-256 | 142,641,044 | 1,900,362 | x75.06 |

### KEM operations

| Level | Op | ref_scalar (instret) | karatsuba_zbc (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 19,454,626 | 453,609 | x42.89 |
| HQC-128 | encaps | 38,875,925 | 880,279 | x44.16 |
| HQC-128 | decaps | 59,013,715 | 2,060,797 | x28.64 |
| HQC-128 | **Total KEM** | **117,344,266** | **3,394,685** | **x34.57** |
| HQC-192 | keypair | 58,895,392 | 1,113,280 | x52.90 |
| HQC-192 | encaps | 117,744,966 | 2,196,403 | x53.61 |
| HQC-192 | decaps | 177,662,247 | 4,383,449 | x40.53 |
| HQC-192 | **Total KEM** | **354,302,605** | **7,693,132** | **x46.05** |
| HQC-256 | keypair | 143,382,635 | 2,642,594 | x54.26 |
| HQC-256 | encaps | 286,701,285 | 5,246,039 | x54.65 |
| HQC-256 | decaps | 431,890,526 | 9,787,742 | x44.13 |
| HQC-256 | **Total KEM** | **861,974,446** | **17,676,375** | **x48.76** |

## Table 2 — Tier 2 (Edge RVV)

> rv64gcv + Zbc/Zbb/Zbkb; AP-2 fixed-tree polymul, scalar Keccak

### `vect_mul` (polynomial multiply)

| Level | ref_vector (instret) | karatsuba_rvv (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,527,412 | 157,377 | x117.73 |
| HQC-192 | 56,315,140 | 481,065 | x117.06 |
| HQC-256 | 139,115,660 | 1,608,062 | x86.51 |

### KEM operations

| Level | Op | ref_vector (instret) | karatsuba_rvv (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,655,515 | 286,680 | x65.07 |
| HQC-128 | encaps | 37,330,685 | 587,126 | x63.58 |
| HQC-128 | decaps | 56,400,528 | 1,051,644 | x53.63 |
| HQC-128 | **Total KEM** | **112,386,728** | **1,925,450** | **x58.37** |
| HQC-192 | keypair | 56,547,117 | 713,711 | x79.23 |
| HQC-192 | encaps | 113,137,214 | 1,472,637 | x76.83 |
| HQC-192 | decaps | 170,303,459 | 2,505,472 | x67.97 |
| HQC-192 | **Total KEM** | **339,987,790** | **4,691,820** | **x72.46** |
| HQC-256 | keypair | 139,482,222 | 1,976,880 | x70.56 |
| HQC-256 | encaps | 279,052,508 | 4,023,862 | x69.35 |
| HQC-256 | decaps | 419,696,191 | 6,535,320 | x64.22 |
| HQC-256 | **Total KEM** | **838,230,921** | **12,536,062** | **x66.87** |

## Table 3 — Tier 3 (High-End Zvbc)

> rv64gcv + Zvbc/Zvbb/Zvl256b

### `vect_mul` (polynomial multiply)

| Level | ref_vector (instret) | karatsuba_zvbc (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,527,412 | 72,644 | x255.04 |
| HQC-192 | 56,315,140 | 219,338 | x256.75 |
| HQC-256 | 139,115,660 | 326,260 | x426.40 |

### KEM operations

| Level | Op | ref_vector (instret) | karatsuba_zvbc (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,655,515 | 118,025 | x158.06 |
| HQC-128 | encaps | 37,330,685 | 242,738 | x153.79 |
| HQC-128 | decaps | 56,400,528 | 463,581 | x121.66 |
| HQC-128 | **Total KEM** | **112,386,728** | **824,344** | **x136.33** |
| HQC-192 | keypair | 56,547,117 | 297,213 | x190.26 |
| HQC-192 | encaps | 113,137,214 | 612,536 | x184.70 |
| HQC-192 | decaps | 170,303,459 | 1,067,117 | x159.59 |
| HQC-192 | **Total KEM** | **339,987,790** | **1,976,866** | **x171.98** |
| HQC-256 | keypair | 139,482,222 | 446,316 | x312.52 |
| HQC-256 | encaps | 279,052,508 | 940,855 | x296.59 |
| HQC-256 | decaps | 419,696,191 | 1,656,148 | x253.42 |
| HQC-256 | **Total KEM** | **838,230,921** | **3,043,319** | **x275.43** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 4,680 | 2,127 | x2.20 |
| HQC-192 | 4,680 | 2,127 | x2.20 |
| HQC-256 | 4,680 | 2,127 | x2.20 |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 384,986 | 87,951 | x4.38 |
| HQC-192 | 562,125 | 119,930 | x4.69 |
| HQC-256 | 1,059,605 | 204,237 | x5.19 |

