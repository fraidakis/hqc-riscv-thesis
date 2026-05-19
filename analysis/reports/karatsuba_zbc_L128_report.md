# Full Analysis Report

- Variant: karatsuba_zbc
- Level: 128
- Date: 2026-05-19 17:27:21

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 211841 | 14254 |
| 192 | vect_mul | 650635 | 16056 |
| 256 | vect_mul | 1900362 | 15150 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 458121 | 47398 |  |
| 128 | encaps | 905479 | 47398 |  |
| 128 | decaps | 2099075 | 47398 |  |
| 128 | total | 3462675 | 47398 |  |
| 192 | keypair | 1123538 | 50348 |  |
| 192 | encaps | 2254714 | 50348 |  |
| 192 | decaps | 4462247 | 50348 |  |
| 192 | total | 7840499 | 50348 |  |
| 256 | keypair | 2660281 | 56332 |  |
| 256 | encaps | 5345720 | 56332 |  |
| 256 | decaps | 9917432 | 56332 |  |
| 256 | total | 17923433 | 56332 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1284 | 100 | 1184 | 105 | 11493639 | 6120771 | 1733783 | 1113920 |
| 192 | 3021 | 271 | 2750 | 716 | 25979584 | 16609891 | 3738354 | 2701060 |
| 256 | 6835 | 989 | 5846 | 3194 | 61195689 | 33909404 | 8474793 | 5238730 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 3137 | 2780 | 20825 | 18043 | 326 | 282 |
| 192 | 3362 | 2909 | 22935 | 19329 | 359 | 303 |
| 256 | 3396 | 2936 | 22715 | 19344 | 355 | 303 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 16.64 |  |
| clmul_leaf | 477096 | 13.71 |  |
| memset | 316635 | 9.10 |  |
| clmul64 | 228870 | 6.58 |  |
| hadamard | 227516 | 6.54 |  |
| keccak_inc_absorb | 141116 | 4.06 |  |
| memcpy | 137922 | 3.96 |  |
| kmul_35 | 113520 | 3.26 |  |
| find_peaks | 105919 | 3.04 |  |
| gf_reduce | 105600 | 3.04 |  |
| kmul_18 | 100320 | 2.88 |  |
| keccak_inc_squeeze | 96430 | 2.77 |  |
| expand_and_sum | 93748 | 2.69 |  |
| compare_u32 | 83250 | 2.39 |  |
| kmul_17 | 80142 | 2.30 |  |
| vect_generate_random_support2 | 79260 | 2.28 |  |
| clmul_leaf_n8 | 67710 | 1.95 |  |
| vect_generate_random_support1 | 49869 | 1.43 |  |
| kmul_69 | 40950 | 1.18 |  |
| kmul_277 | 36480 | 1.05 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:36 | 228648 | 6.57 |  |
| memset at mem.c:60 | 210996 | 6.07 |  |
| memset at mem.c:61 | 105357 | 3.03 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 2.83 |  |
| clmul_leaf at gf2x_clmul.c:371 | 88992 | 2.56 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.47 |  |
| compare_u32 at vector.c:43 | 83250 | 2.39 |  |
| memcpy at mem.c:21 | 82652 | 2.38 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.37 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.37 |  |
| clmul_leaf at gf2x_clmul.c:372 | 79104 | 2.27 |  |
| clmul_leaf at gf2x_clmul.c:373 | 79104 | 2.27 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.97 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.80 |  |
| memcpy at mem.c:22 | 54764 | 1.57 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.49 |  |
| clmul_leaf at gf2x_clmul.c:370 | 49440 | 1.42 |  |
| gf_reduce at gf.c:106 | 44712 | 1.29 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 1.24 |  |
| clmul_leaf at gf2x_clmul.c:374 | 39552 | 1.14 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 865233 | 20.29 |  |
| xor | 626348 | 14.69 |  |
| bnez | 395621 | 9.28 |  |
| addi | 374794 | 8.79 |  |
| sd | 348830 | 8.18 |  |
| bne | 219474 | 5.15 |  |
| sb | 144373 | 3.39 |  |
| clmul | 119322 | 2.80 |  |
| clmulh | 114324 | 2.68 |  |
| lh | 110790 | 2.60 |  |
| rori | 98832 | 2.32 |  |
| andn | 85723 | 2.01 |  |
| add | 80513 | 1.89 |  |
| slli | 71370 | 1.67 |  |
| sh | 63067 | 1.48 |  |
| mv | 61055 | 1.43 |  |
| subw | 54754 | 1.28 |  |
| lbu | 50896 | 1.19 |  |
| or | 49417 | 1.16 |  |
| andi | 35555 | 0.83 |  |

