# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-18 19:14:28 |
| Commit | `c5f821b` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 60,380 | 5830 |
| 192 | vect_mul | 178,624 | 8034 |
| 256 | vect_mul | 260,624 | 7274 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 103,129 | 46,000 |
| 128 | encaps | 213,179 | 46,000 |
| 128 | decaps | 386,447 | 46,000 |
| 128 | **total** | 702,755 | 46,000 |
| 192 | keypair | 251,755 | 50,214 |
| 192 | encaps | 521,642 | 50,214 |
| 192 | decaps | 880,407 | 50,214 |
| 192 | **total** | 1,653,804 | 50,214 |
| 256 | keypair | 373,210 | 57,346 |
| 256 | encaps | 795,269 | 57,346 |
| 256 | decaps | 1,375,654 | 57,346 |
| 256 | **total** | 2,544,133 | 57,346 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1313 | 105 | 1208 | 142 | 12,941,247 | 7,168,903 | 1,921,470 | 1,193,813 |
| 192 | 3067 | 267 | 2800 | 773 | 30,195,236 | 19,772,475 | 4,275,928 | 2,925,103 |
| 256 | 6746 | 952 | 5794 | 3103 | 59,124,975 | 42,075,906 | 8,222,072 | 5,994,861 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1227 | 1459 | 23,282 | 18,867 | 364 | 295 |
| 192 | 1502 | 1774 | 32,244 | 24,523 | 504 | 384 |
| 256 | 1485 | 1803 | 28,130 | 22,461 | 440 | 351 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| vclmul_leaf | 1,121,106 | 43.73 |
| KeccakF1600_StatePermute | 527,318 | 20.57 |
| gf_reduce | 140,866 | 5.49 |
| rvv_xor3_u64 | 126,234 | 4.92 |
| rvv_copy_u64 | 108,066 | 4.21 |
| rvv_xor_u64 | 78,480 | 3.06 |
| rol1_zbb | 50,520 | 1.97 |
| kmul_29 | 37,488 | 1.46 |
| reed_solomon_encode | 35,698 | 1.39 |
| kmul_28 | 34,938 | 1.36 |
| gf_mul | 31,724 | 1.24 |
| support_contains_value | 24,082 | 0.94 |
| kmul_57 | 19,008 | 0.74 |
| expand_and_sum | 17,640 | 0.69 |
| ?? ??:0 | 13,089 | 0.51 |
| vect_write_support_to_vector | 11,598 | 0.45 |
| kmul_56 | 11,544 | 0.45 |
| vect_generate_random_support2 | 11,100 | 0.43 |
| memset | 11,073 | 0.43 |
| _start | 10,994 | 0.43 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| vclmul_leaf at gf2x_vclmul.c:? | 89,592 | 3.49 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:? | 71,161 | 2.78 |
| gf_reduce at gf.c:82 | 66,726 | 2.60 |
| vclmul_leaf at gf2x_vclmul.c:185 | 52,488 | 2.05 |
| gf_reduce at gf.c:80 | 51,898 | 2.02 |
| rol1_zbb at fips202_zvbb_x2.c:113 | 50,520 | 1.97 |
| vclmul_leaf at gf2x_vclmul.c:186 | 39,366 | 1.54 |
| vclmul_leaf at gf2x_vclmul.c:187 | 39,366 | 1.54 |
| vclmul_leaf at gf2x_vclmul.c:188 | 39,366 | 1.54 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:156 | 30,312 | 1.18 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:209 | 30,312 | 1.18 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:192 | 30,312 | 1.18 |
| vclmul_leaf at gf2x_vclmul.c:189 | 26,244 | 1.02 |
| vclmul_leaf at gf2x_vclmul.c:193 | 26,244 | 1.02 |
| vclmul_leaf at gf2x_vclmul.c:195 | 26,244 | 1.02 |
| vclmul_leaf at gf2x_vclmul.c:215 | 26,244 | 1.02 |
| vclmul_leaf at gf2x_vclmul.c:218 | 26,244 | 1.02 |
| vclmul_leaf at gf2x_vclmul.c:223 | 26,244 | 1.02 |
| vclmul_leaf at gf2x_vclmul.c:226 | 26,244 | 1.02 |
| vclmul_leaf at gf2x_vclmul.c:231 | 26,244 | 1.02 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 492,899 | 14.74 |
| bnez | 435,888 | 13.03 |
| sh3add | 248,638 | 7.43 |
| addi | 237,716 | 7.11 |
| vxor.vv | 216,696 | 6.48 |
| vle64.v | 208,775 | 6.24 |
| vse64.v | 162,700 | 4.86 |
| xor | 114,635 | 3.43 |
| add | 92,092 | 2.75 |
| vslidedown.vi | 80,837 | 2.42 |
| vsetvli | 71,358 | 2.13 |
| slli | 70,189 | 2.10 |
| sub | 66,654 | 1.99 |
| mv | 63,882 | 1.91 |
| vclmul.vx | 63,238 | 1.89 |
| vclmulh.vx | 62,292 | 1.86 |
| vmv.x.s | 50,888 | 1.52 |
| vl8r.v | 50,520 | 1.51 |
| rori | 50,520 | 1.51 |
| bltu | 49,583 | 1.48 |

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

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zvbc | OK | 702,755 | 46,000 |
| clang | -O3 | karatsuba_zvbc | OK | 702,755 | 46,000 |
| clang | -Os | karatsuba_zvbc | OK | 702,755 | 46,000 |
| gcc | -O2 | karatsuba_zvbc | SKIP |  |  |
| gcc | -O3 | karatsuba_zvbc | SKIP |  |  |
| gcc | -Os | karatsuba_zvbc | SKIP |  |  |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zvbc | OK | 1,653,804 | 50,214 |
| clang | -O3 | karatsuba_zvbc | OK | 1,653,804 | 50,214 |
| clang | -Os | karatsuba_zvbc | OK | 1,653,804 | 50,214 |
| gcc | -O2 | karatsuba_zvbc | SKIP |  |  |
| gcc | -O3 | karatsuba_zvbc | SKIP |  |  |
| gcc | -Os | karatsuba_zvbc | SKIP |  |  |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zvbc | OK | 2,544,133 | 57,346 |
| clang | -O3 | karatsuba_zvbc | OK | 2,544,133 | 57,346 |
| clang | -Os | karatsuba_zvbc | OK | 2,544,133 | 57,346 |
| gcc | -O2 | karatsuba_zvbc | SKIP |  |  |
| gcc | -O3 | karatsuba_zvbc | SKIP |  |  |
| gcc | -Os | karatsuba_zvbc | SKIP |  |  |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

