# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-22 13:40:04

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
| 128 | keypair | 118245 | 47274 |  |
| 128 | encaps | 250310 | 47274 |  |
| 128 | decaps | 485507 | 47274 |  |
| 128 | total | 854062 | 47274 |  |
| 192 | keypair | 297433 | 50266 |  |
| 192 | encaps | 627377 | 50266 |  |
| 192 | decaps | 1110836 | 50266 |  |
| 192 | total | 2035646 | 50266 |  |
| 256 | keypair | 446536 | 59834 |  |
| 256 | encaps | 964409 | 59834 |  |
| 256 | decaps | 1726034 | 59834 |  |
| 256 | total | 3136979 | 59834 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1294 | 113 | 1181 | 117 | 12897959 | 7289519 | 1916059 | 1371514 |
| 192 | 3067 | 290 | 2777 | 786 | 30029668 | 20116643 | 4255232 | 3500348 |
| 256 | 6814 | 1025 | 5789 | 3153 | 59766447 | 42788474 | 8663120 | 7382908 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1534 | 1762 | 25172 | 22457 | 394 | 351 |
| 192 | 1684 | 1913 | 29393 | 25653 | 460 | 401 |
| 256 | 1785 | 2020 | 34715 | 28851 | 543 | 451 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vclmul_leaf | 258732 | 29.92 |  |
| KeccakF1600_StatePermute | 191564 | 22.16 |  |
| gf_reduce | 59451 | 6.88 |  |
| kmul_18 | 49368 | 5.71 |  |
| kmul_17 | 39294 | 4.54 |  |
| kmul_35 | 37356 | 4.32 |  |
| keccak_xor_bytes | 32276 | 3.73 |  |
| rol1_zbb | 17040 | 1.97 |  |
| kmul_34 | 13530 | 1.56 |  |
| gf_mul | 11402 | 1.32 |  |
| kmul_69 | 10080 | 1.17 |  |
| support_contains_value | 9579 | 1.11 |  |
| fft_rec | 8738 | 1.01 |  |
| kmul_70 | 8112 | 0.94 |  |
| reed_solomon_encode | 8058 | 0.93 |  |
| kmul_139 | 7104 | 0.82 |  |
| memset | 6856 | 0.79 |  |
| vect_generate_random_support2 | 6618 | 0.77 |  |
| vect_write_support_to_vector | 6498 | 0.75 |  |
| expand_and_sum | 5060 | 0.59 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| gf_reduce at gf.c:79 | 28161 | 3.26 |  |
| keccak_xor_bytes at fips202_zvbb_x2.c:255 | 24528 | 2.84 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:0 | 24010 | 2.78 |  |
| gf_reduce at gf.c:77 | 21903 | 2.53 |  |
| rol1_zbb at fips202_zvbb_x2.c:111 | 17040 | 1.97 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 1.71 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 1.25 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 1.25 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:155 | 10224 | 1.18 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:208 | 10224 | 1.18 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:191 | 10224 | 1.18 |  |
| kmul_35 at karatsuba_zvbc_specialized_gen.h:484 | 9504 | 1.10 |  |
| kmul_17 at karatsuba_zvbc_specialized_gen.h:74 | 8880 | 1.03 |  |
| vclmul_leaf at gf2x_vclmul.c:138 | 8748 | 1.01 |  |
| vclmul_leaf at gf2x_vclmul.c:158 | 8082 | 0.93 |  |
| vclmul_leaf at gf2x_vclmul.c:161 | 8082 | 0.93 |  |
| vclmul_leaf at gf2x_vclmul.c:159 | 8082 | 0.93 |  |
| vclmul_leaf at gf2x_vclmul.c:160 | 8082 | 0.93 |  |
| vclmul_leaf at gf2x_vclmul.c:167 | 8082 | 0.93 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:156 | 6816 | 0.79 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 422543 | 26.00 |  |
| bnez | 382973 | 23.56 |  |
| addi | 97954 | 6.03 |  |
| vxor.vv | 64813 | 3.99 |  |
| add | 63279 | 3.89 |  |
| vle64.v | 52947 | 3.26 |  |
| xor | 50930 | 3.13 |  |
| slli | 42837 | 2.64 |  |
| vse64.v | 39624 | 2.44 |  |
| vsetivli | 38529 | 2.37 |  |
| vslidedown.vi | 27329 | 1.68 |  |
| sd | 23792 | 1.46 |  |
| mv | 18073 | 1.11 |  |
| vmv.x.s | 17222 | 1.06 |  |
| vmv.v.x | 17157 | 1.06 |  |
| vl8r.v | 17040 | 1.05 |  |
| rori | 17040 | 1.05 |  |
| vrgather.vv | 13632 | 0.84 |  |
| vslide1up.vx | 13632 | 0.84 |  |
| li | 13118 | 0.81 |  |

