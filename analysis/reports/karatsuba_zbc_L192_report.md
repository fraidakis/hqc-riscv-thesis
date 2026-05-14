# Full Analysis Report

- Variant: karatsuba_zbc
- Level: 192
- Date: 2026-05-14 12:21:11

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 360961 | 2796 |
| 192 | vect_mul | 1098930 | 2828 |
| 256 | vect_mul | 2285122 | 2818 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 604115 | 39758 |  |
| 128 | encaps | 1252538 | 39758 |  |
| 128 | decaps | 2680559 | 39758 |  |
| 192 | keypair | 1622996 | 40804 |  |
| 192 | encaps | 3348382 | 40804 |  |
| 192 | decaps | 6101623 | 40804 |  |
| 256 | keypair | 3297029 | 47786 |  |
| 256 | encaps | 6654995 | 47786 |  |
| 256 | decaps | 12324577 | 47786 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1313 | 101 | 1212 | 136 | 20387859 | 8943859 | 3028535 | 2455904 |
| 192 | 3021 | 295 | 2726 | 740 | 52101014 | 24559237 | 7560994 | 6770664 |
| 256 | 6934 | 1124 | 5810 | 3284 | 103516229 | 49378364 | 14924623 | 12087120 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1376 | 1376 | 7877 | 11841 | 124 | 186 |
| 192 | 1479 | 1373 | 8271 | 12813 | 130 | 201 |
| 256 | 1469 | 1347 | 8203 | 12682 | 129 | 199 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset | 5891574 | 25.43 |  |
| vect_write_support_to_vector | 5579760 | 24.08 |  |
| clmul_leaf | 3910140 | 16.87 |  |
| karatsuba_clmul | 3033114 | 13.09 |  |
| KeccakF1600_StatePermute | 1129052 | 4.87 |  |
| clmul64 | 695328 | 3.00 |  |
| gf_carryless_mul | 573534 | 2.48 |  |
| keccak_inc_absorb | 279123 | 1.20 |  |
| hadamard | 276976 | 1.20 |  |
| memcpy | 275464 | 1.19 |  |
| expand_and_sum | 199808 | 0.86 |  |
| compare_u32 | 193230 | 0.83 |  |
| keccak_inc_squeeze | 187760 | 0.81 |  |
| vect_generate_random_support2 | 173796 | 0.75 |  |
| find_peaks | 128899 | 0.56 |  |
| gf_reduce | 121806 | 0.53 |  |
| gf_mul | 113373 | 0.49 |  |
| vect_generate_random_support1 | 111751 | 0.48 |  |
| vect_compare | 62873 | 0.27 |  |
| reduce | 40434 | 0.17 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 3923268 | 16.93 |  |
| vect_write_support_to_vector at vector.c:153 | 2208096 | 9.53 |  |
| clmul_leaf at gf2x_clmul.c:81 | 2085984 | 9.00 |  |
| memset at mem.c:41 | 1954962 | 8.44 |  |
| vect_write_support_to_vector at vector.c:149 | 1656072 | 7.15 |  |
| vect_write_support_to_vector at vector.c:151 | 1104048 | 4.76 |  |
| clmul_leaf at gf2x_clmul.c:78 | 1042992 | 4.50 |  |
| clmul64 at gf2x_clmul.c:36 | 695328 | 3.00 |  |
| vect_write_support_to_vector at vector.c:150 | 552024 | 2.38 |  |
| karatsuba_clmul at gf2x_clmul.c:132 | 507276 | 2.19 |  |
| karatsuba_clmul at gf2x_clmul.c:131 | 495096 | 2.14 |  |
| karatsuba_clmul at gf2x_clmul.c:136 | 427320 | 1.84 |  |
| karatsuba_clmul at gf2x_clmul.c:133 | 427320 | 1.84 |  |
| clmul_leaf at gf2x_clmul.c:80 | 347664 | 1.50 |  |
| gf_carryless_mul at gf.c:132 | 280098 | 1.21 |  |
| clmul_leaf at gf2x_clmul.c:? | 277206 | 1.20 |  |
| karatsuba_clmul at gf2x_clmul.c:121 | 222396 | 0.96 |  |
| karatsuba_clmul at gf2x_clmul.c:122 | 212790 | 0.92 |  |
| compare_u32 at vector.c:24 | 193230 | 0.83 |  |
| KeccakF1600_StatePermute at fips202.c:? | 192238 | 0.83 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 5790499 | 24.10 |  |
| bne | 3219211 | 13.40 |  |
| ld | 2415678 | 10.05 |  |
| sb | 2029363 | 8.45 |  |
| xor | 1708352 | 7.11 |  |
| and | 688076 | 2.86 |  |
| or | 657217 | 2.73 |  |
| snez | 632052 | 2.63 |  |
| sd | 561818 | 2.34 |  |
| lwu | 554812 | 2.31 |  |
| bnez | 438270 | 1.82 |  |
| add | 363416 | 1.51 |  |
| vsetivli | 349150 | 1.45 |  |
| vslide1down.vx | 348940 | 1.45 |  |
| vmv.v.x | 347918 | 1.45 |  |
| vse64.v | 347884 | 1.45 |  |
| vxor.vv | 347809 | 1.45 |  |
| clmulh | 347664 | 1.45 |  |
| clmul | 347664 | 1.45 |  |
| vle64.v | 347664 | 1.45 |  |

