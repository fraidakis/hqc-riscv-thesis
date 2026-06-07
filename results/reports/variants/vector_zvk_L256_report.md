# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-07 15:56:41 |
| Commit | `20def7e` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 62,530 | 5854 |
| 192 | vect_mul | 185,120 | 8058 |
| 256 | vect_mul | 271,006 | 7298 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 107,408 | 45,504 |
| 128 | encaps | 221,511 | 45,504 |
| 128 | decaps | 400,478 | 45,504 |
| 128 | **total** | 729,397 | 45,504 |
| 192 | keypair | 262,108 | 49,716 |
| 192 | encaps | 542,410 | 49,716 |
| 192 | decaps | 914,520 | 49,716 |
| 192 | **total** | 1,719,038 | 49,716 |
| 256 | keypair | 389,753 | 56,848 |
| 256 | encaps | 827,937 | 56,848 |
| 256 | decaps | 1,430,020 | 56,848 |
| 256 | **total** | 2,647,710 | 56,848 |

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
| 128 | 1244 | 1462 | 23,306 | 18,891 | 365 | 296 |
| 192 | 1519 | 1777 | 32,268 | 24,547 | 505 | 384 |
| 256 | 1502 | 1805 | 28,154 | 22,477 | 440 | 352 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| vclmul_leaf | 1,183,398 | 44.37 |
| KeccakF1600_StatePermute | 567,734 | 21.29 |
| gf_reduce | 140,866 | 5.28 |
| rvv_xor3_u64 | 126,234 | 4.73 |
| rvv_copy_u64 | 108,066 | 4.05 |
| rvv_xor_u64 | 78,480 | 2.94 |
| rol1_zbb | 50,520 | 1.89 |
| kmul_29 | 37,488 | 1.41 |
| reed_solomon_encode | 35,698 | 1.34 |
| kmul_28 | 34,938 | 1.31 |
| gf_mul | 31,724 | 1.19 |
| support_contains_value | 24,082 | 0.90 |
| kmul_57 | 19,008 | 0.71 |
| expand_and_sum | 17,640 | 0.66 |
| ?? ??:0 | 13,089 | 0.49 |
| vect_write_support_to_vector | 11,598 | 0.43 |
| kmul_56 | 11,544 | 0.43 |
| vect_generate_random_support2 | 11,100 | 0.42 |
| memset | 11,073 | 0.42 |
| _start | 10,994 | 0.41 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| vclmul_leaf at gf2x_vclmul.c:? | 89,592 | 3.36 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:? | 71,161 | 2.67 |
| gf_reduce at gf.c:82 | 66,726 | 2.50 |
| vclmul_leaf at gf2x_vclmul.c:185 | 52,488 | 1.97 |
| gf_reduce at gf.c:80 | 51,898 | 1.95 |
| rol1_zbb at fips202_zvbb_x2.c:111 | 50,520 | 1.89 |
| vclmul_leaf at gf2x_vclmul.c:186 | 39,366 | 1.48 |
| vclmul_leaf at gf2x_vclmul.c:187 | 39,366 | 1.48 |
| vclmul_leaf at gf2x_vclmul.c:188 | 39,366 | 1.48 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:155 | 30,312 | 1.14 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:208 | 30,312 | 1.14 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:191 | 30,312 | 1.14 |
| vclmul_leaf at gf2x_vclmul.c:189 | 26,244 | 0.98 |
| vclmul_leaf at gf2x_vclmul.c:193 | 26,244 | 0.98 |
| vclmul_leaf at gf2x_vclmul.c:195 | 26,244 | 0.98 |
| vclmul_leaf at gf2x_vclmul.c:219 | 26,244 | 0.98 |
| vclmul_leaf at gf2x_vclmul.c:222 | 26,244 | 0.98 |
| vclmul_leaf at gf2x_vclmul.c:227 | 26,244 | 0.98 |
| vclmul_leaf at gf2x_vclmul.c:230 | 26,244 | 0.98 |
| vclmul_leaf at gf2x_vclmul.c:235 | 26,244 | 0.98 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 493,756 | 14.31 |
| bnez | 436,744 | 12.66 |
| sh3add | 248,638 | 7.21 |
| addi | 237,715 | 6.89 |
| vxor.vv | 216,696 | 6.28 |
| vle64.v | 208,775 | 6.05 |
| vse64.v | 162,700 | 4.72 |
| xor | 114,635 | 3.32 |
| add | 92,092 | 2.67 |
| vslidedown.vi | 80,837 | 2.34 |
| vmv.v.x | 74,900 | 2.17 |
| vsetivli | 71,638 | 2.08 |
| vsetvli | 71,376 | 2.07 |
| slli | 70,189 | 2.03 |
| sub | 66,654 | 1.93 |
| mv | 63,881 | 1.85 |
| vclmul.vv | 63,438 | 1.84 |
| vclmulh.vv | 62,292 | 1.81 |
| vmv.x.s | 50,888 | 1.47 |
| vl8r.v | 50,520 | 1.46 |

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

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

