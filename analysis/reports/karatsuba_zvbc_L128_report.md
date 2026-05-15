# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-15 19:46:26

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 90302 | 2924 |
| 192 | vect_mul | 273547 | 2956 |
| 256 | vect_mul | 399347 | 2946 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 231639 | 38742 |  |
| 128 | encaps | 487373 | 38742 |  |
| 128 | decaps | 1201908 | 38742 |  |
| 192 | keypair | 525971 | 39774 |  |
| 192 | encaps | 1123016 | 39774 |  |
| 192 | decaps | 2419285 | 39774 |  |
| 256 | keypair | 796345 | 46772 |  |
| 256 | encaps | 1706581 | 46772 |  |
| 256 | decaps | 3914454 | 46772 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1296 | 90 | 1206 | 119 | 13332359 | 7557647 | 1970367 | 1134438 |
| 192 | 3036 | 310 | 2726 | 745 | 31528010 | 21002505 | 4442542 | 2927054 |
| 256 | 6989 | 1128 | 5861 | 3275 | 60342953 | 43200896 | 8368366 | 5893154 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1319 | 1331 | 9083 | 12697 | 142 | 199 |
| 192 | 1546 | 1473 | 10455 | 14833 | 164 | 232 |
| 256 | 1536 | 1434 | 10401 | 14542 | 163 | 228 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 22.32 |  |
| karatsuba_vclmul | 309024 | 11.92 |  |
| vclmul_leaf | 254580 | 9.82 |  |
| hadamard | 227516 | 8.77 |  |
| keccak_inc_absorb | 141116 | 5.44 |  |
| memcpy | 137922 | 5.32 |  |
| find_peaks | 105919 | 4.08 |  |
| gf_reduce | 105600 | 4.07 |  |
| keccak_inc_squeeze | 96430 | 3.72 |  |
| expand_and_sum | 93748 | 3.61 |  |
| compare_u32 | 83250 | 3.21 |  |
| vect_generate_random_support2 | 79260 | 3.06 |  |
| rvv_xor3_u64 | 54282 | 2.09 |  |
| vect_generate_random_support1 | 49869 | 1.92 |  |
| rvv_copy_u64 | 33822 | 1.30 |  |
| memset | 33045 | 1.27 |  |
| vect_compare | 31058 | 1.20 |  |
| gf_mul | 23794 | 0.92 |  |
| reduce | 19974 | 0.77 |  |
| vect_add | 17744 | 0.68 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 3.80 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 3.31 |  |
| compare_u32 at vector.c:43 | 83250 | 3.21 |  |
| memcpy at mem.c:21 | 82652 | 3.19 |  |
| hadamard at reed_muller.c:98 | 82432 | 3.18 |  |
| hadamard at reed_muller.c:99 | 82432 | 3.18 |  |
| karatsuba_vclmul at gf2x_vclmul_c3.c:286 | 70332 | 2.71 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 2.64 |  |
| hadamard at reed_muller.c:97 | 62606 | 2.41 |  |
| karatsuba_vclmul at gf2x_vclmul_c3.c:288 | 56190 | 2.17 |  |
| karatsuba_vclmul at gf2x_vclmul_c3.c:287 | 55788 | 2.15 |  |
| memcpy at mem.c:22 | 54764 | 2.11 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.99 |  |
| gf_reduce at gf.c:106 | 44712 | 1.72 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 1.66 |  |
| karatsuba_vclmul at gf2x_vclmul_c3.c:261 | 41496 | 1.60 |  |
| find_peaks at reed_muller.c:155 | 35328 | 1.36 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 1.33 |  |
| vect_generate_random_support2 at vector.c:136 | 34188 | 1.32 |  |
| gf_reduce at gf.c:105 | 33534 | 1.29 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 585908 | 17.36 |  |
| bnez | 401161 | 11.89 |  |
| xor | 356350 | 10.56 |  |
| addi | 312119 | 9.25 |  |
| add | 175850 | 5.21 |  |
| sd | 129503 | 3.84 |  |
| lh | 110766 | 3.28 |  |
| bne | 107208 | 3.18 |  |
| rori | 98832 | 2.93 |  |
| slli | 95721 | 2.84 |  |
| andn | 85375 | 2.53 |  |
| mv | 72686 | 2.15 |  |
| sh | 62575 | 1.85 |  |
| subw | 54754 | 1.62 |  |
| lbu | 49840 | 1.48 |  |
| or | 49165 | 1.46 |  |
| sb | 47835 | 1.42 |  |
| vle64.v | 46404 | 1.37 |  |
| andi | 36377 | 1.08 |  |
| vse64.v | 35024 | 1.04 |  |

