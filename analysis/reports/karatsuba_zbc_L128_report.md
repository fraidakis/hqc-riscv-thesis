# Full Analysis Report

- Variant: karatsuba_zbc
- Level: 128
- Date: 2026-05-14 10:38:28

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 360961 | 2796 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | karatsuba_zbc | keypair | 604115 | 39758 |
| 128 | karatsuba_zbc | encaps | 1252538 | 39758 |
| 128 | karatsuba_zbc | decaps | 2680559 | 39758 |

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
| memset | 2011629 | 23.13 |  |
| vect_write_support_to_vector | 1825896 | 20.99 |  |
| clmul_leaf | 1286820 | 14.79 |  |
| karatsuba_clmul | 961980 | 11.06 |  |
| KeccakF1600_StatePermute | 578792 | 6.65 |  |
| gf_carryless_mul | 429828 | 4.94 |  |
| clmul64 | 228648 | 2.63 |  |
| hadamard | 227516 | 2.62 |  |
| keccak_inc_absorb | 141116 | 1.62 |  |
| memcpy | 137922 | 1.59 |  |
| find_peaks | 105919 | 1.22 |  |
| keccak_inc_squeeze | 96430 | 1.11 |  |
| expand_and_sum | 93748 | 1.08 |  |
| gf_reduce | 90606 | 1.04 |  |
| gf_mul | 84966 | 0.98 |  |
| compare_u32 | 83250 | 0.96 |  |
| vect_generate_random_support2 | 79260 | 0.91 |  |
| vect_generate_random_support1 | 49869 | 0.57 |  |
| vect_compare | 31058 | 0.36 |  |
| reduce | 19974 | 0.23 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 1339554 | 15.40 |  |
| vect_write_support_to_vector at vector.c:153 | 717984 | 8.25 |  |
| clmul_leaf at gf2x_clmul.c:81 | 685944 | 7.89 |  |
| memset at mem.c:41 | 667479 | 7.67 |  |
| vect_write_support_to_vector at vector.c:149 | 538488 | 6.19 |  |
| vect_write_support_to_vector at vector.c:151 | 358992 | 4.13 |  |
| clmul_leaf at gf2x_clmul.c:78 | 342972 | 3.94 |  |
| clmul64 at gf2x_clmul.c:36 | 228648 | 2.63 |  |
| gf_carryless_mul at gf.c:132 | 209916 | 2.41 |  |
| vect_write_support_to_vector at vector.c:150 | 179496 | 2.06 |  |
| karatsuba_clmul at gf2x_clmul.c:132 | 160236 | 1.84 |  |
| karatsuba_clmul at gf2x_clmul.c:131 | 154608 | 1.78 |  |
| karatsuba_clmul at gf2x_clmul.c:136 | 134856 | 1.55 |  |
| karatsuba_clmul at gf2x_clmul.c:133 | 134856 | 1.55 |  |
| clmul_leaf at gf2x_clmul.c:80 | 114324 | 1.31 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 1.13 |  |
| clmul_leaf at gf2x_clmul.c:0 | 91758 | 1.05 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 0.99 |  |
| compare_u32 at vector.c:24 | 83250 | 0.96 |  |
| memcpy at mem.c:18 | 82652 | 0.95 |  |

