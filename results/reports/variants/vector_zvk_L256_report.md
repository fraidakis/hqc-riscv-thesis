# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-23 14:23:32 |
| Commit | `d38ed14` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 63,804 | 5882 |
| 192 | vect_mul | 189,388 | 6894 |
| 256 | vect_mul | 272,896 | 7082 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 209,826 | 30,774 |
| 128 | encaps | 419,771 | 30,774 |
| 128 | decaps | 824,660 | 30,774 |
| 128 | **total** | 1,454,257 | 30,774 |
| 192 | keypair | 450,111 | 33,834 |
| 192 | encaps | 914,528 | 33,834 |
| 192 | decaps | 1,719,604 | 33,834 |
| 192 | **total** | 3,084,243 | 33,834 |
| 256 | keypair | 690,994 | 39,906 |
| 256 | encaps | 1,407,194 | 39,906 |
| 256 | decaps | 2,764,123 | 39,906 |
| 256 | **total** | 4,862,311 | 39,906 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1314 | 84 | 1230 | 146 | 13,142,297 | 7,174,511 | 1,922,241 | 1,080,842 |
| 192 | 3026 | 206 | 2820 | 735 | 30,742,554 | 19,952,135 | 4,294,814 | 2,786,754 |
| 256 | 6814 | 867 | 5947 | 3193 | 59,825,441 | 42,194,770 | 8,239,574 | 5,770,845 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1583 | 1530 | 23,269 | 17,714 | 364 | 277 |
| 192 | 1596 | 1508 | 25,285 | 18,620 | 396 | 291 |
| 256 | 1880 | 1759 | 27,291 | 20,521 | 427 | 321 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute | 1,752,202 | 35.97 |
| vclmul_leaf | 1,101,198 | 22.60 |
| keccak_inc_absorb | 473,564 | 9.72 |
| keccak_inc_squeeze | 300,162 | 6.16 |
| rvv_xor3_u64 | 155,442 | 3.19 |
| gf_reduce | 126,038 | 2.59 |
| memset | 112,827 | 2.32 |
| rvv_copy_u64 | 112,794 | 2.32 |
| vect_compare | 100,968 | 2.07 |
| compute_error_values_zvbc | 92,328 | 1.90 |
| rvv_xor_u64 | 90,384 | 1.86 |
| reduce | 59,502 | 1.22 |
| reed_solomon_encode | 56,680 | 1.16 |
| kmul_28 | 46,500 | 0.95 |
| gf_mul | 31,724 | 0.65 |
| kmul_29 | 27,984 | 0.57 |
| support_contains_value | 23,697 | 0.49 |
| vect_generate_random_support2 | 19,822 | 0.41 |
| expand_and_sum | 15,033 | 0.31 |
| kmul_57 | 14,784 | 0.30 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| keccak_inc_absorb at fips202.c:438 | 315,953 | 6.49 |
| keccak_inc_squeeze at fips202.c:505 | 212,416 | 4.36 |
| keccak_inc_absorb at fips202.c:435 | 86,615 | 1.78 |
| gf_reduce at gf.c:80 | 59,312 | 1.22 |
| gf_reduce at gf.c:82 | 59,312 | 1.22 |
| vect_compare at vector.c:282 | 57,684 | 1.18 |
| vclmul_leaf at gf2x_vclmul.c:218 | 52,488 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:226 | 52,488 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:234 | 52,488 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:242 | 52,488 | 1.08 |
| memset at mem.c:85 | 52,384 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:215 | 39,366 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:110 | 35,364 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:114 | 35,364 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:208 | 35,364 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:134 | 34,101 | 0.70 |
| KeccakF1600_StatePermute at fips202.c:229 | 34,101 | 0.70 |
| vclmul_leaf at gf2x_vclmul.c:193 | 30,618 | 0.63 |
| KeccakF1600_StatePermute at fips202.c:113 | 30,312 | 0.62 |
| KeccakF1600_StatePermute at fips202.c:111 | 30,312 | 0.62 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| xor | 895,320 | 15.76 |
| ld | 851,677 | 14.99 |
| bnez | 449,599 | 7.92 |
| addi | 327,928 | 5.77 |
| rori | 293,016 | 5.16 |
| add | 286,480 | 5.04 |
| andn | 252,600 | 4.45 |
| sd | 246,770 | 4.34 |
| vle64.v | 208,202 | 3.67 |
| vse64.v | 162,252 | 2.86 |
| vxor.vv | 153,813 | 2.71 |
| sh3add | 137,993 | 2.43 |
| andi | 118,463 | 2.09 |
| slliw | 101,845 | 1.79 |
| bne | 87,939 | 1.55 |
| li | 80,054 | 1.41 |
| slli | 77,974 | 1.37 |
| sub | 66,548 | 1.17 |
| vsetvli | 66,370 | 1.17 |
| lbu | 64,639 | 1.14 |

## 4. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 702,755 |
| total | 512 | 677,092 |
| total | 1024 | 679,548 |
| total | 2048 | 703,870 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 1,653,804 |
| total | 512 | 1,561,614 |
| total | 1024 | 1,549,935 |
| total | 2048 | 1,569,907 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 2,544,133 |
| total | 512 | 2,404,440 |
| total | 1024 | 2,386,495 |
| total | 2048 | 2,404,029 |

## 5. Compiler Comparison

### HQC-128

> _No data available._

### HQC-192

> _No data available._

### HQC-256

> _No data available._

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

