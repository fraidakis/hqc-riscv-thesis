# Full Analysis Report

- Variant: karatsuba_rvv
- Level: 128
- Date: 2026-05-20 22:21:34

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
| 128 | keypair | 288804 | 42634 |  |
| 128 | encaps | 593488 | 42634 |  |
| 128 | decaps | 1094338 | 42634 |  |
| 128 | total | 1976630 | 42634 |  |
| 192 | keypair | 717999 | 44692 |  |
| 192 | encaps | 1485491 | 44692 |  |
| 192 | decaps | 2558677 | 44692 |  |
| 192 | total | 4762167 | 44692 |  |
| 256 | keypair | 1983756 | 49950 |  |
| 256 | encaps | 4044480 | 49950 |  |
| 256 | decaps | 6691546 | 49950 |  |
| 256 | total | 12719782 | 49950 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1337 | 124 | 1213 | 162 | 11077871 | 5623823 | 1688548 | 1107974 |
| 192 | 3046 | 266 | 2780 | 758 | 24669050 | 15075065 | 3585160 | 2877996 |
| 256 | 6665 | 995 | 5670 | 3028 | 59103997 | 31777712 | 8219455 | 5658248 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 2041 | 2075 | 33488 | 26492 | 524 | 414 |
| 192 | 2275 | 2277 | 30412 | 26734 | 476 | 418 |
| 256 | 2200 | 2237 | 28464 | 25109 | 445 | 393 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 29.07 |  |
| clmul_leaf | 477096 | 23.96 |  |
| clmul64 | 228648 | 11.48 |  |
| keccak_inc_absorb | 141116 | 7.09 |  |
| keccak_inc_squeeze | 96430 | 4.84 |  |
| gf_reduce | 80541 | 4.05 |  |
| clmul_leaf_n8 | 70596 | 3.55 |  |
| rvv_xor3_u64 | 54282 | 2.73 |  |
| rvv_copy_u64 | 30264 | 1.52 |  |
| kmul_18 | 23496 | 1.18 |  |
| kmul_17 | 22200 | 1.12 |  |
| gf_mul | 16952 | 0.85 |  |
| vect_add | 15978 | 0.80 |  |
| kmul_35 | 14094 | 0.71 |  |
| compute_elp | 9934 | 0.50 |  |
| support_contains_value | 9579 | 0.48 |  |
| fft_rec | 8738 | 0.44 |  |
| compute_error_values | 8089 | 0.41 |  |
| reed_solomon_encode | 8058 | 0.40 |  |
| memset | 6824 | 0.34 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:42 | 228648 | 11.48 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 4.95 |  |
| clmul_leaf at gf2x_clmul.c:447 | 88992 | 4.47 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 4.31 |  |
| clmul_leaf at gf2x_clmul.c:448 | 79104 | 3.97 |  |
| clmul_leaf at gf2x_clmul.c:449 | 79104 | 3.97 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 2.60 |  |
| clmul_leaf at gf2x_clmul.c:446 | 49440 | 2.48 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 2.16 |  |
| clmul_leaf at gf2x_clmul.c:450 | 39552 | 1.99 |  |
| gf_reduce at gf.c:79 | 38151 | 1.92 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 1.74 |  |
| gf_reduce at gf.c:77 | 29673 | 1.49 |  |
| clmul_leaf at gf2x_clmul.c:435 | 22248 | 1.12 |  |
| clmul_leaf at gf2x_clmul.c:473 | 19776 | 0.99 |  |
| clmul_leaf at gf2x_clmul.c:474 | 19776 | 0.99 |  |
| clmul_leaf at gf2x_clmul.c:477 | 12360 | 0.62 |  |
| clmul_leaf at gf2x_clmul.c:385 | 11124 | 0.56 |  |
| KeccakF1600_StatePermute at fips202.c:208 | 10224 | 0.51 |  |
| clmul_leaf at gf2x_clmul.c:0 | 9888 | 0.50 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 646679 | 23.30 |  |
| xor | 500498 | 18.03 |  |
| bnez | 401952 | 14.48 |  |
| sd | 216007 | 7.78 |  |
| clmul | 118563 | 4.27 |  |
| clmulh | 114324 | 4.12 |  |
| addi | 102682 | 3.70 |  |
| rori | 98832 | 3.56 |  |
| andn | 85375 | 3.08 |  |
| add | 70933 | 2.56 |  |
| slli | 52255 | 1.88 |  |
| mv | 34004 | 1.23 |  |
| vle64.v | 29940 | 1.08 |  |
| andi | 28807 | 1.04 |  |
| bne | 28467 | 1.03 |  |
| vxor.vv | 21783 | 0.78 |  |
| sub | 18825 | 0.68 |  |
| vse64.v | 17179 | 0.62 |  |
| zext.b | 14991 | 0.54 |  |
| bltu | 11366 | 0.41 |  |

