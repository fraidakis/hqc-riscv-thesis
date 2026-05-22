# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-22 15:12:19

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
| 128 | keypair | 118025 | 47042 |  |
| 128 | encaps | 242738 | 47042 |  |
| 128 | decaps | 463581 | 47042 |  |
| 128 | total | 824344 | 47042 |  |
| 192 | keypair | 297213 | 50034 |  |
| 192 | encaps | 612536 | 50034 |  |
| 192 | decaps | 1067117 | 50034 |  |
| 192 | total | 1976866 | 50034 |  |
| 256 | keypair | 446316 | 59602 |  |
| 256 | encaps | 940855 | 59602 |  |
| 256 | decaps | 1656148 | 59602 |  |
| 256 | total | 3043319 | 59602 |  |

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
| 128 | 1473 | 1746 | 25034 | 22329 | 392 | 349 |
| 192 | 1620 | 1899 | 29256 | 25541 | 458 | 400 |
| 256 | 1720 | 2003 | 34552 | 28715 | 540 | 449 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vclmul_leaf | 258732 | 30.99 |  |
| KeccakF1600_StatePermute | 191564 | 22.95 |  |
| gf_reduce | 59451 | 7.12 |  |
| kmul_18 | 49368 | 5.91 |  |
| kmul_17 | 39294 | 4.71 |  |
| kmul_35 | 37356 | 4.48 |  |
| rol1_zbb | 17040 | 2.04 |  |
| kmul_34 | 13530 | 1.62 |  |
| gf_mul | 11402 | 1.37 |  |
| kmul_69 | 10080 | 1.21 |  |
| support_contains_value | 9579 | 1.15 |  |
| fft_rec | 8738 | 1.05 |  |
| kmul_70 | 8112 | 0.97 |  |
| reed_solomon_encode | 8058 | 0.97 |  |
| kmul_139 | 7104 | 0.85 |  |
| memset | 6856 | 0.82 |  |
| vect_generate_random_support2 | 6618 | 0.79 |  |
| vect_write_support_to_vector | 6498 | 0.78 |  |
| expand_and_sum | 5060 | 0.61 |  |
| >>>> | 5037 | 0.60 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| gf_reduce at gf.c:79 | 28161 | 3.37 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:0 | 24010 | 2.88 |  |
| gf_reduce at gf.c:77 | 21903 | 2.62 |  |
| rol1_zbb at fips202_zvbb_x2.c:111 | 17040 | 2.04 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 1.77 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 1.30 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 1.29 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:155 | 10224 | 1.22 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:208 | 10224 | 1.22 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:191 | 10224 | 1.22 |  |
| kmul_35 at karatsuba_zvbc_specialized_gen.h:484 | 9504 | 1.14 |  |
| kmul_17 at karatsuba_zvbc_specialized_gen.h:74 | 8880 | 1.06 |  |
| vclmul_leaf at gf2x_vclmul.c:138 | 8748 | 1.05 |  |
| vclmul_leaf at gf2x_vclmul.c:158 | 8082 | 0.97 |  |
| vclmul_leaf at gf2x_vclmul.c:161 | 8082 | 0.97 |  |
| vclmul_leaf at gf2x_vclmul.c:159 | 8082 | 0.97 |  |
| vclmul_leaf at gf2x_vclmul.c:160 | 8082 | 0.97 |  |
| vclmul_leaf at gf2x_vclmul.c:167 | 8082 | 0.97 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:156 | 6816 | 0.82 |  |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:157 | 6816 | 0.82 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 418732 | 26.33 |  |
| bnez | 380608 | 23.93 |  |
| addi | 94405 | 5.94 |  |
| vxor.vv | 64908 | 4.08 |  |
| add | 62441 | 3.93 |  |
| vle64.v | 52947 | 3.33 |  |
| xor | 49747 | 3.13 |  |
| slli | 40486 | 2.55 |  |
| vse64.v | 39624 | 2.49 |  |
| vsetivli | 38529 | 2.42 |  |
| vslidedown.vi | 27329 | 1.72 |  |
| sd | 22539 | 1.42 |  |
| vmv.x.s | 17222 | 1.08 |  |
| vmv.v.x | 17157 | 1.08 |  |
| rori | 17040 | 1.07 |  |
| vl8r.v | 17040 | 1.07 |  |
| mv | 16899 | 1.06 |  |
| vslide1up.vx | 13632 | 0.86 |  |
| vrgather.vv | 13632 | 0.86 |  |
| li | 13118 | 0.82 |  |

