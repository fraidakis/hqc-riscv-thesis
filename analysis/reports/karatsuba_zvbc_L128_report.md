# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-15 14:15:15

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
| 128 | keypair | 490089 | 34972 |  |
| 128 | encaps | 883427 | 34972 |  |
| 128 | decaps | 1940445 | 34972 |  |
| 192 | keypair | 993044 | 36002 |  |
| 192 | encaps | 1927716 | 36002 |  |
| 192 | decaps | 3912039 | 36002 |  |
| 256 | keypair | 1505947 | 43000 |  |
| 256 | encaps | 2981702 | 43000 |  |
| 256 | decaps | 6274588 | 43000 |  |

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
| inner_loop | 1763388 | 32.62 |  |
| memset | 1275785 | 23.60 |  |
| memcpy | 431090 | 7.97 |  |
| karatsuba_vclmul | 309024 | 5.72 |  |
| vclmul_leaf | 256668 | 4.75 |  |
| hadamard | 227516 | 4.21 |  |
| keccak_inc_absorb | 150199 | 2.78 |  |
| find_peaks | 106067 | 1.96 |  |
| gf_reduce | 105600 | 1.95 |  |
| keccak_inc_squeeze | 103897 | 1.92 |  |
| expand_and_sum | 93748 | 1.73 |  |
| vect_generate_random_support1 | 84119 | 1.56 |  |
| compare_u32 | 83250 | 1.54 |  |
| vect_generate_random_support2 | 79260 | 1.47 |  |
| rvv_xor3_u64 | 54282 | 1.00 |  |
| rvv_copy_u64 | 33822 | 0.63 |  |
| vect_compare | 31058 | 0.57 |  |
| gf_mul | 23794 | 0.44 |  |
| reduce | 19974 | 0.37 |  |
| vect_add | 17744 | 0.33 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| inner_loop at fips202_rv64imbv_hybrid_x4.S:601 | 1748496 | 32.35 |  |
| memset at mem.c:40 | 849390 | 15.71 |  |
| memset at mem.c:41 | 422995 | 7.83 |  |
| memcpy at mem.c:18 | 258436 | 4.78 |  |
| memcpy at mem.c:19 | 171564 | 3.17 |  |
| compare_u32 at vector.c:43 | 83250 | 1.54 |  |
| hadamard at reed_muller.c:98 | 82432 | 1.52 |  |
| hadamard at reed_muller.c:99 | 82432 | 1.52 |  |
| karatsuba_vclmul at gf2x_vclmul.c:265 | 70332 | 1.30 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.27 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.16 |  |
| karatsuba_vclmul at gf2x_vclmul.c:267 | 56190 | 1.04 |  |
| karatsuba_vclmul at gf2x_vclmul.c:266 | 55788 | 1.03 |  |
| vect_generate_random_support1 at vector.c:106 | 54310 | 1.00 |  |
| keccak_inc_squeeze at fips202_rvv_x4.c:173 | 54192 | 1.00 |  |
| keccak_inc_absorb at fips202_rvv_x4.c:138 | 51642 | 0.96 |  |
| gf_reduce at gf.c:106 | 44712 | 0.83 |  |
| keccak_inc_absorb at fips202_rvv_x4.c:140 | 42875 | 0.79 |  |
| keccak_inc_absorb at fips202_rvv_x4.c:139 | 42875 | 0.79 |  |
| karatsuba_vclmul at gf2x_vclmul.c:240 | 41496 | 0.77 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 871204 | 14.03 |  |
| xor | 634377 | 10.22 |  |
| bne | 582833 | 9.39 |  |
| ld | 575871 | 9.27 |  |
| sb | 520011 | 8.37 |  |
| bnez | 419549 | 6.76 |  |
| vxor.vv | 400315 | 6.45 |  |
| rori | 203232 | 3.27 |  |
| add | 180027 | 2.90 |  |
| andn | 175375 | 2.82 |  |
| li | 120405 | 1.94 |  |
| lh | 110770 | 1.78 |  |
| lbu | 108669 | 1.75 |  |
| slli | 101995 | 1.64 |  |
| sd | 98552 | 1.59 |  |
| mv | 68918 | 1.11 |  |
| vsrl.vx | 66576 | 1.07 |  |
| vsll.vi | 66576 | 1.07 |  |
| subw | 63525 | 1.02 |  |
| sh | 62575 | 1.01 |  |

