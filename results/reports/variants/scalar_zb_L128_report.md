# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-23 14:17:57 |
| Commit | `d38ed14` |

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
| KeccakF1600_StatePermute | 591,004 | 18.05 |
| clmul_leaf | 457,320 | 13.96 |
| memset | 307,101 | 9.38 |
| hadamard | 229,586 | 7.01 |
| keccak_inc_absorb | 159,678 | 4.88 |
| support_contains_value | 140,436 | 4.29 |
| memcpy | 137,922 | 4.21 |
| __riscv_clmul_64 | 119,913 | 3.66 |
| expand_and_sum | 116,426 | 3.55 |
| __riscv_clmulh_64 | 114,324 | 3.49 |
| keccak_inc_squeeze | 103,803 | 3.17 |
| gf_reduce | 95,013 | 2.90 |
| kmul_35 | 84,216 | 2.57 |
| find_peaks | 71,293 | 2.18 |
| kmul_18 | 70,752 | 2.16 |
| kmul_17 | 51,282 | 1.57 |
| kmul_69 | 41,520 | 1.27 |
| kmul_139 | 35,340 | 1.08 |
| kmul_277 | 34,896 | 1.07 |
| kmul_70 | 32,496 | 0.99 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 119,913 | 3.66 |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 114,324 | 3.49 |
| memset at mem.c:84 (discriminator 1) | 102,557 | 3.13 |
| memset at mem.c:85 | 102,215 | 3.12 |
| memset at mem.c:84 | 102,215 | 3.12 |
| keccak_inc_absorb at fips202.c:438 | 94,325 | 2.88 |
| support_contains_value at vector.c:81 | 92,340 | 2.82 |
| hadamard at reed_muller.c:98 | 82,432 | 2.52 |
| hadamard at reed_muller.c:99 | 82,432 | 2.52 |
| clmul_leaf at gf2x_clmul.c:370 | 79,104 | 2.42 |
| clmul_leaf at gf2x_clmul.c:372 | 79,104 | 2.42 |
| clmul_leaf at gf2x_clmul.c:371 | 79,104 | 2.42 |
| keccak_inc_squeeze at fips202.c:505 | 68,928 | 2.10 |
| memcpy at mem.c:33 (discriminator 1) | 55,270 | 1.69 |
| memcpy at mem.c:34 | 54,764 | 1.67 |
| clmul_leaf at gf2x_clmul.c:369 | 49,440 | 1.51 |
| expand_and_sum at reed_muller.c:132 | 48,024 | 1.47 |
| support_contains_value at vector.c:80 (discriminator 1) | 47,013 | 1.44 |
| gf_reduce at gf.c:80 | 44,712 | 1.37 |
| gf_reduce at gf.c:82 | 44,712 | 1.37 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 789,904 | 19.46 |
| xor | 621,405 | 15.31 |
| bnez | 394,853 | 9.73 |
| addi | 369,500 | 9.10 |
| sd | 285,682 | 7.04 |
| bne | 244,815 | 6.03 |
| sb | 141,039 | 3.47 |
| clmul | 119,913 | 2.95 |
| clmulh | 114,324 | 2.82 |
| lhu | 105,605 | 2.60 |
| rori | 98,832 | 2.43 |
| andn | 85,606 | 2.11 |
| sh | 63,977 | 1.58 |
| slliw | 52,634 | 1.30 |
| add | 51,354 | 1.26 |
| lbu | 49,743 | 1.23 |
| andi | 38,826 | 0.96 |
| addw | 35,580 | 0.88 |
| or | 33,851 | 0.83 |
| addiw | 31,824 | 0.78 |

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

