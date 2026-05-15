# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-15 14:04:15

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
| 128 | keypair | 241030 | 38722 |  |
| 128 | encaps | 506290 | 38722 |  |
| 128 | decaps | 1225209 | 38722 |  |
| 192 | keypair | 565026 | 39754 |  |
| 192 | encaps | 1200330 | 39754 |  |
| 192 | decaps | 2524413 | 39754 |  |
| 256 | keypair | 876604 | 46752 |  |
| 256 | encaps | 1867963 | 46752 |  |
| 256 | decaps | 4137101 | 46752 |  |

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
| memset | 924801 | 26.95 |  |
| KeccakF1600_StatePermute | 521424 | 15.20 |  |
| karatsuba_vclmul | 309024 | 9.01 |  |
| vclmul_leaf | 256668 | 7.48 |  |
| hadamard | 227516 | 6.63 |  |
| keccak_inc_absorb | 141116 | 4.11 |  |
| memcpy | 137922 | 4.02 |  |
| find_peaks | 105919 | 3.09 |  |
| gf_reduce | 105600 | 3.08 |  |
| keccak_inc_squeeze | 96430 | 2.81 |  |
| expand_and_sum | 93748 | 2.73 |  |
| compare_u32 | 83250 | 2.43 |  |
| vect_generate_random_support2 | 79260 | 2.31 |  |
| rvv_xor3_u64 | 54282 | 1.58 |  |
| vect_generate_random_support1 | 49869 | 1.45 |  |
| rvv_copy_u64 | 33822 | 0.99 |  |
| vect_compare | 31058 | 0.91 |  |
| gf_mul | 23794 | 0.69 |  |
| reduce | 19974 | 0.58 |  |
| vect_add | 17744 | 0.52 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 615498 | 17.94 |  |
| memset at mem.c:41 | 306195 | 8.92 |  |
| KeccakF1600_StatePermute at fips202_rvv_gather.c:148 | 194256 | 5.66 |  |
| KeccakF1600_StatePermute at fips202_rvv_gather.c:143 | 170400 | 4.97 |  |
| compare_u32 at vector.c:43 | 83250 | 2.43 |  |
| memcpy at mem.c:18 | 82652 | 2.41 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.40 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.40 |  |
| karatsuba_vclmul at gf2x_vclmul.c:265 | 70332 | 2.05 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.99 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.82 |  |
| karatsuba_vclmul at gf2x_vclmul.c:267 | 56190 | 1.64 |  |
| karatsuba_vclmul at gf2x_vclmul.c:266 | 55788 | 1.63 |  |
| memcpy at mem.c:19 | 54764 | 1.60 |  |
| keccak_inc_squeeze at fips202_rvv_gather.c:294 | 51696 | 1.51 |  |
| KeccakF1600_StatePermute at fips202_rvv_gather.c:0 | 45156 | 1.32 |  |
| gf_reduce at gf.c:106 | 44712 | 1.30 |  |
| keccak_inc_absorb at fips202_rvv_gather.c:259 | 43067 | 1.26 |  |
| keccak_inc_absorb at fips202_rvv_gather.c:261 | 42875 | 1.25 |  |
| keccak_inc_absorb at fips202_rvv_gather.c:260 | 42875 | 1.25 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 648446 | 15.36 |  |
| addi | 632441 | 14.98 |  |
| bne | 405834 | 9.61 |  |
| bnez | 402349 | 9.53 |  |
| sb | 344187 | 8.15 |  |
| xor | 267742 | 6.34 |  |
| add | 184368 | 4.37 |  |
| sd | 181759 | 4.30 |  |
| lh | 110766 | 2.62 |  |
| slli | 99871 | 2.37 |  |
| mv | 67304 | 1.59 |  |
| sh | 62575 | 1.48 |  |
| subw | 54754 | 1.30 |  |
| vle64.v | 51314 | 1.22 |  |
| lbu | 49840 | 1.18 |  |
| or | 49069 | 1.16 |  |
| vxor.vv | 39307 | 0.93 |  |
| vse64.v | 36734 | 0.87 |  |
| andi | 36281 | 0.86 |  |
| srli | 33604 | 0.80 |  |

