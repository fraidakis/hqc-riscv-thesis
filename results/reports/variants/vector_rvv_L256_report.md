# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-23 12:43:29 |
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
| clmul_leaf | 6,735,858 | 54.97 |
| clmul64 | 1,833,774 | 14.96 |
| KeccakF1600_StatePermute | 1,715,996 | 14.00 |
| keccak_inc_absorb | 443,726 | 3.62 |
| keccak_inc_squeeze | 274,928 | 2.24 |
| gf_reduce | 245,670 | 2.00 |
| kmul_28 | 116,514 | 0.95 |
| vect_compare | 100,971 | 0.82 |
| kmul_29 | 89,232 | 0.73 |
| memset | 82,616 | 0.67 |
| reed_solomon_encode | 63,948 | 0.52 |
| kmul_57 | 61,776 | 0.50 |
| gf_mul | 56,375 | 0.46 |
| kmul_113 | 49,236 | 0.40 |
| kmul_56 | 44,400 | 0.36 |
| compute_elp | 43,491 | 0.35 |
| compute_error_values | 29,331 | 0.24 |
| support_contains_value | 24,082 | 0.20 |
| kmul_225 | 19,590 | 0.16 |
| vect_generate_random_support2 | 18,840 | 0.15 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:46 | 945,846 | 7.72 |
| clmul64 at gf2x_clmul.c:45 | 887,928 | 7.25 |
| clmul_leaf at gf2x_clmul.c:491 | 695,016 | 5.67 |
| clmul_leaf at gf2x_clmul.c:492 | 695,016 | 5.67 |
| clmul_leaf at gf2x_clmul.c:493 | 695,016 | 5.67 |
| clmul_leaf at gf2x_clmul.c:494 | 695,016 | 5.67 |
| clmul_leaf at gf2x_clmul.c:484 | 695,016 | 5.67 |
| clmul_leaf at gf2x_clmul.c:490 | 521,262 | 4.25 |
| clmul_leaf at gf2x_clmul.c:? | 501,660 | 4.09 |
| KeccakF1600_StatePermute at fips202.c:? | 292,174 | 2.38 |
| keccak_inc_absorb at fips202.c:438 | 287,230 | 2.34 |
| clmul_leaf at gf2x_clmul.c:512 | 215,670 | 1.76 |
| clmul_leaf at gf2x_clmul.c:479 | 186,876 | 1.52 |
| clmul_leaf at gf2x_clmul.c:485 | 173,754 | 1.42 |
| clmul_leaf at gf2x_clmul.c:486 | 173,754 | 1.42 |
| clmul_leaf at gf2x_clmul.c:487 | 173,754 | 1.42 |
| clmul_leaf at gf2x_clmul.c:488 | 173,754 | 1.42 |
| keccak_inc_squeeze at fips202.c:505 | 159,312 | 1.30 |
| keccak_inc_absorb at fips202.c:435 | 144,257 | 1.18 |
| clmul_leaf at gf2x_clmul.c:511 | 129,402 | 1.06 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 2,660,208 | 20.33 |
| xor | 2,549,552 | 19.48 |
| sd | 1,410,868 | 10.78 |
| addi | 1,261,614 | 9.64 |
| clmul | 900,858 | 6.88 |
| clmulh | 887,928 | 6.79 |
| sh3add | 468,276 | 3.58 |
| bnez | 452,664 | 3.46 |
| rori | 293,016 | 2.24 |
| andn | 252,600 | 1.93 |
| bltu | 236,370 | 1.81 |
| mv | 198,620 | 1.52 |
| add | 133,745 | 1.02 |
| bne | 129,098 | 0.99 |
| slli | 124,589 | 0.95 |
| bgeu | 122,023 | 0.93 |
| vle64.v | 95,216 | 0.73 |
| li | 85,502 | 0.65 |
| andi | 83,551 | 0.64 |
| vxor.vv | 69,022 | 0.53 |

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

