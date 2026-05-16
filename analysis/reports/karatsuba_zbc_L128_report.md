# Full Analysis Report

- Variant: karatsuba_zbc
- Level: 128
- Date: 2026-05-17 00:11:11

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 274566 | 5806 |
| 192 | vect_mul | 848395 | 5838 |
| 256 | vect_mul | 1907358 | 5828 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 415903 | 41624 |  |
| 128 | encaps | 855901 | 41624 |  |
| 128 | decaps | 1754700 | 41624 |  |
| 192 | keypair | 1100819 | 42656 |  |
| 192 | encaps | 2272712 | 42656 |  |
| 192 | decaps | 4143829 | 42656 |  |
| 256 | keypair | 2304356 | 49654 |  |
| 256 | encaps | 4722603 | 49654 |  |
| 256 | decaps | 8438487 | 49654 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1326 | 106 | 1220 | 145 | 13233741 | 6525701 | 1958045 | 1005450 |
| 192 | 3008 | 294 | 2714 | 726 | 31302176 | 17884479 | 4414318 | 2537306 |
| 256 | 7007 | 1150 | 5857 | 3314 | 60436825 | 35227520 | 8380100 | 4896482 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1431 | 1426 | 10747 | 14337 | 168 | 225 |
| 192 | 1656 | 1556 | 12117 | 16389 | 190 | 257 |
| 256 | 1564 | 1445 | 9839 | 13954 | 154 | 219 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf | 949248 | 25.38 |  |
| KeccakF1600_StatePermute | 578792 | 15.48 |  |
| karatsuba_clmul | 527550 | 14.11 |  |
| clmul64 | 248424 | 6.64 |  |
| hadamard | 227516 | 6.08 |  |
| keccak_inc_absorb | 141116 | 3.77 |  |
| memcpy | 137922 | 3.69 |  |
| find_peaks | 105919 | 2.83 |  |
| gf_reduce | 105600 | 2.82 |  |
| keccak_inc_squeeze | 96430 | 2.58 |  |
| expand_and_sum | 93748 | 2.51 |  |
| compare_u32 | 83250 | 2.23 |  |
| vect_generate_random_support2 | 79260 | 2.12 |  |
| clmul_leaf_n8 | 72816 | 1.95 |  |
| vect_generate_random_support1 | 49869 | 1.33 |  |
| memset | 33045 | 0.88 |  |
| vect_compare | 31058 | 0.83 |  |
| gf_mul | 23794 | 0.64 |  |
| reduce | 19974 | 0.53 |  |
| vect_add | 17744 | 0.47 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf at gf2x_clmul.c:442 | 276864 | 7.40 |  |
| clmul64 at gf2x_clmul.c:38 | 228648 | 6.11 |  |
| clmul_leaf at gf2x_clmul.c:439 | 158208 | 4.23 |  |
| clmul_leaf at gf2x_clmul.c:444 | 118656 | 3.17 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 2.64 |  |
| clmul_leaf at gf2x_clmul.c:0 | 87756 | 2.35 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 2.29 |  |
| compare_u32 at vector.c:43 | 83250 | 2.23 |  |
| memcpy at mem.c:21 | 82652 | 2.21 |  |
| hadamard at reed_muller.c:98 | 82432 | 2.20 |  |
| hadamard at reed_muller.c:99 | 82432 | 2.20 |  |
| clmul_leaf at gf2x_clmul.c:443 | 79104 | 2.12 |  |
| karatsuba_clmul at gf2x_clmul.c:503 | 70332 | 1.88 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 1.83 |  |
| karatsuba_clmul at gf2x_clmul.c:504 | 67026 | 1.79 |  |
| hadamard at reed_muller.c:97 | 62606 | 1.67 |  |
| karatsuba_clmul at gf2x_clmul.c:505 | 56190 | 1.50 |  |
| memcpy at mem.c:22 | 54764 | 1.46 |  |
| karatsuba_clmul at gf2x_clmul.c:547 | 51840 | 1.39 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 1.38 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 764437 | 16.78 |  |
| addi | 580955 | 12.75 |  |
| xor | 472078 | 10.36 |  |
| bnez | 411642 | 9.04 |  |
| sd | 218861 | 4.80 |  |
| add | 159464 | 3.50 |  |
| bne | 119766 | 2.63 |  |
| clmul | 119322 | 2.62 |  |
| clmulh | 114324 | 2.51 |  |
| lh | 110766 | 2.43 |  |
| rori | 98832 | 2.17 |  |
| slli | 93447 | 2.05 |  |
| mv | 92666 | 2.03 |  |
| vsetivli | 87677 | 1.92 |  |
| andn | 85375 | 1.87 |  |
| bltu | 79976 | 1.76 |  |
| vle64.v | 77016 | 1.69 |  |
| sh | 62575 | 1.37 |  |
| vse64.v | 61988 | 1.36 |  |
| vxor.vv | 61417 | 1.35 |  |

