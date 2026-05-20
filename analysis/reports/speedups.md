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
| HQC-128 | keypair | 18,655,515 | 340,875 | x54.73 |
| HQC-128 | encaps | 37,330,685 | 694,007 | x53.79 |
| HQC-128 | decaps | 56,400,528 | 1,278,629 | x44.11 |
| HQC-128 | **Total KEM** | **112,386,728** | **2,313,511** | **x48.58** |
| HQC-192 | keypair | 56,547,117 | 813,216 | x69.54 |
| HQC-192 | encaps | 113,137,214 | 1,679,456 | x67.37 |
| HQC-192 | decaps | 170,303,459 | 2,917,845 | x58.37 |
| HQC-192 | **Total KEM** | **339,987,790** | **5,410,517** | **x62.84** |
| HQC-256 | keypair | 139,482,222 | 2,136,500 | x65.29 |
| HQC-256 | encaps | 279,052,508 | 4,341,476 | x64.28 |
| HQC-256 | decaps | 419,696,191 | 7,256,735 | x57.84 |
| HQC-256 | **Total KEM** | **838,230,921** | **13,734,711** | **x61.03** |

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
| HQC-128 | keypair | 18,655,515 | 126,404 | x147.59 |
| HQC-128 | encaps | 37,330,685 | 263,940 | x141.44 |
| HQC-128 | decaps | 56,400,528 | 536,454 | x105.14 |
| HQC-128 | **Total KEM** | **112,386,728** | **926,798** | **x121.26** |
| HQC-192 | keypair | 56,547,117 | 313,384 | x180.44 |
| HQC-192 | encaps | 113,137,214 | 654,219 | x172.93 |
| HQC-192 | decaps | 170,303,459 | 1,179,590 | x144.38 |
| HQC-192 | **Total KEM** | **339,987,790** | **2,147,193** | **x158.34** |
| HQC-256 | keypair | 139,482,222 | 472,898 | x294.95 |
| HQC-256 | encaps | 279,052,508 | 1,006,398 | x277.28 |
| HQC-256 | decaps | 419,696,191 | 1,906,657 | x220.12 |
| HQC-256 | **Total KEM** | **838,230,921** | **3,385,953** | **x247.56** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 4,680 | 2,114 | x2.21 |
| HQC-192 | 4,680 | 2,114 | x2.21 |
| HQC-256 | 4,680 | 2,114 | x2.21 |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 395,808 | 134,879 | x2.93 |
| HQC-192 | 577,483 | 175,270 | x3.29 |
| HQC-256 | 1,080,683 | 360,313 | x3.00 |

