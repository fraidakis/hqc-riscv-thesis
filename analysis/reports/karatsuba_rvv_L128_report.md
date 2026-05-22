# Full Analysis Report

- Variant: karatsuba_rvv
- Level: 128
- Date: 2026-05-22 15:13:31

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
| 128 | keypair | 286680 | 41758 |  |
| 128 | encaps | 587126 | 41758 |  |
| 128 | decaps | 1051644 | 41758 |  |
| 128 | total | 1925450 | 41758 |  |
| 192 | keypair | 713711 | 44108 |  |
| 192 | encaps | 1472637 | 44108 |  |
| 192 | decaps | 2505472 | 44108 |  |
| 192 | total | 4691820 | 44108 |  |
| 256 | keypair | 1976880 | 50266 |  |
| 256 | encaps | 4023862 | 50266 |  |
| 256 | decaps | 6535320 | 50266 |  |
| 256 | total | 12536062 | 50266 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1299 | 111 | 1188 | 130 | 11098463 | 5623823 | 1691122 | 1104166 |
| 192 | 3077 | 291 | 2786 | 778 | 24671970 | 15075065 | 3585525 | 2870156 |
| 256 | 6793 | 995 | 5798 | 3124 | 59113773 | 31777712 | 8220677 | 5645704 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1950 | 1999 | 33190 | 25844 | 519 | 404 |
| 192 | 2226 | 2282 | 30460 | 26722 | 476 | 418 |
| 256 | 2147 | 2250 | 28294 | 24867 | 443 | 389 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 29.86 |  |
| clmul_leaf | 477096 | 24.61 |  |
| clmul64 | 228648 | 11.79 |  |
| keccak_inc_absorb | 141116 | 7.28 |  |
| keccak_inc_squeeze | 96430 | 4.97 |  |
| clmul_leaf_n8 | 70596 | 3.64 |  |
| gf_reduce | 59451 | 3.07 |  |
| rvv_xor3_u64 | 54282 | 2.80 |  |
| rvv_copy_u64 | 30264 | 1.56 |  |
| kmul_18 | 23496 | 1.21 |  |
| kmul_17 | 22200 | 1.15 |  |
| kmul_35 | 14094 | 0.73 |  |
| gf_mul | 11402 | 0.59 |  |
| support_contains_value | 9579 | 0.49 |  |
| fft_rec | 8738 | 0.45 |  |
| reed_solomon_encode | 8058 | 0.42 |  |
| memset | 6824 | 0.35 |  |
| vect_generate_random_support2 | 6618 | 0.34 |  |
| vect_write_support_to_vector | 6498 | 0.34 |  |
| kmul_69 | 5340 | 0.28 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:42 | 228648 | 11.79 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 5.08 |  |
| clmul_leaf at gf2x_clmul.c:447 | 88992 | 4.59 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 4.42 |  |
| clmul_leaf at gf2x_clmul.c:448 | 79104 | 4.08 |  |
| clmul_leaf at gf2x_clmul.c:449 | 79104 | 4.08 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 2.67 |  |
| clmul_leaf at gf2x_clmul.c:446 | 49440 | 2.55 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 2.22 |  |
| clmul_leaf at gf2x_clmul.c:450 | 39552 | 2.04 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 1.79 |  |
| gf_reduce at gf.c:79 | 28161 | 1.45 |  |
| clmul_leaf at gf2x_clmul.c:435 | 22248 | 1.15 |  |
| gf_reduce at gf.c:77 | 21903 | 1.13 |  |
| clmul_leaf at gf2x_clmul.c:473 | 19776 | 1.02 |  |
| clmul_leaf at gf2x_clmul.c:474 | 19776 | 1.02 |  |
| clmul_leaf at gf2x_clmul.c:477 | 12360 | 0.64 |  |
| clmul_leaf at gf2x_clmul.c:385 | 11124 | 0.57 |  |
| KeccakF1600_StatePermute at fips202.c:208 | 10224 | 0.53 |  |
| clmul_leaf at gf2x_clmul.c:0 | 9888 | 0.51 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 642195 | 23.57 |  |
| xor | 488836 | 17.94 |  |
| bnez | 402673 | 14.78 |  |
| sd | 213180 | 7.82 |  |
| clmul | 117453 | 4.31 |  |
| clmulh | 114324 | 4.20 |  |
| rori | 98832 | 3.63 |  |
| addi | 94881 | 3.48 |  |
| andn | 85317 | 3.13 |  |
| add | 70911 | 2.60 |  |
| slli | 44227 | 1.62 |  |
| mv | 32708 | 1.20 |  |
| vle64.v | 30084 | 1.10 |  |
| andi | 28807 | 1.06 |  |
| bne | 25816 | 0.95 |  |
| vxor.vv | 22724 | 0.83 |  |
| sub | 18831 | 0.69 |  |
| vse64.v | 17251 | 0.63 |  |
| bltu | 11454 | 0.42 |  |
| vsetvli | 11238 | 0.41 |  |

