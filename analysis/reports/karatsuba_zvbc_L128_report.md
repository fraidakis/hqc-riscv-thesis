# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-20 19:22:24

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 72644 | 10368 |
| 192 | vect_mul | 219338 | 11830 |
| 256 | vect_mul | 326260 | 13724 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 155539 | 52186 |  |
| 128 | encaps | 335516 | 52186 |  |
| 128 | decaps | 723152 | 52186 |  |
| 128 | total | 1214207 | 52186 |  |
| 192 | keypair | 365814 | 54990 |  |
| 192 | encaps | 788030 | 54990 |  |
| 192 | decaps | 1507071 | 54990 |  |
| 192 | total | 2660915 | 54990 |  |
| 256 | keypair | 554405 | 63864 |  |
| 256 | encaps | 1212462 | 63864 |  |
| 256 | decaps | 2495396 | 63864 |  |
| 256 | total | 4262263 | 63864 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1327 | 109 | 1218 | 152 | 12918463 | 7289855 | 1927022 | 1375532 |
| 192 | 3058 | 280 | 2778 | 763 | 30039764 | 20116979 | 4270662 | 3508398 |
| 256 | 6567 | 903 | 5664 | 2940 | 59770103 | 42788810 | 8683037 | 7395662 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1555 | 1746 | 25118 | 23851 | 393 | 373 |
| 192 | 1662 | 1810 | 28884 | 26840 | 452 | 420 |
| 256 | 1772 | 1876 | 34498 | 29912 | 540 | 468 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vclmul_leaf | 258732 | 21.08 |  |
| KeccakF1600_StatePermute | 191639 | 15.61 |  |
| keccak_inc_absorb | 149040 | 12.14 |  |
| gf_reduce | 105600 | 8.60 |  |
| keccak_inc_squeeze | 96890 | 7.89 |  |
| kmul_18 | 49368 | 4.02 |  |
| kmul_17 | 39294 | 3.20 |  |
| kmul_35 | 37356 | 3.04 |  |
| vect_generate_random_support2 | 30252 | 2.46 |  |
| gf_mul | 23794 | 1.94 |  |
| compare_u32 | 21090 | 1.72 |  |
| vect_generate_random_support1 | 19365 | 1.58 |  |
| rol1_zbb | 17040 | 1.39 |  |
| vect_add | 15978 | 1.30 |  |
| compute_syndromes | 13895 | 1.13 |  |
| kmul_34 | 13530 | 1.10 |  |
| gf_square | 12489 | 1.02 |  |
| compute_elp | 10295 | 0.84 |  |
| kmul_69 | 10080 | 0.82 |  |
| fft_rec | 8738 | 0.71 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| keccak_inc_squeeze at fips202_zvbb_gather.c:342 | 51696 | 4.21 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:307 | 51578 | 4.20 |  |
| gf_reduce at gf.c:106 | 44712 | 3.64 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:309 | 42875 | 3.49 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:308 | 42875 | 3.49 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:341 | 34536 | 2.81 |  |
| gf_reduce at gf.c:105 | 33534 | 2.73 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:0 | 24082 | 1.96 |  |
| compare_u32 at vector.c:43 | 21090 | 1.72 |  |
| rol1_zbb at fips202_zvbb_gather.c:129 | 17040 | 1.39 |  |
| gf_reduce at gf.c:97 | 16767 | 1.37 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 1.21 |  |
| vect_generate_random_support2 at vector.c:136 | 11844 | 0.96 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:226 | 11760 | 0.96 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 0.88 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 0.88 |  |
| gf_reduce at gf.c:98 | 10587 | 0.86 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:173 | 10224 | 0.83 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:209 | 10224 | 0.83 |  |
| vect_generate_random_support2 at vector.c:137 | 9780 | 0.80 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 454619 | 22.90 |  |
| bnez | 383430 | 19.31 |  |
| addi | 148068 | 7.46 |  |
| add | 100815 | 5.08 |  |
| xor | 83718 | 4.22 |  |
| slli | 78627 | 3.96 |  |
| vxor.vv | 63149 | 3.18 |  |
| vle64.v | 52803 | 2.66 |  |
| vse64.v | 39571 | 1.99 |  |
| vsetivli | 39139 | 1.97 |  |
| sd | 34553 | 1.74 |  |
| andi | 33716 | 1.70 |  |
| vslidedown.vi | 27361 | 1.38 |  |
| bne | 26997 | 1.36 |  |
| srli | 24345 | 1.23 |  |
| zext.b | 19956 | 1.01 |  |
| bltu | 18898 | 0.95 |  |
| mv | 17694 | 0.89 |  |
| vmv.x.s | 17471 | 0.88 |  |
| rori | 17040 | 0.86 |  |

