# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-15 18:32:41

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
| 128 | keypair | 249918 | 38636 |  |
| 128 | encaps | 523258 | 38636 |  |
| 128 | decaps | 1256317 | 38636 |  |
| 192 | keypair | 581186 | 39668 |  |
| 192 | encaps | 1233054 | 39668 |  |
| 192 | decaps | 2585013 | 39668 |  |
| 256 | keypair | 902460 | 46666 |  |
| 256 | encaps | 1918059 | 46666 |  |
| 256 | decaps | 4232445 | 46666 |  |

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
| memset | 924801 | 26.51 |  |
| KeccakF1600_StatePermute | 578792 | 16.59 |  |
| karatsuba_vclmul | 309024 | 8.86 |  |
| vclmul_leaf | 256668 | 7.36 |  |
| hadamard | 227516 | 6.52 |  |
| keccak_inc_absorb | 141116 | 4.04 |  |
| memcpy | 137922 | 3.95 |  |
| find_peaks | 105919 | 3.04 |  |
| gf_reduce | 105600 | 3.03 |  |
| keccak_inc_squeeze | 96430 | 2.76 |  |
| expand_and_sum | 93748 | 2.69 |  |
| compare_u32 | 83250 | 2.39 |  |
| vect_generate_random_support2 | 79260 | 2.27 |  |
| rvv_xor3_u64 | 54282 | 1.56 |  |
| vect_generate_random_support1 | 49869 | 1.43 |  |
| rvv_copy_u64 | 33822 | 0.97 |  |
| vect_compare | 31058 | 0.89 |  |
| gf_mul | 23794 | 0.68 |  |
| reduce | 19974 | 0.57 |  |
| vect_add | 17744 | 0.51 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 615498 | 17.64 |  |
| memset at mem.c:41 | 306195 | 8.78 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 2.82 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.46 |  |
| compare_u32 at vector.c:43 | 83250 | 2.39 |  |
| memcpy at mem.c:18 | 82652 | 2.37 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.36 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.36 |  |
| karatsuba_vclmul at gf2x_vclmul.c:265 | 70332 | 2.02 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.96 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.79 |  |
| karatsuba_vclmul at gf2x_vclmul.c:267 | 56190 | 1.61 |  |
| karatsuba_vclmul at gf2x_vclmul.c:266 | 55788 | 1.60 |  |
| memcpy at mem.c:19 | 54764 | 1.57 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.48 |  |
| gf_reduce at gf.c:106 | 44712 | 1.28 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 1.23 |  |
| karatsuba_vclmul at gf2x_vclmul.c:240 | 41496 | 1.19 |  |
| find_peaks at reed_muller.c:155 | 35328 | 1.01 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 0.99 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 612845 | 14.35 |  |
| ld | 586486 | 13.73 |  |
| bne | 404130 | 9.46 |  |
| bnez | 401165 | 9.39 |  |
| xor | 356350 | 8.35 |  |
| sb | 344187 | 8.06 |  |
| add | 177126 | 4.15 |  |
| sd | 129503 | 3.03 |  |
| lh | 110766 | 2.59 |  |
| rori | 98832 | 2.31 |  |
| slli | 92771 | 2.17 |  |
| andn | 85375 | 2.00 |  |
| mv | 75824 | 1.78 |  |
| sh | 62575 | 1.47 |  |
| subw | 54754 | 1.28 |  |
| lbu | 49840 | 1.17 |  |
| or | 49069 | 1.15 |  |
| vle64.v | 47196 | 1.11 |  |
| andi | 36281 | 0.85 |  |
| vxor.vv | 35899 | 0.84 |  |

