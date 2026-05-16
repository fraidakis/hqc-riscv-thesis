# Full Analysis Report

- Variant: karatsuba_zbc_exp
- Level: 128
- Date: 2026-05-14 16:39:27

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 296703 | 3104 |
| 192 | vect_mul | 905153 | 3136 |
| 256 | vect_mul | 1956574 | 3126 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 436979 | 38914 |  |
| 128 | encaps | 946788 | 38914 |  |
| 128 | decaps | 2259112 | 38914 |  |
| 192 | keypair | 1158277 | 39946 |  |
| 192 | encaps | 2467406 | 39946 |  |
| 192 | decaps | 4891399 | 39946 |  |
| 256 | keypair | 2356327 | 46944 |  |
| 256 | encaps | 5013251 | 46944 |  |
| 256 | decaps | 10048206 | 46944 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1310 | 95 | 1215 | 132 | 13804733 | 6926149 | 2026143 | 1209590 |
| 192 | 3017 | 317 | 2700 | 732 | 32079488 | 18418063 | 4506274 | 2919354 |
| 256 | 6830 | 1088 | 5742 | 3167 | 61906223 | 36591494 | 8563780 | 5556732 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1378 | 1400 | 7917 | 12003 | 124 | 188 |
| 192 | 1479 | 1397 | 8295 | 12965 | 130 | 203 |
| 256 | 1467 | 1371 | 8211 | 12834 | 129 | 201 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf | 1078674 | 22.07 |  |
| karatsuba_clmul | 869328 | 17.79 |  |
| KeccakF1600_StatePermute | 578792 | 11.84 |  |
| gf_carryless_mul | 429828 | 8.80 |  |
| memset | 299769 | 6.13 |  |
| clmul64 | 251532 | 5.15 |  |
| hadamard | 227516 | 4.66 |  |
| keccak_inc_absorb | 141116 | 2.89 |  |
| memcpy | 137922 | 2.82 |  |
| find_peaks | 105919 | 2.17 |  |
| keccak_inc_squeeze | 96430 | 1.97 |  |
| expand_and_sum | 93748 | 1.92 |  |
| gf_reduce | 90606 | 1.85 |  |
| gf_mul | 84966 | 1.74 |  |
| compare_u32 | 83250 | 1.70 |  |
| vect_generate_random_support2 | 79260 | 1.62 |  |
| vect_generate_random_support1 | 49869 | 1.02 |  |
| vect_compare | 31058 | 0.64 |  |
| reduce | 19974 | 0.41 |  |
| vect_add | 17744 | 0.36 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf at gf2x_clmul.c:134 | 309498 | 6.33 |  |
| clmul64 at gf2x_clmul.c:36 | 228648 | 4.68 |  |
| gf_carryless_mul at gf.c:132 | 209916 | 4.30 |  |
| memset at mem.c:40 | 199782 | 4.09 |  |
| clmul_leaf at gf2x_clmul.c:131 | 176856 | 3.62 |  |
| karatsuba_clmul at gf2x_clmul.c:217 | 157332 | 3.22 |  |
| karatsuba_clmul at gf2x_clmul.c:215 | 134856 | 2.76 |  |
| clmul_leaf at gf2x_clmul.c:136 | 132642 | 2.71 |  |
| karatsuba_clmul at gf2x_clmul.c:212 | 115284 | 2.36 |  |
| karatsuba_clmul at gf2x_clmul.c:209 | 111264 | 2.28 |  |
| clmul_leaf at gf2x_clmul.c:0 | 102408 | 2.10 |  |
| memset at mem.c:41 | 99795 | 2.04 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 2.02 |  |
| clmul_leaf at gf2x_clmul.c:135 | 88428 | 1.81 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 1.75 |  |
| compare_u32 at vector.c:43 | 83250 | 1.70 |  |
| memcpy at mem.c:18 | 82652 | 1.69 |  |
| hadamard at reed_muller.c:98 | 82432 | 1.69 |  |
| hadamard at reed_muller.c:99 | 82432 | 1.69 |  |
| karatsuba_clmul at gf2x_clmul.c:198 | 70332 | 1.44 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 945981 | 16.60 |  |
| ld | 925958 | 16.24 |  |
| xor | 618166 | 10.84 |  |
| bnez | 409185 | 7.18 |  |
| sd | 310019 | 5.44 |  |
| bne | 250530 | 4.40 |  |
| add | 146994 | 2.58 |  |
| sb | 137793 | 2.42 |  |
| slli | 127583 | 2.24 |  |
| clmul | 114324 | 2.01 |  |
| clmulh | 114324 | 2.01 |  |
| lh | 110766 | 1.94 |  |
| bltu | 108974 | 1.91 |  |
| rori | 98832 | 1.73 |  |
| and | 92866 | 1.63 |  |
| mv | 89988 | 1.58 |  |
| vsetivli | 89171 | 1.56 |  |
| andn | 85375 | 1.50 |  |
| srli | 63598 | 1.12 |  |
| sh | 62575 | 1.10 |  |

