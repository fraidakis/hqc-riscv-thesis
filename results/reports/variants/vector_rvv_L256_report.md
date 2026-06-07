# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-07 15:55:19 |
| Commit | `20def7e` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 157,473 | 7992 |
| 192 | vect_mul | 473,083 | 10,560 |
| 256 | vect_mul | 1,502,171 | 7648 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 286,255 | 40,890 |
| 128 | encaps | 586,400 | 40,890 |
| 128 | decaps | 1,084,132 | 40,890 |
| 128 | **total** | 1,956,787 | 40,890 |
| 192 | keypair | 704,882 | 44,602 |
| 192 | encaps | 1,455,147 | 44,602 |
| 192 | decaps | 2,515,258 | 44,602 |
| 192 | **total** | 4,675,287 | 44,602 |
| 256 | keypair | 1,869,816 | 48,600 |
| 256 | encaps | 3,809,926 | 48,600 |
| 256 | decaps | 6,341,725 | 48,600 |
| 256 | **total** | 12,021,467 | 48,600 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 898 | 69 | 829 | 52 | 10,890,615 | 5,428,135 | 1,665,141 | 953,369 |
| 192 | 2010 | 210 | 1800 | 407 | 23,841,570 | 14,148,273 | 3,481,725 | 2,175,043 |
| 256 | 3658 | 555 | 3103 | 1045 | 54,337,365 | 29,274,936 | 7,623,626 | 4,319,481 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 2058 | 2140 | 31,096 | 23,204 | 486 | 363 |
| 192 | 2472 | 2330 | 43,628 | 28,894 | 682 | 452 |
| 256 | 2283 | 2207 | 37,304 | 26,045 | 583 | 407 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul_leaf | 6,735,858 | 55.97 |
| clmul64 | 1,833,774 | 15.24 |
| KeccakF1600_StatePermute | 1,715,996 | 14.26 |
| keccak_inc_absorb | 443,726 | 3.69 |
| keccak_inc_squeeze | 274,928 | 2.28 |
| gf_reduce | 245,670 | 2.04 |
| kmul_28 | 116,514 | 0.97 |
| kmul_29 | 89,232 | 0.74 |
| kmul_57 | 61,776 | 0.51 |
| gf_mul | 56,375 | 0.47 |
| kmul_113 | 49,236 | 0.41 |
| kmul_56 | 44,400 | 0.37 |
| reed_solomon_encode | 35,698 | 0.30 |
| compute_elp | 30,637 | 0.25 |
| compute_error_values | 29,331 | 0.24 |
| support_contains_value | 24,082 | 0.20 |
| kmul_225 | 19,590 | 0.16 |
| ?? ??:0 | 18,345 | 0.15 |
| expand_and_sum | 17,640 | 0.15 |
| kmul_226 | 14,880 | 0.12 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:46 | 945,846 | 7.86 |
| clmul64 at gf2x_clmul.c:45 | 887,928 | 7.38 |
| clmul_leaf at gf2x_clmul.c:491 | 695,016 | 5.77 |
| clmul_leaf at gf2x_clmul.c:492 | 695,016 | 5.77 |
| clmul_leaf at gf2x_clmul.c:493 | 695,016 | 5.77 |
| clmul_leaf at gf2x_clmul.c:494 | 695,016 | 5.77 |
| clmul_leaf at gf2x_clmul.c:484 | 695,016 | 5.77 |
| clmul_leaf at gf2x_clmul.c:490 | 521,262 | 4.33 |
| clmul_leaf at gf2x_clmul.c:? | 501,660 | 4.17 |
| KeccakF1600_StatePermute at fips202.c:? | 292,174 | 2.43 |
| keccak_inc_absorb at fips202.c:438 | 287,230 | 2.39 |
| clmul_leaf at gf2x_clmul.c:512 | 215,670 | 1.79 |
| clmul_leaf at gf2x_clmul.c:479 | 186,876 | 1.55 |
| clmul_leaf at gf2x_clmul.c:485 | 173,754 | 1.44 |
| clmul_leaf at gf2x_clmul.c:486 | 173,754 | 1.44 |
| clmul_leaf at gf2x_clmul.c:487 | 173,754 | 1.44 |
| clmul_leaf at gf2x_clmul.c:488 | 173,754 | 1.44 |
| keccak_inc_squeeze at fips202.c:505 | 159,312 | 1.32 |
| keccak_inc_absorb at fips202.c:435 | 144,257 | 1.20 |
| clmul_leaf at gf2x_clmul.c:511 | 129,402 | 1.08 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 2,665,492 | 20.70 |
| xor | 2,531,928 | 19.67 |
| sd | 1,411,071 | 10.96 |
| addi | 1,193,646 | 9.27 |
| clmul | 900,858 | 7.00 |
| clmulh | 887,928 | 6.90 |
| sh3add | 468,282 | 3.64 |
| bnez | 453,823 | 3.52 |
| rori | 293,016 | 2.28 |
| andn | 252,600 | 1.96 |
| bltu | 236,443 | 1.84 |
| mv | 193,484 | 1.50 |
| add | 130,701 | 1.02 |
| slli | 124,695 | 0.97 |
| bgeu | 122,112 | 0.95 |
| vle64.v | 95,216 | 0.74 |
| bne | 88,133 | 0.68 |
| li | 85,605 | 0.66 |
| andi | 83,559 | 0.65 |
| vxor.vv | 69,675 | 0.54 |

## 4. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 1,975,424 |
| total | 512 | 1,936,810 |
| total | 1024 | 1,923,629 |
| total | 2048 | 1,933,704 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 4,730,302 |
| total | 512 | 4,604,736 |
| total | 1024 | 4,551,910 |
| total | 2048 | 4,542,751 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 12,135,502 |
| total | 512 | 11,933,953 |
| total | 1024 | 11,847,600 |
| total | 2048 | 11,819,885 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_rvv | OK | 1,975,424 | 40,248 |
| clang | -O3 | karatsuba_rvv | OK | 1,975,424 | 40,248 |
| clang | -Os | karatsuba_rvv | OK | 1,975,424 | 40,248 |
| gcc | -O2 | karatsuba_rvv | OK | 2,029,880 | 26,158 |
| gcc | -O3 | karatsuba_rvv | OK | 2,029,880 | 26,158 |
| gcc | -Os | karatsuba_rvv | OK | 2,029,880 | 26,158 |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_rvv | OK | 4,730,302 | 43,810 |
| clang | -O3 | karatsuba_rvv | OK | 4,730,302 | 43,810 |
| clang | -Os | karatsuba_rvv | OK | 4,730,302 | 43,810 |
| gcc | -O2 | karatsuba_rvv | OK | 4,792,531 | 30,536 |
| gcc | -O3 | karatsuba_rvv | OK | 4,792,531 | 30,536 |
| gcc | -Os | karatsuba_rvv | OK | 4,792,531 | 30,536 |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | karatsuba_rvv | OK | 12,135,502 | 47,824 |
| clang | -O3 | karatsuba_rvv | OK | 12,135,502 | 47,824 |
| clang | -Os | karatsuba_rvv | OK | 12,135,502 | 47,824 |
| gcc | -O2 | karatsuba_rvv | OK | 9,732,294 | 35,348 |
| gcc | -O3 | karatsuba_rvv | OK | 9,732,294 | 35,348 |
| gcc | -Os | karatsuba_rvv | OK | 9,732,294 | 35,348 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

