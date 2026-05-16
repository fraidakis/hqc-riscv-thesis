# Full Analysis Report

- Variant: karatsuba_rvv
- Level: 128
- Date: 2026-05-16 18:02:56

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 270021 | 2932 |
| 192 | vect_mul | 823102 | 2964 |
| 256 | vect_mul | 1857509 | 2954 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 411289 | 41634 |  |
| 128 | encaps | 846696 | 41634 |  |
| 128 | decaps | 1740894 | 41634 |  |
| 192 | keypair | 1075457 | 42666 |  |
| 192 | encaps | 2222011 | 42666 |  |
| 192 | decaps | 4067779 | 42666 |  |
| 256 | keypair | 2254438 | 49664 |  |
| 256 | encaps | 4622790 | 49664 |  |
| 256 | decaps | 8288769 | 49664 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1286 | 82 | 1204 | 109 | 13282215 | 6525647 | 1964099 | 1005438 |
| 192 | 3021 | 323 | 2698 | 740 | 31569024 | 17884479 | 4447674 | 2537306 |
| 256 | 6887 | 1122 | 5765 | 3203 | 60436777 | 35227520 | 8380094 | 4896482 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1342 | 1356 | 8539 | 12225 | 134 | 192 |
| 192 | 1581 | 1507 | 10007 | 14433 | 157 | 226 |
| 256 | 1569 | 1466 | 9937 | 14126 | 156 | 221 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf | 1078674 | 29.40 |  |
| KeccakF1600_StatePermute | 578792 | 15.77 |  |
| karatsuba_clmul | 309024 | 8.42 |  |
| clmul64 | 251532 | 6.85 |  |
| hadamard | 227516 | 6.20 |  |
| keccak_inc_absorb | 141116 | 3.85 |  |
| memcpy | 137922 | 3.76 |  |
| find_peaks | 105919 | 2.89 |  |
| gf_reduce | 105600 | 2.88 |  |
| keccak_inc_squeeze | 96430 | 2.63 |  |
| expand_and_sum | 93748 | 2.55 |  |
| compare_u32 | 83250 | 2.27 |  |
| vect_generate_random_support2 | 79260 | 2.16 |  |
| rvv_xor3_u64 | 54684 | 1.49 |  |
| vect_generate_random_support1 | 49869 | 1.36 |  |
| rvv_copy_u64 | 33822 | 0.92 |  |
| memset | 33045 | 0.90 |  |
| vect_compare | 31058 | 0.85 |  |
| gf_mul | 23794 | 0.65 |  |
| reduce | 19974 | 0.54 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf at gf2x_clmul.c:179 | 309498 | 8.43 |  |
| clmul64 at gf2x_clmul.c:42 | 228648 | 6.23 |  |
| clmul_leaf at gf2x_clmul.c:176 | 176856 | 4.82 |  |
| clmul_leaf at gf2x_clmul.c:181 | 132642 | 3.61 |  |
| clmul_leaf at gf2x_clmul.c:0 | 102408 | 2.79 |  |
| KeccakF1600_StatePermute at fips202_rvv_only.c:0 | 98548 | 2.69 |  |
| clmul_leaf at gf2x_clmul.c:180 | 88428 | 2.41 |  |
| keccak_inc_absorb at fips202_rvv_only.c:290 | 85750 | 2.34 |  |
| compare_u32 at vector.c:43 | 83250 | 2.27 |  |
| memcpy at mem.c:21 | 82652 | 2.25 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.25 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.25 |  |
| karatsuba_clmul at gf2x_clmul.c:239 | 70332 | 1.92 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.87 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.71 |  |
| karatsuba_clmul at gf2x_clmul.c:241 | 56190 | 1.53 |  |
| karatsuba_clmul at gf2x_clmul.c:240 | 55788 | 1.52 |  |
| memcpy at mem.c:22 | 54764 | 1.49 |  |
| keccak_inc_squeeze at fips202_rvv_only.c:320 | 51696 | 1.41 |  |
| gf_reduce at gf.c:106 | 44712 | 1.22 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 753636 | 16.92 |  |
| addi | 525188 | 11.79 |  |
| xor | 470770 | 10.57 |  |
| bnez | 403187 | 9.05 |  |
| sd | 218123 | 4.90 |  |
| add | 168464 | 3.78 |  |
| clmul | 119322 | 2.68 |  |
| bne | 116430 | 2.61 |  |
| clmulh | 114324 | 2.57 |  |
| lh | 110766 | 2.49 |  |
| rori | 98832 | 2.22 |  |
| slli | 98763 | 2.22 |  |
| mv | 92876 | 2.08 |  |
| vsetivli | 89141 | 2.00 |  |
| bltu | 86492 | 1.94 |  |
| andn | 85375 | 1.92 |  |
| sh | 62575 | 1.40 |  |
| vle64.v | 61458 | 1.38 |  |
| subw | 54754 | 1.23 |  |
| vxor.vv | 54313 | 1.22 |  |

