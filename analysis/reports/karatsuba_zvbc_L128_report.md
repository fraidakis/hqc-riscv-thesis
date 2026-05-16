# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-16 16:40:29

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
| 128 | keypair | 228823 | 37110 |  |
| 128 | encaps | 481997 | 37110 |  |
| 128 | decaps | 975141 | 37110 |  |
| 192 | keypair | 520851 | 38478 |  |
| 192 | encaps | 1112648 | 38478 |  |
| 192 | decaps | 2055181 | 38478 |  |
| 256 | keypair | 788153 | 45476 |  |
| 256 | encaps | 1690709 | 45476 |  |
| 256 | decaps | 3329924 | 45476 |  |

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
| KeccakF1600_StatePermute | 560616 | 26.15 |  |
| karatsuba_vclmul | 309024 | 14.42 |  |
| vclmul_leaf | 254580 | 11.88 |  |
| keccak_inc_absorb | 141116 | 6.58 |  |
| memcpy | 137922 | 6.43 |  |
| gf_reduce | 105600 | 4.93 |  |
| keccak_inc_squeeze | 96430 | 4.50 |  |
| compare_u32 | 83250 | 3.88 |  |
| vect_generate_random_support2 | 79260 | 3.70 |  |
| rvv_xor3_u64 | 54282 | 2.53 |  |
| vect_generate_random_support1 | 49869 | 2.33 |  |
| rvv_copy_u64 | 33822 | 1.58 |  |
| memset | 33045 | 1.54 |  |
| vect_compare | 31058 | 1.45 |  |
| gf_mul | 23794 | 1.11 |  |
| reduce | 19974 | 0.93 |  |
| vect_add | 17744 | 0.83 |  |
| compute_syndromes | 13895 | 0.65 |  |
| gf_square | 12489 | 0.58 |  |
| compute_elp | 10382 | 0.48 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:154 | 194256 | 9.06 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:149 | 170400 | 7.95 |  |
| compare_u32 at vector.c:43 | 83250 | 3.88 |  |
| memcpy at mem.c:21 | 82652 | 3.86 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:? | 77532 | 3.62 |  |
| karatsuba_vclmul at gf2x_vclmul.c:286 | 70332 | 3.28 |  |
| karatsuba_vclmul at gf2x_vclmul.c:288 | 56190 | 2.62 |  |
| karatsuba_vclmul at gf2x_vclmul.c:287 | 55788 | 2.60 |  |
| memcpy at mem.c:22 | 54764 | 2.55 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:295 | 51696 | 2.41 |  |
| gf_reduce at gf.c:106 | 44712 | 2.09 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:260 | 43067 | 2.01 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:262 | 42875 | 2.00 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:261 | 42875 | 2.00 |  |
| karatsuba_vclmul at gf2x_vclmul.c:261 | 41496 | 1.94 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:294 | 34608 | 1.61 |  |
| vect_generate_random_support2 at vector.c:136 | 34188 | 1.59 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:151 | 34080 | 1.59 |  |
| gf_reduce at gf.c:105 | 33534 | 1.56 |  |
| vect_generate_random_support2 at vector.c:137 | 33300 | 1.55 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 649093 | 22.04 |  |
| bnez | 403574 | 13.71 |  |
| addi | 283393 | 9.62 |  |
| xor | 267742 | 9.09 |  |
| sd | 181755 | 6.17 |  |
| add | 165054 | 5.61 |  |
| slli | 88523 | 3.01 |  |
| bne | 87706 | 2.98 |  |
| vle64.v | 50522 | 1.72 |  |
| lbu | 48736 | 1.66 |  |
| sb | 47835 | 1.62 |  |
| or | 43277 | 1.47 |  |
| vse64.v | 38574 | 1.31 |  |
| vxor.vv | 37849 | 1.29 |  |
| mv | 34809 | 1.18 |  |
| andi | 34169 | 1.16 |  |
| subw | 34146 | 1.16 |  |
| bltu | 32516 | 1.10 |  |
| csrr | 29352 | 1.00 |  |
| lw | 24081 | 0.82 |  |

