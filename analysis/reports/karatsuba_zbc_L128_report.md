# Full Analysis Report

- Variant: karatsuba_zbc
- Level: 128
- Date: 2026-05-22 15:12:55

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
| 128 | keypair | 453609 | 46690 |  |
| 128 | encaps | 880279 | 46690 |  |
| 128 | decaps | 2060797 | 46690 |  |
| 128 | total | 3394685 | 46690 |  |
| 192 | keypair | 1113280 | 49642 |  |
| 192 | encaps | 2196403 | 49642 |  |
| 192 | decaps | 4383449 | 49642 |  |
| 192 | total | 7693132 | 49642 |  |
| 256 | keypair | 2642594 | 55650 |  |
| 256 | encaps | 5246039 | 55650 |  |
| 256 | decaps | 9787742 | 55650 |  |
| 256 | total | 17676375 | 55650 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1298 | 97 | 1201 | 119 | 11517751 | 6120963 | 1736797 | 1113944 |
| 192 | 3087 | 257 | 2830 | 787 | 25998984 | 16610083 | 3740779 | 2701084 |
| 256 | 6767 | 956 | 5811 | 3128 | 61191153 | 33909596 | 8474226 | 5238754 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 3141 | 2784 | 20857 | 18075 | 326 | 283 |
| 192 | 3366 | 2913 | 22967 | 19361 | 359 | 303 |
| 256 | 3400 | 2940 | 22747 | 19376 | 356 | 303 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 16.97 |  |
| clmul_leaf | 477096 | 13.99 |  |
| memset | 316635 | 9.28 |  |
| clmul64 | 228870 | 6.71 |  |
| hadamard | 227516 | 6.67 |  |
| keccak_inc_absorb | 141116 | 4.14 |  |
| support_contains_value | 140430 | 4.12 |  |
| memcpy | 137922 | 4.04 |  |
| kmul_35 | 113520 | 3.33 |  |
| gf_reduce | 106191 | 3.11 |  |
| find_peaks | 105919 | 3.11 |  |
| kmul_18 | 100320 | 2.94 |  |
| keccak_inc_squeeze | 96430 | 2.83 |  |
| expand_and_sum | 93748 | 2.75 |  |
| kmul_17 | 80142 | 2.35 |  |
| clmul_leaf_n8 | 67710 | 1.99 |  |
| kmul_69 | 40950 | 1.20 |  |
| kmul_277 | 36480 | 1.07 |  |
| kmul_139 | 34968 | 1.03 |  |
| kmul_70 | 33192 | 0.97 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:36 | 228648 | 6.70 |  |
| memset at mem.c:84 | 210996 | 6.19 |  |
| memset at mem.c:85 | 105357 | 3.09 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 2.89 |  |
| support_contains_value at vector.c:81 | 92340 | 2.71 |  |
| clmul_leaf at gf2x_clmul.c:371 | 88992 | 2.61 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.51 |  |
| memcpy at mem.c:33 | 82652 | 2.42 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.42 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.42 |  |
| clmul_leaf at gf2x_clmul.c:372 | 79104 | 2.32 |  |
| clmul_leaf at gf2x_clmul.c:373 | 79104 | 2.32 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 2.01 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.84 |  |
| memcpy at mem.c:34 | 54764 | 1.61 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.52 |  |
| gf_reduce at gf.c:79 | 50301 | 1.47 |  |
| clmul_leaf at gf2x_clmul.c:370 | 49440 | 1.45 |  |
| support_contains_value at vector.c:80 | 46812 | 1.37 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 1.26 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 866740 | 20.64 |  |
| xor | 643589 | 15.32 |  |
| bnez | 397116 | 9.46 |  |
| addi | 367525 | 8.75 |  |
| sd | 348842 | 8.31 |  |
| bne | 219468 | 5.23 |  |
| sb | 144373 | 3.44 |  |
| clmul | 119913 | 2.86 |  |
| clmulh | 114324 | 2.72 |  |
| lh | 110790 | 2.64 |  |
| rori | 98832 | 2.35 |  |
| andn | 85723 | 2.04 |  |
| add | 80318 | 1.91 |  |
| slli | 69006 | 1.64 |  |
| sh | 63067 | 1.50 |  |
| mv | 61265 | 1.46 |  |
| lbu | 50896 | 1.21 |  |
| or | 35065 | 0.83 |  |
| srli | 31105 | 0.74 |  |
| andi | 30827 | 0.73 |  |

