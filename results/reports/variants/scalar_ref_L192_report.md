# HQC-RISC-V Analysis: `scalar_ref`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-08 14:02:15 |
| Commit | `216edab` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 19,214,277 | 2012 |
| 192 | vect_mul | 58,433,896 | 2016 |
| 256 | vect_mul | 142,641,043 | 2026 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 19,454,626 | 32,176 |
| 128 | encaps | 38,875,925 | 32,176 |
| 128 | decaps | 59,013,743 | 32,176 |
| 128 | **total** | 117,344,294 | 32,176 |
| 192 | keypair | 58,895,392 | 33,930 |
| 192 | encaps | 117,744,966 | 33,930 |
| 192 | decaps | 177,662,495 | 33,930 |
| 192 | **total** | 354,302,853 | 33,930 |
| 256 | keypair | 143,382,635 | 41,392 |
| 256 | encaps | 286,701,285 | 41,392 |
| 256 | decaps | 431,890,526 | 41,392 |
| 256 | **total** | 861,974,446 | 41,392 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1358 | 87 | 1271 | 125 | 372,774,966 | 237,223,951 | 46,807,726 | 30,974,223 |
| 192 | 3325 | 287 | 3038 | 906 | 1,123,633,067 | 719,436,907 | 140,822,271 | 93,607,383 |
| 256 | 7878 | 1136 | 6742 | 4027 | 2,808,117,202 | 1,823,990,638 | 351,646,625 | 233,895,589 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1643 | 2131 | 7715 | 10,629 | 121 | 167 |
| 192 | 1810 | 2240 | 8127 | 10,776 | 127 | 169 |
| 256 | 1829 | 2226 | 7659 | 9993 | 120 | 157 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul | 342,273,744 | 96.59 |
| memset | 5,868,378 | 1.66 |
| karatsuba_mul | 3,021,750 | 0.85 |
| KeccakF1600_StatePermute | 1,129,052 | 0.32 |
| support_contains_value | 323,157 | 0.09 |
| keccak_inc_absorb | 279,125 | 0.08 |
| memcpy | 247,526 | 0.07 |
| hadamard | 226,632 | 0.06 |
| expand_and_sum | 197,456 | 0.06 |
| keccak_inc_squeeze | 185,052 | 0.05 |
| gf_reduce | 142,652 | 0.04 |
| find_peaks | 128,955 | 0.04 |
| vect_compare | 62,873 | 0.02 |
| reduce | 40,462 | 0.01 |
| vect_add | 31,963 | 0.01 |
| gf_mul | 31,553 | 0.01 |
| compute_syndromes | 17,888 | 0.01 |
| vect_generate_random_support2 | 17,124 | 0.00 |
| reed_solomon_encode | 13,926 | 0.00 |
| compute_elp | 13,778 | 0.00 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul at gf2x.c:55 | 131,416,992 | 37.09 |
| schoolbook_mul at gf2x.c:54 | 109,514,160 | 30.90 |
| schoolbook_mul at gf2x.c:53 | 68,163,984 | 19.24 |
| schoolbook_mul at gf2x.c:? | 12,433,344 | 3.51 |
| schoolbook_mul at gf2x.c:44 | 7,483,392 | 2.11 |
| schoolbook_mul at gf2x.c:43 | 4,988,928 | 1.41 |
| memset at mem.c:84 | 3,914,332 | 1.10 |
| schoolbook_mul at gf2x.c:48 | 2,494,464 | 0.70 |
| schoolbook_mul at gf2x.c:47 | 2,455,488 | 0.69 |
| memset at mem.c:85 | 1,949,360 | 0.55 |
| schoolbook_mul at gf2x.c:50 | 1,738,320 | 0.49 |
| schoolbook_mul at gf2x.c:49 | 1,081,968 | 0.31 |
| karatsuba_mul at gf2x.c:115 | 505,092 | 0.14 |
| karatsuba_mul at gf2x.c:114 | 495,096 | 0.14 |
| schoolbook_mul at gf2x.c:42 | 311,808 | 0.09 |
| karatsuba_mul at gf2x.c:119 | 284,880 | 0.08 |
| karatsuba_mul at gf2x.c:116 | 284,880 | 0.08 |
| karatsuba_mul at gf2x.c:104 | 222,396 | 0.06 |
| support_contains_value at vector.c:81 | 214,240 | 0.06 |
| karatsuba_mul at gf2x.c:120 | 213,660 | 0.06 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 70,415,170 | 19.82 |
| addi | 50,104,608 | 14.10 |
| xor | 45,244,683 | 12.73 |
| sd | 44,692,485 | 12.58 |
| and | 44,181,610 | 12.43 |
| bne | 24,511,567 | 6.90 |
| srl | 24,415,530 | 6.87 |
| sll | 21,922,412 | 6.17 |
| mv | 10,034,458 | 2.82 |
| slli | 2,672,582 | 0.75 |
| beq | 2,648,600 | 0.75 |
| j | 2,552,365 | 0.72 |
| beqz | 2,506,262 | 0.71 |
| srai | 2,495,360 | 0.70 |
| sub | 2,478,654 | 0.70 |
| sb | 2,022,546 | 0.57 |
| add | 620,104 | 0.17 |
| bnez | 498,703 | 0.14 |
| rori | 192,792 | 0.05 |
| andn | 166,790 | 0.05 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_scalar | OK | 117,418,020 | 23,972 |
| clang | -O3 | ref_scalar | OK | 117,344,266 | 32,158 |
| clang | -Os | ref_scalar | OK | 117,459,473 | 18,264 |
| gcc | -O2 | ref_scalar | OK | 120,486,098 | 17,834 |
| gcc | -O3 | ref_scalar | OK | 113,958,344 | 26,270 |
| gcc | -Os | ref_scalar | OK | 122,366,611 | 16,888 |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_scalar | OK | 354,434,380 | 25,844 |
| clang | -O3 | ref_scalar | OK | 354,302,605 | 33,916 |
| clang | -Os | ref_scalar | OK | 354,513,619 | 19,574 |
| gcc | -O2 | ref_scalar | OK | 363,797,867 | 18,942 |
| gcc | -O3 | ref_scalar | OK | 344,003,671 | 28,980 |
| gcc | -Os | ref_scalar | OK | 369,265,318 | 17,786 |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_scalar | OK | 862,204,770 | 31,746 |
| clang | -O3 | ref_scalar | OK | 861,974,446 | 41,390 |
| clang | -Os | ref_scalar | OK | 862,317,151 | 26,516 |
| gcc | -O2 | ref_scalar | OK | 898,156,204 | 25,928 |
| gcc | -O3 | ref_scalar | OK | 845,613,464 | 33,532 |
| gcc | -Os | ref_scalar | OK | 907,517,922 | 24,678 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

