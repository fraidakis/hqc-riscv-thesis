# Full Analysis Report

- Variant: karatsuba_rvv
- Level: 128
- Date: 2026-05-20 23:32:18

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 157377 | 10442 |
| 192 | vect_mul | 481065 | 11262 |
| 256 | vect_mul | 1608062 | 9746 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 340875 | 50826 |  |
| 128 | encaps | 694007 | 50826 |  |
| 128 | decaps | 1278629 | 50826 |  |
| 128 | total | 2313511 | 50826 |  |
| 192 | keypair | 813216 | 52884 |  |
| 192 | encaps | 1679456 | 52884 |  |
| 192 | decaps | 2917845 | 52884 |  |
| 192 | total | 5410517 | 52884 |  |
| 256 | keypair | 2136500 | 58174 |  |
| 256 | encaps | 4341476 | 58174 |  |
| 256 | decaps | 7256735 | 58174 |  |
| 256 | total | 13734711 | 58174 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1337 | 124 | 1213 | 162 | 11077871 | 5623823 | 1688548 | 1107974 |
| 192 | 3046 | 266 | 2780 | 758 | 24669050 | 15075065 | 3585160 | 2877996 |
| 256 | 6665 | 995 | 5670 | 3028 | 59103997 | 31777712 | 8219455 | 5658248 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 2176 | 2103 | 34504 | 26764 | 540 | 419 |
| 192 | 2451 | 2339 | 31750 | 27265 | 497 | 427 |
| 256 | 2415 | 2331 | 30114 | 25896 | 471 | 405 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 918882 | 39.43 |  |
| clmul_leaf | 477096 | 20.47 |  |
| clmul64 | 228648 | 9.81 |  |
| keccak_inc_absorb | 140218 | 6.02 |  |
| keccak_inc_squeeze | 96560 | 4.14 |  |
| gf_reduce | 80541 | 3.46 |  |
| clmul_leaf_n8 | 70596 | 3.03 |  |
| rvv_xor3_u64 | 54282 | 2.33 |  |
| rvv_copy_u64 | 30264 | 1.30 |  |
| kmul_18 | 23496 | 1.01 |  |
| kmul_17 | 22200 | 0.95 |  |
| gf_mul | 16952 | 0.73 |  |
| vect_add | 15978 | 0.69 |  |
| kmul_35 | 14094 | 0.60 |  |
| compute_elp | 9934 | 0.43 |  |
| support_contains_value | 9579 | 0.41 |  |
| fft_rec | 8738 | 0.37 |  |
| compute_error_values | 8089 | 0.35 |  |
| reed_solomon_encode | 8058 | 0.35 |  |
| memset | 6856 | 0.29 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:42 | 228648 | 9.81 |  |
| KeccakF1600_StatePermute at fips202_rvv_only.c:0 | 136746 | 5.87 |  |
| clmul_leaf at gf2x_clmul.c:447 | 88992 | 3.82 |  |
| keccak_inc_absorb at fips202_rvv_only.c:290 | 85750 | 3.68 |  |
| clmul_leaf at gf2x_clmul.c:448 | 79104 | 3.39 |  |
| clmul_leaf at gf2x_clmul.c:449 | 79104 | 3.39 |  |
| keccak_inc_squeeze at fips202_rvv_only.c:320 | 51696 | 2.22 |  |
| clmul_leaf at gf2x_clmul.c:446 | 49440 | 2.12 |  |
| keccak_inc_absorb at fips202_rvv_only.c:289 | 43067 | 1.85 |  |
| clmul_leaf at gf2x_clmul.c:450 | 39552 | 1.70 |  |
| gf_reduce at gf.c:79 | 38151 | 1.64 |  |
| keccak_inc_squeeze at fips202_rvv_only.c:319 | 34536 | 1.48 |  |
| gf_reduce at gf.c:77 | 29673 | 1.27 |  |
| KeccakF1600_StatePermute at fips202_rvv_only.c:89 | 23856 | 1.02 |  |
| clmul_leaf at gf2x_clmul.c:435 | 22248 | 0.95 |  |
| KeccakF1600_StatePermute at fips202_rvv_only.c:154 | 22152 | 0.95 |  |
| clmul_leaf at gf2x_clmul.c:473 | 19776 | 0.85 |  |
| clmul_leaf at gf2x_clmul.c:474 | 19776 | 0.85 |  |
| KeccakF1600_StatePermute at fips202_rvv_only.c:88 | 15336 | 0.66 |  |
| KeccakF1600_StatePermute at fips202_rvv_only.c:161 | 15336 | 0.66 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 677938 | 21.76 |  |
| xor | 500430 | 16.06 |  |
| bnez | 402327 | 12.92 |  |
| sd | 240306 | 7.71 |  |
| slli | 151008 | 4.85 |  |
| clmul | 118563 | 3.81 |  |
| clmulh | 114324 | 3.67 |  |
| srli | 109650 | 3.52 |  |
| addi | 102634 | 3.29 |  |
| or | 100266 | 3.22 |  |
| and | 95869 | 3.08 |  |
| not | 85200 | 2.74 |  |
| add | 70730 | 2.27 |  |
| mv | 35768 | 1.15 |  |
| vle64.v | 29940 | 0.96 |  |
| andi | 28735 | 0.92 |  |
| bne | 28397 | 0.91 |  |
| vxor.vv | 21787 | 0.70 |  |
| sub | 18829 | 0.60 |  |
| vse64.v | 17125 | 0.55 |  |

