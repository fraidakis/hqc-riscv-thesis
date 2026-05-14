# Full Analysis Report

- Variant: karatsuba_zbc_exp
- Level: 128
- Date: 2026-05-14 19:21:20

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 285528 | 3744 |
| 192 | vect_mul | 869373 | 3776 |
| 256 | vect_mul | 1897961 | 3766 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 425804 | 39562 |  |
| 128 | encaps | 877318 | 39562 |  |
| 128 | decaps | 1794033 | 39562 |  |
| 192 | keypair | 1122497 | 40594 |  |
| 192 | encaps | 2320606 | 40594 |  |
| 192 | decaps | 4220368 | 40594 |  |
| 256 | keypair | 2297714 | 47592 |  |
| 256 | encaps | 4716665 | 47592 |  |
| 256 | decaps | 8434772 | 47592 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1286 | 82 | 1204 | 109 | 13372253 | 6521893 | 1972083 | 1159058 |
| 192 | 3008 | 294 | 2714 | 726 | 31547880 | 17878399 | 4439823 | 2851896 |
| 256 | 7007 | 1150 | 5857 | 3314 | 60557335 | 35227574 | 8395169 | 5386242 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1366 | 1358 | 8437 | 11947 | 132 | 187 |
| 192 | 1474 | 1358 | 8871 | 12933 | 139 | 203 |
| 256 | 1465 | 1333 | 8811 | 12810 | 138 | 201 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf | 1078674 | 26.58 |  |
| KeccakF1600_StatePermute | 578792 | 14.26 |  |
| karatsuba_clmul | 519030 | 12.79 |  |
| memset | 299769 | 7.39 |  |
| clmul64 | 251532 | 6.20 |  |
| hadamard | 227516 | 5.61 |  |
| keccak_inc_absorb | 141116 | 3.48 |  |
| memcpy | 137922 | 3.40 |  |
| find_peaks | 105919 | 2.61 |  |
| gf_reduce | 105600 | 2.60 |  |
| keccak_inc_squeeze | 96430 | 2.38 |  |
| expand_and_sum | 93748 | 2.31 |  |
| compare_u32 | 83250 | 2.05 |  |
| vect_generate_random_support2 | 79260 | 1.95 |  |
| vect_generate_random_support1 | 49869 | 1.23 |  |
| vect_compare | 31058 | 0.77 |  |
| gf_mul | 23794 | 0.59 |  |
| reduce | 19974 | 0.49 |  |
| vect_add | 17744 | 0.44 |  |
| compute_syndromes | 13895 | 0.34 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf at gf2x_clmul.c:118 | 309498 | 7.63 |  |
| clmul64 at gf2x_clmul.c:36 | 228648 | 5.63 |  |
| memset at mem.c:40 | 199782 | 4.92 |  |
| clmul_leaf at gf2x_clmul.c:115 | 176856 | 4.36 |  |
| clmul_leaf at gf2x_clmul.c:120 | 132642 | 3.27 |  |
| clmul_leaf at gf2x_clmul.c:0 | 102408 | 2.52 |  |
| memset at mem.c:41 | 99795 | 2.46 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 2.43 |  |
| clmul_leaf at gf2x_clmul.c:119 | 88428 | 2.18 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.11 |  |
| compare_u32 at vector.c:43 | 83250 | 2.05 |  |
| memcpy at mem.c:18 | 82652 | 2.04 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.03 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.03 |  |
| karatsuba_clmul at gf2x_clmul.c:182 | 70332 | 1.73 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.69 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.54 |  |
| karatsuba_clmul at gf2x_clmul.c:184 | 56190 | 1.38 |  |
| karatsuba_clmul at gf2x_clmul.c:183 | 55788 | 1.37 |  |
| memcpy at mem.c:19 | 54764 | 1.35 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 775186 | 15.92 |  |
| addi | 694091 | 14.25 |  |
| xor | 475186 | 9.76 |  |
| bnez | 409607 | 8.41 |  |
| sd | 221969 | 4.56 |  |
| bne | 208482 | 4.28 |  |
| add | 164040 | 3.37 |  |
| sb | 137793 | 2.83 |  |
| clmul | 119322 | 2.45 |  |
| clmulh | 114324 | 2.35 |  |
| lh | 110766 | 2.27 |  |
| rori | 98832 | 2.03 |  |
| slli | 94841 | 1.95 |  |
| mv | 94322 | 1.94 |  |
| vsetivli | 89897 | 1.85 |  |
| bltu | 87302 | 1.79 |  |
| andn | 85375 | 1.75 |  |
| vle64.v | 75462 | 1.55 |  |
| sh | 62575 | 1.28 |  |
| vse64.v | 60052 | 1.23 |  |

