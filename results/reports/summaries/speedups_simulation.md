# HQC-RISC-V Thesis Speedup Summary

Speedup = Baseline instret / Champion instret (higher is better).
All measurements are deterministic instruction-retired counts from Spike ISS.

## Table 1 — Tier 1 (IoT Scalar)

> rv64gc_zbc_zbb, no vector

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | scalar_zb (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,504,302 | 182,726 | ×101.3 |
| HQC-192 | 56,292,809 | 570,293 | ×98.7 |
| HQC-256 | 139,222,953 | 1,262,888 | ×110.2 |

### KEM operations

| Level | Op | scalar_ref (instret) | scalar_zb (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,750,499 | 428,923 | ×43.7 |
| HQC-128 | encaps | 37,477,397 | 834,245 | ×44.9 |
| HQC-128 | decaps | 56,937,074 | 1,972,347 | ×28.9 |
| HQC-128 | **Total KEM** | **113,164,970** | **3,235,515** | **×35.0** |
| HQC-192 | keypair | 56,763,859 | 1,041,343 | ×54.5 |
| HQC-192 | encaps | 113,506,273 | 2,061,241 | ×55.1 |
| HQC-192 | decaps | 171,349,089 | 4,181,542 | ×41.0 |
| HQC-192 | **Total KEM** | **341,619,221** | **7,284,126** | **×46.9** |
| HQC-256 | keypair | 139,979,206 | 2,019,141 | ×69.3 |
| HQC-256 | encaps | 279,919,351 | 3,999,221 | ×70.0 |
| HQC-256 | decaps | 421,860,950 | 7,980,756 | ×52.9 |
| HQC-256 | **Total KEM** | **841,759,507** | **13,999,118** | **×60.1** |

## Table 2 — Tier 2 (Edge RVV)

> rv64gcv + Zbc/Zbb; specialized fixed-tree polymul, scalar Keccak

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | vector_rvv (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,504,302 | 148,479 | ×124.6 |
| HQC-192 | 56,292,809 | 449,808 | ×125.1 |
| HQC-256 | 139,222,953 | 1,043,726 | ×133.4 |

### KEM operations

| Level | Op | scalar_ref (instret) | vector_rvv (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,750,499 | 294,501 | ×63.7 |
| HQC-128 | encaps | 37,477,397 | 589,121 | ×63.6 |
| HQC-128 | decaps | 56,937,074 | 1,139,356 | ×50.0 |
| HQC-128 | **Total KEM** | **113,164,970** | **2,022,978** | **×55.9** |
| HQC-192 | keypair | 56,763,859 | 710,531 | ×79.9 |
| HQC-192 | encaps | 113,506,273 | 1,435,368 | ×79.1 |
| HQC-192 | decaps | 171,349,089 | 2,579,762 | ×66.4 |
| HQC-192 | **Total KEM** | **341,619,221** | **4,725,661** | **×72.3** |
| HQC-256 | keypair | 139,979,206 | 1,461,824 | ×95.8 |
| HQC-256 | encaps | 279,919,351 | 2,948,854 | ×94.9 |
| HQC-256 | decaps | 421,860,950 | 5,274,100 | ×80.0 |
| HQC-256 | **Total KEM** | **841,759,507** | **9,684,778** | **×86.9** |

## Table 3 — Tier 3 (High-End Zvbc)

> rv64gcv + Zvbc/Zvbb/Zvl256b

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,504,302 | 70,070 | ×264.1 |
| HQC-192 | 56,292,809 | 207,443 | ×271.4 |
| HQC-256 | 139,222,953 | 318,972 | ×436.5 |

### KEM operations

| Level | Op | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,750,499 | 216,092 | ×86.8 |
| HQC-128 | encaps | 37,477,397 | 432,303 | ×86.7 |
| HQC-128 | decaps | 56,937,074 | 843,459 | ×67.5 |
| HQC-128 | **Total KEM** | **113,164,970** | **1,491,854** | **×75.9** |
| HQC-192 | keypair | 56,763,859 | 468,166 | ×121.2 |
| HQC-192 | encaps | 113,506,273 | 950,638 | ×119.4 |
| HQC-192 | decaps | 171,349,089 | 1,773,770 | ×96.6 |
| HQC-192 | **Total KEM** | **341,619,221** | **3,192,574** | **×107.0** |
| HQC-256 | keypair | 139,979,206 | 737,070 | ×189.9 |
| HQC-256 | encaps | 279,919,351 | 1,499,346 | ×186.7 |
| HQC-256 | decaps | 421,860,950 | 2,902,352 | ×145.4 |
| HQC-256 | **Total KEM** | **841,759,507** | **5,138,768** | **×163.8** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 758,580 | 554,900 | ×1.4 |
| HQC-192 | 1,505,161 | 1,103,161 | ×1.4 |
| HQC-256 | 2,360,607 | 1,736,167 | ×1.4 |

## Table 5 — Decode Speedups

### `decode`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 1,052,692 | 73,769 | ×14.3 |
| HQC-192 | 1,430,377 | 89,688 | ×15.9 |
| HQC-256 | 3,066,751 | 231,570 | ×13.2 |

