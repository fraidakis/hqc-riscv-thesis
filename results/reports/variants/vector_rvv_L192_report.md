# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
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
| clmul_leaf_n9 | 1,359,426 | 28.46 |
| KeccakF1600_StatePermute | 1,152,874 | 24.13 |
| __riscv_clmul_64 | 354,582 | 7.42 |
| __riscv_clmulh_64 | 350,868 | 7.34 |
| keccak_inc_absorb | 298,128 | 6.24 |
| kmul_18 | 237,096 | 4.96 |
| keccak_inc_squeeze | 203,516 | 4.26 |
| clmul_leaf_n8 | 106,860 | 2.24 |
| gf_reduce | 97,716 | 2.05 |
| kmul_17 | 84,630 | 1.77 |
| rvv_xor3_u64 | 74,334 | 1.56 |
| vect_compare | 62,867 | 1.32 |
| memset | 47,560 | 1.00 |
| rvv_xor_u64 | 42,918 | 0.90 |
| reduce | 37,056 | 0.78 |
| reed_solomon_encode | 23,814 | 0.50 |
| kmul_35 | 23,400 | 0.49 |
| gf_mul | 22,753 | 0.48 |
| compute_error_values | 21,956 | 0.46 |
| compute_elp | 16,692 | 0.35 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 692,400 | 14.68 |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 635,658 | 13.48 |
| keccak_inc_absorb at fips202.c:438 | 196,658 | 4.17 |
| keccak_inc_squeeze at fips202.c:505 | 139,520 | 2.96 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:113 | 60,876 | 1.29 |
| keccak_inc_absorb at fips202.c:435 | 53,920 | 1.14 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:123 | 53,400 | 1.13 |
| gf_reduce at gf.c:80 | 45,984 | 0.98 |
| gf_reduce at gf.c:82 | 45,984 | 0.98 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:92 | 38,448 | 0.82 |
| vect_compare at vector.c:282 | 35,912 | 0.76 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:91 | 26,700 | 0.57 |
| KeccakF1600_StatePermute at fips202.c:110 | 23,268 | 0.49 |
| KeccakF1600_StatePermute at fips202.c:114 | 23,268 | 0.49 |
| KeccakF1600_StatePermute at fips202.c:208 | 23,268 | 0.49 |
| kmul_17 at karatsuba_rvv_specialized_gen.h:51 | 22,620 | 0.48 |
| KeccakF1600_StatePermute at fips202.c:134 | 22,437 | 0.48 |
| KeccakF1600_StatePermute at fips202.c:229 | 22,437 | 0.48 |
| memset at mem.c:85 | 20,655 | 0.44 |
| kmul_17 at karatsuba_rvv_specialized_gen.h:61 | 20,280 | 0.43 |

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

