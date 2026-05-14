# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-14 23:22:42

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 156770 | 2874 |
| 192 | vect_mul | 481781 | 2906 |
| 256 | vect_mul | 670758 | 2896 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 297046 | 38692 |  |
| 128 | encaps | 619802 | 38692 |  |
| 128 | decaps | 1407759 | 38692 |  |
| 192 | keypair | 734905 | 39724 |  |
| 192 | encaps | 1545422 | 39724 |  |
| 192 | decaps | 3057592 | 39724 |  |
| 256 | keypair | 1070511 | 46722 |  |
| 256 | encaps | 2262259 | 46722 |  |
| 256 | decaps | 4753163 | 46722 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1301 | 94 | 1207 | 124 | 14292061 | 8478277 | 2087059 | 2382878 |
| 192 | 3058 | 309 | 2749 | 777 | 34374456 | 23906239 | 4793145 | 6666504 |
| 256 | 6965 | 1090 | 5875 | 3292 | 65132951 | 47842166 | 8967121 | 11865642 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1317 | 1330 | 9116 | 12346 | 143 | 193 |
| 192 | 1426 | 1333 | 9551 | 13349 | 150 | 209 |
| 256 | 1432 | 1313 | 9611 | 13252 | 151 | 208 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset | 1987257 | 41.90 |  |
| KeccakF1600_StatePermute | 578792 | 12.20 |  |
| vclmul_leaf | 404988 | 8.54 |  |
| karatsuba_vclmul | 365166 | 7.70 |  |
| hadamard | 227516 | 4.80 |  |
| keccak_inc_absorb | 141116 | 2.98 |  |
| memcpy | 137922 | 2.91 |  |
| find_peaks | 105919 | 2.23 |  |
| gf_reduce | 105600 | 2.23 |  |
| keccak_inc_squeeze | 96430 | 2.03 |  |
| expand_and_sum | 93748 | 1.98 |  |
| compare_u32 | 83250 | 1.76 |  |
| vect_generate_random_support2 | 79260 | 1.67 |  |
| vxor_words | 74250 | 1.57 |  |
| vect_generate_random_support1 | 49869 | 1.05 |  |
| vect_compare | 31058 | 0.65 |  |
| gf_mul | 23794 | 0.50 |  |
| reduce | 19974 | 0.42 |  |
| vect_add | 17744 | 0.37 |  |
| compute_syndromes | 13895 | 0.29 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 1323318 | 27.90 |  |
| memset at mem.c:41 | 659379 | 13.90 |  |
| KeccakF1600_StatePermute at fips202.c:? | 98548 | 2.08 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 1.81 |  |
| compare_u32 at vector.c:43 | 83250 | 1.76 |  |
| memcpy at mem.c:18 | 82652 | 1.74 |  |
| hadamard at reed_muller.c:98 | 82432 | 1.74 |  |
| hadamard at reed_muller.c:99 | 82432 | 1.74 |  |
| karatsuba_vclmul at gf2x_vclmul.c:223 | 70332 | 1.48 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.44 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.32 |  |
| karatsuba_vclmul at gf2x_vclmul.c:225 | 56190 | 1.18 |  |
| karatsuba_vclmul at gf2x_vclmul.c:224 | 55788 | 1.18 |  |
| memcpy at mem.c:19 | 54764 | 1.15 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.09 |  |
| gf_reduce at gf.c:106 | 44712 | 0.94 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 0.91 |  |
| karatsuba_vclmul at gf2x_vclmul.c:198 | 41496 | 0.87 |  |
| find_peaks at reed_muller.c:155 | 35328 | 0.74 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 0.73 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 977171 | 17.59 |  |
| bne | 757314 | 13.63 |  |
| sb | 697377 | 12.55 |  |
| ld | 603126 | 10.86 |  |
| bnez | 409843 | 7.38 |  |
| xor | 356350 | 6.41 |  |
| add | 210180 | 3.78 |  |
| sd | 130235 | 2.34 |  |
| lh | 110766 | 1.99 |  |
| slli | 99065 | 1.78 |  |
| rori | 98832 | 1.78 |  |
| andn | 85375 | 1.54 |  |
| vle64.v | 82086 | 1.48 |  |
| mv | 71474 | 1.29 |  |
| vxor.vv | 64501 | 1.16 |  |
| sh | 62575 | 1.13 |  |
| vse64.v | 58012 | 1.04 |  |
| subw | 54766 | 0.99 |  |
| lbu | 49846 | 0.90 |  |
| or | 49069 | 0.88 |  |

