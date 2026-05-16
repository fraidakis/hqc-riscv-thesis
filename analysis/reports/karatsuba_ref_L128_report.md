# Full Analysis Report

- Variant: karatsuba_ref
- Level: 128
- Date: 2026-05-16 17:19:39

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
| 128 | keypair | 18665586 | 35014 |  |
| 128 | encaps | 37351087 | 35014 |  |
| 128 | decaps | 56442183 | 35014 |  |
| 192 | keypair | 56563041 | 37184 |  |
| 192 | encaps | 113170607 | 37184 |  |
| 192 | decaps | 170368590 | 37184 |  |
| 256 | keypair | 139504217 | 44188 |  |
| 256 | encaps | 279099204 | 44188 |  |
| 256 | decaps | 419815566 | 44188 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1381 | 104 | 1277 | 145 | 373223134 | 237263543 | 46891088 | 32240032 |
| 192 | 3285 | 277 | 3008 | 868 | 1125055817 | 719637087 | 141031827 | 97466867 |
| 256 | 7621 | 1044 | 6577 | 3779 | 2810284180 | 1824090662 | 351945764 | 240883689 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1737 | 1841 | 12028 | 17637 | 188 | 276 |
| 192 | 1830 | 1852 | 12224 | 18655 | 191 | 292 |
| 256 | 2025 | 1979 | 13879 | 19843 | 217 | 311 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul | 110543874 | 98.28 |  |
| karatsuba_mul | 602934 | 0.54 |  |
| KeccakF1600_StatePermute | 578792 | 0.51 |  |
| keccak_inc_absorb | 141110 | 0.13 |  |
| gf_reduce | 105600 | 0.09 |  |
| keccak_inc_squeeze | 90932 | 0.08 |  |
| find_peaks | 88504 | 0.08 |  |
| memset | 41623 | 0.04 |  |
| expand_and_sum | 33856 | 0.03 |  |
| vect_generate_random_support2 | 29664 | 0.03 |  |
| code_decode | 28926 | 0.03 |  |
| gf_mul | 23794 | 0.02 |  |
| vect_generate_random_support1 | 21096 | 0.02 |  |
| compare_u32 | 21090 | 0.02 |  |
| compute_syndromes | 13773 | 0.01 |  |
| gf_square | 12489 | 0.01 |  |
| compute_elp | 11516 | 0.01 |  |
| hadamard | 10501 | 0.01 |  |
| fft_rec | 8770 | 0.01 |  |
| reed_solomon_encode | 8320 | 0.01 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul at gf2x.c:55 | 43214472 | 38.42 |  |
| schoolbook_mul at gf2x.c:54 | 36012060 | 32.02 |  |
| schoolbook_mul at gf2x.c:53 | 22419936 | 19.93 |  |
| schoolbook_mul at gf2x.c:0 | 2580000 | 2.29 |  |
| schoolbook_mul at gf2x.c:44 | 2476800 | 2.20 |  |
| schoolbook_mul at gf2x.c:43 | 1651200 | 1.47 |  |
| schoolbook_mul at gf2x.c:48 | 825600 | 0.73 |  |
| schoolbook_mul at gf2x.c:47 | 812700 | 0.72 |  |
| schoolbook_mul at gf2x.c:42 | 193500 | 0.17 |  |
| schoolbook_mul at gf2x.c:49 | 142368 | 0.13 |  |
| schoolbook_mul at gf2x.c:50 | 120120 | 0.11 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 0.09 |  |
| schoolbook_mul at gf2x.c:41 | 93660 | 0.08 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 0.08 |  |
| karatsuba_mul at gf2x.c:116 | 57996 | 0.05 |  |
| karatsuba_mul at gf2x.c:115 | 53568 | 0.05 |  |
| keccak_inc_squeeze at fips202.c:505 | 49428 | 0.04 |  |
| karatsuba_mul at gf2x.c:104 | 48414 | 0.04 |  |
| karatsuba_mul at gf2x.c:108 | 47580 | 0.04 |  |
| karatsuba_mul at gf2x.c:107 | 47334 | 0.04 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 23166680 | 20.43 |  |
| addi | 15433199 | 13.61 |  |
| xor | 14786244 | 13.04 |  |
| sd | 14559810 | 12.84 |  |
| and | 14451196 | 12.74 |  |
| srl | 8036646 | 7.09 |  |
| bne | 7260341 | 6.40 |  |
| sll | 7212529 | 6.36 |  |
| mv | 1760000 | 1.55 |  |
| slli | 928973 | 0.82 |  |
| beq | 878672 | 0.77 |  |
| j | 850330 | 0.75 |  |
| beqz | 845482 | 0.75 |  |
| sub | 834125 | 0.74 |  |
| srai | 826336 | 0.73 |  |
| bnez | 475617 | 0.42 |  |
| add | 357231 | 0.31 |  |
| rori | 98832 | 0.09 |  |
| andn | 85375 | 0.08 |  |
| andi | 48335 | 0.04 |  |

