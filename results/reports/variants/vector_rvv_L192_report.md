# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-23 12:43:28 |
| Commit | `c828690` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 157,473 | 7694 |
| 192 | vect_mul | 473,083 | 10,262 |
| 256 | vect_mul | 1,502,171 | 7350 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 295,804 | 36,730 |
| 128 | encaps | 593,089 | 36,730 |
| 128 | decaps | 1,129,719 | 36,730 |
| 128 | **total** | 2,018,612 | 36,730 |
| 192 | keypair | 720,307 | 40,368 |
| 192 | encaps | 1,458,667 | 40,368 |
| 192 | decaps | 2,594,207 | 40,368 |
| 192 | **total** | 4,773,181 | 40,368 |
| 256 | keypair | 1,897,102 | 43,668 |
| 256 | encaps | 3,838,620 | 43,668 |
| 256 | decaps | 6,505,347 | 43,668 |
| 256 | **total** | 12,241,069 | 43,668 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 900 | 80 | 820 | 56 | 10,879,463 | 5,428,519 | 1,663,755 | 868,297 |
| 192 | 1909 | 175 | 1734 | 309 | 23,848,426 | 14,148,929 | 3,482,594 | 2,070,357 |
| 256 | 3742 | 600 | 3142 | 1114 | 54,307,425 | 29,272,360 | 7,613,925 | 4,152,087 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 2119 | 2188 | 31,524 | 23,443 | 493 | 367 |
| 192 | 2278 | 2094 | 41,912 | 26,856 | 655 | 420 |
| 256 | 2308 | 2167 | 37,401 | 25,568 | 585 | 400 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul_leaf | 1,581,648 | 32.77 |
| KeccakF1600_StatePermute | 1,129,052 | 23.39 |
| clmul64 | 739,788 | 15.33 |
| keccak_inc_absorb | 279,123 | 5.78 |
| keccak_inc_squeeze | 187,760 | 3.89 |
| clmul_leaf_n8 | 156,390 | 3.24 |
| kmul_18 | 128,544 | 2.66 |
| gf_reduce | 109,212 | 2.26 |
| kmul_35 | 77,610 | 1.61 |
| vect_compare | 62,870 | 1.30 |
| kmul_17 | 46,800 | 0.97 |
| memset | 35,955 | 0.74 |
| kmul_70 | 28,290 | 0.59 |
| gf_mul | 22,753 | 0.47 |
| kmul_36 | 18,048 | 0.37 |
| support_contains_value | 16,295 | 0.34 |
| vect_generate_random_support2 | 14,430 | 0.30 |
| kmul_71 | 13,968 | 0.29 |
| reed_solomon_encode | 12,844 | 0.27 |
| compute_elp | 12,152 | 0.25 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:45 | 347,664 | 7.20 |
| clmul64 at gf2x_clmul.c:46 | 347,664 | 7.20 |
| clmul_leaf at gf2x_clmul.c:491 | 286,848 | 5.94 |
| clmul_leaf at gf2x_clmul.c:492 | 254,976 | 5.28 |
| clmul_leaf at gf2x_clmul.c:493 | 254,976 | 5.28 |
| KeccakF1600_StatePermute at fips202.c:? | 192,238 | 3.98 |
| keccak_inc_absorb at fips202.c:438 | 178,780 | 3.70 |
| clmul_leaf at gf2x_clmul.c:490 | 159,360 | 3.30 |
| clmul_leaf at gf2x_clmul.c:494 | 127,488 | 2.64 |
| keccak_inc_squeeze at fips202.c:505 | 104,640 | 2.17 |
| clmul_leaf at gf2x_clmul.c:518 | 95,616 | 1.98 |
| keccak_inc_absorb at fips202.c:435 | 89,792 | 1.86 |
| clmul_leaf at gf2x_clmul.c:479 | 71,712 | 1.49 |
| keccak_inc_squeeze at fips202.c:504 | 70,034 | 1.45 |
| clmul_leaf at gf2x_clmul.c:517 | 63,744 | 1.32 |
| clmul_leaf at gf2x_clmul.c:521 | 59,760 | 1.24 |
| clmul_leaf at gf2x_clmul.c:429 | 55,776 | 1.16 |
| gf_reduce at gf.c:82 | 51,732 | 1.07 |
| clmul64 at gf2x_clmul.c:? | 44,460 | 0.92 |
| gf_reduce at gf.c:80 | 40,236 | 0.83 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| xor | 1,201,440 | 21.30 |
| ld | 1,132,494 | 20.08 |
| sd | 643,745 | 11.41 |
| bnez | 431,345 | 7.65 |
| clmul | 353,412 | 6.27 |
| clmulh | 347,664 | 6.16 |
| addi | 225,492 | 4.00 |
| rori | 192,792 | 3.42 |
| andn | 166,200 | 2.95 |
| mv | 90,709 | 1.61 |
| add | 83,510 | 1.48 |
| bne | 77,677 | 1.38 |
| sh3add | 76,889 | 1.36 |
| vle64.v | 65,454 | 1.16 |
| slli | 62,084 | 1.10 |
| andi | 55,241 | 0.98 |
| vxor.vv | 43,266 | 0.77 |
| sub | 41,521 | 0.74 |
| lbu | 38,754 | 0.69 |
| sb | 29,943 | 0.53 |

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

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_rvv | OK | 2,021,399 | 38,038 |
| clang | -O3 | karatsuba_rvv | OK | 2,021,399 | 38,038 |
| gcc | -O2 | karatsuba_rvv | OK | 1,985,286 | 26,440 |
| gcc | -O3 | karatsuba_rvv | OK | 1,985,286 | 26,440 |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_rvv | OK | 4,775,972 | 42,022 |
| clang | -O3 | karatsuba_rvv | OK | 4,775,972 | 42,022 |
| gcc | -O2 | karatsuba_rvv | OK | 4,674,391 | 30,776 |
| gcc | -O3 | karatsuba_rvv | OK | 4,674,391 | 30,776 |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_rvv | OK | 12,227,283 | 45,762 |
| clang | -O3 | karatsuba_rvv | OK | 12,227,283 | 45,762 |
| gcc | -O2 | karatsuba_rvv | OK | 9,509,416 | 35,472 |
| gcc | -O3 | karatsuba_rvv | OK | 9,509,416 | 35,472 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

