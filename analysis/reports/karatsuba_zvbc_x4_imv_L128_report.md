# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-15 14:05:59

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 107024 | 2818 |
| 192 | vect_mul | 327395 | 2850 |
| 256 | vect_mul | 503293 | 2840 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 569914 | 35548 |  |
| 128 | encaps | 1017533 | 35548 |  |
| 128 | decaps | 2189499 | 35548 |  |
| 192 | keypair | 1133536 | 36578 |  |
| 192 | encaps | 2186349 | 36578 |  |
| 192 | decaps | 4397375 | 36578 |  |
| 256 | keypair | 1719878 | 43576 |  |
| 256 | encaps | 3377634 | 43576 |  |
| 256 | decaps | 7028136 | 43576 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1286 | 82 | 1204 | 109 | 13442759 | 7764047 | 1984167 | 1678854 |
| 192 | 2965 | 309 | 2656 | 672 | 32106648 | 21626175 | 4514877 | 4417016 |
| 256 | 7034 | 1106 | 5928 | 3362 | 61355601 | 44197568 | 8494947 | 8251662 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1336 | 1332 | 9037 | 12355 | 142 | 194 |
| 192 | 1459 | 1351 | 9591 | 13479 | 150 | 211 |
| 256 | 1453 | 1317 | 9555 | 13284 | 150 | 208 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| inner_loop | 2225916 | 37.91 |  |
| memset | 1275785 | 21.73 |  |
| memcpy | 431090 | 7.34 |  |
| karatsuba_vclmul | 309024 | 5.26 |  |
| vclmul_leaf | 256668 | 4.37 |  |
| hadamard | 227516 | 3.87 |  |
| keccak_inc_absorb | 150199 | 2.56 |  |
| find_peaks | 106067 | 1.81 |  |
| gf_reduce | 105600 | 1.80 |  |
| keccak_inc_squeeze | 103897 | 1.77 |  |
| expand_and_sum | 93748 | 1.60 |  |
| vect_generate_random_support1 | 84119 | 1.43 |  |
| compare_u32 | 83250 | 1.42 |  |
| vect_generate_random_support2 | 79260 | 1.35 |  |
| rvv_xor3_u64 | 54282 | 0.92 |  |
| rvv_copy_u64 | 33822 | 0.58 |  |
| vect_compare | 31058 | 0.53 |  |
| gf_mul | 23794 | 0.41 |  |
| reduce | 19974 | 0.34 |  |
| vect_add | 17744 | 0.30 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| inner_loop at fips202_rv64imv_hybrid_x4.S:794 | 2211024 | 37.66 |  |
| memset at mem.c:40 | 849390 | 14.47 |  |
| memset at mem.c:41 | 422995 | 7.20 |  |
| memcpy at mem.c:18 | 258436 | 4.40 |  |
| memcpy at mem.c:19 | 171564 | 2.92 |  |
| compare_u32 at vector.c:43 | 83250 | 1.42 |  |
| hadamard at reed_muller.c:98 | 82432 | 1.40 |  |
| hadamard at reed_muller.c:99 | 82432 | 1.40 |  |
| karatsuba_vclmul at gf2x_vclmul.c:265 | 70332 | 1.20 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.17 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.07 |  |
| karatsuba_vclmul at gf2x_vclmul.c:267 | 56190 | 0.96 |  |
| karatsuba_vclmul at gf2x_vclmul.c:266 | 55788 | 0.95 |  |
| vect_generate_random_support1 at vector.c:106 | 54310 | 0.92 |  |
| keccak_inc_squeeze at fips202_rvv_x4.c:173 | 54192 | 0.92 |  |
| keccak_inc_absorb at fips202_rvv_x4.c:138 | 51642 | 0.88 |  |
| gf_reduce at gf.c:106 | 44712 | 0.76 |  |
| keccak_inc_absorb at fips202_rvv_x4.c:140 | 42875 | 0.73 |  |
| keccak_inc_absorb at fips202_rvv_x4.c:139 | 42875 | 0.73 |  |
| karatsuba_vclmul at gf2x_vclmul.c:240 | 41496 | 0.71 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 871204 | 13.04 |  |
| xor | 837609 | 12.54 |  |
| bne | 582833 | 8.72 |  |
| ld | 577782 | 8.65 |  |
| sb | 520011 | 7.78 |  |
| bnez | 421460 | 6.31 |  |
| vxor.vv | 400315 | 5.99 |  |
| slli | 305227 | 4.57 |  |
| srli | 245645 | 3.68 |  |
| add | 180027 | 2.69 |  |
| or | 140443 | 2.10 |  |
| li | 120551 | 1.80 |  |
| lh | 110770 | 1.66 |  |
| lbu | 108669 | 1.63 |  |
| and | 102953 | 1.54 |  |
| sd | 98552 | 1.48 |  |
| not | 76247 | 1.14 |  |
| mv | 68918 | 1.03 |  |
| vsll.vi | 66576 | 1.00 |  |
| vsrl.vx | 66576 | 1.00 |  |

