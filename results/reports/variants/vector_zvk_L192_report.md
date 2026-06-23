# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
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
| KeccakF1600_StatePermute | 1,152,874 | 36.74 |
| vclmul_leaf | 698,976 | 22.27 |
| keccak_inc_absorb | 298,128 | 9.50 |
| keccak_inc_squeeze | 203,516 | 6.49 |
| rvv_xor3_u64 | 121,692 | 3.88 |
| rvv_copy_u64 | 88,896 | 2.83 |
| rvv_xor_u64 | 78,972 | 2.52 |
| vect_compare | 62,867 | 2.00 |
| gf_reduce | 53,856 | 1.72 |
| kmul_18 | 53,400 | 1.70 |
| memset | 47,353 | 1.51 |
| reduce | 37,056 | 1.18 |
| kmul_35 | 25,350 | 0.81 |
| reed_solomon_encode | 23,814 | 0.76 |
| compute_error_values_zvbc | 19,471 | 0.62 |
| support_contains_value | 16,002 | 0.51 |
| vect_generate_random_support2 | 15,198 | 0.48 |
| gf_mul | 12,782 | 0.41 |
| kmul_17 | 11,700 | 0.37 |
| ?? ??:0 | 9545 | 0.30 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| keccak_inc_absorb at fips202.c:438 | 196,658 | 6.27 |
| keccak_inc_squeeze at fips202.c:505 | 139,520 | 4.45 |
| keccak_inc_absorb at fips202.c:435 | 53,920 | 1.72 |
| vect_compare at vector.c:282 | 35,912 | 1.14 |
| vclmul_leaf at gf2x_vclmul.c:218 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:226 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:234 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:242 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:215 | 25,464 | 0.81 |
| gf_reduce at gf.c:80 | 25,344 | 0.81 |
| gf_reduce at gf.c:82 | 25,344 | 0.81 |
| vclmul_leaf at gf2x_vclmul.c:185 (discriminator 1) | 23,904 | 0.76 |
| KeccakF1600_StatePermute at fips202.c:110 | 23,268 | 0.74 |
| KeccakF1600_StatePermute at fips202.c:114 | 23,268 | 0.74 |
| KeccakF1600_StatePermute at fips202.c:208 | 23,268 | 0.74 |
| KeccakF1600_StatePermute at fips202.c:134 | 22,437 | 0.72 |
| KeccakF1600_StatePermute at fips202.c:229 | 22,437 | 0.72 |
| vclmul_leaf at gf2x_vclmul.c:193 | 21,090 | 0.67 |
| memset at mem.c:85 | 20,742 | 0.66 |
| KeccakF1600_StatePermute at fips202.c:113 | 19,944 | 0.64 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 693,408 | 17.67 |
| xor | 569,376 | 14.51 |
| bnez | 429,171 | 10.93 |
| addi | 196,493 | 5.01 |
| rori | 192,792 | 4.91 |
| add | 174,691 | 4.45 |
| sd | 169,681 | 4.32 |
| andn | 166,200 | 4.23 |
| vle64.v | 139,482 | 3.55 |
| vse64.v | 108,878 | 2.77 |
| sh3add | 103,370 | 2.63 |
| vxor.vv | 100,104 | 2.55 |
| andi | 75,581 | 1.93 |
| li | 57,675 | 1.47 |
| slliw | 56,441 | 1.44 |
| mv | 52,958 | 1.35 |
| bne | 51,217 | 1.30 |
| sub | 49,143 | 1.25 |
| vsetvli | 48,340 | 1.23 |
| slli | 45,697 | 1.16 |

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

