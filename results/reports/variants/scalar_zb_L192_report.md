# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-30 23:42:27 |
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
| clmul_leaf_n9 | 1,357,764 | 18.50 |
| KeccakF1600_StatePermute | 1,152,874 | 15.71 |
| scalar_xor3_u64 | 648,270 | 8.83 |
| memset | 597,330 | 8.14 |
| scalar_xor_u64 | 360,450 | 4.91 |
| __riscv_clmul_64 | 355,562 | 4.85 |
| __riscv_clmulh_64 | 348,732 | 4.75 |
| support_contains_value | 324,300 | 4.42 |
| keccak_inc_absorb | 316,045 | 4.31 |
| hadamard | 279,496 | 3.81 |
| memcpy | 275,464 | 3.75 |
| expand_and_sum | 246,064 | 3.35 |
| keccak_inc_squeeze | 203,516 | 2.77 |
| scalar_copy_u64 | 144,060 | 1.96 |
| gf_reduce | 127,636 | 1.74 |
| clmul_leaf_n8 | 107,640 | 1.47 |
| find_peaks | 86,883 | 1.18 |
| vect_compare | 62,873 | 0.86 |
| reduce | 37,056 | 0.50 |
| vect_add | 35,920 | 0.49 |

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

