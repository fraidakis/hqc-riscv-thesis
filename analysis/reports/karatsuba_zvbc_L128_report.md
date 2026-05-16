# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-16 18:04:09

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
| 128 | keypair | 222751 | 37030 |  |
| 128 | encaps | 470405 | 37030 |  |
| 128 | decaps | 953889 | 37030 |  |
| 192 | keypair | 509811 | 38398 |  |
| 192 | encaps | 1090292 | 38398 |  |
| 192 | decaps | 2013781 | 38398 |  |
| 256 | keypair | 770489 | 45396 |  |
| 256 | encaps | 1656485 | 45396 |  |
| 256 | decaps | 3264788 | 45396 |  |

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
| KeccakF1600_StatePermute | 521424 | 24.78 |  |
| karatsuba_vclmul | 309024 | 14.69 |  |
| vclmul_leaf | 254580 | 12.10 |  |
| keccak_inc_absorb | 141116 | 6.71 |  |
| memcpy | 137922 | 6.55 |  |
| gf_reduce | 105600 | 5.02 |  |
| keccak_inc_squeeze | 96430 | 4.58 |  |
| compare_u32 | 83250 | 3.96 |  |
| vect_generate_random_support2 | 79260 | 3.77 |  |
| rvv_xor3_u64 | 54282 | 2.58 |  |
| vect_generate_random_support1 | 49869 | 2.37 |  |
| rvv_copy_u64 | 33822 | 1.61 |  |
| memset | 33045 | 1.57 |  |
| vect_compare | 31058 | 1.48 |  |
| gf_mul | 23794 | 1.13 |  |
| reduce | 19974 | 0.95 |  |
| vect_add | 17744 | 0.84 |  |
| compute_syndromes | 13895 | 0.66 |  |
| gf_square | 12489 | 0.59 |  |
| compute_elp | 10382 | 0.49 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:148 | 194256 | 9.23 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:143 | 170400 | 8.10 |  |
| compare_u32 at vector.c:43 | 83250 | 3.96 |  |
| memcpy at mem.c:21 | 82652 | 3.93 |  |
| karatsuba_vclmul at gf2x_vclmul.c:286 | 70332 | 3.34 |  |
| karatsuba_vclmul at gf2x_vclmul.c:288 | 56190 | 2.67 |  |
| karatsuba_vclmul at gf2x_vclmul.c:287 | 55788 | 2.65 |  |
| memcpy at mem.c:22 | 54764 | 2.60 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:294 | 51696 | 2.46 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:? | 45156 | 2.15 |  |
| gf_reduce at gf.c:106 | 44712 | 2.12 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:259 | 43067 | 2.05 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:261 | 42875 | 2.04 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:260 | 42875 | 2.04 |  |
| karatsuba_vclmul at gf2x_vclmul.c:261 | 41496 | 1.97 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:293 | 34608 | 1.64 |  |
| vect_generate_random_support2 at vector.c:136 | 34188 | 1.62 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:145 | 34080 | 1.62 |  |
| gf_reduce at gf.c:105 | 33534 | 1.59 |  |
| vect_generate_random_support2 at vector.c:137 | 33300 | 1.58 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 648689 | 22.39 |  |
| bnez | 403170 | 13.91 |  |
| addi | 276293 | 9.54 |  |
| xor | 267742 | 9.24 |  |
| sd | 181755 | 6.27 |  |
| add | 150570 | 5.20 |  |
| slli | 88239 | 3.05 |  |
| bne | 87706 | 3.03 |  |
| vle64.v | 50522 | 1.74 |  |
| lbu | 48736 | 1.68 |  |
| sb | 47835 | 1.65 |  |
| or | 43277 | 1.49 |  |
| vse64.v | 38574 | 1.33 |  |
| vxor.vv | 37849 | 1.31 |  |
| mv | 34809 | 1.20 |  |
| andi | 34169 | 1.18 |  |
| subw | 34146 | 1.18 |  |
| bltu | 32516 | 1.12 |  |
| lw | 24081 | 0.83 |  |
| sub | 23648 | 0.82 |  |

