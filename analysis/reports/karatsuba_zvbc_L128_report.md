# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-19 16:48:33

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
| 128 | keypair | 205124 | 45272 |  |
| 128 | encaps | 435123 | 45272 |  |
| 128 | decaps | 901150 | 45272 |  |
| 128 | total | 1541397 | 45272 |  |
| 192 | keypair | 455663 | 48044 |  |
| 192 | encaps | 981947 | 48044 |  |
| 192 | decaps | 1851452 | 48044 |  |
| 192 | total | 3289062 | 48044 |  |
| 256 | keypair | 697510 | 56950 |  |
| 256 | encaps | 1510430 | 56950 |  |
| 256 | decaps | 3045877 | 56950 |  |
| 256 | total | 5253817 | 56950 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1335 | 110 | 1225 | 160 | 12918559 | 7289855 | 1927034 | 1375532 |
| 192 | 3023 | 279 | 2744 | 731 | 30170394 | 20117033 | 4286996 | 3508410 |
| 256 | 6858 | 1044 | 5814 | 3245 | 59807367 | 42788810 | 8687695 | 7395662 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1545 | 1832 | 23974 | 22826 | 375 | 357 |
| 192 | 1684 | 1966 | 28003 | 26363 | 438 | 412 |
| 256 | 1782 | 2037 | 33503 | 29452 | 524 | 461 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 521424 | 33.49 |  |
| vclmul_leaf | 258732 | 16.62 |  |
| keccak_inc_absorb | 141116 | 9.06 |  |
| gf_reduce | 105600 | 6.78 |  |
| keccak_inc_squeeze | 96560 | 6.20 |  |
| kmul_18 | 49368 | 3.17 |  |
| kmul_17 | 39294 | 2.52 |  |
| kmul_35 | 37356 | 2.40 |  |
| vect_generate_random_support2 | 30252 | 1.94 |  |
| memcpy | 28078 | 1.80 |  |
| gf_mul | 23794 | 1.53 |  |
| compare_u32 | 21090 | 1.35 |  |
| vect_generate_random_support1 | 19365 | 1.24 |  |
| vect_add | 15978 | 1.03 |  |
| compute_syndromes | 13895 | 0.89 |  |
| kmul_34 | 13530 | 0.87 |  |
| gf_square | 12489 | 0.80 |  |
| compute_elp | 10295 | 0.66 |  |
| kmul_69 | 10080 | 0.65 |  |
| fft_rec | 8738 | 0.56 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:148 | 194256 | 12.48 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:143 | 170400 | 10.94 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:294 | 51696 | 3.32 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:0 | 45156 | 2.90 |  |
| gf_reduce at gf.c:106 | 44712 | 2.87 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:259 | 43067 | 2.77 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:261 | 42875 | 2.75 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:260 | 42875 | 2.75 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:293 | 34536 | 2.22 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:145 | 34080 | 2.19 |  |
| gf_reduce at gf.c:105 | 33534 | 2.15 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:178 | 27264 | 1.75 |  |
| compare_u32 at vector.c:43 | 21090 | 1.35 |  |
| memcpy at mem.c:21 | 17555 | 1.13 |  |
| gf_reduce at gf.c:97 | 16767 | 1.08 |  |
| vclmul_leaf at gf2x_vclmul.c:0 | 14802 | 0.95 |  |
| vect_generate_random_support2 at vector.c:136 | 11844 | 0.76 |  |
| kmul_18 at karatsuba_zvbc_specialized_gen.h:156 | 10824 | 0.70 |  |
| vclmul_leaf at gf2x_vclmul.c:157 | 10776 | 0.69 |  |
| gf_reduce at gf.c:98 | 10587 | 0.68 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 576578 | 24.82 |  |
| bnez | 383432 | 16.51 |  |
| xor | 237148 | 10.21 |  |
| addi | 159171 | 6.85 |  |
| sd | 146005 | 6.28 |  |
| add | 108495 | 4.67 |  |
| slli | 77342 | 3.33 |  |
| vle64.v | 56642 | 2.44 |  |
| vxor.vv | 46092 | 1.98 |  |
| vse64.v | 43008 | 1.85 |  |
| andi | 33788 | 1.45 |  |
| bne | 32069 | 1.38 |  |
| vsetivli | 22198 | 0.96 |  |
| zext.b | 19956 | 0.86 |  |
| bltu | 19185 | 0.83 |  |
| mv | 17515 | 0.75 |  |
| rori | 17040 | 0.73 |  |
| lbu | 16941 | 0.73 |  |
| sub | 16922 | 0.73 |  |
| csrr | 16208 | 0.70 |  |

