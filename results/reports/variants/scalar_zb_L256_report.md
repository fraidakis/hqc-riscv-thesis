# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
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
| clmul_leaf | 3,861,438 | 27.05 |
| KeccakF1600_StatePermute | 1,752,202 | 12.27 |
| memset | 933,355 | 6.54 |
| __riscv_clmul_64 | 909,188 | 6.37 |
| __riscv_clmulh_64 | 887,928 | 6.22 |
| support_contains_value | 554,346 | 3.88 |
| keccak_inc_absorb | 502,326 | 3.52 |
| hadamard | 449,190 | 3.15 |
| memcpy | 448,010 | 3.14 |
| kmul_28 | 402,690 | 2.82 |
| expand_and_sum | 395,460 | 2.77 |
| kmul_113 | 318,252 | 2.23 |
| gf_reduce | 307,564 | 2.15 |
| kmul_57 | 302,016 | 2.12 |
| keccak_inc_squeeze | 300,162 | 2.10 |
| kmul_56 | 244,644 | 1.71 |
| kmul_29 | 238,128 | 1.67 |
| kmul_225 | 142,140 | 1.00 |
| find_peaks | 139,505 | 0.98 |
| kmul_901 | 134,382 | 0.94 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 909,188 | 6.37 |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 887,928 | 6.22 |
| clmul_leaf at gf2x_clmul.c:370 | 695,016 | 4.87 |
| clmul_leaf at gf2x_clmul.c:371 | 695,016 | 4.87 |
| clmul_leaf at gf2x_clmul.c:372 | 695,016 | 4.87 |
| clmul_leaf at gf2x_clmul.c:369 | 405,426 | 2.84 |
| support_contains_value at vector.c:81 | 367,000 | 2.57 |
| clmul_leaf at gf2x_clmul.c:373 | 347,508 | 2.43 |
| keccak_inc_absorb at fips202.c:438 | 315,953 | 2.21 |
| memset at mem.c:84 (discriminator 1) | 311,355 | 2.18 |
| memset at mem.c:85 | 310,929 | 2.18 |
| memset at mem.c:84 | 310,929 | 2.18 |
| keccak_inc_squeeze at fips202.c:505 | 212,416 | 1.49 |
| expand_and_sum at reed_muller.c:132 | 187,200 | 1.31 |
| support_contains_value at vector.c:80 (discriminator 1) | 185,179 | 1.30 |
| memcpy at mem.c:33 (discriminator 1) | 179,536 | 1.26 |
| memcpy at mem.c:34 | 177,876 | 1.25 |
| clmul_leaf at gf2x_clmul.c:358 (discriminator 1) | 173,754 | 1.22 |
| clmul_leaf at gf2x_clmul.c:391 | 172,536 | 1.21 |
| hadamard at reed_muller.c:98 | 161,280 | 1.13 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| xor | 3,101,853 | 20.54 |
| ld | 2,654,164 | 17.57 |
| sd | 1,675,981 | 11.10 |
| addi | 1,299,350 | 8.60 |
| clmul | 906,020 | 6.00 |
| clmulh | 887,928 | 5.88 |
| bne | 821,460 | 5.44 |
| sb | 431,943 | 2.86 |
| bnez | 427,891 | 2.83 |
| rori | 293,016 | 1.94 |
| andn | 254,253 | 1.68 |
| lhu | 246,291 | 1.63 |
| slliw | 165,901 | 1.10 |
| add | 164,702 | 1.09 |
| lbu | 158,745 | 1.05 |
| sh | 155,720 | 1.03 |
| or | 118,572 | 0.78 |
| andi | 118,373 | 0.78 |
| addw | 98,062 | 0.65 |
| sub | 96,141 | 0.64 |

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

