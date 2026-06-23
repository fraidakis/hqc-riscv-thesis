# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-23 18:43:06 |
| Commit | `f21e6c4` |

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
| clmul_leaf | 3,862,494 | 40.56 |
| KeccakF1600_StatePermute | 1,752,202 | 18.40 |
| __riscv_clmul_64 | 904,026 | 9.49 |
| __riscv_clmulh_64 | 887,928 | 9.32 |
| keccak_inc_absorb | 473,564 | 4.97 |
| keccak_inc_squeeze | 300,162 | 3.15 |
| gf_reduce | 219,810 | 2.31 |
| kmul_28 | 127,410 | 1.34 |
| memset | 113,049 | 1.19 |
| compute_error_values | 109,356 | 1.15 |
| vect_compare | 100,968 | 1.06 |
| kmul_29 | 80,256 | 0.84 |
| kmul_57 | 60,456 | 0.63 |
| compute_elp | 57,622 | 0.61 |
| reed_solomon_encode | 56,680 | 0.60 |
| gf_mul | 56,375 | 0.59 |
| kmul_113 | 48,180 | 0.51 |
| kmul_56 | 45,954 | 0.48 |
| support_contains_value | 23,697 | 0.25 |
| vect_generate_random_support2 | 19,822 | 0.21 |

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

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| xor | 2,586,251 | 24.98 |
| ld | 1,783,673 | 17.23 |
| sd | 1,229,400 | 11.87 |
| clmul | 900,858 | 8.70 |
| clmulh | 887,928 | 8.57 |
| bnez | 452,473 | 4.37 |
| addi | 350,976 | 3.39 |
| rori | 293,016 | 2.83 |
| andn | 252,600 | 2.44 |
| add | 181,797 | 1.76 |
| slliw | 134,926 | 1.30 |
| bne | 127,688 | 1.23 |
| andi | 118,521 | 1.14 |
| sh3add | 92,706 | 0.90 |
| vle64.v | 80,432 | 0.78 |
| bltu | 64,994 | 0.63 |
| lbu | 64,645 | 0.62 |
| sb | 58,207 | 0.56 |
| mv | 54,428 | 0.53 |
| vxor.vv | 54,238 | 0.52 |

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

