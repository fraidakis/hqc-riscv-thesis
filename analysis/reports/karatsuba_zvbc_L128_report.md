# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-20 22:16:11

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
| 128 | keypair | 126404 | 46534 |  |
| 128 | encaps | 263940 | 46534 |  |
| 128 | decaps | 536454 | 46534 |  |
| 128 | total | 926798 | 46534 |  |
| 192 | keypair | 313384 | 49234 |  |
| 192 | encaps | 654219 | 49234 |  |
| 192 | decaps | 1179590 | 49234 |  |
| 192 | total | 2147193 | 49234 |  |
| 256 | keypair | 472898 | 57934 |  |
| 256 | encaps | 1006398 | 57934 |  |
| 256 | decaps | 1906657 | 57934 |  |
| 256 | total | 3385953 | 57934 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1331 | 108 | 1223 | 154 | 12917367 | 7289519 | 1918485 | 1375322 |
| 192 | 3064 | 291 | 2773 | 772 | 30046748 | 20116643 | 4257367 | 3508188 |
| 256 | 6827 | 971 | 5856 | 3200 | 59756671 | 42788474 | 8661898 | 7395452 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1595 | 1803 | 25573 | 23441 | 400 | 367 |
| 192 | 1718 | 1883 | 29561 | 26193 | 462 | 410 |
| 256 | 1803 | 1951 | 34948 | 29429 | 547 | 460 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vclmul_leaf | 258732 | 27.50 |  |
| KeccakF1600_StatePermute | 194388 | 20.66 |  |
| gf_reduce | 80541 | 8.56 |  |
| kmul_18 | 49368 | 5.25 |  |
| kmul_17 | 39294 | 4.18 |  |
| kmul_35 | 37356 | 3.97 |  |
| memcpy | 27090 | 2.88 |  |
| rol1_zbb | 17040 | 1.81 |  |
| gf_mul | 16952 | 1.80 |  |
| vect_add | 15978 | 1.70 |  |
| kmul_34 | 13530 | 1.44 |  |
| keccak_xor_bytes | 11252 | 1.20 |  |
| kmul_69 | 10080 | 1.07 |  |
| compute_elp | 9934 | 1.06 |  |
| keccak_extract_bytes | 9780 | 1.04 |  |
| support_contains_value | 9579 | 1.02 |  |
| fft_rec | 8738 | 0.93 |  |
| >>>> | 8541 | 0.91 |  |
| kmul_70 | 8112 | 0.86 |  |
| compute_error_values | 8089 | 0.86 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| gf_reduce at gf.c:79 | 38151 | 4.05 |  |
| gf_reduce at gf.c:77 | 29673 | 3.15 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:0 | 24906 | 2.65 |  |
| rol1_zbb at fips202_zvbb_gather.c:129 | 17040 | 1.81 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 1.57 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 1.15 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 1.15 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:173 | 10224 | 1.09 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:209 | 10224 | 1.09 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:226 | 10224 | 1.09 |  |
| kmul_35 at karatsuba_zvbc_specialized_gen.h:484 | 9504 | 1.01 |  |
| kmul_17 at karatsuba_zvbc_specialized_gen.h:74 | 8880 | 0.94 |  |
| vclmul_leaf at gf2x_vclmul.c:138 | 8748 | 0.93 |  |
| >>>> | 8541 | 0.91 |  |
| gf_reduce at gf.c:76 | 8478 | 0.90 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:166 | 8352 | 0.89 |  |
| vclmul_leaf at gf2x_vclmul.c:158 | 8082 | 0.86 |  |
| vclmul_leaf at gf2x_vclmul.c:161 | 8082 | 0.86 |  |
| vclmul_leaf at gf2x_vclmul.c:159 | 8082 | 0.86 |  |
| vclmul_leaf at gf2x_vclmul.c:160 | 8082 | 0.86 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 429071 | 25.31 |  |
| bnez | 383812 | 22.64 |  |
| addi | 111190 | 6.56 |  |
| add | 70021 | 4.13 |  |
| vxor.vv | 63872 | 3.77 |  |
| xor | 62592 | 3.69 |  |
| vle64.v | 53103 | 3.13 |  |
| slli | 48392 | 2.85 |  |
| vse64.v | 39552 | 2.33 |  |
| vsetivli | 38796 | 2.29 |  |
| sd | 28257 | 1.67 |  |
| vslidedown.vi | 27387 | 1.62 |  |
| mv | 22679 | 1.34 |  |
| vmv.x.s | 17207 | 1.01 |  |
| vmv.v.x | 17041 | 1.01 |  |
| rori | 17040 | 1.01 |  |
| vl8r.v | 17040 | 1.01 |  |
| li | 15297 | 0.90 |  |
| zext.b | 14991 | 0.88 |  |
| vslide1up.vx | 13661 | 0.81 |  |

