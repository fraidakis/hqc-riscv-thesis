# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-24 15:21:07 |
| Commit | `0eb2718` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 140,487 | 14,674 |
| 192 | vect_mul | 439,516 | 16,590 |
| 256 | vect_mul | 1,014,499 | 7064 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 286,509 | 38,924 |
| 128 | encaps | 573,137 | 38,924 |
| 128 | decaps | 1,115,379 | 38,924 |
| 128 | **total** | 1,975,025 | 38,924 |
| 192 | keypair | 700,239 | 44,010 |
| 192 | encaps | 1,414,784 | 44,010 |
| 192 | decaps | 2,548,885 | 44,010 |
| 192 | **total** | 4,663,908 | 44,010 |
| 256 | keypair | 1,432,597 | 39,730 |
| 256 | encaps | 2,890,400 | 39,730 |
| 256 | decaps | 5,186,418 | 39,730 |
| 256 | **total** | 9,509,415 | 39,730 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 893 | 76 | 817 | 66 | 10,706,609 | 3,676,551 | 1,617,780 | 643,597 |
| 192 | 1901 | 161 | 1740 | 311 | 24,683,632 | 9,204,117 | 3,537,454 | 1,443,257 |
| 256 | 3621 | 502 | 3119 | 1010 | 39,538,599 | 25,888,832 | 5,703,724 | 3,732,608 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 5030 | 2861 | 49,777 | 25,950 | 778 | 406 |
| 192 | 4637 | 2509 | 49,263 | 24,156 | 770 | 378 |
| 256 | 2394 | 2178 | 33,019 | 22,105 | 516 | 346 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute | 1,152,874 | 24.45 |
| clmul_leaf_n9 | 772,386 | 16.38 |
| __riscv_clmulh_64 | 692,400 | 14.68 |
| __riscv_clmul_64 | 635,658 | 13.48 |
| keccak_inc_absorb | 298,128 | 6.32 |
| kmul_18 | 221,076 | 4.69 |
| keccak_inc_squeeze | 203,516 | 4.32 |
| gf_reduce | 97,716 | 2.07 |
| kmul_35 | 81,510 | 1.73 |
| kmul_17 | 76,440 | 1.62 |
| vect_compare | 62,867 | 1.33 |
| clmul_leaf_n8 | 53,430 | 1.13 |
| memset | 47,560 | 1.01 |
| kmul_70 | 29,754 | 0.63 |
| reed_solomon_encode | 23,814 | 0.51 |
| gf_mul | 22,753 | 0.48 |
| compute_error_values | 21,956 | 0.47 |
| kmul_36 | 18,624 | 0.39 |
| compute_elp | 16,692 | 0.35 |
| support_contains_value | 16,002 | 0.34 |

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

## 4. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 2,018,612 |
| total | 512 | 1,986,842 |
| total | 1024 | 1,979,136 |
| total | 2048 | 1,975,917 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 4,773,181 |
| total | 512 | 4,669,568 |
| total | 1024 | 4,643,585 |
| total | 2048 | 4,633,345 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 12,241,069 |
| total | 512 | 12,099,878 |
| total | 1024 | 12,054,204 |
| total | 2048 | 12,040,736 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_rvv | OK | 1,820,793 | 529,971 |
| gcc | -O3 | karatsuba_rvv | OK | 1,673,598 | 537,187 |
| clang | -O2 | karatsuba_rvv | OK | 1,684,379 | 542,297 |
| clang | -O3 | karatsuba_rvv | OK | 1,660,813 | 546,833 |

### HQC-192

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_rvv | OK | 4,430,532 | 532,515 |
| gcc | -O3 | karatsuba_rvv | OK | 3,975,160 | 541,955 |
| clang | -O2 | karatsuba_rvv | OK | 3,958,739 | 546,945 |
| clang | -O3 | karatsuba_rvv | OK | 3,954,544 | 550,985 |

### HQC-256

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_rvv | OK | 9,429,684 | 538,579 |
| gcc | -O3 | karatsuba_rvv | OK | 7,558,296 | 546,827 |
| clang | -O2 | karatsuba_rvv | OK | 9,629,208 | 549,753 |
| clang | -O3 | karatsuba_rvv | OK | 9,366,365 | 553,481 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

