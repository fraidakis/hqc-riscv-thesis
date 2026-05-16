# Full Analysis Report

- Variant: karatsuba_zbc
- Level: 128
- Date: 2026-05-16 17:09:08

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 283686 | 3784 |
| 192 | vect_mul | 863483 | 3816 |
| 256 | vect_mul | 1904461 | 3806 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 425023 | 39602 |  |
| 128 | encaps | 874141 | 39602 |  |
| 128 | decaps | 1782060 | 39602 |  |
| 192 | keypair | 1115907 | 40634 |  |
| 192 | encaps | 2302888 | 40634 |  |
| 192 | decaps | 4189093 | 40634 |  |
| 256 | keypair | 2301459 | 47632 |  |
| 256 | encaps | 4716809 | 47632 |  |
| 256 | decaps | 8429796 | 47632 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1286 | 82 | 1204 | 109 | 13389789 | 6525701 | 1977551 | 1005450 |
| 192 | 3017 | 317 | 2700 | 732 | 31545520 | 17884479 | 4444736 | 2537306 |
| 256 | 6943 | 1113 | 5830 | 3298 | 60426729 | 35227520 | 8378838 | 4896482 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1356 | 1354 | 8427 | 12097 | 132 | 190 |
| 192 | 1570 | 1474 | 9695 | 14057 | 152 | 220 |
| 256 | 1553 | 1431 | 9585 | 13734 | 150 | 215 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf | 1078674 | 28.45 |  |
| KeccakF1600_StatePermute | 578792 | 15.27 |  |
| karatsuba_clmul | 519030 | 13.69 |  |
| clmul64 | 251532 | 6.64 |  |
| hadamard | 227516 | 6.00 |  |
| keccak_inc_absorb | 141116 | 3.72 |  |
| memcpy | 137922 | 3.64 |  |
| find_peaks | 105919 | 2.79 |  |
| gf_reduce | 105600 | 2.79 |  |
| keccak_inc_squeeze | 96430 | 2.54 |  |
| expand_and_sum | 93748 | 2.47 |  |
| compare_u32 | 83250 | 2.20 |  |
| vect_generate_random_support2 | 79260 | 2.09 |  |
| vect_generate_random_support1 | 49869 | 1.32 |  |
| memset | 33045 | 0.87 |  |
| vect_compare | 31058 | 0.82 |  |
| gf_mul | 23794 | 0.63 |  |
| reduce | 19974 | 0.53 |  |
| vect_add | 17744 | 0.47 |  |
| compute_syndromes | 13895 | 0.37 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf at gf2x_clmul.c:119 | 309498 | 8.16 |  |
| clmul64 at gf2x_clmul.c:37 | 228648 | 6.03 |  |
| clmul_leaf at gf2x_clmul.c:116 | 176856 | 4.67 |  |
| clmul_leaf at gf2x_clmul.c:121 | 132642 | 3.50 |  |
| clmul_leaf at gf2x_clmul.c:0 | 102408 | 2.70 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 2.60 |  |
| clmul_leaf at gf2x_clmul.c:120 | 88428 | 2.33 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.26 |  |
| compare_u32 at vector.c:43 | 83250 | 2.20 |  |
| memcpy at mem.c:21 | 82652 | 2.18 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.17 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.17 |  |
| karatsuba_clmul at gf2x_clmul.c:183 | 70332 | 1.86 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.81 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.65 |  |
| karatsuba_clmul at gf2x_clmul.c:185 | 56190 | 1.48 |  |
| karatsuba_clmul at gf2x_clmul.c:184 | 55788 | 1.47 |  |
| memcpy at mem.c:22 | 54764 | 1.44 |  |
| karatsuba_clmul at gf2x_clmul.c:228 | 51840 | 1.37 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.36 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 776048 | 16.85 |  |
| addi | 604139 | 13.12 |  |
| xor | 475186 | 10.32 |  |
| bnez | 411043 | 8.93 |  |
| sd | 221969 | 4.82 |  |
| add | 164348 | 3.57 |  |
| clmul | 119322 | 2.59 |  |
| bne | 118530 | 2.57 |  |
| clmulh | 114324 | 2.48 |  |
| lh | 110766 | 2.41 |  |
| rori | 98832 | 2.15 |  |
| slli | 95223 | 2.07 |  |
| mv | 94322 | 2.05 |  |
| vsetivli | 89897 | 1.95 |  |
| bltu | 87302 | 1.90 |  |
| andn | 85375 | 1.85 |  |
| vle64.v | 75462 | 1.64 |  |
| sh | 62575 | 1.36 |  |
| vse64.v | 60434 | 1.31 |  |
| vxor.vv | 59863 | 1.30 |  |

