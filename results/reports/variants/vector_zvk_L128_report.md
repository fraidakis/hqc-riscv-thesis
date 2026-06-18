# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
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
| vclmul_leaf | 237,654 | 33.36 |
| KeccakF1600_StatePermute | 177,860 | 24.97 |
| gf_reduce | 44,726 | 6.28 |
| rvv_xor3_u64 | 31,464 | 4.42 |
| rvv_copy_u64 | 26,418 | 3.71 |
| rvv_xor_u64 | 22,320 | 3.13 |
| rol1_zbb | 17,040 | 2.39 |
| kmul_17 | 15,762 | 2.21 |
| kmul_18 | 9636 | 1.35 |
| kmul_35 | 9108 | 1.28 |
| gf_mul | 8952 | 1.26 |
| support_contains_value | 8877 | 1.25 |
| reed_solomon_encode | 8058 | 1.13 |
| memset | 6790 | 0.95 |
| vect_generate_random_support2 | 6156 | 0.86 |
| vect_write_support_to_vector | 5847 | 0.82 |
| expand_and_sum | 5060 | 0.71 |
| ?? ??:0 | 4292 | 0.60 |
| keccak_extract_bytes | 3909 | 0.55 |
| reduce | 3594 | 0.50 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:? | 24,010 | 3.37 |
| gf_reduce at gf.c:82 | 21,186 | 2.97 |
| vclmul_leaf at gf2x_vclmul.c:? | 19,176 | 2.69 |
| rol1_zbb at fips202_zvbb_x2.c:113 | 17,040 | 2.39 |
| gf_reduce at gf.c:80 | 16,478 | 2.31 |
| vclmul_leaf at gf2x_vclmul.c:185 | 10,776 | 1.51 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:156 | 10,224 | 1.44 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:209 | 10,224 | 1.44 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:192 | 10,224 | 1.44 |
| vclmul_leaf at gf2x_vclmul.c:186 | 8082 | 1.13 |
| vclmul_leaf at gf2x_vclmul.c:187 | 8082 | 1.13 |
| vclmul_leaf at gf2x_vclmul.c:188 | 8082 | 1.13 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:157 | 6816 | 0.96 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:158 | 6816 | 0.96 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:159 | 6816 | 0.96 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:181 | 6816 | 0.96 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:183 | 6816 | 0.96 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:149 | 6816 | 0.96 |
| gf_mul at gf.c:147 | 5644 | 0.79 |
| vclmul_leaf at gf2x_vclmul.c:189 | 5388 | 0.76 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 405,910 | 27.61 |
| bnez | 390,738 | 26.58 |
| addi | 65,905 | 4.48 |
| sh3add | 56,689 | 3.86 |
| vxor.vv | 54,236 | 3.69 |
| vle64.v | 46,131 | 3.14 |
| xor | 38,079 | 2.59 |
| vse64.v | 36,085 | 2.45 |
| vslidedown.vi | 27,334 | 1.86 |
| add | 22,308 | 1.52 |
| slli | 22,147 | 1.51 |
| mv | 20,896 | 1.42 |
| vsetvli | 18,239 | 1.24 |
| sub | 17,234 | 1.17 |
| vmv.x.s | 17,166 | 1.17 |
| vl8r.v | 17,040 | 1.16 |
| rori | 17,040 | 1.16 |
| li | 15,186 | 1.03 |
| vslide1up.vx | 13,632 | 0.93 |
| vrgather.vv | 13,632 | 0.93 |

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

