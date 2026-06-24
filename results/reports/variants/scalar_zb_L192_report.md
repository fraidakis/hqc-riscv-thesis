# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-24 15:21:07 |
| Commit | `0eb2718` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 186,629 | 12,220 |
| 192 | vect_mul | 591,557 | 11,904 |
| 256 | vect_mul | 1,306,068 | 11,514 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 432,968 | 36,490 |
| 128 | encaps | 842,287 | 36,490 |
| 128 | decaps | 2,002,282 | 36,490 |
| 128 | **total** | 3,277,537 | 36,490 |
| 192 | keypair | 1,062,817 | 38,724 |
| 192 | encaps | 2,104,122 | 38,724 |
| 192 | decaps | 4,281,887 | 38,724 |
| 192 | **total** | 7,448,826 | 38,724 |
| 256 | keypair | 2,062,593 | 43,628 |
| 256 | encaps | 4,086,039 | 43,628 |
| 256 | decaps | 8,168,745 | 43,628 |
| 256 | **total** | 14,317,377 | 43,628 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1331 | 90 | 1241 | 155 | 10,225,881 | 5,107,503 | 1,557,477 | 982,784 |
| 192 | 3098 | 197 | 2901 | 809 | 22,916,958 | 13,988,195 | 3,316,397 | 2,369,632 |
| 256 | 6890 | 883 | 6007 | 3266 | 41,210,119 | 28,307,416 | 5,912,256 | 4,531,455 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 2072 | 1731 | 14,707 | 12,338 | 230 | 193 |
| 192 | 2127 | 1786 | 14,809 | 12,598 | 232 | 197 |
| 256 | 2140 | 1779 | 15,052 | 12,729 | 236 | 199 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul_leaf | 1,474,080 | 19.73 |
| KeccakF1600_StatePermute | 1,152,874 | 15.43 |
| memset | 597,330 | 8.00 |
| __riscv_clmul_64 | 366,092 | 4.90 |
| __riscv_clmulh_64 | 358,974 | 4.81 |
| support_contains_value | 324,300 | 4.34 |
| keccak_inc_absorb | 316,045 | 4.23 |
| kmul_18 | 286,224 | 3.83 |
| hadamard | 279,496 | 3.74 |
| memcpy | 275,464 | 3.69 |
| kmul_35 | 247,650 | 3.32 |
| expand_and_sum | 246,064 | 3.29 |
| keccak_inc_squeeze | 203,516 | 2.72 |
| kmul_70 | 154,356 | 2.07 |
| gf_reduce | 127,636 | 1.71 |
| kmul_140 | 86,970 | 1.16 |
| find_peaks | 86,883 | 1.16 |
| kmul_561 | 83,856 | 1.12 |
| kmul_17 | 81,510 | 1.09 |
| kmul_281 | 72,636 | 0.97 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 366,092 | 4.90 |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 358,974 | 4.81 |
| clmul_leaf at gf2x_clmul.c:370 | 254,976 | 3.41 |
| clmul_leaf at gf2x_clmul.c:372 | 254,976 | 3.41 |
| clmul_leaf at gf2x_clmul.c:371 | 254,976 | 3.41 |
| support_contains_value at vector.c:81 | 214,240 | 2.87 |
| memset at mem.c:84 (discriminator 1) | 199,340 | 2.67 |
| memset at mem.c:85 | 198,926 | 2.66 |
| memset at mem.c:84 | 198,926 | 2.66 |
| keccak_inc_absorb at fips202.c:438 | 196,658 | 2.63 |
| clmul_leaf at gf2x_clmul.c:369 | 159,360 | 2.13 |
| keccak_inc_squeeze at fips202.c:505 | 139,520 | 1.87 |
| clmul_leaf at gf2x_clmul.c:373 | 127,488 | 1.71 |
| expand_and_sum at reed_muller.c:132 | 116,480 | 1.56 |
| memcpy at mem.c:33 (discriminator 1) | 110,396 | 1.48 |
| memcpy at mem.c:34 | 109,344 | 1.46 |
| support_contains_value at vector.c:80 (discriminator 1) | 108,405 | 1.45 |
| hadamard at reed_muller.c:98 | 100,352 | 1.34 |
| hadamard at reed_muller.c:99 | 100,352 | 1.34 |
| clmul_leaf at gf2x_clmul.c:358 (discriminator 1) | 95,616 | 1.28 |

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

