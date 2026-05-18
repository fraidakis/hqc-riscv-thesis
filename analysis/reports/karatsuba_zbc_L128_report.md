# Full Analysis Report

- Variant: karatsuba_zbc
- Level: 128
- Date: 2026-05-17 11:46:57

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 158928 | 11858 |
| 192 | vect_mul | 483283 | 13528 |
| 256 | vect_mul | 1614035 | 13808 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 300265 | 47676 |  |
| 128 | encaps | 624625 | 47676 |  |
| 128 | decaps | 1407786 | 47676 |  |
| 192 | keypair | 735707 | 50346 |  |
| 192 | encaps | 1542488 | 50346 |  |
| 192 | decaps | 3048493 | 50346 |  |
| 256 | keypair | 2011033 | 57634 |  |
| 256 | encaps | 4135957 | 57634 |  |
| 256 | decaps | 7558518 | 57634 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1292 | 88 | 1204 | 110 | 11160103 | 5848079 | 1822483 | 1044390 |
| 192 | 3075 | 295 | 2780 | 771 | 24832762 | 15687369 | 3900644 | 2557670 |
| 256 | 6960 | 1077 | 5883 | 3296 | 60099657 | 32902784 | 9069762 | 5337698 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 2457 | 2332 | 49099 | 40809 | 768 | 638 |
| 192 | 2821 | 2602 | 61423 | 52425 | 960 | 820 |
| 256 | 2687 | 2364 | 61609 | 44998 | 963 | 704 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 19.47
 |  |
| clmul_leaf | 477096 | 16.05
 |  |
| clmul64 | 228648 | 7.69
 |  |
| hadamard | 227516 | 7.65
 |  |
| keccak_inc_absorb | 141116 | 4.75
 |  |
| memcpy | 137922 | 4.64
 |  |
| find_peaks | 105919 | 3.56
 |  |
| gf_reduce | 105600 | 3.55
 |  |
| keccak_inc_squeeze | 96430 | 3.24
 |  |
| expand_and_sum | 93748 | 3.15
 |  |
| compare_u32 | 83250 | 2.80
 |  |
| vect_generate_random_support2 | 79260 | 2.67
 |  |
| clmul_leaf_n8 | 70818 | 2.38
 |  |
| vect_generate_random_support1 | 49869 | 1.68
 |  |
| kmul_35 | 49236 | 1.66
 |  |
| kmul_18 | 48576 | 1.63
 |  |
| kmul_17 | 39960 | 1.34
 |  |
| memset | 33045 | 1.11
 |  |
| vect_compare | 31058 | 1.04
 |  |
| kmul_69 | 29520 | 0.99
 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:36 | 228648 | 7.69
 |  |
| KeccakF1600_StatePermute at fips202.c:? | 98548 | 3.32
 |  |
| clmul_leaf at gf2x_clmul.c:371 | 88992 | 2.99
 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.88
 |  |
| compare_u32 at vector.c:43 | 83250 | 2.80
 |  |
| memcpy at mem.c:21 | 82652 | 2.78
 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.77
 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.77
 |  |
| clmul_leaf at gf2x_clmul.c:372 | 79104 | 2.66
 |  |
| clmul_leaf at gf2x_clmul.c:373 | 79104 | 2.66
 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 2.30
 |  |
| hadamard at reed_muller.c:97 | 62606 | 2.11
 |  |
| memcpy at mem.c:22 | 54764 | 1.84
 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.74
 |  |
| clmul_leaf at gf2x_clmul.c:370 | 49440 | 1.66
 |  |
| gf_reduce at gf.c:106 | 44712 | 1.50
 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 1.45
 |  |
| clmul_leaf at gf2x_clmul.c:374 | 39552 | 1.33
 |  |
| find_peaks at reed_muller.c:155 | 35328 | 1.19
 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 1.16
 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 684588 | 18.22 |  |
| xor | 541132 | 14.40 |  |
| bnez | 394331 | 10.49 |  |
| addi | 307943 | 8.20 |  |
| sd | 238271 | 6.34 |  |
| bne | 121386 | 3.23 |  |
| clmul | 119322 | 3.18 |  |
| clmulh | 114324 | 3.04 |  |
| lh | 110766 | 2.95 |  |
| rori | 98832 | 2.63 |  |
| andn | 85375 | 2.27 |  |
| add | 82136 | 2.19 |  |
| slli | 72651 | 1.93 |  |
| sh | 62575 | 1.67 |  |
| mv | 60812 | 1.62 |  |
| subw | 54754 | 1.46 |  |
| lbu | 49840 | 1.33 |  |
| or | 49165 | 1.31 |  |
| sb | 47835 | 1.27 |  |
| vle64.v | 38706 | 1.03 |  |

