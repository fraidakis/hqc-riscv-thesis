# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-30 23:42:22 |
| Commit | `bdd9cf4` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 182,726 | 10,362 |
| 192 | vect_mul | 570,293 | 9808 |
| 256 | vect_mul | 1,262,888 | 11,994 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 428,923 | 29,520 |
| 128 | encaps | 834,245 | 29,520 |
| 128 | decaps | 1,972,347 | 29,520 |
| 128 | **total** | 3,235,515 | 29,520 |
| 192 | keypair | 1,041,343 | 31,646 |
| 192 | encaps | 2,061,241 | 31,646 |
| 192 | decaps | 4,181,542 | 31,646 |
| 192 | **total** | 7,284,126 | 31,646 |
| 256 | keypair | 2,019,141 | 39,104 |
| 256 | encaps | 3,999,221 | 39,104 |
| 256 | decaps | 7,980,756 | 39,104 |
| 256 | **total** | 13,999,118 | 39,104 |

## 2. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute | 591,004 | 18.18 |
| clmul_leaf_n9 | 420,366 | 12.93 |
| memset | 307,101 | 9.45 |
| hadamard | 229,586 | 7.06 |
| scalar_xor3_u64 | 193,260 | 5.95 |
| keccak_inc_absorb | 159,678 | 4.91 |
| support_contains_value | 140,436 | 4.32 |
| memcpy | 137,922 | 4.24 |
| __riscv_clmul_64 | 119,913 | 3.69 |
| expand_and_sum | 116,426 | 3.58 |
| __riscv_clmulh_64 | 114,588 | 3.53 |
| scalar_xor_u64 | 105,930 | 3.26 |
| keccak_inc_squeeze | 103,803 | 3.19 |
| gf_reduce | 95,013 | 2.92 |
| find_peaks | 71,293 | 2.19 |
| clmul_leaf_n8 | 61,272 | 1.89 |
| scalar_copy_u64 | 42,198 | 1.30 |
| vect_compare | 31,058 | 0.96 |
| gf_mul | 23,702 | 0.73 |
| reduce | 18,312 | 0.56 |

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

## 4. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | scalar_zb | OK | 2,834,259 | 522,825 |
| gcc | -O3 | scalar_zb | OK | 1,902,665 | 539,721 |
| clang | -O2 | scalar_zb | OK | 2,151,335 | 542,145 |
| clang | -O3 | scalar_zb | OK | 2,163,048 | 563,401 |

### HQC-192

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | scalar_zb | OK | 6,996,043 | 525,137 |
| gcc | -O3 | scalar_zb | OK | 4,223,873 | 541,065 |
| clang | -O2 | scalar_zb | OK | 4,874,759 | 544,435 |
| clang | -O3 | scalar_zb | OK | 4,882,834 | 565,625 |

### HQC-256

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | scalar_zb | OK | 17,234,639 | 532,185 |
| gcc | -O3 | scalar_zb | OK | 8,106,449 | 548,257 |
| clang | -O2 | scalar_zb | OK | 11,467,224 | 551,225 |
| clang | -O3 | scalar_zb | OK | 11,144,875 | 567,675 |

## 5. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

