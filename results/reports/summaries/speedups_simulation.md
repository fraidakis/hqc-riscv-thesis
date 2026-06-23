# HQC-RISC-V Thesis Speedup Summary

Speedup = Baseline instret / Champion instret (higher is better).
All measurements are deterministic instruction-retired counts from Spike ISS.

## Table 1 — Tier 1 (IoT Scalar)

> rv64gc_zbc_zbb, no vector

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | scalar_zb (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,639,752 | 186,629 | ×99.9 |
| HQC-192 | 56,702,057 | 591,557 | ×95.9 |
| HQC-256 | 139,877,019 | 1,306,068 | ×107.1 |

### KEM operations

| Level | Op | scalar_ref (instret) | scalar_zb (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,886,091 | 432,968 | ×43.6 |
| HQC-128 | encaps | 37,748,533 | 842,287 | ×44.8 |
| HQC-128 | decaps | 57,361,651 | 2,002,282 | ×28.6 |
| HQC-128 | **Total KEM** | **113,996,275** | **3,277,537** | **×34.8** |
| HQC-192 | keypair | 57,173,317 | 1,062,817 | ×53.8 |
| HQC-192 | encaps | 114,325,122 | 2,104,122 | ×54.3 |
| HQC-192 | decaps | 172,613,387 | 4,281,887 | ×40.3 |
| HQC-192 | **Total KEM** | **344,111,826** | **7,448,826** | **×46.2** |
| HQC-256 | keypair | 140,633,544 | 2,062,593 | ×68.2 |
| HQC-256 | encaps | 281,227,941 | 4,086,039 | ×68.8 |
| HQC-256 | decaps | 423,881,598 | 8,168,745 | ×51.9 |
| HQC-256 | **Total KEM** | **845,743,083** | **14,317,377** | **×59.1** |

## Table 2 — Tier 2 (Edge RVV)

> rv64gcv + Zbc/Zbb; AP-2 fixed-tree polymul, scalar Keccak

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | vector_rvv (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,639,752 | 140,487 | ×132.7 |
| HQC-192 | 56,702,057 | 439,516 | ×129.0 |
| HQC-256 | 139,877,019 | 1,014,499 | ×137.9 |

### KEM operations

| Level | Op | scalar_ref (instret) | vector_rvv (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,886,091 | 286,509 | ×65.9 |
| HQC-128 | encaps | 37,748,533 | 573,137 | ×65.9 |
| HQC-128 | decaps | 57,361,651 | 1,115,379 | ×51.4 |
| HQC-128 | **Total KEM** | **113,996,275** | **1,975,025** | **×57.7** |
| HQC-192 | keypair | 57,173,317 | 700,239 | ×81.6 |
| HQC-192 | encaps | 114,325,122 | 1,414,784 | ×80.8 |
| HQC-192 | decaps | 172,613,387 | 2,548,885 | ×67.7 |
| HQC-192 | **Total KEM** | **344,111,826** | **4,663,908** | **×73.8** |
| HQC-256 | keypair | 140,633,544 | 1,432,597 | ×98.2 |
| HQC-256 | encaps | 281,227,941 | 2,890,400 | ×97.3 |
| HQC-256 | decaps | 423,881,598 | 5,186,418 | ×81.7 |
| HQC-256 | **Total KEM** | **845,743,083** | **9,509,415** | **×88.9** |

## Table 3 — Tier 3 (High-End Zvbc)

> rv64gcv + Zvbc/Zvbb/Zvl256b

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 18,639,752 | 63,804 | ×292.1 |
| HQC-192 | 56,702,057 | 189,388 | ×299.4 |
| HQC-256 | 139,877,019 | 272,896 | ×512.6 |

### KEM operations

| Level | Op | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 18,886,091 | 209,826 | ×90.0 |
| HQC-128 | encaps | 37,748,533 | 419,771 | ×89.9 |
| HQC-128 | decaps | 57,361,651 | 824,660 | ×69.6 |
| HQC-128 | **Total KEM** | **113,996,275** | **1,454,257** | **×78.4** |
| HQC-192 | keypair | 57,173,317 | 450,111 | ×127.0 |
| HQC-192 | encaps | 114,325,122 | 914,528 | ×125.0 |
| HQC-192 | decaps | 172,613,387 | 1,719,604 | ×100.4 |
| HQC-192 | **Total KEM** | **344,111,826** | **3,084,243** | **×111.6** |
| HQC-256 | keypair | 140,633,544 | 690,994 | ×203.5 |
| HQC-256 | encaps | 281,227,941 | 1,407,194 | ×199.9 |
| HQC-256 | decaps | 423,881,598 | 2,764,123 | ×153.4 |
| HQC-256 | **Total KEM** | **845,743,083** | **4,862,311** | **×173.9** |

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

