# Full Analysis Report

- Variant: ref_scalar
- Level: 128
- Date: 2026-05-20 23:17:44

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 19214281 | 2292 |
| 192 | vect_mul | 58433901 | 2316 |
| 256 | vect_mul | 142641044 | 2314 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 19454626 | 32158 |  |
| 128 | encaps | 38875925 | 32158 |  |
| 128 | decaps | 59013715 | 32158 |  |
| 128 | total | 117344266 | 32158 |  |
| 192 | keypair | 58895392 | 33916 |  |
| 192 | encaps | 117744966 | 33916 |  |
| 192 | decaps | 177662247 | 33916 |  |
| 192 | total | 354302605 | 33916 |  |
| 256 | keypair | 143382635 | 41390 |  |
| 256 | encaps | 286701285 | 41390 |  |
| 256 | decaps | 431890526 | 41390 |  |
| 256 | total | 861974446 | 41390 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1368 | 89 | 1279 | 134 | 372778750 | 237238975 | 46808199 | 30976101 |
| 192 | 3338 | 297 | 3041 | 909 | 1123678443 | 719469355 | 140827943 | 93611439 |
| 256 | 7736 | 1135 | 6601 | 3880 | 2808152658 | 1824016558 | 351651057 | 233898829 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1667 | 2153 | 7907 | 10805 | 124 | 169 |
| 192 | 1836 | 2265 | 8335 | 10976 | 131 | 172 |
| 256 | 1844 | 2242 | 7799 | 10135 | 122 | 159 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul | 112621824 | 95.96 |  |
| memset | 1995963 | 1.70 |  |
| karatsuba_mul | 958428 | 0.82 |  |
| KeccakF1600_StatePermute | 578792 | 0.49 |  |
| hadamard | 186162 | 0.16 |  |
| keccak_inc_absorb | 141110 | 0.12 |  |
| support_contains_value | 139683 | 0.12 |  |
| memcpy | 124446 | 0.11 |  |
| gf_reduce | 106191 | 0.09 |  |
| find_peaks | 105965 | 0.09 |  |
| keccak_inc_squeeze | 95576 | 0.08 |  |
| expand_and_sum | 90206 | 0.08 |  |
| vect_compare | 31058 | 0.03 |  |
| gf_mul | 23702 | 0.02 |  |
| reduce | 19997 | 0.02 |  |
| vect_add | 15781 | 0.01 |  |
| compute_syndromes | 13770 | 0.01 |  |
| compute_elp | 12090 | 0.01 |  |
| vect_generate_random_support2 | 11718 | 0.01 |  |
| code_decode | 11387 | 0.01 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul at gf2x.c:55 | 43214472 | 36.82 |  |
| schoolbook_mul at gf2x.c:54 | 36012060 | 30.69 |  |
| schoolbook_mul at gf2x.c:53 | 22419936 | 19.10 |  |
| schoolbook_mul at gf2x.c:0 | 4115100 | 3.51 |  |
| schoolbook_mul at gf2x.c:44 | 2476800 | 2.11 |  |
| schoolbook_mul at gf2x.c:43 | 1651200 | 1.41 |  |
| memset at mem.c:84 | 1331280 | 1.13 |  |
| schoolbook_mul at gf2x.c:48 | 825600 | 0.70 |  |
| schoolbook_mul at gf2x.c:47 | 812700 | 0.69 |  |
| memset at mem.c:85 | 662961 | 0.56 |  |
| schoolbook_mul at gf2x.c:50 | 571620 | 0.49 |  |
| schoolbook_mul at gf2x.c:49 | 355872 | 0.30 |  |
| karatsuba_mul at gf2x.c:115 | 159510 | 0.14 |  |
| karatsuba_mul at gf2x.c:114 | 154608 | 0.13 |  |
| schoolbook_mul at gf2x.c:42 | 103200 | 0.09 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 0.08 |  |
| support_contains_value at vector.c:81 | 92340 | 0.08 |  |
| karatsuba_mul at gf2x.c:119 | 89904 | 0.08 |  |
| karatsuba_mul at gf2x.c:116 | 89904 | 0.08 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 0.07 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 23497986 | 19.86 |  |
| addi | 16579202 | 14.02 |  |
| xor | 15005645 | 12.68 |  |
| sd | 14724263 | 12.45 |  |
| and | 14536031 | 12.29 |  |
| bne | 8116107 | 6.86 |  |
| srl | 8037222 | 6.79 |  |
| sll | 7212529 | 6.10 |  |
| mv | 3344975 | 2.83 |  |
| slli | 918016 | 0.78 |  |
| beq | 878540 | 0.74 |  |
| j | 848758 | 0.72 |  |
| beqz | 829992 | 0.70 |  |
| srai | 826336 | 0.70 |  |
| sub | 824034 | 0.70 |  |
| sb | 700036 | 0.59 |  |
| bnez | 470020 | 0.40 |  |
| add | 232454 | 0.20 |  |
| rori | 98832 | 0.08 |  |
| andn | 85723 | 0.07 |  |

