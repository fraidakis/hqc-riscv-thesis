# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 192
- Date: 2026-05-15 00:23:52

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
| memset | 2469030 | 28.75 |  |
| vclmul_leaf | 1255260 | 14.62 |  |
| KeccakF1600_StatePermute | 1129052 | 13.15 |  |
| karatsuba_vclmul | 958500 | 11.16 |  |
| rvv_xor3_u64 | 302190 | 3.52 |  |
| keccak_inc_absorb | 279123 | 3.25 |  |
| hadamard | 276976 | 3.23 |  |
| memcpy | 275464 | 3.21 |  |
| expand_and_sum | 199808 | 2.33 |  |
| compare_u32 | 193230 | 2.25 |  |
| keccak_inc_squeeze | 187760 | 2.19 |  |
| vect_generate_random_support2 | 173796 | 2.02 |  |
| rvv_copy_u64 | 172488 | 2.01 |  |
| gf_reduce | 141813 | 1.65 |  |
| find_peaks | 128899 | 1.50 |  |
| vect_generate_random_support1 | 111751 | 1.30 |  |
| vect_compare | 62873 | 0.73 |  |
| reduce | 40434 | 0.47 |  |
| vect_add | 35920 | 0.42 |  |
| gf_mul | 31681 | 0.37 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 1643040 | 19.13 |  |
| memset at mem.c:41 | 817050 | 9.51 |  |
| karatsuba_vclmul at gf2x_vclmul.c:265 | 222396 | 2.59 |  |
| compare_u32 at vector.c:43 | 193230 | 2.25 |  |
| KeccakF1600_StatePermute at fips202.c:? | 192238 | 2.24 |  |
| keccak_inc_absorb at fips202.c:438 | 178780 | 2.08 |  |
| karatsuba_vclmul at gf2x_vclmul.c:267 | 178050 | 2.07 |  |
| karatsuba_vclmul at gf2x_vclmul.c:266 | 177180 | 2.06 |  |
| expand_and_sum at reed_muller.c:132 | 166656 | 1.94 |  |
| memcpy at mem.c:18 | 165068 | 1.92 |  |
| karatsuba_vclmul at gf2x_vclmul.c:240 | 124602 | 1.45 |  |
| memcpy at mem.c:19 | 109344 | 1.27 |  |
| keccak_inc_squeeze at fips202.c:505 | 104640 | 1.22 |  |
| hadamard at reed_muller.c:98 | 100352 | 1.17 |  |
| hadamard at reed_muller.c:99 | 100352 | 1.17 |  |
| karatsuba_vclmul at gf2x_vclmul.c:307 | 98370 | 1.15 |  |
| keccak_inc_absorb at fips202.c:435 | 89792 | 1.05 |  |
| vect_generate_random_support2 at vector.c:136 | 78648 | 0.92 |  |
| vect_generate_random_support2 at vector.c:137 | 77292 | 0.90 |  |
| hadamard at reed_muller.c:97 | 76216 | 0.89 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 1517463 | 16.11 |  |
| bne | 1000870 | 10.62 |  |
| ld | 897283 | 9.53 |  |
| sb | 891445 | 9.46 |  |
| xor | 695264 | 7.38 |  |
| add | 557186 | 5.91 |  |
| bnez | 463321 | 4.92 |  |
| sd | 308930 | 3.28 |  |
| vle64.v | 269874 | 2.86 |  |
| slli | 206678 | 2.19 |  |
| vxor.vv | 206089 | 2.19 |  |
| rori | 192792 | 2.05 |  |
| vse64.v | 189130 | 2.01 |  |
| andn | 166356 | 1.77 |  |
| mv | 157226 | 1.67 |  |
| lh | 155032 | 1.65 |  |
| subw | 103397 | 1.10 |  |
| or | 101128 | 1.07 |  |
| bltu | 100740 | 1.07 |  |
| lbu | 97399 | 1.03 |  |

