# Full Analysis Report

- Variant: karatsuba_rvv
- Level: 128
- Date: 2026-05-20 21:07:35

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 157377 | 10442 |
| 192 | vect_mul | 481065 | 11262 |
| 256 | vect_mul | 1608062 | 9746 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 297410 | 43820 |  |
| 128 | encaps | 612457 | 43820 |  |
| 128 | decaps | 1128432 | 43820 |  |
| 128 | total | 2038299 | 43820 |  |
| 192 | keypair | 731623 | 45880 |  |
| 192 | encaps | 1518293 | 45880 |  |
| 192 | decaps | 2613649 | 45880 |  |
| 192 | total | 4863565 | 45880 |  |
| 256 | keypair | 2002860 | 51148 |  |
| 256 | encaps | 4090887 | 51148 |  |
| 256 | decaps | 6768650 | 51148 |  |
| 256 | total | 12862397 | 51148 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1299 | 104 | 1195 | 127 | 11098967 | 5624159 | 1699585 | 1108184 |
| 192 | 3020 | 291 | 2729 | 726 | 24682066 | 15075401 | 3600955 | 2878206 |
| 256 | 6681 | 991 | 5690 | 3042 | 59117429 | 31778048 | 8240594 | 5658458 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 2166 | 2206 | 34385 | 27527 | 538 | 431 |
| 192 | 2400 | 2402 | 31309 | 27721 | 490 | 434 |
| 256 | 2203 | 2246 | 28385 | 25189 | 444 | 394 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 578792 | 28.20 |  |
| clmul_leaf | 477096 | 23.24 |  |
| clmul64 | 228648 | 11.14 |  |
| keccak_inc_absorb | 141116 | 6.87 |  |
| keccak_inc_squeeze | 96430 | 4.70 |  |
| gf_reduce | 79950 | 3.90 |  |
| clmul_leaf_n8 | 70596 | 3.44 |  |
| rvv_xor3_u64 | 54282 | 2.64 |  |
| rvv_copy_u64 | 30264 | 1.47 |  |
| vect_generate_random_support2 | 30252 | 1.47 |  |
| kmul_18 | 23496 | 1.14 |  |
| kmul_17 | 22200 | 1.08 |  |
| compare_u32 | 21090 | 1.03 |  |
| vect_generate_random_support1 | 19365 | 0.94 |  |
| gf_mul | 17044 | 0.83 |  |
| vect_add | 15978 | 0.78 |  |
| kmul_35 | 14094 | 0.69 |  |
| gf_square | 12489 | 0.61 |  |
| compute_elp | 9934 | 0.48 |  |
| fft_rec | 8738 | 0.43 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| clmul64 at gf2x_clmul.c:42 | 228648 | 11.14 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 4.80 |  |
| clmul_leaf at gf2x_clmul.c:447 | 88992 | 4.34 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 4.18 |  |
| clmul_leaf at gf2x_clmul.c:448 | 79104 | 3.85 |  |
| clmul_leaf at gf2x_clmul.c:449 | 79104 | 3.85 |  |
| keccak_inc_squeeze at fips202.c:505 | 51696 | 2.52 |  |
| clmul_leaf at gf2x_clmul.c:446 | 49440 | 2.41 |  |
| keccak_inc_absorb at fips202.c:435 | 43067 | 2.10 |  |
| clmul_leaf at gf2x_clmul.c:450 | 39552 | 1.93 |  |
| keccak_inc_squeeze at fips202.c:504 | 34608 | 1.69 |  |
| gf_reduce at gf.c:106 | 33912 | 1.65 |  |
| gf_reduce at gf.c:105 | 25434 | 1.24 |  |
| clmul_leaf at gf2x_clmul.c:435 | 22248 | 1.08 |  |
| compare_u32 at vector.c:43 | 21090 | 1.03 |  |
| clmul_leaf at gf2x_clmul.c:473 | 19776 | 0.96 |  |
| clmul_leaf at gf2x_clmul.c:474 | 19776 | 0.96 |  |
| gf_reduce at gf.c:97 | 12717 | 0.62 |  |
| clmul_leaf at gf2x_clmul.c:477 | 12360 | 0.60 |  |
| vect_generate_random_support2 at vector.c:136 | 11844 | 0.58 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 648647 | 22.84 |  |
| xor | 501350 | 17.65 |  |
| bnez | 404457 | 14.24 |  |
| sd | 216016 | 7.61 |  |
| clmul | 117972 | 4.15 |  |
| clmulh | 114324 | 4.03 |  |
| addi | 110941 | 3.91 |  |
| rori | 98832 | 3.48 |  |
| andn | 85375 | 3.01 |  |
| add | 71695 | 2.52 |  |
| slli | 54679 | 1.93 |  |
| mv | 34343 | 1.21 |  |
| andi | 33535 | 1.18 |  |
| bne | 33438 | 1.18 |  |
| vle64.v | 29940 | 1.05 |  |
| vxor.vv | 21783 | 0.77 |  |
| sub | 18972 | 0.67 |  |
| vse64.v | 17179 | 0.60 |  |
| zext.b | 14556 | 0.51 |  |
| srli | 12654 | 0.45 |  |

