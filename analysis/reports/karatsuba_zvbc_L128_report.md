# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-16 10:48:25

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 90302 | 2924 |
| 192 | vect_mul | 273547 | 2956 |
| 256 | vect_mul | 399347 | 2946 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 222751 | 38828 |  |
| 128 | encaps | 470405 | 38828 |  |
| 128 | decaps | 1170800 | 38828 |  |
| 192 | keypair | 509811 | 39860 |  |
| 192 | encaps | 1090292 | 39860 |  |
| 192 | decaps | 2358685 | 39860 |  |
| 256 | keypair | 770489 | 46858 |  |
| 256 | encaps | 1656485 | 46858 |  |
| 256 | decaps | 3819110 | 46858 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1296 | 90 | 1206 | 119 | 13332359 | 7557647 | 1970367 | 1134438 |
| 192 | 3036 | 310 | 2726 | 745 | 31528010 | 21002505 | 4442542 | 2927054 |
| 256 | 6989 | 1128 | 5861 | 3275 | 60342953 | 43200896 | 8368366 | 5893154 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1319 | 1331 | 9083 | 12697 | 142 | 199 |
| 192 | 1546 | 1473 | 10455 | 14833 | 164 | 232 |
| 256 | 1536 | 1434 | 10401 | 14542 | 163 | 228 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 521424 | 20.56 |  |
| karatsuba_vclmul | 309024 | 12.19 |  |
| vclmul_leaf | 254580 | 10.04 |  |
| hadamard | 227516 | 8.97 |  |
| keccak_inc_absorb | 141116 | 5.56 |  |
| memcpy | 137922 | 5.44 |  |
| find_peaks | 105919 | 4.18 |  |
| gf_reduce | 105600 | 4.16 |  |
| keccak_inc_squeeze | 96430 | 3.80 |  |
| expand_and_sum | 93748 | 3.70 |  |
| compare_u32 | 83250 | 3.28 |  |
| vect_generate_random_support2 | 79260 | 3.13 |  |
| rvv_xor3_u64 | 54282 | 2.14 |  |
| vect_generate_random_support1 | 49869 | 1.97 |  |
| rvv_copy_u64 | 33822 | 1.33 |  |
| memset | 33045 | 1.30 |  |
| vect_compare | 31058 | 1.22 |  |
| gf_mul | 23794 | 0.94 |  |
| reduce | 19974 | 0.79 |  |
| vect_add | 17744 | 0.70 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:148 | 194256 | 7.66 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:143 | 170400 | 6.72 |  |
| compare_u32 at vector.c:43 | 83250 | 3.28 |  |
| memcpy at mem.c:21 | 82652 | 3.26 |  |
| hadamard at reed_muller.c:98 | 82432 | 3.25 |  |
| hadamard at reed_muller.c:99 | 82432 | 3.25 |  |
| karatsuba_vclmul at gf2x_vclmul.c:286 | 70332 | 2.77 |  |
| expand_and_sum at reed_muller.c:132 | 68448 | 2.70 |  |
| hadamard at reed_muller.c:97 | 62606 | 2.47 |  |
| karatsuba_vclmul at gf2x_vclmul.c:288 | 56190 | 2.22 |  |
| karatsuba_vclmul at gf2x_vclmul.c:287 | 55788 | 2.20 |  |
| memcpy at mem.c:22 | 54764 | 2.16 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:294 | 51696 | 2.04 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:0 | 45156 | 1.78 |  |
| gf_reduce at gf.c:106 | 44712 | 1.76 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:259 | 43067 | 1.70 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:261 | 42875 | 1.69 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:260 | 42875 | 1.69 |  |
| karatsuba_vclmul at gf2x_vclmul.c:261 | 41496 | 1.64 |  |
| find_peaks at reed_muller.c:155 | 35328 | 1.39 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 647868 | 19.47 |  |
| bnez | 402345 | 12.09 |  |
| addi | 331715 | 9.97 |  |
| xor | 267742 | 8.05 |  |
| add | 183092 | 5.50 |  |
| sd | 181759 | 5.46 |  |
| lh | 110766 | 3.33 |  |
| bne | 108912 | 3.27 |  |
| slli | 102821 | 3.09 |  |
| mv | 64166 | 1.93 |  |
| sh | 62575 | 1.88 |  |
| subw | 54754 | 1.65 |  |
| vle64.v | 50522 | 1.52 |  |
| lbu | 49840 | 1.50 |  |
| or | 49165 | 1.48 |  |
| sb | 47835 | 1.44 |  |
| vse64.v | 38574 | 1.16 |  |
| vxor.vv | 37849 | 1.14 |  |
| andi | 36377 | 1.09 |  |
| srli | 33678 | 1.01 |  |

