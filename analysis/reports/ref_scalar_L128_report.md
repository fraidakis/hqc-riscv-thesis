# Full Analysis Report

- Variant: ref_scalar
- Level: 128
- Date: 2026-05-18 16:07:29

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
| 128 | keypair | 19458964 | 32556 |  |
| 128 | encaps | 38900675 | 32556 |  |
| 128 | decaps | 59051432 | 32556 |  |
| 192 | keypair | 58905340 | 34314 |  |
| 192 | encaps | 117802593 | 34314 |  |
| 192 | decaps | 177740182 | 34314 |  |
| 256 | keypair | 143399884 | 41792 |  |
| 256 | encaps | 286800072 | 41792 |  |
| 256 | decaps | 432019077 | 41792 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1368 | 89 | 1279 | 134 | 372784310 | 237239007 | 46808894 | 30976133 |
| 192 | 3318 | 289 | 3029 | 887 | 1123694107 | 719469387 | 140829901 | 93611471 |
| 256 | 7922 | 1154 | 6768 | 4064 | 2808137138 | 1824016590 | 351649117 | 233898861 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1654 | 2142 | 7810 | 10731 | 123 | 168 |
| 192 | 1824 | 2256 | 8239 | 10904 | 129 | 171 |
| 256 | 1833 | 2234 | 7691 | 10057 | 121 | 158 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul | 112621824 | 95.91 |  |
| memset | 1996011 | 1.70 |  |
| karatsuba_mul | 958428 | 0.82 |  |
| KeccakF1600_StatePermute | 578792 | 0.49 |  |
| hadamard | 186162 | 0.16 |  |
| keccak_inc_absorb | 141110 | 0.12 |  |
| memcpy | 124446 | 0.11 |  |
| find_peaks | 105965 | 0.09 |  |
| gf_reduce | 105600 | 0.09 |  |
| keccak_inc_squeeze | 95576 | 0.08 |  |
| expand_and_sum | 90206 | 0.08 |  |
| compare_u32 | 83250 | 0.07 |  |
| vect_generate_random_support2 | 77868 | 0.07 |  |
| vect_generate_random_support1 | 51069 | 0.04 |  |
| vect_compare | 31058 | 0.03 |  |
| gf_mul | 23794 | 0.02 |  |
| reduce | 19997 | 0.02 |  |
| vect_add | 15781 | 0.01 |  |
| compute_syndromes | 13770 | 0.01 |  |
| gf_square | 12489 | 0.01 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| schoolbook_mul at gf2x.c:55 | 43214472 | 36.80 |  |
| schoolbook_mul at gf2x.c:54 | 36012060 | 30.67 |  |
| schoolbook_mul at gf2x.c:53 | 22419936 | 19.09 |  |
| schoolbook_mul at gf2x.c:0 | 4115100 | 3.50 |  |
| schoolbook_mul at gf2x.c:44 | 2476800 | 2.11 |  |
| schoolbook_mul at gf2x.c:43 | 1651200 | 1.41 |  |
| memset at mem.c:60 | 1331312 | 1.13 |  |
| schoolbook_mul at gf2x.c:48 | 825600 | 0.70 |  |
| schoolbook_mul at gf2x.c:47 | 812700 | 0.69 |  |
| memset at mem.c:61 | 662977 | 0.56 |  |
| schoolbook_mul at gf2x.c:50 | 571620 | 0.49 |  |
| schoolbook_mul at gf2x.c:49 | 355872 | 0.30 |  |
| karatsuba_mul at gf2x.c:115 | 159510 | 0.14 |  |
| karatsuba_mul at gf2x.c:114 | 154608 | 0.13 |  |
| schoolbook_mul at gf2x.c:42 | 103200 | 0.09 |  |
| KeccakF1600_StatePermute at fips202.c:0 | 98548 | 0.08 |  |
| karatsuba_mul at gf2x.c:119 | 89904 | 0.08 |  |
| karatsuba_mul at gf2x.c:116 | 89904 | 0.08 |  |
| keccak_inc_absorb at fips202.c:438 | 85750 | 0.07 |  |
| compare_u32 at vector.c:43 | 83250 | 0.07 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 23499583 | 19.85 |  |
| addi | 16585653 | 14.01 |  |
| xor | 14988404 | 12.66 |  |
| sd | 14724263 | 12.44 |  |
| and | 14542466 | 12.29 |  |
| bne | 8116123 | 6.86 |  |
| srl | 8037222 | 6.79 |  |
| sll | 7212529 | 6.09 |  |
| mv | 3344975 | 2.83 |  |
| slli | 920380 | 0.78 |  |
| beq | 878540 | 0.74 |  |
| j | 848758 | 0.72 |  |
| beqz | 829992 | 0.70 |  |
| srai | 826336 | 0.70 |  |
| sub | 824034 | 0.70 |  |
| sb | 700052 | 0.59 |  |
| bnez | 472061 | 0.40 |  |
| add | 232454 | 0.20 |  |
| rori | 98832 | 0.08 |  |
| andn | 85723 | 0.07 |  |

