# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-30 23:42:29 |
| Commit | `bdd9cf4` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 148,479 | 9606 |
| 192 | vect_mul | 449,808 | 10,796 |
| 256 | vect_mul | 1,043,726 | 13,834 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 294,501 | 29,776 |
| 128 | encaps | 589,121 | 29,776 |
| 128 | decaps | 1,139,356 | 29,776 |
| 128 | **total** | 2,022,978 | 29,776 |
| 192 | keypair | 710,531 | 34,124 |
| 192 | encaps | 1,435,368 | 34,124 |
| 192 | decaps | 2,579,762 | 34,124 |
| 192 | **total** | 4,725,661 | 34,124 |
| 256 | keypair | 1,461,824 | 42,402 |
| 256 | encaps | 2,948,854 | 42,402 |
| 256 | decaps | 5,274,100 | 42,402 |
| 256 | **total** | 9,684,778 | 42,402 |

## 2. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul_leaf_n14 | 2,715,708 | 28.01 |
| KeccakF1600_StatePermute | 1,752,202 | 18.07 |
| clmul_leaf_n15 | 990,000 | 10.21 |
| __riscv_clmul_64 | 900,858 | 9.29 |
| __riscv_clmulh_64 | 889,386 | 9.17 |
| keccak_inc_absorb | 473,564 | 4.88 |
| kmul_28 | 319,920 | 3.30 |
| keccak_inc_squeeze | 300,162 | 3.10 |
| gf_reduce | 219,810 | 2.27 |
| kmul_29 | 190,080 | 1.96 |
| memset | 113,049 | 1.17 |
| compute_error_values | 109,356 | 1.13 |
| vect_compare | 100,968 | 1.04 |
| rvv_xor3_u64 | 98,010 | 1.01 |
| reduce | 59,502 | 0.61 |
| compute_elp | 57,622 | 0.59 |
| reed_solomon_encode | 56,680 | 0.58 |
| gf_mul | 56,375 | 0.58 |
| rvv_xor_u64 | 53,406 | 0.55 |
| support_contains_value | 23,697 | 0.24 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 904,026 | 9.49 |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 887,928 | 9.32 |
| clmul_leaf at gf2x_clmul.c:797 | 695,016 | 7.30 |
| clmul_leaf at gf2x_clmul.c:798 | 695,016 | 7.30 |
| clmul_leaf at gf2x_clmul.c:799 | 695,016 | 7.30 |
| clmul_leaf at gf2x_clmul.c:796 | 405,426 | 4.26 |
| clmul_leaf at gf2x_clmul.c:800 | 347,508 | 3.65 |
| keccak_inc_absorb at fips202.c:438 | 315,953 | 3.32 |
| keccak_inc_squeeze at fips202.c:505 | 212,416 | 2.23 |
| clmul_leaf at gf2x_clmul.c:785 (discriminator 1) | 173,754 | 1.82 |
| clmul_leaf at gf2x_clmul.c:818 | 172,536 | 1.81 |
| gf_reduce at gf.c:80 | 103,440 | 1.09 |
| gf_reduce at gf.c:82 | 103,440 | 1.09 |
| keccak_inc_absorb at fips202.c:435 | 86,615 | 0.91 |
| clmul_leaf at gf2x_clmul.c:817 | 86,268 | 0.91 |
| clmul_leaf at gf2x_clmul.c:731 | 63,498 | 0.67 |
| clmul_leaf at gf2x_clmul.c:827 | 61,236 | 0.64 |
| clmul_leaf at gf2x_clmul.c:808 | 59,136 | 0.62 |
| clmul_leaf at gf2x_clmul.c:791 | 57,918 | 0.61 |
| vect_compare at vector.c:282 | 57,684 | 0.61 |

## 3. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

> _No data rows._

### VLEN Sweep — HQC-192

> _No data rows._

### VLEN Sweep — HQC-256

> _No data rows._

## 4. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | vector_rvv | OK | 2,667,282 | 531,235 |
| gcc | -O3 | vector_rvv | OK | 1,585,434 | 541,883 |
| clang | -O2 | vector_rvv | OK | 1,712,970 | 544,425 |
| clang | -O3 | vector_rvv | OK | 1,769,249 | 550,849 |

### HQC-192

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | vector_rvv | OK | 6,726,919 | 533,787 |
| gcc | -O3 | vector_rvv | OK | 3,592,013 | 546,763 |
| clang | -O2 | vector_rvv | OK | 4,039,884 | 547,601 |
| clang | -O3 | vector_rvv | OK | 4,145,892 | 554,665 |

### HQC-256

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | vector_rvv | OK | 15,911,458 | 540,867 |
| gcc | -O3 | vector_rvv | OK | 6,848,116 | 555,067 |
| clang | -O2 | vector_rvv | OK | 9,756,962 | 554,225 |
| clang | -O3 | vector_rvv | OK | 9,757,317 | 561,785 |

## 5. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

