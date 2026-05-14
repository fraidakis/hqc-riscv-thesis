# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-15 00:22:31

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 149354 | 2818 |
| 192 | vect_mul | 461757 | 2850 |
| 256 | vect_mul | 646883 | 2840 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 289630 | 38636 |  |
| 128 | encaps | 604970 | 38636 |  |
| 128 | decaps | 1385511 | 38636 |  |
| 192 | keypair | 714881 | 39668 |  |
| 192 | encaps | 1505374 | 39668 |  |
| 192 | decaps | 2997520 | 39668 |  |
| 256 | keypair | 1046636 | 46666 |  |
| 256 | encaps | 2214509 | 46666 |  |
| 256 | decaps | 4681538 | 46666 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1286 | 82 | 1204 | 109 | 13436327 | 7760239 | 1980087 | 1675046 |
| 192 | 3008 | 294 | 2714 | 726 | 32112440 | 21620095 | 4510393 | 4410936 |
| 256 | 6859 | 1091 | 5768 | 3188 | 61369633 | 44197568 | 8496701 | 8251662 |

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
| memset | 924801 | 25.00 |  |
| KeccakF1600_StatePermute | 578792 | 15.65 |  |
| vclmul_leaf | 404988 | 10.95 |  |
| karatsuba_vclmul | 309024 | 8.35 |  |
| hadamard | 227516 | 6.15 |  |
| keccak_inc_absorb | 141116 | 3.81 |  |
| memcpy | 137922 | 3.73 |  |
| find_peaks | 105919 | 2.86 |  |
| gf_reduce | 105600 | 2.85 |  |
| keccak_inc_squeeze | 96430 | 2.61 |  |
| rvv_xor3_u64 | 95658 | 2.59 |  |
| expand_and_sum | 93748 | 2.53 |  |
| compare_u32 | 83250 | 2.25 |  |
| vect_generate_random_support2 | 79260 | 2.14 |  |
| rvv_copy_u64 | 54462 | 1.47 |  |
| vect_generate_random_support1 | 49869 | 1.35 |  |
| vect_compare | 31058 | 0.84 |  |
| gf_mul | 23794 | 0.64 |  |
| reduce | 19974 | 0.54 |  |
| vect_add | 17744 | 0.48 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 615498 | 16.64 |  |
| memset at mem.c:41 | 306195 | 8.28 |  |
| KeccakF1600_StatePermute at fips202.c:? | 98548 | 2.66 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.32 |  |
| compare_u32 at vector.c:43 | 83250 | 2.25 |  |
| memcpy at mem.c:18 | 82652 | 2.23 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.23 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.23 |  |
| karatsuba_vclmul at gf2x_vclmul.c:265 | 70332 | 1.90 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.85 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.69 |  |
| karatsuba_vclmul at gf2x_vclmul.c:267 | 56190 | 1.52 |  |
| karatsuba_vclmul at gf2x_vclmul.c:266 | 55788 | 1.51 |  |
| memcpy at mem.c:19 | 54764 | 1.48 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.40 |  |
| gf_reduce at gf.c:106 | 44712 | 1.21 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 1.16 |  |
| karatsuba_vclmul at gf2x_vclmul.c:240 | 41496 | 1.12 |  |
| find_peaks at reed_muller.c:155 | 35328 | 0.96 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 0.94 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 623969 | 13.93 |  |
| ld | 586318 | 13.09 |  |
| bnez | 406163 | 9.07 |  |
| bne | 404130 | 9.02 |  |
| xor | 356350 | 7.95 |  |
| sb | 344187 | 7.68 |  |
| add | 211170 | 4.71 |  |
| sd | 129503 | 2.89 |  |
| lh | 110766 | 2.47 |  |
| slli | 101645 | 2.27 |  |
| rori | 98832 | 2.21 |  |
| vle64.v | 86076 | 1.92 |  |
| andn | 85375 | 1.91 |  |
| mv | 75824 | 1.69 |  |
| vxor.vv | 65905 | 1.47 |  |
| sh | 62575 | 1.40 |  |
| vse64.v | 60598 | 1.35 |  |
| subw | 54754 | 1.22 |  |
| lbu | 49840 | 1.11 |  |
| or | 49069 | 1.10 |  |

