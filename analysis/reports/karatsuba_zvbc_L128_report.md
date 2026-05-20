# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-20 22:03:49

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
| 128 | keypair | 126404 | 46920 |  |
| 128 | encaps | 263940 | 46920 |  |
| 128 | decaps | 547276 | 46920 |  |
| 128 | total | 937620 | 46920 |  |
| 192 | keypair | 313384 | 49620 |  |
| 192 | encaps | 654219 | 49620 |  |
| 192 | decaps | 1194948 | 49620 |  |
| 192 | total | 2162551 | 49620 |  |
| 256 | keypair | 472898 | 58320 |  |
| 256 | encaps | 1006398 | 58320 |  |
| 256 | decaps | 1927735 | 58320 |  |
| 256 | total | 3407031 | 58320 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1331 | 108 | 1223 | 154 | 12910791 | 7289519 | 1917663 | 1375322 |
| 192 | 3064 | 291 | 2773 | 772 | 30043884 | 20116643 | 4257009 | 3508188 |
| 256 | 6827 | 971 | 5856 | 3200 | 59748047 | 42788474 | 8660820 | 7395452 |

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
| vclmul_leaf | 258732 | 27.18 |  |
| KeccakF1600_StatePermute | 194388 | 20.42 |  |
| gf_reduce | 79950 | 8.40 |  |
| kmul_18 | 49368 | 5.19 |  |
| kmul_17 | 39294 | 4.13 |  |
| kmul_35 | 37356 | 3.92 |  |
| memcpy | 27090 | 2.85 |  |
| gf_mul | 17044 | 1.79 |  |
| rol1_zbb | 17040 | 1.79 |  |
| vect_add | 15978 | 1.68 |  |
| kmul_34 | 13530 | 1.42 |  |
| gf_square | 12489 | 1.31 |  |
| keccak_xor_bytes | 11252 | 1.18 |  |
| kmul_69 | 10080 | 1.06 |  |
| compute_elp | 9934 | 1.04 |  |
| keccak_extract_bytes | 9780 | 1.03 |  |
| support_contains_value | 9579 | 1.01 |  |
| fft_rec | 8738 | 0.92 |  |
| >>>> | 8541 | 0.90 |  |
| kmul_70 | 8112 | 0.85 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| gf_reduce at gf.c:106 | 33912 | 3.56 |  |
| gf_reduce at gf.c:105 | 25434 | 2.67 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:0 | 24906 | 2.62 |  |
| rol1_zbb at fips202_zvbb_gather.c:129 | 17040 | 1.79 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 1.56 |  |
| gf_reduce at gf.c:97 | 12717 | 1.34 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 1.14 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 1.13 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:173 | 10224 | 1.07 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:209 | 10224 | 1.07 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:226 | 10224 | 1.07 |  |
| kmul_35 at karatsuba_zvbc_specialized_gen.h:484 | 9504 | 1.00 |  |
| kmul_17 at karatsuba_zvbc_specialized_gen.h:74 | 8880 | 0.93 |  |
| gf_square at gf.c:201 | 8865 | 0.93 |  |
| vclmul_leaf at gf2x_vclmul.c:138 | 8748 | 0.92 |  |
| >>>> | 8541 | 0.90 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:166 | 8352 | 0.88 |  |
| vclmul_leaf at gf2x_vclmul.c:158 | 8082 | 0.85 |  |
| vclmul_leaf at gf2x_vclmul.c:161 | 8082 | 0.85 |  |
| vclmul_leaf at gf2x_vclmul.c:159 | 8082 | 0.85 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 428660 | 25.14 |  |
| bnez | 383401 | 22.48 |  |
| addi | 111190 | 6.52 |  |
| add | 70021 | 4.11 |  |
| vxor.vv | 63872 | 3.75 |  |
| xor | 62001 | 3.64 |  |
| vle64.v | 53103 | 3.11 |  |
| slli | 50756 | 2.98 |  |
| vse64.v | 39552 | 2.32 |  |
| vsetivli | 38796 | 2.28 |  |
| sd | 28257 | 1.66 |  |
| vslidedown.vi | 27387 | 1.61 |  |
| mv | 22679 | 1.33 |  |
| vmv.x.s | 17207 | 1.01 |  |
| vmv.v.x | 17041 | 1.00 |  |
| vl8r.v | 17040 | 1.00 |  |
| rori | 17040 | 1.00 |  |
| li | 15297 | 0.90 |  |
| zext.b | 14361 | 0.84 |  |
| vslide1up.vx | 13661 | 0.80 |  |

