# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 256
- Date: 2026-05-14 23:26:57

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
| memset | 9347359 | 49.16 |  |
| vclmul_leaf | 2083692 | 10.96 |  |
| KeccakF1600_StatePermute | 1715996 | 9.03 |  |
| karatsuba_vclmul | 1580934 | 8.32 |  |
| memcpy | 448010 | 2.36 |  |
| hadamard | 445140 | 2.34 |  |
| keccak_inc_absorb | 443726 | 2.33 |  |
| gf_reduce | 342597 | 1.80 |  |
| vxor_words | 342114 | 1.80 |  |
| compare_u32 | 330780 | 1.74 |  |
| expand_and_sum | 321120 | 1.69 |  |
| vect_generate_random_support2 | 289716 | 1.52 |  |
| keccak_inc_squeeze | 274928 | 1.45 |  |
| find_peaks | 207223 | 1.09 |  |
| vect_generate_random_support1 | 188693 | 0.99 |  |
| vect_compare | 100974 | 0.53 |  |
| gf_mul | 82365 | 0.43 |  |
| reduce | 64920 | 0.34 |  |
| reed_solomon_encode | 63948 | 0.34 |  |
| vect_add | 57680 | 0.30 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 6227134 | 32.75 |  |
| memset at mem.c:41 | 3106909 | 16.34 |  |
| karatsuba_vclmul at gf2x_vclmul.c:223 | 350052 | 1.84 |  |
| compare_u32 at vector.c:43 | 330780 | 1.74 |  |
| KeccakF1600_StatePermute at fips202.c:? | 292174 | 1.54 |  |
| keccak_inc_absorb at fips202.c:438 | 287230 | 1.51 |  |
| karatsuba_vclmul at gf2x_vclmul.c:225 | 284430 | 1.50 |  |
| karatsuba_vclmul at gf2x_vclmul.c:224 | 283458 | 1.49 |  |
| memcpy at mem.c:18 | 268474 | 1.41 |  |
| expand_and_sum at reed_muller.c:132 | 267840 | 1.41 |  |
| memcpy at mem.c:19 | 177876 | 0.94 |  |
| hadamard at reed_muller.c:98 | 161280 | 0.85 |  |
| hadamard at reed_muller.c:99 | 161280 | 0.85 |  |
| keccak_inc_squeeze at fips202.c:505 | 159312 | 0.84 |  |
| gf_reduce at gf.c:106 | 144736 | 0.76 |  |
| keccak_inc_absorb at fips202.c:435 | 144257 | 0.76 |  |
| vect_generate_random_support2 at vector.c:136 | 134088 | 0.71 |  |
| vect_generate_random_support2 at vector.c:137 | 132312 | 0.70 |  |
| vect_generate_random_support1 at vector.c:106 | 127970 | 0.67 |  |
| karatsuba_vclmul at gf2x_vclmul.c:198 | 124602 | 0.66 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 4248611 | 21.39 |  |
| bne | 3418549 | 17.21 |  |
| sb | 3227993 | 16.25 |  |
| ld | 1132139 | 5.70 |  |
| xor | 1129018 | 5.68 |  |
| add | 872821 | 4.39 |  |
| bnez | 444088 | 2.24 |  |
| sd | 430711 | 2.17 |  |
| vle64.v | 416724 | 2.10 |  |
| slli | 351215 | 1.77 |  |
| vxor.vv | 325249 | 1.64 |  |
| vse64.v | 295037 | 1.49 |  |
| rori | 293016 | 1.48 |  |
| andn | 254482 | 1.28 |  |
| lh | 252748 | 1.27 |  |
| bltu | 205567 | 1.03 |  |
| mv | 191492 | 0.96 |  |
| subw | 173433 | 0.87 |  |
| or | 170403 | 0.86 |  |
| lbu | 165996 | 0.84 |  |

