# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
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
| KeccakF1600_StatePermute | 591,004 | 40.27 |
| vclmul_leaf | 233,154 | 15.89 |
| keccak_inc_absorb | 151,064 | 10.29 |
| keccak_inc_squeeze | 103,803 | 7.07 |
| memset | 46,358 | 3.16 |
| gf_reduce | 40,018 | 2.73 |
| rvv_xor3_u64 | 39,300 | 2.68 |
| vect_compare | 31,052 | 2.12 |
| rvv_copy_u64 | 28,086 | 1.91 |
| rvv_xor_u64 | 25,728 | 1.75 |
| reduce | 18,312 | 1.25 |
| compute_error_values_zvbc | 15,418 | 1.05 |
| reed_solomon_encode | 14,950 | 1.02 |
| kmul_18 | 13,200 | 0.90 |
| kmul_17 | 11,544 | 0.79 |
| vect_generate_random_support2 | 10,050 | 0.68 |
| gf_mul | 8952 | 0.61 |
| support_contains_value | 8688 | 0.59 |
| kmul_35 | 7392 | 0.50 |
| vect_write_support_to_vector | 5868 | 0.40 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| keccak_inc_absorb at fips202.c:438 | 94,325 | 6.43 |
| keccak_inc_squeeze at fips202.c:505 | 68,928 | 4.70 |
| keccak_inc_absorb at fips202.c:435 | 25,865 | 1.76 |
| memset at mem.c:85 | 21,480 | 1.46 |
| gf_reduce at gf.c:80 | 18,832 | 1.28 |
| gf_reduce at gf.c:82 | 18,832 | 1.28 |
| vect_compare at vector.c:282 | 17,732 | 1.21 |
| KeccakF1600_StatePermute at fips202.c:110 | 11,928 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:114 | 11,928 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:208 | 11,928 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:134 | 11,502 | 0.78 |
| KeccakF1600_StatePermute at fips202.c:229 | 11,502 | 0.78 |
| vclmul_leaf at gf2x_vclmul.c:218 | 10,776 | 0.73 |
| vclmul_leaf at gf2x_vclmul.c:226 | 10,776 | 0.73 |
| vclmul_leaf at gf2x_vclmul.c:234 | 10,776 | 0.73 |
| vclmul_leaf at gf2x_vclmul.c:242 | 10,776 | 0.73 |
| memset at mem.c:84 (discriminator 3) | 10,723 | 0.73 |
| memset at mem.c:84 (discriminator 1) | 10,723 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:113 | 10,224 | 0.70 |
| KeccakF1600_StatePermute at fips202.c:111 | 10,224 | 0.70 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 516,398 | 23.26 |
| bnez | 388,207 | 17.49 |
| xor | 298,394 | 13.44 |
| rori | 98,832 | 4.45 |
| addi | 93,562 | 4.21 |
| andn | 85,200 | 3.84 |
| add | 84,665 | 3.81 |
| sd | 82,029 | 3.69 |
| vle64.v | 45,852 | 2.07 |
| andi | 38,986 | 1.76 |
| vse64.v | 35,914 | 1.62 |
| sh3add | 34,042 | 1.53 |
| slliw | 33,242 | 1.50 |
| vxor.vv | 33,029 | 1.49 |
| bne | 26,062 | 1.17 |
| bltu | 22,470 | 1.01 |
| sb | 22,159 | 1.00 |
| li | 21,641 | 0.97 |
| lbu | 21,005 | 0.95 |
| mv | 20,115 | 0.91 |

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

