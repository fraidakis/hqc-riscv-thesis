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
| HQC-128 | keypair | 19,454,626 | 286,255 | ×68.0 |
| HQC-128 | encaps | 38,875,925 | 586,400 | ×66.3 |
| HQC-128 | decaps | 59,013,743 | 1,084,132 | ×54.4 |
| HQC-128 | **Total KEM** | **117,344,294** | **1,956,787** | **×60.0** |
| HQC-192 | keypair | 58,895,392 | 704,882 | ×83.6 |
| HQC-192 | encaps | 117,744,966 | 1,455,147 | ×80.9 |
| HQC-192 | decaps | 177,662,495 | 2,515,258 | ×70.6 |
| HQC-192 | **Total KEM** | **354,302,853** | **4,675,287** | **×75.8** |
| HQC-256 | keypair | 143,382,635 | 1,869,816 | ×76.7 |
| HQC-256 | encaps | 286,701,285 | 3,809,926 | ×75.3 |
| HQC-256 | decaps | 431,890,526 | 6,341,725 | ×68.1 |
| HQC-256 | **Total KEM** | **861,974,446** | **12,021,467** | **×71.7** |

## Table 3 — Tier 3 (High-End Zvbc)

> rv64gcv + Zvbc/Zvbb/Zvl256b

### `vect_mul` (polynomial multiply)

| Level | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|----------------------|------------------------|--------|
| HQC-128 | 19,214,277 | 62,530 | ×307.3 |
| HQC-192 | 58,433,896 | 185,120 | ×315.7 |
| HQC-256 | 142,641,043 | 271,006 | ×526.3 |

### KEM operations

| Level | Op | scalar_ref (instret) | vector_zvk (instret) | Speedup |
|-------|-----|----------------------|------------------------|--------|
| HQC-128 | keypair | 19,454,626 | 105,279 | ×184.8 |
| HQC-128 | encaps | 38,875,925 | 217,479 | ×178.8 |
| HQC-128 | decaps | 59,013,743 | 393,086 | ×150.1 |
| HQC-128 | **Total KEM** | **117,344,294** | **715,844** | **×163.9** |
| HQC-192 | keypair | 58,895,392 | 258,251 | ×228.1 |
| HQC-192 | encaps | 117,744,966 | 534,634 | ×220.2 |
| HQC-192 | decaps | 177,662,495 | 900,120 | ×197.4 |
| HQC-192 | **Total KEM** | **354,302,853** | **1,693,005** | **×209.3** |
| HQC-256 | keypair | 143,382,635 | 383,592 | ×373.8 |
| HQC-256 | encaps | 286,701,285 | 816,033 | ×351.3 |
| HQC-256 | decaps | 431,890,526 | 1,407,364 | ×306.9 |
| HQC-256 | **Total KEM** | **861,974,446** | **2,606,989** | **×330.6** |

## Table 4 — Keccak Speedups

### `keccak`

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 463,069 | 131,944 | ×3.5 |
| HQC-192 | 902,926 | 249,581 | ×3.6 |
| HQC-256 | 1,405,641 | 383,277 | ×3.7 |

## Table 5 — Decode Speedups

### `decode`

<!-- Source: sim_decode_ref_latest.csv (commit ccbe5e7) + sim_decode_zvbc_latest.csv (commit b709a5f) -->
<!-- Speedup = reference instret / zvbc instret -->

| Level | reference (instret) | optimized (instret) | Speedup |
|-------|----------------------|---------------------|--------|
| HQC-128 | 384,986 | 56,515 | ×6.8 |
| HQC-192 | 562,125 | 72,237 | ×7.8 |
| HQC-256 | 1,059,605 | 147,077 | ×7.2 |

> **Note**: Earlier runs with `isolated_decode_zvbc_2026-05-26_01-25.csv` (commit 95eaf30)
> reported 60,353 / 77,114 / 159,206 — those were produced by a pre-refactor isolated harness
> before the `bench_decode.c` rewrite in commit b709a5f (2026-05-31). The canonical values
> above come from the unified `sim_decode_zvbc_latest.csv` harness. The decode-instret-table
> in ch7 and all prose speedup ranges now reference these values.

