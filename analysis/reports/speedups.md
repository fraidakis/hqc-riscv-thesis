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
| HQC-128 | keypair | 19,458,964 | 458,121 | x42.48 |
| HQC-128 | encaps | 38,900,675 | 905,479 | x42.96 |
| HQC-128 | decaps | 59,051,432 | 2,099,075 | x28.13 |
| HQC-128 | **Total KEM** | **117,411,071** | **3,462,675** | **x33.91** |
| HQC-192 | keypair | 58,905,340 | 1,123,538 | x52.43 |
| HQC-192 | encaps | 117,802,593 | 2,254,714 | x52.25 |
| HQC-192 | decaps | 177,740,182 | 4,462,247 | x39.83 |
| HQC-192 | **Total KEM** | **354,448,115** | **7,840,499** | **x45.21** |
| HQC-256 | keypair | 143,399,884 | 2,660,281 | x53.90 |
| HQC-256 | encaps | 286,800,072 | 5,345,720 | x53.65 |
| HQC-256 | decaps | 432,019,077 | 9,917,432 | x43.56 |
| HQC-256 | **Total KEM** | **862,219,033** | **17,923,433** | **x48.11** |

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
| HQC-128 | keypair | 18,665,579 | 298,707 | x62.49 |
| HQC-128 | encaps | 37,351,077 | 621,513 | x60.10 |
| HQC-128 | decaps | 56,442,467 | 1,186,497 | x47.57 |
| HQC-128 | **Total KEM** | **112,459,123** | **2,106,717** | **x53.38** |
| HQC-192 | keypair | 56,563,028 | 733,476 | x77.12 |
| HQC-192 | encaps | 113,170,595 | 1,538,040 | x73.58 |
| HQC-192 | decaps | 170,368,869 | 2,697,205 | x63.16 |
| HQC-192 | **Total KEM** | **340,102,492** | **4,968,721** | **x68.45** |
| HQC-256 | keypair | 139,504,204 | 2,005,047 | x69.58 |
| HQC-256 | encaps | 279,099,192 | 4,123,999 | x67.68 |
| HQC-256 | decaps | 419,815,844 | 6,986,546 | x60.09 |
| HQC-256 | **Total KEM** | **838,419,240** | **13,115,592** | **x63.93** |

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
| HQC-128 | keypair | 18,665,579 | 155,539 | x120.01 |
| HQC-128 | encaps | 37,351,077 | 335,516 | x111.32 |
| HQC-128 | decaps | 56,442,467 | 723,152 | x78.05 |
| HQC-128 | **Total KEM** | **112,459,123** | **1,214,207** | **x92.62** |
| HQC-192 | keypair | 56,563,028 | 365,814 | x154.62 |
| HQC-192 | encaps | 113,170,595 | 788,030 | x143.61 |
| HQC-192 | decaps | 170,368,869 | 1,507,071 | x113.05 |
| HQC-192 | **Total KEM** | **340,102,492** | **2,660,915** | **x127.81** |
| HQC-256 | keypair | 139,504,204 | 554,405 | x251.63 |
| HQC-256 | encaps | 279,099,192 | 1,212,462 | x230.19 |
| HQC-256 | decaps | 419,815,844 | 2,495,396 | x168.24 |
| HQC-256 | **Total KEM** | **838,419,240** | **4,262,263** | **x196.71** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 4,680 | 2,426 | x1.93 |
| HQC-192 | 4,680 | 2,426 | x1.93 |
| HQC-256 | 4,680 | 2,426 | x1.93 |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 400,787 | 183,339 | x2.19 |
| HQC-192 | 583,821 | 238,239 | x2.45 |
| HQC-256 | 1,099,622 | 544,226 | x2.02 |

