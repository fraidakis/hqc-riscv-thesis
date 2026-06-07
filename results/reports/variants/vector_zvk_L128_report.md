# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
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
| vclmul_leaf | 250,554 | 33.90 |
| KeccakF1600_StatePermute | 191,492 | 25.91 |
| gf_reduce | 44,726 | 6.05 |
| rvv_xor3_u64 | 31,464 | 4.26 |
| rvv_copy_u64 | 26,418 | 3.57 |
| rvv_xor_u64 | 22,320 | 3.02 |
| rol1_zbb | 17,040 | 2.31 |
| kmul_17 | 15,762 | 2.13 |
| kmul_18 | 9636 | 1.30 |
| kmul_35 | 9108 | 1.23 |
| gf_mul | 8952 | 1.21 |
| support_contains_value | 8877 | 1.20 |
| reed_solomon_encode | 8058 | 1.09 |
| memset | 6790 | 0.92 |
| vect_generate_random_support2 | 6156 | 0.83 |
| vect_write_support_to_vector | 5847 | 0.79 |
| expand_and_sum | 5060 | 0.68 |
| ?? ??:0 | 4292 | 0.58 |
| keccak_extract_bytes | 3909 | 0.53 |
| reduce | 3594 | 0.49 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:? | 24,010 | 3.25 |
| gf_reduce at gf.c:82 | 21,186 | 2.87 |
| vclmul_leaf at gf2x_vclmul.c:? | 19,176 | 2.59 |
| rol1_zbb at fips202_zvbb_x2.c:111 | 17,040 | 2.31 |
| gf_reduce at gf.c:80 | 16,478 | 2.23 |
| vclmul_leaf at gf2x_vclmul.c:185 | 10,776 | 1.46 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:155 | 10,224 | 1.38 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:208 | 10,224 | 1.38 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:191 | 10,224 | 1.38 |
| vclmul_leaf at gf2x_vclmul.c:186 | 8082 | 1.09 |
| vclmul_leaf at gf2x_vclmul.c:187 | 8082 | 1.09 |
| vclmul_leaf at gf2x_vclmul.c:188 | 8082 | 1.09 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:156 | 6816 | 0.92 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:157 | 6816 | 0.92 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:158 | 6816 | 0.92 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:164 | 6816 | 0.92 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:166 | 6816 | 0.92 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:168 | 6816 | 0.92 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:176 | 6816 | 0.92 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:180 | 6816 | 0.92 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 407,542 | 27.17 |
| bnez | 392,369 | 26.16 |
| addi | 65,904 | 4.39 |
| sh3add | 56,689 | 3.78 |
| vxor.vv | 54,236 | 3.62 |
| vle64.v | 46,131 | 3.08 |
| xor | 38,079 | 2.54 |
| vse64.v | 36,085 | 2.41 |
| vslidedown.vi | 27,334 | 1.82 |
| vsetivli | 24,551 | 1.64 |
| add | 22,308 | 1.49 |
| slli | 22,147 | 1.48 |
| mv | 20,894 | 1.39 |
| vsetvli | 18,257 | 1.22 |
| vmv.v.x | 17,294 | 1.15 |
| sub | 17,234 | 1.15 |
| vmv.x.s | 17,166 | 1.14 |
| rori | 17,040 | 1.14 |
| vl8r.v | 17,040 | 1.14 |
| li | 15,186 | 1.01 |

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

