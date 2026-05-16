# Full Analysis Report

- Variant: karatsuba_zbc_nomemset
- Level: 128
- Date: 2026-05-14 14:14:38

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 317645 | 2810 |
| 192 | vect_mul | 967470 | 2842 |
| 256 | vect_mul | 2142432 | 2832 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 560799 | 39772 |  |
| 128 | encaps | 1165906 | 39772 |  |
| 128 | decaps | 2550611 | 39772 |  |
| 192 | keypair | 1491536 | 40818 |  |
| 192 | encaps | 3085462 | 40818 |  |
| 192 | decaps | 5707243 | 40818 |  |
| 256 | keypair | 3154339 | 47800 |  |
| 256 | encaps | 6369615 | 47800 |  |
| 256 | decaps | 11896507 | 47800 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1331 | 105 | 1226 | 155 | 19267763 | 7813075 | 2888523 | 1335284 |
| 192 | 3052 | 308 | 2744 | 765 | 48450464 | 21025807 | 7104670 | 3267852 |
| 256 | 7047 | 1172 | 5875 | 3359 | 97872869 | 43740476 | 14219203 | 6479808 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1391 | 1395 | 7997 | 11947 | 125 | 187 |
| 192 | 1494 | 1392 | 8391 | 12909 | 132 | 202 |
| 256 | 1484 | 1366 | 8323 | 12778 | 131 | 200 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vect_write_support_to_vector | 1825896 | 26.62 |  |
| clmul_leaf | 1230624 | 17.94 |  |
| karatsuba_clmul | 869328 | 12.67 |  |
| KeccakF1600_StatePermute | 578792 | 8.44 |  |
| gf_carryless_mul | 429828 | 6.27 |  |
| memset | 324141 | 4.73 |  |
| clmul64 | 228648 | 3.33 |  |
| hadamard | 227516 | 3.32 |  |
| keccak_inc_absorb | 141116 | 2.06 |  |
| memcpy | 137922 | 2.01 |  |
| find_peaks | 105919 | 1.54 |  |
| keccak_inc_squeeze | 96430 | 1.41 |  |
| expand_and_sum | 93748 | 1.37 |  |
| gf_reduce | 90606 | 1.32 |  |
| gf_mul | 84966 | 1.24 |  |
| compare_u32 | 83250 | 1.21 |  |
| vect_generate_random_support2 | 79260 | 1.16 |  |
| vect_generate_random_support1 | 49869 | 0.73 |  |
| vect_compare | 31058 | 0.45 |  |
| reduce | 19974 | 0.29 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vect_write_support_to_vector at vector.c:153 | 717984 | 10.47 |  |
| clmul_leaf at gf2x_clmul.c:96 | 539892 | 7.87 |  |
| vect_write_support_to_vector at vector.c:149 | 538488 | 7.85 |  |
| vect_write_support_to_vector at vector.c:151 | 358992 | 5.23 |  |
| clmul_leaf at gf2x_clmul.c:94 | 269946 | 3.94 |  |
| clmul64 at gf2x_clmul.c:39 | 228648 | 3.33 |  |
| memset at mem.c:40 | 216018 | 3.15 |  |
| gf_carryless_mul at gf.c:132 | 209916 | 3.06 |  |
| vect_write_support_to_vector at vector.c:150 | 179496 | 2.62 |  |
| karatsuba_clmul at gf2x_clmul.c:167 | 157332 | 2.29 |  |
| karatsuba_clmul at gf2x_clmul.c:165 | 134856 | 1.97 |  |
| karatsuba_clmul at gf2x_clmul.c:162 | 115284 | 1.68 |  |
| karatsuba_clmul at gf2x_clmul.c:159 | 111264 | 1.62 |  |
| memset at mem.c:41 | 107895 | 1.57 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 1.44 |  |
| clmul_leaf at gf2x_clmul.c:0 | 95910 | 1.40 |  |
| clmul_leaf at gf2x_clmul.c:95 | 89982 | 1.31 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 1.25 |  |
| compare_u32 at vector.c:24 | 83250 | 1.21 |  |
| memcpy at mem.c:18 | 82652 | 1.20 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 1505553 | 19.54 |  |
| ld | 1094836 | 14.21 |  |
| xor | 706126 | 9.16 |  |
| bne | 543597 | 7.05 |  |
| bnez | 425222 | 5.52 |  |
| sd | 272582 | 3.54 |  |
| and | 272362 | 3.53 |  |
| snez | 239472 | 3.11 |  |
| or | 230410 | 2.99 |  |
| lwu | 181146 | 2.35 |  |
| add | 158070 | 2.05 |  |
| sb | 145899 | 1.89 |  |
| slli | 139058 | 1.80 |  |
| clmulh | 114324 | 1.48 |  |
| clmul | 114324 | 1.48 |  |
| lh | 110766 | 1.44 |  |
| mv | 101430 | 1.32 |  |
| rori | 98832 | 1.28 |  |
| vslide1down.vx | 90750 | 1.18 |  |
| vsetivli | 90725 | 1.18 |  |

