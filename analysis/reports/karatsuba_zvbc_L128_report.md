# HQC-RISC-V Analysis: `karatsuba_zvbc`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-05-26 17:54:53 |
| Commit | `c56c136` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 72,644 | 10,368 |
| 192 | vect_mul | 219,338 | 11,830 |
| 256 | vect_mul | 326,260 | 13,724 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 105,279 | 45,570 |
| 128 | encaps | 217,479 | 45,570 |
| 128 | decaps | 394,225 | 45,570 |
| 128 | **total** | 716,983 | 45,570 |
| 192 | keypair | 258,251 | 49,782 |
| 192 | encaps | 534,634 | 49,782 |
| 192 | decaps | 902,007 | 49,782 |
| 192 | **total** | 1,694,892 | 49,782 |
| 256 | keypair | 383,592 | 56,916 |
| 256 | encaps | 816,033 | 56,916 |
| 256 | decaps | 1,411,713 | 56,916 |
| 256 | **total** | 2,611,338 | 56,916 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1313 | 105 | 1208 | 142 | 12,958,047 | 7,168,903 | 1,923,570 | 1,193,813 |
| 192 | 2976 | 255 | 2721 | 684 | 30,163,428 | 19,772,475 | 4,271,952 | 2,925,103 |
| 256 | 6701 | 928 | 5773 | 3036 | 59,146,639 | 42,075,906 | 8,224,780 | 5,994,861 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1229 | 1459 | 23,306 | 18,867 | 365 | 295 |
| 192 | 1504 | 1773 | 32,268 | 24,519 | 505 | 384 |
| 256 | 1485 | 1803 | 28,130 | 22,461 | 440 | 351 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| vclmul_leaf | 250,554 | 34.49 |
| KeccakF1600_StatePermute | 177,860 | 24.48 |
| gf_reduce | 44,726 | 6.16 |
| rvv_xor3_u64 | 31,464 | 4.33 |
| rvv_copy_u64 | 26,418 | 3.64 |
| rvv_xor_u64 | 22,320 | 3.07 |
| rol1_zbb | 17,040 | 2.35 |
| kmul_17 | 15,762 | 2.17 |
| kmul_18 | 9636 | 1.33 |
| kmul_35 | 9108 | 1.25 |
| gf_mul | 8952 | 1.23 |
| support_contains_value | 8877 | 1.22 |
| reed_solomon_encode | 8058 | 1.11 |
| memset | 6790 | 0.93 |
| vect_generate_random_support2 | 6156 | 0.85 |
| vect_write_support_to_vector | 5847 | 0.80 |
| expand_and_sum | 5060 | 0.70 |
| >>>> | 4287 | 0.59 |
| keccak_extract_bytes | 3909 | 0.54 |
| reduce | 3594 | 0.49 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:0 | 24,010 | 3.30 |
| gf_reduce at gf.c:82 | 21,186 | 2.92 |
| vclmul_leaf at gf2x_vclmul.c:0 | 19,176 | 2.64 |
| rol1_zbb at fips202_zvbb_x2.c:111 | 17,040 | 2.35 |
| gf_reduce at gf.c:80 | 16,478 | 2.27 |
| vclmul_leaf at gf2x_vclmul.c:185 | 10,776 | 1.48 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:154 | 10,224 | 1.41 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:207 | 10,224 | 1.41 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:190 | 10,224 | 1.41 |
| vclmul_leaf at gf2x_vclmul.c:186 | 8082 | 1.11 |
| vclmul_leaf at gf2x_vclmul.c:187 | 8082 | 1.11 |
| vclmul_leaf at gf2x_vclmul.c:188 | 8082 | 1.11 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:155 | 6816 | 0.94 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:156 | 6816 | 0.94 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:157 | 6816 | 0.94 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:179 | 6816 | 0.94 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:181 | 6816 | 0.94 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:147 | 6816 | 0.94 |
| gf_mul at gf.c:147 | 5644 | 0.78 |
| vclmul_leaf at gf2x_vclmul.c:189 | 5388 | 0.74 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 406,297 | 27.36 |
| bnez | 391,185 | 26.34 |
| addi | 65,829 | 4.43 |
| sh3add | 56,689 | 3.82 |
| vxor.vv | 54,326 | 3.66 |
| vle64.v | 46,131 | 3.11 |
| xor | 38,139 | 2.57 |
| vse64.v | 36,085 | 2.43 |
| vslidedown.vi | 27,334 | 1.84 |
| add | 22,428 | 1.51 |
| slli | 22,207 | 1.50 |
| mv | 21,000 | 1.41 |
| vsetvli | 18,299 | 1.23 |
| vmv.v.x | 17,384 | 1.17 |
| sub | 17,294 | 1.16 |
| vmv.x.s | 17,226 | 1.16 |
| rori | 17,040 | 1.15 |
| vl8r.v | 17,040 | 1.15 |
| li | 15,186 | 1.02 |
| vrgather.vv | 13,632 | 0.92 |

## 4. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 716,983 |
| total | 512 | 690,678 |
| total | 1024 | 692,500 |
| total | 2048 | 716,821 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 1,694,892 |
| total | 512 | 1,601,345 |
| total | 1024 | 1,588,990 |
| total | 2048 | 1,608,961 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 2,611,338 |
| total | 512 | 2,469,285 |
| total | 1024 | 2,450,118 |
| total | 2048 | 2,466,433 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zvbc | OK | 716,983 | 45,570 |
| clang | -O3 | karatsuba_zvbc | OK | 716,983 | 45,570 |
| clang | -Os | karatsuba_zvbc | OK | 716,983 | 45,570 |
| gcc | -O2 | karatsuba_zvbc | SKIP |  |  |
| gcc | -O3 | karatsuba_zvbc | SKIP |  |  |
| gcc | -Os | karatsuba_zvbc | SKIP |  |  |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zvbc | OK | 1,694,892 | 49,782 |
| clang | -O3 | karatsuba_zvbc | OK | 1,694,892 | 49,782 |
| clang | -Os | karatsuba_zvbc | OK | 1,694,892 | 49,782 |
| gcc | -O2 | karatsuba_zvbc | SKIP |  |  |
| gcc | -O3 | karatsuba_zvbc | SKIP |  |  |
| gcc | -Os | karatsuba_zvbc | SKIP |  |  |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_zvbc | OK | 2,611,338 | 56,916 |
| clang | -O3 | karatsuba_zvbc | OK | 2,611,338 | 56,916 |
| clang | -Os | karatsuba_zvbc | OK | 2,611,338 | 56,916 |
| gcc | -O2 | karatsuba_zvbc | SKIP |  |  |
| gcc | -O3 | karatsuba_zvbc | SKIP |  |  |
| gcc | -Os | karatsuba_zvbc | SKIP |  |  |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `analysis/reports/speedups.md`
- Compiler showdown: `analysis/reports/compiler_analysis.md`

