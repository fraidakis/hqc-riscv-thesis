# Full Analysis Report

- Variant: karatsuba_zbc_exp
- Level: 128
- Date: 2026-05-14 15:17:57

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 317645 | 2810 |
| 192 | vect_mul | 967470 | 2842 |
| 256 | vect_mul | 2142432 | 2832 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 457921 | 38620 |  |
| 128 | encaps | 988672 | 38620 |  |
| 128 | decaps | 2321938 | 38620 |  |
| 192 | keypair | 1220594 | 39652 |  |
| 192 | encaps | 2592040 | 39652 |  |
| 192 | decaps | 5078350 | 39652 |  |
| 256 | keypair | 2542185 | 46650 |  |
| 256 | encaps | 5384967 | 46650 |  |
| 256 | decaps | 10605780 | 46650 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1331 | 105 | 1226 | 149 | 14820877 | 7751749 | 2153161 | 1312790 |
| 192 | 3006 | 302 | 2704 | 724 | 35132080 | 20912527 | 4887848 | 3231162 |
| 256 | 6830 | 1088 | 5742 | 3167 | 69807567 | 43568198 | 9551448 | 6428820 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1381 | 1389 | 7941 | 11915 | 125 | 187 |
| 192 | 1484 | 1386 | 8335 | 12877 | 131 | 202 |
| 256 | 1474 | 1360 | 8267 | 12746 | 130 | 200 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf | 1230624 | 24.53 |  |
| karatsuba_clmul | 869328 | 17.33 |  |
| KeccakF1600_StatePermute | 578792 | 11.54 |  |
| gf_carryless_mul | 429828 | 8.57 |  |
| memset | 299769 | 5.98 |  |
| clmul64 | 228648 | 4.56 |  |
| hadamard | 227516 | 4.54 |  |
| keccak_inc_absorb | 141116 | 2.81 |  |
| memcpy | 137922 | 2.75 |  |
| find_peaks | 105919 | 2.11 |  |
| keccak_inc_squeeze | 96430 | 1.92 |  |
| expand_and_sum | 93748 | 1.87 |  |
| gf_reduce | 90606 | 1.81 |  |
| gf_mul | 84966 | 1.69 |  |
| compare_u32 | 83250 | 1.66 |  |
| vect_generate_random_support2 | 79260 | 1.58 |  |
| vect_generate_random_support1 | 49869 | 0.99 |  |
| vect_compare | 31058 | 0.62 |  |
| reduce | 19974 | 0.40 |  |
| vect_add | 17744 | 0.35 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul_leaf at gf2x_clmul.c:80 | 539892 | 10.76 |  |
| clmul_leaf at gf2x_clmul.c:78 | 269946 | 5.38 |  |
| clmul64 at gf2x_clmul.c:23 | 228648 | 4.56 |  |
| gf_carryless_mul at gf.c:132 | 209916 | 4.18 |  |
| memset at mem.c:40 | 199782 | 3.98 |  |
| karatsuba_clmul at gf2x_clmul.c:151 | 157332 | 3.14 |  |
| karatsuba_clmul at gf2x_clmul.c:149 | 134856 | 2.69 |  |
| karatsuba_clmul at gf2x_clmul.c:146 | 115284 | 2.30 |  |
| karatsuba_clmul at gf2x_clmul.c:143 | 111264 | 2.22 |  |
| memset at mem.c:41 | 99795 | 1.99 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 1.96 |  |
| clmul_leaf at gf2x_clmul.c:0 | 95910 | 1.91 |  |
| clmul_leaf at gf2x_clmul.c:79 | 89982 | 1.79 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 1.71 |  |
| compare_u32 at vector.c:42 | 83250 | 1.66 |  |
| memcpy at mem.c:18 | 82652 | 1.65 |  |
| hadamard at reed_muller.c:98 | 82432 | 1.64 |  |
| hadamard at reed_muller.c:99 | 82432 | 1.64 |  |
| karatsuba_clmul at gf2x_clmul.c:132 | 70332 | 1.40 |  |
| gf_carryless_mul at gf.c:121 | 69972 | 1.39 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| addi | 950103 | 16.28 |  |
| ld | 900431 | 15.43 |  |
| xor | 526630 | 9.03 |  |
| bnez | 412152 | 7.06 |  |
| bne | 353508 | 6.06 |  |
| sd | 270083 | 4.63 |  |
| add | 156186 | 2.68 |  |
| sb | 137793 | 2.36 |  |
| slli | 136553 | 2.34 |  |
| clmul | 114324 | 1.96 |  |
| clmulh | 114324 | 1.96 |  |
| lh | 110766 | 1.90 |  |
| mv | 101430 | 1.74 |  |
| rori | 98832 | 1.69 |  |
| and | 92866 | 1.59 |  |
| vslide1down.vx | 90750 | 1.56 |  |
| vsetivli | 90725 | 1.55 |  |
| vse64.v | 90124 | 1.54 |  |
| vmv.v.x | 90110 | 1.54 |  |
| vxor.vv | 90079 | 1.54 |  |

