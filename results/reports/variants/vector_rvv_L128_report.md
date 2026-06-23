# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-23 13:26:11 |
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
| KeccakF1600_StatePermute | 578,792 | 28.47 |
| clmul_leaf | 490,692 | 24.14 |
| clmul64 | 254,400 | 12.51 |
| keccak_inc_absorb | 141,116 | 6.94 |
| keccak_inc_squeeze | 96,430 | 4.74 |
| gf_reduce | 80,541 | 3.96 |
| clmul_leaf_n8 | 75,036 | 3.69 |
| memset | 32,746 | 1.61 |
| kmul_17 | 32,634 | 1.61 |
| kmul_18 | 31,680 | 1.56 |
| vect_compare | 31,055 | 1.53 |
| kmul_35 | 26,796 | 1.32 |
| gf_mul | 16,952 | 0.83 |
| compute_elp | 10,112 | 0.50 |
| vect_generate_random_support2 | 9516 | 0.47 |
| support_contains_value | 8877 | 0.44 |
| kmul_69 | 8640 | 0.43 |
| fft_rec | 8480 | 0.42 |
| compute_error_values | 8110 | 0.40 |
| reed_solomon_encode | 8058 | 0.40 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:45 | 114,324 | 5.62 |
| clmul64 at gf2x_clmul.c:46 | 114,324 | 5.62 |
| KeccakF1600_StatePermute at fips202.c:? | 98,548 | 4.85 |
| clmul_leaf at gf2x_clmul.c:491 | 88,992 | 4.38 |
| keccak_inc_absorb at fips202.c:438 | 85,750 | 4.22 |
| clmul_leaf at gf2x_clmul.c:492 | 79,104 | 3.89 |
| clmul_leaf at gf2x_clmul.c:493 | 79,104 | 3.89 |
| keccak_inc_squeeze at fips202.c:505 | 51,696 | 2.54 |
| clmul_leaf at gf2x_clmul.c:490 | 49,440 | 2.43 |
| keccak_inc_absorb at fips202.c:435 | 43,067 | 2.12 |
| clmul_leaf at gf2x_clmul.c:494 | 39,552 | 1.95 |
| gf_reduce at gf.c:82 | 38,151 | 1.88 |
| keccak_inc_squeeze at fips202.c:504 | 34,608 | 1.70 |
| gf_reduce at gf.c:80 | 29,673 | 1.46 |
| clmul_leaf at gf2x_clmul.c:518 | 29,664 | 1.46 |
| clmul64 at gf2x_clmul.c:? | 25,752 | 1.27 |
| clmul_leaf at gf2x_clmul.c:479 | 22,248 | 1.09 |
| memset at mem.c:84 | 19,792 | 0.97 |
| clmul_leaf at gf2x_clmul.c:517 | 19,776 | 0.97 |
| clmul_leaf at gf2x_clmul.c:521 | 18,540 | 0.91 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 670,748 | 23.83 |
| xor | 513,859 | 18.25 |
| bnez | 399,901 | 14.21 |
| sd | 246,273 | 8.75 |
| clmul | 118,563 | 4.21 |
| clmulh | 114,324 | 4.06 |
| addi | 105,310 | 3.74 |
| rori | 98,832 | 3.51 |
| andn | 85,200 | 3.03 |
| add | 43,970 | 1.56 |
| slli | 42,050 | 1.49 |
| bne | 39,556 | 1.41 |
| mv | 36,234 | 1.29 |
| andi | 27,956 | 0.99 |
| sh3add | 25,513 | 0.91 |
| vle64.v | 21,204 | 0.75 |
| sb | 20,515 | 0.73 |
| lbu | 20,039 | 0.71 |
| sub | 17,659 | 0.63 |
| zext.b | 14,991 | 0.53 |

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

