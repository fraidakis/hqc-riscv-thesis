# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-07 15:50:56 |
| Commit | `20def7e` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 221,030 | 15,270 |
| 192 | vect_mul | 670,293 | 17,258 |
| 256 | vect_mul | 1,879,602 | 15,090 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 462,794 | 47,788 |
| 128 | encaps | 898,649 | 47,788 |
| 128 | decaps | 2,088,352 | 47,788 |
| 128 | **total** | 3,449,795 | 47,788 |
| 192 | keypair | 1,132,934 | 50,940 |
| 192 | encaps | 2,235,711 | 50,940 |
| 192 | decaps | 4,442,411 | 50,940 |
| 192 | **total** | 7,811,056 | 50,940 |
| 256 | keypair | 2,621,830 | 55,672 |
| 256 | encaps | 5,204,511 | 55,672 |
| 256 | decaps | 9,725,450 | 55,672 |
| 256 | **total** | 17,551,791 | 55,672 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1343 | 111 | 1232 | 142 | 11,924,919 | 6,573,507 | 1,787,693 | 1,170,512 |
| 192 | 3121 | 286 | 2835 | 816 | 26,911,032 | 17,591,587 | 3,854,785 | 2,823,772 |
| 256 | 6797 | 1019 | 5778 | 3161 | 59,034,481 | 34,049,564 | 8,204,642 | 5,256,250 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 3239 | 2880 | 21,641 | 18,859 | 339 | 295 |
| 192 | 3566 | 3064 | 24,007 | 20,249 | 376 | 317 |
| 256 | 3399 | 2942 | 22,739 | 19,392 | 356 | 303 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul_leaf | 6,995,880 | 39.82 |
| clmul64 | 1,775,856 | 10.11 |
| KeccakF1600_StatePermute | 1,715,996 | 9.77 |
| memset | 951,375 | 5.41 |
| kmul_28 | 613,800 | 3.49 |
| support_contains_value | 554,340 | 3.15 |
| memcpy | 448,010 | 2.55 |
| hadamard | 445,140 | 2.53 |
| keccak_inc_absorb | 443,726 | 2.53 |
| kmul_29 | 361,152 | 2.06 |
| gf_reduce | 343,748 | 1.96 |
| expand_and_sum | 321,120 | 1.83 |
| kmul_57 | 300,696 | 1.71 |
| kmul_113 | 298,980 | 1.70 |
| keccak_inc_squeeze | 274,928 | 1.56 |
| kmul_56 | 244,644 | 1.39 |
| find_peaks | 207,223 | 1.18 |
| kmul_225 | 140,700 | 0.80 |
| kmul_901 | 122,046 | 0.69 |
| kmul_451 | 114,624 | 0.65 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:40 | 887,928 | 5.05 |
| clmul64 at gf2x_clmul.c:39 | 887,928 | 5.05 |
| clmul_leaf at gf2x_clmul.c:370 | 695,016 | 3.96 |
| clmul_leaf at gf2x_clmul.c:371 | 695,016 | 3.96 |
| clmul_leaf at gf2x_clmul.c:372 | 695,016 | 3.96 |
| clmul_leaf at gf2x_clmul.c:373 | 695,016 | 3.96 |
| clmul_leaf at gf2x_clmul.c:363 | 695,016 | 3.96 |
| memset at mem.c:84 | 634,122 | 3.61 |
| clmul_leaf at gf2x_clmul.c:? | 602,712 | 3.43 |
| clmul_leaf at gf2x_clmul.c:369 | 521,262 | 2.97 |
| support_contains_value at vector.c:81 | 367,000 | 2.09 |
| memset at mem.c:85 | 316,869 | 1.80 |
| KeccakF1600_StatePermute at fips202.c:? | 292,174 | 1.66 |
| keccak_inc_absorb at fips202.c:438 | 287,230 | 1.63 |
| memcpy at mem.c:33 | 268,474 | 1.53 |
| expand_and_sum at reed_muller.c:132 | 267,840 | 1.52 |
| clmul_leaf at gf2x_clmul.c:391 | 215,670 | 1.23 |
| clmul_leaf at gf2x_clmul.c:358 | 186,876 | 1.06 |
| support_contains_value at vector.c:80 | 184,782 | 1.05 |
| memcpy at mem.c:34 | 177,876 | 1.01 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 3,753,464 | 20.40 |
| xor | 3,186,029 | 17.32 |
| addi | 2,145,459 | 11.66 |
| sd | 2,034,685 | 11.06 |
| clmul | 906,020 | 4.92 |
| clmulh | 887,928 | 4.83 |
| bne | 726,423 | 3.95 |
| add | 617,565 | 3.36 |
| sb | 438,075 | 2.38 |
| slli | 431,613 | 2.35 |
| bnez | 429,642 | 2.34 |
| rori | 293,016 | 1.59 |
| andn | 254,482 | 1.38 |
| lh | 252,790 | 1.37 |
| mv | 251,917 | 1.37 |
| bltu | 235,375 | 1.28 |
| lbu | 166,150 | 0.90 |
| sh | 153,235 | 0.83 |
| bgeu | 121,966 | 0.66 |
| or | 121,784 | 0.66 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zbc | OK | 3,449,795 | 47,788 |
| clang | -O3 | karatsuba_zbc | OK | 3,449,795 | 47,788 |
| clang | -Os | karatsuba_zbc | OK | 3,449,795 | 47,788 |
| gcc | -O2 | karatsuba_zbc | OK | 3,277,538 | 31,248 |
| gcc | -O3 | karatsuba_zbc | OK | 3,277,538 | 31,248 |
| gcc | -Os | karatsuba_zbc | OK | 3,277,538 | 31,248 |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zbc | OK | 7,811,056 | 50,940 |
| clang | -O3 | karatsuba_zbc | OK | 7,811,056 | 50,940 |
| clang | -Os | karatsuba_zbc | OK | 7,811,056 | 50,940 |
| gcc | -O2 | karatsuba_zbc | OK | 7,448,827 | 33,610 |
| gcc | -O3 | karatsuba_zbc | OK | 7,448,827 | 33,610 |
| gcc | -Os | karatsuba_zbc | OK | 7,448,827 | 33,610 |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zbc | OK | 17,551,791 | 55,672 |
| clang | -O3 | karatsuba_zbc | OK | 17,551,791 | 55,672 |
| clang | -Os | karatsuba_zbc | OK | 17,551,791 | 55,672 |
| gcc | -O2 | karatsuba_zbc | OK | 14,317,378 | 38,488 |
| gcc | -O3 | karatsuba_zbc | OK | 14,317,378 | 38,488 |
| gcc | -Os | karatsuba_zbc | OK | 14,317,378 | 38,488 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

