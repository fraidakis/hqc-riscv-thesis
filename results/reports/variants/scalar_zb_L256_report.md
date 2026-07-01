# HQC-RISC-V Analysis: `scalar_zb`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
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
| clmul_leaf_n14 | 2,715,180 | 19.38 |
| KeccakF1600_StatePermute | 1,752,202 | 12.50 |
| scalar_xor3_u64 | 1,086,480 | 7.75 |
| clmul_leaf_n15 | 990,000 | 7.07 |
| memset | 933,355 | 6.66 |
| __riscv_clmul_64 | 906,020 | 6.47 |
| __riscv_clmulh_64 | 889,386 | 6.35 |
| scalar_xor_u64 | 648,714 | 4.63 |
| support_contains_value | 554,346 | 3.96 |
| keccak_inc_absorb | 502,326 | 3.58 |
| hadamard | 449,190 | 3.21 |
| memcpy | 448,010 | 3.20 |
| expand_and_sum | 395,460 | 2.82 |
| gf_reduce | 307,564 | 2.19 |
| keccak_inc_squeeze | 300,162 | 2.14 |
| scalar_copy_u64 | 224,886 | 1.60 |
| find_peaks | 139,505 | 1.00 |
| compute_error_values | 104,949 | 0.75 |
| vect_compare | 100,974 | 0.72 |
| gf_mul | 82,185 | 0.59 |

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

