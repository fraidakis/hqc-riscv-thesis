# Full Analysis Report

- Variant: ref_vector
- Level: 128
- Date: 2026-05-19 17:26:46

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 18527412 | 3840 |
| 192 | vect_mul | 56315140 | 3884 |
| 256 | vect_mul | 139115660 | 3884 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 18665579 | 35822 |  |
| 128 | encaps | 37351077 | 35822 |  |
| 128 | decaps | 56442467 | 35822 |  |
| 128 | total | 112459123 | 35822 |  |
| 192 | keypair | 56563028 | 37966 |  |
| 192 | encaps | 113170595 | 37966 |  |
| 192 | decaps | 170368869 | 37966 |  |
| 192 | total | 340102492 | 37966 |  |
| 256 | keypair | 139504204 | 44976 |  |
| 256 | encaps | 279099192 | 44976 |  |
| 256 | decaps | 419815844 | 44976 |  |
| 256 | total | 838419240 | 44976 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1374 | 105 | 1269 | 130 | 373249582 | 237250571 | 46894623 | 32212034 |
| 192 | 3354 | 294 | 3060 | 910 | 1125116449 | 719596503 | 141036298 | 97403839 |
| 256 | 7984 | 1156 | 6828 | 4086 | 2810292940 | 1824060598 | 351945905 | 240798632 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1745 | 1863 | 11666 | 16330 | 183 | 256 |
| 192 | 1967 | 2001 | 12889 | 18479 | 202 | 289 |
| 256 | 2021 | 2019 | 13378 | 18356 | 210 | 287 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul | 110519532 | 98.26 |  |
| karatsuba_mul | 604860 | 0.54 |  |
| KeccakF1600_StatePermute | 578792 | 0.51 |  |
| keccak_inc_absorb | 141110 | 0.13 |  |
| find_peaks | 111650 | 0.10 |  |
| gf_reduce | 105600 | 0.09 |  |
| keccak_inc_squeeze | 90932 | 0.08 |  |
| memset | 43101 | 0.04 |  |
| expand_and_sum | 33028 | 0.03 |  |
| vect_generate_random_support2 | 28848 | 0.03 |  |
| gf_mul | 23794 | 0.02 |  |
| compare_u32 | 21090 | 0.02 |  |
| vect_generate_random_support1 | 20907 | 0.02 |  |
| compute_syndromes | 13990 | 0.01 |  |
| vect_add | 13903 | 0.01 |  |
| gf_square | 12489 | 0.01 |  |
| compute_elp | 11575 | 0.01 |  |
| code_decode | 11494 | 0.01 |  |
| hadamard | 9298 | 0.01 |  |
| fft_rec | 8792 | 0.01 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul at gf2x.c:55 | 43214472 | 38.42 |  |
| schoolbook_mul at gf2x.c:54 | 36012060 | 32.02 |  |
| schoolbook_mul at gf2x.c:53 | 22419936 | 19.93 |  |
| schoolbook_mul at gf2x.c:0 | 2554200 | 2.27 |  |
| schoolbook_mul at gf2x.c:44 | 2476800 | 2.20 |  |
| schoolbook_mul at gf2x.c:43 | 1651200 | 1.47 |  |
| schoolbook_mul at gf2x.c:48 | 825600 | 0.73 |  |
| schoolbook_mul at gf2x.c:47 | 812700 | 0.72 |  |
| schoolbook_mul at gf2x.c:42 | 206400 | 0.18 |  |
| schoolbook_mul at gf2x.c:49 | 142368 | 0.13 |  |
| schoolbook_mul at gf2x.c:50 | 120120 | 0.11 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 0.09 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 0.08 |  |
| schoolbook_mul at gf2x.c:41 | 82218 | 0.07 |  |
| karatsuba_mul at gf2x.c:116 | 58722 | 0.05 |  |
| karatsuba_mul at gf2x.c:115 | 54294 | 0.05 |  |
| keccak_inc_squeeze at fips202.c:505 | 49428 | 0.04 |  |
| karatsuba_mul at gf2x.c:104 | 47664 | 0.04 |  |
| karatsuba_mul at gf2x.c:108 | 47580 | 0.04 |  |
| karatsuba_mul at gf2x.c:107 | 47334 | 0.04 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 23172120 | 20.43 |  |
| addi | 15436882 | 13.61 |  |
| xor | 14788180 | 13.04 |  |
| sd | 14560939 | 12.84 |  |
| and | 14451261 | 12.74 |  |
| srl | 8036646 | 7.09 |  |
| bne | 7262830 | 6.40 |  |
| sll | 7212529 | 6.36 |  |
| mv | 1732965 | 1.53 |  |
| slli | 932185 | 0.82 |  |
| beq | 878384 | 0.77 |  |
| j | 855732 | 0.75 |  |
| beqz | 845360 | 0.75 |  |
| sub | 834128 | 0.74 |  |
| srai | 826336 | 0.73 |  |
| bnez | 475648 | 0.42 |  |
| add | 356077 | 0.31 |  |
| rori | 98832 | 0.09 |  |
| andn | 85375 | 0.08 |  |
| andi | 48007 | 0.04 |  |

