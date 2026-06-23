# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-23 13:26:11 |
| Commit | `c828690` |

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
| KeccakF1600_StatePermute | 578,792 | 16.70 |
| clmul_leaf | 490,692 | 14.16 |
| memset | 316,635 | 9.14 |
| clmul64 | 253,956 | 7.33 |
| hadamard | 227,516 | 6.56 |
| keccak_inc_absorb | 141,116 | 4.07 |
| support_contains_value | 140,430 | 4.05 |
| memcpy | 137,922 | 3.98 |
| kmul_35 | 113,520 | 3.28 |
| gf_reduce | 106,191 | 3.06 |
| find_peaks | 105,919 | 3.06 |
| kmul_18 | 100,320 | 2.89 |
| keccak_inc_squeeze | 96,430 | 2.78 |
| expand_and_sum | 93,748 | 2.70 |
| kmul_17 | 88,134 | 2.54 |
| clmul_leaf_n8 | 76,146 | 2.20 |
| kmul_69 | 40,950 | 1.18 |
| kmul_277 | 36,480 | 1.05 |
| kmul_139 | 34,968 | 1.01 |
| kmul_70 | 33,192 | 0.96 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| memset at mem.c:84 | 210,996 | 6.09 |
| clmul64 at gf2x_clmul.c:39 | 114,324 | 3.30 |
| clmul64 at gf2x_clmul.c:40 | 114,324 | 3.30 |
| memset at mem.c:85 | 105,357 | 3.04 |
| KeccakF1600_StatePermute at fips202.c:? | 98,548 | 2.84 |
| support_contains_value at vector.c:81 | 92,340 | 2.66 |
| clmul_leaf at gf2x_clmul.c:370 | 88,992 | 2.57 |
| keccak_inc_absorb at fips202.c:438 | 85,750 | 2.47 |
| memcpy at mem.c:33 | 82,652 | 2.38 |
| hadamard at reed_muller.c:98 | 82,432 | 2.38 |
| hadamard at reed_muller.c:99 | 82,432 | 2.38 |
| clmul_leaf at gf2x_clmul.c:371 | 79,104 | 2.28 |
| clmul_leaf at gf2x_clmul.c:372 | 79,104 | 2.28 |
| expand_and_sum at reed_muller.c:132 | 68,448 | 1.97 |
| hadamard at reed_muller.c:97 | 62,606 | 1.81 |
| memcpy at mem.c:34 | 54,764 | 1.58 |
| keccak_inc_squeeze at fips202.c:505 | 51,696 | 1.49 |
| gf_reduce at gf.c:82 | 50,301 | 1.45 |
| clmul_leaf at gf2x_clmul.c:369 | 49,440 | 1.43 |
| support_contains_value at vector.c:80 | 46,812 | 1.35 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 891,011 | 20.96 |
| xor | 643,589 | 15.14 |
| bnez | 394,561 | 9.28 |
| sd | 377,126 | 8.87 |
| addi | 367,525 | 8.65 |
| bne | 219,468 | 5.16 |
| sb | 144,373 | 3.40 |
| clmul | 119,913 | 2.82 |
| clmulh | 114,324 | 2.69 |
| lh | 110,790 | 2.61 |
| rori | 98,832 | 2.33 |
| andn | 85,723 | 2.02 |
| add | 80,318 | 1.89 |
| slli | 69,006 | 1.62 |
| sh | 63,067 | 1.48 |
| mv | 61,266 | 1.44 |
| lbu | 50,896 | 1.20 |
| or | 35,065 | 0.83 |
| srli | 31,105 | 0.73 |
| andi | 30,827 | 0.73 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_zbc | OK | 2,234,431 | 523,497 |
| gcc | -O3 | karatsuba_zbc | OK | 2,032,805 | 540,153 |
| clang | -O2 | karatsuba_zbc | OK | 2,179,591 | 542,507 |
| clang | -O3 | karatsuba_zbc | OK | 2,158,898 | 561,411 |

### HQC-192

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_zbc | OK | 5,225,191 | 525,817 |
| gcc | -O3 | karatsuba_zbc | OK | 4,621,302 | 541,337 |
| clang | -O2 | karatsuba_zbc | OK | 4,897,752 | 544,679 |
| clang | -O3 | karatsuba_zbc | OK | 4,912,525 | 564,729 |

### HQC-256

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_zbc | OK | 11,223,044 | 531,865 |
| gcc | -O3 | karatsuba_zbc | OK | 8,958,436 | 545,905 |
| clang | -O2 | karatsuba_zbc | OK | 11,420,506 | 549,393 |
| clang | -O3 | karatsuba_zbc | OK | 11,360,831 | 566,783 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

