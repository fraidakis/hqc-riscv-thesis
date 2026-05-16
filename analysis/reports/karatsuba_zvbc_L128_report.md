# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-16 14:13:53

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
| 128 | keypair | 222751 | 38570 |  |
| 128 | encaps | 470405 | 38570 |  |
| 128 | decaps | 1094907 | 38570 |  |
| 192 | keypair | 509811 | 39600 |  |
| 192 | encaps | 1090292 | 39600 |  |
| 192 | decaps | 2199642 | 39600 |  |
| 256 | keypair | 770489 | 46598 |  |
| 256 | encaps | 1656485 | 46598 |  |
| 256 | decaps | 3563473 | 46598 |  |

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
| KeccakF1600_StatePermute | 521424 | 22.90 |  |
| karatsuba_vclmul | 309024 | 13.57 |  |
| vclmul_leaf | 254580 | 11.18 |  |
| keccak_inc_absorb | 141116 | 6.20 |  |
| memcpy | 137922 | 6.06 |  |
| find_peaks_rvv | 105965 | 4.65 |  |
| gf_reduce | 105600 | 4.64 |  |
| keccak_inc_squeeze | 96430 | 4.24 |  |
| compare_u32 | 83250 | 3.66 |  |
| vect_generate_random_support2 | 79260 | 3.48 |  |
| expand_and_sum | 55062 | 2.42 |  |
| rvv_xor3_u64 | 54282 | 2.38 |  |
| vect_generate_random_support1 | 49869 | 2.19 |  |
| rvv_copy_u64 | 33822 | 1.49 |  |
| memset | 33045 | 1.45 |  |
| vect_compare | 31058 | 1.36 |  |
| gf_mul | 23794 | 1.05 |  |
| reduce | 19974 | 0.88 |  |
| vect_add | 17744 | 0.78 |  |
| reed_muller_decode | 17095 | 0.75 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:148 | 194256 | 8.53 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:143 | 170400 | 7.48 |  |
| compare_u32 at vector.c:43 | 83250 | 3.66 |  |
| memcpy at mem.c:21 | 82652 | 3.63 |  |
| karatsuba_vclmul at gf2x_vclmul.c:286 | 70332 | 3.09 |  |
| karatsuba_vclmul at gf2x_vclmul.c:288 | 56190 | 2.47 |  |
| karatsuba_vclmul at gf2x_vclmul.c:287 | 55788 | 2.45 |  |
| memcpy at mem.c:22 | 54764 | 2.41 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:294 | 51696 | 2.27 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:? | 45156 | 1.98 |  |
| gf_reduce at gf.c:106 | 44712 | 1.96 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:259 | 43067 | 1.89 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:261 | 42875 | 1.88 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:260 | 42875 | 1.88 |  |
| karatsuba_vclmul at gf2x_vclmul.c:261 | 41496 | 1.82 |  |
| expand_and_sum at reed_muller_rvv.c:77 | 38272 | 1.68 |  |
| find_peaks_rvv at reed_muller_rvv.c:58 | 35328 | 1.55 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:293 | 34608 | 1.52 |  |
| vect_generate_random_support2 at vector.c:136 | 34188 | 1.50 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:145 | 34080 | 1.50 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 647664 | 21.09 |  |
| bnez | 402044 | 13.09 |  |
| addi | 292306 | 9.52 |  |
| xor | 267742 | 8.72 |  |
| sd | 181766 | 5.92 |  |
| add | 154581 | 5.03 |  |
| slli | 90313 | 2.94 |  |
| bne | 87752 | 2.86 |  |
| mv | 63888 | 2.08 |  |
| vle64.v | 50522 | 1.65 |  |
| or | 49165 | 1.60 |  |
| lbu | 48736 | 1.59 |  |
| sb | 47835 | 1.56 |  |
| vse64.v | 38574 | 1.26 |  |
| vxor.vv | 37849 | 1.23 |  |
| andi | 34169 | 1.11 |  |
| subw | 34146 | 1.11 |  |
| bltu | 32516 | 1.06 |  |
| lw | 24081 | 0.78 |  |
| sub | 24018 | 0.78 |  |

