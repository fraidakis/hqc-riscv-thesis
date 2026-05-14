# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 192
- Date: 2026-05-14 23:24:34

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
| memset | 5854566 | 49.14 |  |
| vclmul_leaf | 1255260 | 10.54 |  |
| karatsuba_vclmul | 1135398 | 9.53 |  |
| KeccakF1600_StatePermute | 1129052 | 9.48 |  |
| keccak_inc_absorb | 279123 | 2.34 |  |
| hadamard | 276976 | 2.33 |  |
| memcpy | 275464 | 2.31 |  |
| vxor_words | 235260 | 1.97 |  |
| expand_and_sum | 199808 | 1.68 |  |
| compare_u32 | 193230 | 1.62 |  |
| keccak_inc_squeeze | 187760 | 1.58 |  |
| vect_generate_random_support2 | 173796 | 1.46 |  |
| gf_reduce | 141813 | 1.19 |  |
| find_peaks | 128899 | 1.08 |  |
| vect_generate_random_support1 | 111751 | 0.94 |  |
| vect_compare | 62873 | 0.53 |  |
| reduce | 40434 | 0.34 |  |
| vect_add | 35920 | 0.30 |  |
| gf_mul | 31681 | 0.27 |  |
| ?? ??:0 | 20653 | 0.17 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 3898608 | 32.73 |  |
| memset at mem.c:41 | 1942650 | 16.31 |  |
| karatsuba_vclmul at gf2x_vclmul.c:223 | 222396 | 1.87 |  |
| compare_u32 at vector.c:43 | 193230 | 1.62 |  |
| KeccakF1600_StatePermute at fips202.c:? | 192238 | 1.61 |  |
| keccak_inc_absorb at fips202.c:438 | 178780 | 1.50 |  |
| karatsuba_vclmul at gf2x_vclmul.c:225 | 178050 | 1.49 |  |
| karatsuba_vclmul at gf2x_vclmul.c:224 | 177180 | 1.49 |  |
| expand_and_sum at reed_muller.c:132 | 166656 | 1.40 |  |
| memcpy at mem.c:18 | 165068 | 1.39 |  |
| karatsuba_vclmul at gf2x_vclmul.c:198 | 124602 | 1.05 |  |
| memcpy at mem.c:19 | 109344 | 0.92 |  |
| keccak_inc_squeeze at fips202.c:505 | 104640 | 0.88 |  |
| hadamard at reed_muller.c:98 | 100352 | 0.84 |  |
| hadamard at reed_muller.c:99 | 100352 | 0.84 |  |
| karatsuba_vclmul at gf2x_vclmul.c:285 | 98370 | 0.83 |  |
| keccak_inc_absorb at fips202.c:435 | 89792 | 0.75 |  |
| vect_generate_random_support2 at vector.c:136 | 78648 | 0.66 |  |
| vect_generate_random_support2 at vector.c:137 | 77292 | 0.65 |  |
| hadamard at reed_muller.c:97 | 76216 | 0.64 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 2643063 | 20.73 |  |
| bne | 2126470 | 16.68 |  |
| sb | 2017045 | 15.82 |  |
| ld | 902959 | 7.08 |  |
| xor | 695264 | 5.45 |  |
| add | 554576 | 4.35 |  |
| bnez | 427567 | 3.35 |  |
| sd | 311114 | 2.44 |  |
| vle64.v | 256926 | 2.02 |  |
| vxor.vv | 201427 | 1.58 |  |
| slli | 198392 | 1.56 |  |
| rori | 192792 | 1.51 |  |
| vse64.v | 180844 | 1.42 |  |
| andn | 166356 | 1.30 |  |
| lh | 155032 | 1.22 |  |
| mv | 144122 | 1.13 |  |
| bltu | 131700 | 1.03 |  |
| subw | 103397 | 0.81 |  |
| or | 101128 | 0.79 |  |
| lbu | 97399 | 0.76 |  |

