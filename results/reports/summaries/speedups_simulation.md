# HQC-RISC-V Thesis Speedup Summary

Speedup = Baseline instret / Champion instret (higher is better).
All measurements are deterministic instruction-retired counts from Spike ISS.

## Table 1 — Tier 1 (IoT Scalar)

> rv64gc_zbc_zbb, no vector

### `vect_mul` (polynomial multiply)

| Level | ref_scalar (instret) | karatsuba_zbc (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 19,214,281 | 211,841 | x90.70 |
| HQC-192 | 58,433,901 | 650,635 | x89.81 |
| HQC-256 | 142,641,044 | 1,900,362 | x75.06 |

### KEM operations

| Level | Op | ref_scalar (instret) | karatsuba_zbc (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 19,454,626 | 462,794 | x42.04 |
| HQC-128 | encaps | 38,875,925 | 898,649 | x43.26 |
| HQC-128 | decaps | 59,013,715 | 2,088,352 | x28.26 |
| HQC-128 | **Total KEM** | **117,344,266** | **3,449,795** | **x34.01** |
| HQC-192 | keypair | 58,895,392 | 1,132,934 | x51.98 |
| HQC-192 | encaps | 117,744,966 | 2,235,711 | x52.67 |
| HQC-192 | decaps | 177,662,247 | 4,442,411 | x39.99 |
| HQC-192 | **Total KEM** | **354,302,605** | **7,811,056** | **x45.36** |
| HQC-256 | keypair | 143,382,635 | 2,621,830 | x54.69 |
| HQC-256 | encaps | 286,701,285 | 5,204,511 | x55.09 |
| HQC-256 | decaps | 431,890,526 | 9,725,450 | x44.41 |
| HQC-256 | **Total KEM** | **861,974,446** | **17,551,791** | **x49.11** |

## Table 2 — Tier 2 (Edge RVV)

> rv64gcv + Zbc/Zbb; AP-2 fixed-tree polymul, scalar Keccak

### `vect_mul` (polynomial multiply)

| Level | ref_vector (instret) | karatsuba_rvv (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,527,412 | 157,377 | x117.73 |
| HQC-192 | 56,315,140 | 481,065 | x117.06 |
| HQC-256 | 139,115,660 | 1,608,062 | x86.51 |

### KEM operations

| Level | Op | ref_vector (instret) | karatsuba_rvv (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,655,515 | 289,001 | x64.55 |
| HQC-128 | encaps | 37,330,685 | 591,892 | x63.07 |
| HQC-128 | decaps | 56,400,528 | 1,094,531 | x51.53 |
| HQC-128 | **Total KEM** | **112,386,728** | **1,975,424** | **x56.89** |
| HQC-192 | keypair | 56,547,117 | 713,518 | x79.25 |
| HQC-192 | encaps | 113,137,214 | 1,472,419 | x76.84 |
| HQC-192 | decaps | 170,303,459 | 2,544,365 | x66.93 |
| HQC-192 | **Total KEM** | **339,987,790** | **4,730,302** | **x71.87** |
| HQC-256 | keypair | 139,482,222 | 1,887,450 | x73.90 |
| HQC-256 | encaps | 279,052,508 | 3,845,194 | x72.57 |
| HQC-256 | decaps | 419,696,191 | 6,402,858 | x65.55 |
| HQC-256 | **Total KEM** | **838,230,921** | **12,135,502** | **x69.07** |

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
| HQC-128 | keypair | 18,655,515 | 105,279 | x177.20 |
| HQC-128 | encaps | 37,330,685 | 217,479 | x171.65 |
| HQC-128 | decaps | 56,400,528 | 394,225 | x143.07 |
| HQC-128 | **Total KEM** | **112,386,728** | **716,983** | **x156.75** |
| HQC-192 | keypair | 56,547,117 | 258,251 | x218.96 |
| HQC-192 | encaps | 113,137,214 | 534,634 | x211.62 |
| HQC-192 | decaps | 170,303,459 | 902,007 | x188.81 |
| HQC-192 | **Total KEM** | **339,987,790** | **1,694,892** | **x200.60** |
| HQC-256 | keypair | 139,482,222 | 383,592 | x363.62 |
| HQC-256 | encaps | 279,052,508 | 816,033 | x341.96 |
| HQC-256 | decaps | 419,696,191 | 1,411,713 | x297.30 |
| HQC-256 | **Total KEM** | **838,230,921** | **2,611,338** | **x321.00** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 463,069 | 131,944 | x3.51 |
| HQC-192 | 902,926 | 249,581 | x3.62 |
| HQC-256 | 1,405,641 | 383,277 | x3.67 |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 384,986 | 60,353 | x6.38 |
| HQC-192 | 562,125 | 77,114 | x7.29 |
| HQC-256 | 1,059,605 | 159,206 | x6.66 |

