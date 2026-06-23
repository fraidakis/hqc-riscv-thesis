# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-23 14:21:01 |
| Commit | `d38ed14` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 142,197 | 6430 |
| 192 | vect_mul | 441,263 | 7608 |
| 256 | vect_mul | 1,014,499 | 7064 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 288,219 | 30,680 |
| 128 | encaps | 576,557 | 30,680 |
| 128 | decaps | 1,120,509 | 30,680 |
| 128 | **total** | 1,985,285 | 30,680 |
| 192 | keypair | 701,986 | 35,028 |
| 192 | encaps | 1,418,278 | 35,028 |
| 192 | decaps | 2,554,126 | 35,028 |
| 192 | **total** | 4,674,390 | 35,028 |
| 256 | keypair | 1,432,597 | 39,730 |
| 256 | encaps | 2,890,400 | 39,730 |
| 256 | decaps | 5,186,418 | 39,730 |
| 256 | **total** | 9,509,415 | 39,730 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 878 | 73 | 805 | 57 | 10,377,481 | 4,752,239 | 1,576,639 | 778,058 |
| 192 | 1889 | 176 | 1713 | 310 | 22,901,560 | 12,700,829 | 3,314,695 | 1,880,346 |
| 256 | 3689 | 547 | 3142 | 1087 | 39,538,591 | 25,888,840 | 5,703,723 | 3,732,609 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1844 | 1728 | 26,585 | 17,670 | 416 | 277 |
| 192 | 1889 | 1651 | 29,575 | 18,076 | 463 | 283 |
| 256 | 2394 | 2184 | 33,019 | 22,153 | 516 | 347 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute | 591,004 | 29.55 |
| clmul_leaf | 457,320 | 22.86 |
| keccak_inc_absorb | 151,064 | 7.55 |
| __riscv_clmul_64 | 120,783 | 6.04 |
| __riscv_clmulh_64 | 116,322 | 5.82 |
| keccak_inc_squeeze | 103,803 | 5.19 |
| gf_reduce | 72,063 | 3.60 |
| memset | 44,514 | 2.23 |
| kmul_18 | 36,168 | 1.81 |
| kmul_17 | 34,188 | 1.71 |
| clmul_leaf_n8 | 31,968 | 1.60 |
| vect_compare | 31,052 | 1.55 |
| kmul_35 | 26,532 | 1.33 |
| compute_error_values | 18,405 | 0.92 |
| gf_mul | 16,952 | 0.85 |
| reed_solomon_encode | 14,950 | 0.75 |
| compute_elp | 14,765 | 0.74 |
| vect_generate_random_support2 | 10,050 | 0.50 |
| support_contains_value | 8688 | 0.43 |
| kmul_69 | 8550 | 0.43 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 120,783 | 6.04 |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 116,322 | 5.82 |
| keccak_inc_absorb at fips202.c:438 | 94,325 | 4.72 |
| clmul_leaf at gf2x_clmul.c:491 | 79,104 | 3.95 |
| clmul_leaf at gf2x_clmul.c:493 | 79,104 | 3.95 |
| clmul_leaf at gf2x_clmul.c:492 | 79,104 | 3.95 |
| keccak_inc_squeeze at fips202.c:505 | 68,928 | 3.45 |
| clmul_leaf at gf2x_clmul.c:490 | 49,440 | 2.47 |
| clmul_leaf at gf2x_clmul.c:494 | 39,552 | 1.98 |
| gf_reduce at gf.c:80 | 33,912 | 1.70 |
| gf_reduce at gf.c:82 | 33,912 | 1.70 |
| clmul_leaf at gf2x_clmul.c:479 (discriminator 1) | 29,664 | 1.48 |
| keccak_inc_absorb at fips202.c:435 | 25,865 | 1.29 |
| memset at mem.c:85 | 20,508 | 1.03 |
| clmul_leaf at gf2x_clmul.c:517 | 19,776 | 0.99 |
| vect_compare at vector.c:282 | 17,732 | 0.89 |
| clmul_leaf at gf2x_clmul.c:425 | 14,832 | 0.74 |
| clmul_leaf at gf2x_clmul.c:521 | 12,360 | 0.62 |
| KeccakF1600_StatePermute at fips202.c:110 | 11,928 | 0.60 |
| KeccakF1600_StatePermute at fips202.c:114 | 11,928 | 0.60 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 647,313 | 23.20 |
| xor | 515,437 | 18.47 |
| bnez | 403,999 | 14.48 |
| sd | 204,091 | 7.32 |
| clmul | 118,563 | 4.25 |
| clmulh | 114,324 | 4.10 |
| rori | 98,832 | 3.54 |
| addi | 95,537 | 3.42 |
| andn | 85,200 | 3.05 |
| add | 62,156 | 2.23 |
| slliw | 44,545 | 1.60 |
| andi | 39,000 | 1.40 |
| bne | 31,870 | 1.14 |
| sh3add | 25,344 | 0.91 |
| bltu | 21,953 | 0.79 |
| sb | 21,681 | 0.78 |
| vle64.v | 21,204 | 0.76 |
| lbu | 21,011 | 0.75 |
| mv | 17,406 | 0.62 |
| zext.b | 14,989 | 0.54 |

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

