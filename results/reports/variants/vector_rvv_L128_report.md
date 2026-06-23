# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
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
| KeccakF1600_StatePermute | 591,004 | 29.72 |
| clmul_leaf_n9 | 236,442 | 11.89 |
| __riscv_clmulh_64 | 216,444 | 10.88 |
| __riscv_clmul_64 | 195,483 | 9.83 |
| keccak_inc_absorb | 151,064 | 7.60 |
| keccak_inc_squeeze | 103,803 | 5.22 |
| gf_reduce | 72,063 | 3.62 |
| kmul_18 | 54,648 | 2.75 |
| kmul_17 | 46,620 | 2.34 |
| memset | 44,514 | 2.24 |
| clmul_leaf_n8 | 36,852 | 1.85 |
| vect_compare | 31,052 | 1.56 |
| kmul_35 | 26,532 | 1.33 |
| compute_error_values | 18,405 | 0.93 |
| gf_mul | 16,952 | 0.85 |
| reed_solomon_encode | 14,950 | 0.75 |
| compute_elp | 14,765 | 0.74 |
| vect_generate_random_support2 | 10,050 | 0.51 |
| support_contains_value | 8688 | 0.44 |
| kmul_69 | 8550 | 0.43 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 216,444 | 10.88 |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 195,483 | 9.83 |
| keccak_inc_absorb at fips202.c:438 | 94,325 | 4.74 |
| keccak_inc_squeeze at fips202.c:505 | 68,928 | 3.47 |
| gf_reduce at gf.c:80 | 33,912 | 1.71 |
| gf_reduce at gf.c:82 | 33,912 | 1.71 |
| keccak_inc_absorb at fips202.c:435 | 25,865 | 1.30 |
| memset at mem.c:85 | 20,508 | 1.03 |
| vect_compare at vector.c:282 | 17,732 | 0.89 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:113 | 15,048 | 0.76 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:123 | 13,200 | 0.66 |
| kmul_17 at karatsuba_rvv_specialized_gen.h:51 | 12,654 | 0.64 |
| KeccakF1600_StatePermute at fips202.c:110 | 11,928 | 0.60 |
| KeccakF1600_StatePermute at fips202.c:114 | 11,928 | 0.60 |
| KeccakF1600_StatePermute at fips202.c:208 | 11,928 | 0.60 |
| KeccakF1600_StatePermute at fips202.c:134 | 11,502 | 0.58 |
| KeccakF1600_StatePermute at fips202.c:229 | 11,502 | 0.58 |
| kmul_17 at karatsuba_rvv_specialized_gen.h:61 | 10,434 | 0.52 |
| memset at mem.c:84 (discriminator 3) | 10,239 | 0.51 |
| memset at mem.c:84 (discriminator 1) | 10,239 | 0.51 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 724,663 | 26.07 |
| xor | 549,805 | 19.78 |
| bnez | 401,213 | 14.43 |
| sd | 158,287 | 5.69 |
| clmul | 118,563 | 4.26 |
| clmulh | 114,102 | 4.10 |
| rori | 98,832 | 3.56 |
| andn | 85,200 | 3.06 |
| addi | 64,763 | 2.33 |
| add | 62,156 | 2.24 |
| slliw | 44,545 | 1.60 |
| andi | 39,000 | 1.40 |
| mv | 28,278 | 1.02 |
| bne | 21,982 | 0.79 |
| bltu | 21,953 | 0.79 |
| sb | 21,681 | 0.78 |
| lbu | 21,011 | 0.76 |
| zext.b | 14,989 | 0.54 |
| sh3add | 13,902 | 0.50 |
| srli | 11,467 | 0.41 |

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

