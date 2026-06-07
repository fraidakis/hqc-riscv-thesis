# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
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
| vclmul_leaf | 751,938 | 43.08 |
| KeccakF1600_StatePermute | 373,542 | 21.40 |
| rvv_xor3_u64 | 97,650 | 5.59 |
| rvv_copy_u64 | 84,168 | 4.82 |
| rvv_xor_u64 | 69,120 | 3.96 |
| gf_reduce | 60,192 | 3.45 |
| kmul_18 | 39,810 | 2.28 |
| rol1_zbb | 33,240 | 1.90 |
| kmul_35 | 24,570 | 1.41 |
| kmul_17 | 18,330 | 1.05 |
| support_contains_value | 16,295 | 0.93 |
| reed_solomon_encode | 12,844 | 0.74 |
| gf_mul | 12,782 | 0.73 |
| expand_and_sum | 10,976 | 0.63 |
| vect_write_support_to_vector | 8871 | 0.51 |
| ?? ??:0 | 8469 | 0.49 |
| vect_generate_random_support2 | 8442 | 0.48 |
| _start | 6908 | 0.40 |
| keccak_extract_bytes | 6835 | 0.39 |
| memcpy | 6710 | 0.38 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| vclmul_leaf at gf2x_vclmul.c:? | 57,252 | 3.28 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:? | 46,825 | 2.68 |
| vclmul_leaf at gf2x_vclmul.c:185 | 33,432 | 1.92 |
| rol1_zbb at fips202_zvbb_x2.c:111 | 33,240 | 1.90 |
| gf_reduce at gf.c:82 | 28,512 | 1.63 |
| vclmul_leaf at gf2x_vclmul.c:186 | 25,074 | 1.44 |
| vclmul_leaf at gf2x_vclmul.c:187 | 25,074 | 1.44 |
| vclmul_leaf at gf2x_vclmul.c:188 | 25,074 | 1.44 |
| gf_reduce at gf.c:80 | 22,176 | 1.27 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:155 | 19,944 | 1.14 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:208 | 19,944 | 1.14 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:191 | 19,944 | 1.14 |
| vclmul_leaf at gf2x_vclmul.c:189 | 16,716 | 0.96 |
| vclmul_leaf at gf2x_vclmul.c:193 | 16,716 | 0.96 |
| vclmul_leaf at gf2x_vclmul.c:195 | 16,716 | 0.96 |
| vclmul_leaf at gf2x_vclmul.c:219 | 16,716 | 0.96 |
| vclmul_leaf at gf2x_vclmul.c:222 | 16,716 | 0.96 |
| vclmul_leaf at gf2x_vclmul.c:227 | 16,716 | 0.96 |
| vclmul_leaf at gf2x_vclmul.c:230 | 16,716 | 0.96 |
| vclmul_leaf at gf2x_vclmul.c:235 | 16,716 | 0.96 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 445,486 | 17.82 |
| bnez | 412,596 | 16.50 |
| sh3add | 170,337 | 6.81 |
| addi | 154,019 | 6.16 |
| vxor.vv | 141,443 | 5.66 |
| vle64.v | 139,921 | 5.60 |
| vse64.v | 109,176 | 4.37 |
| xor | 61,438 | 2.46 |
| add | 58,451 | 2.34 |
| vslidedown.vi | 53,285 | 2.13 |
| mv | 52,927 | 2.12 |
| vsetvli | 51,373 | 2.05 |
| sub | 48,684 | 1.95 |
| vsetivli | 47,577 | 1.90 |
| vmv.v.x | 47,281 | 1.89 |
| li | 39,760 | 1.59 |
| vclmul.vv | 39,441 | 1.58 |
| vclmulh.vv | 38,976 | 1.56 |
| slli | 34,198 | 1.37 |
| vmv.x.s | 33,381 | 1.34 |

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

