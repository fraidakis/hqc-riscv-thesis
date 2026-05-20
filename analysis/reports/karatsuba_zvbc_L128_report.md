# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-20 20:08:12

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
| 128 | keypair | 155539 | 50952 |  |
| 128 | encaps | 335585 | 50952 |  |
| 128 | decaps | 679740 | 50952 |  |
| 128 | total | 1170864 | 50952 |  |
| 192 | keypair | 365814 | 53654 |  |
| 192 | encaps | 788107 | 53654 |  |
| 192 | decaps | 1450517 | 53654 |  |
| 192 | total | 2604438 | 53654 |  |
| 256 | keypair | 554405 | 62364 |  |
| 256 | encaps | 1211052 | 62364 |  |
| 256 | decaps | 2329012 | 62364 |  |
| 256 | total | 4094469 | 62364 |  |

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
| 128 | 1571 | 1736 | 25257 | 22871 | 395 | 358 |
| 192 | 1680 | 1797 | 29147 | 25464 | 456 | 398 |
| 256 | 1779 | 1879 | 34639 | 28812 | 542 | 451 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| vclmul_leaf | 258732 | 21.87 |  |
| KeccakF1600_StatePermute | 191639 | 16.20 |  |
| keccak_inc_absorb | 149040 | 12.60 |  |
| keccak_inc_squeeze | 96890 | 8.19 |  |
| gf_reduce | 79950 | 6.76 |  |
| kmul_18 | 49368 | 4.17 |  |
| kmul_17 | 39294 | 3.32 |  |
| kmul_35 | 37356 | 3.16 |  |
| vect_generate_random_support2 | 30252 | 2.56 |  |
| compare_u32 | 21090 | 1.78 |  |
| vect_generate_random_support1 | 19365 | 1.64 |  |
| gf_mul | 17044 | 1.44 |  |
| rol1_zbb | 17040 | 1.44 |  |
| vect_add | 15978 | 1.35 |  |
| kmul_34 | 13530 | 1.14 |  |
| gf_square | 12489 | 1.06 |  |
| kmul_69 | 10080 | 0.85 |  |
| compute_elp | 9934 | 0.84 |  |
| fft_rec | 8738 | 0.74 |  |
| kmul_70 | 8112 | 0.69 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| keccak_inc_squeeze at fips202_zvbb_gather.c:342 | 51696 | 4.37 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:307 | 51578 | 4.36 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:309 | 42875 | 3.62 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:308 | 42875 | 3.62 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:341 | 34536 | 2.92 |  |
| gf_reduce at gf.c:106 | 33912 | 2.87 |  |
| gf_reduce at gf.c:105 | 25434 | 2.15 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:0 | 24082 | 2.04 |  |
| compare_u32 at vector.c:43 | 21090 | 1.78 |  |
| rol1_zbb at fips202_zvbb_gather.c:129 | 17040 | 1.44 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 1.25 |  |
| gf_reduce at gf.c:97 | 12717 | 1.08 |  |
| vect_generate_random_support2 at vector.c:136 | 11844 | 1.00 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:226 | 11760 | 0.99 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 0.92 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 0.91 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:173 | 10224 | 0.86 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:209 | 10224 | 0.86 |  |
| vect_generate_random_support2 at vector.c:137 | 9780 | 0.83 |  |
| kmul_35 at karatsuba_zvbc_specialized_gen.h:484 | 9504 | 0.80 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 453760 | 23.39 |  |
| bnez | 382690 | 19.72 |  |
| addi | 145371 | 7.49 |  |
| add | 100808 | 5.20 |  |
| xor | 71620 | 3.69 |  |
| slli | 69202 | 3.57 |  |
| vxor.vv | 63872 | 3.29 |  |
| vle64.v | 52803 | 2.72 |  |
| vse64.v | 39552 | 2.04 |  |
| vsetivli | 39164 | 2.02 |  |
| sd | 34564 | 1.78 |  |
| andi | 33715 | 1.74 |  |
| vslidedown.vi | 27393 | 1.41 |  |
| bne | 25661 | 1.32 |  |
| srli | 21642 | 1.12 |  |
| bltu | 18898 | 0.97 |  |
| mv | 17749 | 0.91 |  |
| vmv.x.s | 17561 | 0.91 |  |
| vmv.v.x | 17053 | 0.88 |  |
| vl8r.v | 17040 | 0.88 |  |

