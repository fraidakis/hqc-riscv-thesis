# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 256
- Date: 2026-05-15 00:25:41

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
| memset | 3924223 | 28.65 |  |
| vclmul_leaf | 2083692 | 15.21 |  |
| KeccakF1600_StatePermute | 1715996 | 12.53 |  |
| karatsuba_vclmul | 1320600 | 9.64 |  |
| rvv_xor3_u64 | 460284 | 3.36 |  |
| memcpy | 448010 | 3.27 |  |
| hadamard | 445140 | 3.25 |  |
| keccak_inc_absorb | 443726 | 3.24 |  |
| gf_reduce | 342597 | 2.50 |  |
| compare_u32 | 330780 | 2.42 |  |
| expand_and_sum | 321120 | 2.34 |  |
| vect_generate_random_support2 | 289716 | 2.12 |  |
| keccak_inc_squeeze | 274928 | 2.01 |  |
| rvv_copy_u64 | 250200 | 1.83 |  |
| find_peaks | 207223 | 1.51 |  |
| vect_generate_random_support1 | 188693 | 1.38 |  |
| vect_compare | 100974 | 0.74 |  |
| gf_mul | 82365 | 0.60 |  |
| reduce | 64920 | 0.47 |  |
| reed_solomon_encode | 63948 | 0.47 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| memset at mem.c:40 | 2613166 | 19.08 |  |
| memset at mem.c:41 | 1302109 | 9.51 |  |
| karatsuba_vclmul at gf2x_vclmul.c:265 | 350052 | 2.56 |  |
| compare_u32 at vector.c:43 | 330780 | 2.42 |  |
| KeccakF1600_StatePermute at fips202.c:? | 292174 | 2.13 |  |
| keccak_inc_absorb at fips202.c:438 | 287230 | 2.10 |  |
| karatsuba_vclmul at gf2x_vclmul.c:267 | 284430 | 2.08 |  |
| karatsuba_vclmul at gf2x_vclmul.c:266 | 283458 | 2.07 |  |
| memcpy at mem.c:18 | 268474 | 1.96 |  |
| expand_and_sum at reed_muller.c:132 | 267840 | 1.96 |  |
| memcpy at mem.c:19 | 177876 | 1.30 |  |
| hadamard at reed_muller.c:98 | 161280 | 1.18 |  |
| hadamard at reed_muller.c:99 | 161280 | 1.18 |  |
| keccak_inc_squeeze at fips202.c:505 | 159312 | 1.16 |  |
| gf_reduce at gf.c:106 | 144736 | 1.06 |  |
| keccak_inc_absorb at fips202.c:435 | 144257 | 1.05 |  |
| vect_generate_random_support2 at vector.c:136 | 134088 | 0.98 |  |
| vect_generate_random_support2 at vector.c:137 | 132312 | 0.97 |  |
| vect_generate_random_support1 at vector.c:106 | 127970 | 0.93 |  |
| karatsuba_vclmul at gf2x_vclmul.c:240 | 124602 | 0.91 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 2443793 | 16.83 |  |
| bne | 1613749 | 11.11 |  |
| sb | 1423187 | 9.80 |  |
| xor | 1129018 | 7.78 |  |
| ld | 1115059 | 7.68 |  |
| add | 880861 | 6.07 |  |
| bnez | 488046 | 3.36 |  |
| vle64.v | 439758 | 3.03 |  |
| sd | 428521 | 2.95 |  |
| slli | 364445 | 2.51 |  |
| vxor.vv | 335047 | 2.31 |  |
| vse64.v | 308273 | 2.12 |  |
| rori | 293016 | 2.02 |  |
| andn | 254482 | 1.75 |  |
| lh | 252748 | 1.74 |  |
| mv | 204590 | 1.41 |  |
| subw | 173421 | 1.19 |  |
| or | 170403 | 1.17 |  |
| lbu | 165990 | 1.14 |  |
| bltu | 159937 | 1.10 |  |

