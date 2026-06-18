# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
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
| vclmul_leaf | 712,962 | 42.45 |
| KeccakF1600_StatePermute | 346,950 | 20.66 |
| rvv_xor3_u64 | 97,650 | 5.81 |
| rvv_copy_u64 | 84,168 | 5.01 |
| rvv_xor_u64 | 69,120 | 4.12 |
| gf_reduce | 60,192 | 3.58 |
| kmul_18 | 39,810 | 2.37 |
| rol1_zbb | 33,240 | 1.98 |
| kmul_35 | 24,570 | 1.46 |
| kmul_17 | 18,330 | 1.09 |
| support_contains_value | 16,295 | 0.97 |
| reed_solomon_encode | 12,844 | 0.76 |
| gf_mul | 12,782 | 0.76 |
| expand_and_sum | 10,976 | 0.65 |
| vect_write_support_to_vector | 8871 | 0.53 |
| ?? ??:0 | 8469 | 0.50 |
| vect_generate_random_support2 | 8442 | 0.50 |
| _start | 6908 | 0.41 |
| keccak_extract_bytes | 6835 | 0.41 |
| memcpy | 6710 | 0.40 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| vclmul_leaf at gf2x_vclmul.c:? | 57,252 | 3.41 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:? | 46,825 | 2.79 |
| vclmul_leaf at gf2x_vclmul.c:185 | 33,432 | 1.99 |
| rol1_zbb at fips202_zvbb_x2.c:113 | 33,240 | 1.98 |
| gf_reduce at gf.c:82 | 28,512 | 1.70 |
| vclmul_leaf at gf2x_vclmul.c:186 | 25,074 | 1.49 |
| vclmul_leaf at gf2x_vclmul.c:187 | 25,074 | 1.49 |
| vclmul_leaf at gf2x_vclmul.c:188 | 25,074 | 1.49 |
| gf_reduce at gf.c:80 | 22,176 | 1.32 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:156 | 19,944 | 1.19 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:209 | 19,944 | 1.19 |
| KeccakF1600_StatePermute at fips202_zvbb_x2.c:192 | 19,944 | 1.19 |
| vclmul_leaf at gf2x_vclmul.c:189 | 16,716 | 1.00 |
| vclmul_leaf at gf2x_vclmul.c:193 | 16,716 | 1.00 |
| vclmul_leaf at gf2x_vclmul.c:195 | 16,716 | 1.00 |
| vclmul_leaf at gf2x_vclmul.c:215 | 16,716 | 1.00 |
| vclmul_leaf at gf2x_vclmul.c:218 | 16,716 | 1.00 |
| vclmul_leaf at gf2x_vclmul.c:223 | 16,716 | 1.00 |
| vclmul_leaf at gf2x_vclmul.c:226 | 16,716 | 1.00 |
| vclmul_leaf at gf2x_vclmul.c:231 | 16,716 | 1.00 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 445,890 | 18.31 |
| bnez | 413,001 | 16.96 |
| sh3add | 170,337 | 7.00 |
| addi | 154,019 | 6.33 |
| vxor.vv | 141,443 | 5.81 |
| vle64.v | 139,921 | 5.75 |
| vse64.v | 109,176 | 4.48 |
| xor | 61,438 | 2.52 |
| add | 58,451 | 2.40 |
| vslidedown.vi | 53,285 | 2.19 |
| mv | 52,929 | 2.17 |
| vsetvli | 51,355 | 2.11 |
| sub | 48,684 | 2.00 |
| li | 39,760 | 1.63 |
| vclmul.vx | 39,356 | 1.62 |
| vclmulh.vx | 38,976 | 1.60 |
| slli | 34,198 | 1.40 |
| vmv.x.s | 33,381 | 1.37 |
| vl8r.v | 33,240 | 1.37 |
| rori | 33,240 | 1.37 |

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

