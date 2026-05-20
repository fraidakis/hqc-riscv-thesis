# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-20 21:05:41

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
| 128 | keypair | 135010 | 47720 |  |
| 128 | encaps | 282909 | 47720 |  |
| 128 | decaps | 570548 | 47720 |  |
| 128 | total | 988467 | 47720 |  |
| 192 | keypair | 327008 | 50422 |  |
| 192 | encaps | 687021 | 50422 |  |
| 192 | decaps | 1234562 | 50422 |  |
| 192 | total | 2248591 | 50422 |  |
| 256 | keypair | 492002 | 59132 |  |
| 256 | encaps | 1052805 | 59132 |  |
| 256 | decaps | 1983761 | 59132 |  |
| 256 | total | 3528568 | 59132 |  |

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
| 128 | 1600 | 1829 | 25510 | 23657 | 399 | 370 |
| 192 | 1723 | 1903 | 29498 | 26361 | 461 | 412 |
| 256 | 1806 | 1960 | 34869 | 29509 | 545 | 462 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vclmul_leaf | 258732 | 25.80 |  |
| KeccakF1600_StatePermute | 194388 | 19.39 |  |
| gf_reduce | 79950 | 7.97 |  |
| kmul_18 | 49368 | 4.92 |  |
| kmul_17 | 39294 | 3.92 |  |
| kmul_35 | 37356 | 3.73 |  |
| vect_generate_random_support2 | 30252 | 3.02 |  |
| memcpy | 27090 | 2.70 |  |
| compare_u32 | 21090 | 2.10 |  |
| vect_generate_random_support1 | 19365 | 1.93 |  |
| gf_mul | 17044 | 1.70 |  |
| rol1_zbb | 17040 | 1.70 |  |
| vect_add | 15978 | 1.59 |  |
| kmul_34 | 13530 | 1.35 |  |
| gf_square | 12489 | 1.25 |  |
| keccak_xor_bytes | 11252 | 1.12 |  |
| kmul_69 | 10080 | 1.01 |  |
| compute_elp | 9934 | 0.99 |  |
| keccak_extract_bytes | 9780 | 0.98 |  |
| fft_rec | 8738 | 0.87 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| gf_reduce at gf.c:106 | 33912 | 3.38 |  |
| gf_reduce at gf.c:105 | 25434 | 2.54 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:0 | 24906 | 2.48 |  |
| compare_u32 at vector.c:43 | 21090 | 2.10 |  |
| rol1_zbb at fips202_zvbb_gather.c:129 | 17040 | 1.70 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 1.48 |  |
| gf_reduce at gf.c:97 | 12717 | 1.27 |  |
| vect_generate_random_support2 at vector.c:136 | 11844 | 1.18 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 1.08 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 1.07 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:173 | 10224 | 1.02 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:209 | 10224 | 1.02 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:226 | 10224 | 1.02 |  |
| vect_generate_random_support2 at vector.c:137 | 9780 | 0.98 |  |
| kmul_35 at karatsuba_zvbc_specialized_gen.h:484 | 9504 | 0.95 |  |
| kmul_17 at karatsuba_zvbc_specialized_gen.h:74 | 8880 | 0.89 |  |
| gf_square at gf.c:201 | 8865 | 0.88 |  |
| vclmul_leaf at gf2x_vclmul.c:138 | 8748 | 0.87 |  |
| >>>> | 8538 | 0.85 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:166 | 8352 | 0.83 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 431039 | 24.49 |  |
| bnez | 386317 | 21.95 |  |
| addi | 119449 | 6.79 |  |
| add | 70783 | 4.02 |  |
| vxor.vv | 63872 | 3.63 |  |
| xor | 63444 | 3.60 |  |
| vle64.v | 53103 | 3.02 |  |
| slli | 50816 | 2.89 |  |
| vse64.v | 39552 | 2.25 |  |
| vsetivli | 39150 | 2.22 |  |
| sd | 28266 | 1.61 |  |
| vslidedown.vi | 27387 | 1.56 |  |
| mv | 23018 | 1.31 |  |
| vmv.x.s | 17561 | 1.00 |  |
| vmv.v.x | 17041 | 0.97 |  |
| vl8r.v | 17040 | 0.97 |  |
| rori | 17040 | 0.97 |  |
| bne | 15766 | 0.90 |  |
| li | 14649 | 0.83 |  |
| zext.b | 14556 | 0.83 |  |

