# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
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
| clmul_leaf | 1,581,648 | 20.10 |
| KeccakF1600_StatePermute | 1,129,052 | 14.35 |
| clmul64 | 739,788 | 9.40 |
| memset | 607,032 | 7.72 |
| kmul_18 | 405,840 | 5.16 |
| kmul_35 | 345,930 | 4.40 |
| support_contains_value | 324,294 | 4.12 |
| keccak_inc_absorb | 279,123 | 3.55 |
| hadamard | 276,976 | 3.52 |
| memcpy | 275,464 | 3.50 |
| expand_and_sum | 199,808 | 2.54 |
| keccak_inc_squeeze | 187,760 | 2.39 |
| kmul_70 | 157,662 | 2.00 |
| clmul_leaf_n8 | 144,300 | 1.83 |
| gf_reduce | 142,652 | 1.81 |
| find_peaks | 128,899 | 1.64 |
| kmul_17 | 106,470 | 1.35 |
| kmul_140 | 86,910 | 1.10 |
| kmul_36 | 85,248 | 1.08 |
| kmul_561 | 76,170 | 0.97 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| memset at mem.c:84 | 404,578 | 5.14 |
| clmul64 at gf2x_clmul.c:39 | 347,664 | 4.42 |
| clmul64 at gf2x_clmul.c:40 | 347,664 | 4.42 |
| clmul_leaf at gf2x_clmul.c:370 | 286,848 | 3.65 |
| clmul_leaf at gf2x_clmul.c:371 | 254,976 | 3.24 |
| clmul_leaf at gf2x_clmul.c:372 | 254,976 | 3.24 |
| support_contains_value at vector.c:81 | 214,240 | 2.72 |
| memset at mem.c:85 | 202,124 | 2.57 |
| KeccakF1600_StatePermute at fips202.c:? | 192,238 | 2.44 |
| keccak_inc_absorb at fips202.c:438 | 178,780 | 2.27 |
| expand_and_sum at reed_muller.c:132 | 166,656 | 2.12 |
| memcpy at mem.c:33 | 165,068 | 2.10 |
| clmul_leaf at gf2x_clmul.c:369 | 159,360 | 2.03 |
| clmul_leaf at gf2x_clmul.c:373 | 127,488 | 1.62 |
| memcpy at mem.c:34 | 109,344 | 1.39 |
| support_contains_value at vector.c:80 | 108,100 | 1.37 |
| keccak_inc_squeeze at fips202.c:505 | 104,640 | 1.33 |
| hadamard at reed_muller.c:98 | 100,352 | 1.28 |
| hadamard at reed_muller.c:99 | 100,352 | 1.28 |
| clmul_leaf at gf2x_clmul.c:397 | 95,616 | 1.22 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 1,805,735 | 20.74 |
| xor | 1,564,551 | 17.97 |
| sd | 1,043,334 | 11.99 |
| addi | 751,561 | 8.63 |
| bne | 441,360 | 5.07 |
| bnez | 423,826 | 4.87 |
| clmul | 355,172 | 4.08 |
| clmulh | 347,664 | 3.99 |
| sb | 277,991 | 3.19 |
| rori | 192,792 | 2.21 |
| andn | 166,790 | 1.92 |
| lh | 155,056 | 1.78 |
| add | 146,614 | 1.68 |
| mv | 113,403 | 1.30 |
| slli | 109,058 | 1.25 |
| lbu | 98,903 | 1.14 |
| sh | 91,215 | 1.05 |
| or | 71,988 | 0.83 |
| lw | 68,798 | 0.79 |
| andi | 60,733 | 0.70 |

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

