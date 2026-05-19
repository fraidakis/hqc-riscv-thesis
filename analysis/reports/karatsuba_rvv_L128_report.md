# Full Analysis Report

- Variant: karatsuba_rvv
- Level: 128
- Date: 2026-05-19 17:27:58

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 157377 | 10442 |
| 192 | vect_mul | 481065 | 11262 |
| 256 | vect_mul | 1608062 | 9746 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 298707 | 45238 |  |
| 128 | encaps | 621513 | 45238 |  |
| 128 | decaps | 1186497 | 45238 |  |
| 128 | total | 2106717 | 45238 |  |
| 192 | keypair | 733476 | 47366 |  |
| 192 | encaps | 1538040 | 47366 |  |
| 192 | decaps | 2697205 | 47366 |  |
| 192 | total | 4968721 | 47366 |  |
| 256 | keypair | 2005047 | 52864 |  |
| 256 | encaps | 4123999 | 52864 |  |
| 256 | decaps | 6986546 | 52864 |  |
| 256 | total | 13115592 | 52864 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1289 | 102 | 1187 | 118 | 11119063 | 5624159 | 1702097 | 1108184 |
| 192 | 3021 | 283 | 2738 | 726 | 24693122 | 15075401 | 3602337 | 2878206 |
| 256 | 6841 | 1023 | 5818 | 3179 | 59134693 | 31778048 | 8242752 | 5658458 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 2043 | 2082 | 33192 | 27291 | 519 | 427 |
| 192 | 2408 | 2440 | 31066 | 29126 | 486 | 456 |
| 256 | 2209 | 2272 | 28155 | 26327 | 440 | 412 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 27.27 |  |
| clmul_leaf | 477096 | 22.48 |  |
| clmul64 | 228648 | 10.77 |  |
| keccak_inc_absorb | 141116 | 6.65 |  |
| gf_reduce | 105600 | 4.98 |  |
| keccak_inc_squeeze | 96430 | 4.54 |  |
| clmul_leaf_n8 | 70596 | 3.33 |  |
| rvv_xor3_u64 | 54282 | 2.56 |  |
| rvv_copy_u64 | 30264 | 1.43 |  |
| vect_generate_random_support2 | 30252 | 1.43 |  |
| memcpy | 28078 | 1.32 |  |
| gf_mul | 23794 | 1.12 |  |
| kmul_18 | 23496 | 1.11 |  |
| kmul_17 | 22200 | 1.05 |  |
| compare_u32 | 21090 | 0.99 |  |
| vect_generate_random_support1 | 19365 | 0.91 |  |
| vect_add | 15978 | 0.75 |  |
| kmul_35 | 14094 | 0.66 |  |
| compute_syndromes | 13895 | 0.65 |  |
| gf_square | 12489 | 0.59 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:42 | 228648 | 10.77 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 4.64 |  |
| clmul_leaf at gf2x_clmul.c:447 | 88992 | 4.19 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 4.04 |  |
| clmul_leaf at gf2x_clmul.c:448 | 79104 | 3.73 |  |
| clmul_leaf at gf2x_clmul.c:449 | 79104 | 3.73 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 2.44 |  |
| clmul_leaf at gf2x_clmul.c:446 | 49440 | 2.33 |  |
| gf_reduce at gf.c:106 | 44712 | 2.11 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 2.03 |  |
| clmul_leaf at gf2x_clmul.c:450 | 39552 | 1.86 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 1.63 |  |
| gf_reduce at gf.c:105 | 33534 | 1.58 |  |
| clmul_leaf at gf2x_clmul.c:435 | 22248 | 1.05 |  |
| compare_u32 at vector.c:43 | 21090 | 0.99 |  |
| clmul_leaf at gf2x_clmul.c:473 | 19776 | 0.93 |  |
| clmul_leaf at gf2x_clmul.c:474 | 19776 | 0.93 |  |
| memcpy at mem.c:21 | 17555 | 0.83 |  |
| gf_reduce at gf.c:97 | 16767 | 0.79 |  |
| clmul_leaf at gf2x_clmul.c:477 | 12360 | 0.58 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 649468 | 22.31 |  |
| xor | 513448 | 17.64 |  |
| bnez | 404826 | 13.91 |  |
| sd | 216005 | 7.42 |  |
| addi | 122979 | 4.23 |  |
| clmul | 119322 | 4.10 |  |
| clmulh | 114324 | 3.93 |  |
| rori | 98832 | 3.40 |  |
| andn | 85375 | 2.93 |  |
| add | 72501 | 2.49 |  |
| slli | 64374 | 2.21 |  |
| bne | 39791 | 1.37 |  |
| mv | 34069 | 1.17 |  |
| andi | 33536 | 1.15 |  |
| vle64.v | 29940 | 1.03 |  |
| vxor.vv | 21060 | 0.72 |  |
| zext.b | 19956 | 0.69 |  |
| sub | 18586 | 0.64 |  |
| vse64.v | 17198 | 0.59 |  |
| lbu | 16947 | 0.58 |  |

