# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-18 18:58:13 |
| Commit | `c5f821b` |

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
| KeccakF1600_StatePermute | 578,792 | 29.37 |
| clmul_leaf | 490,692 | 24.90 |
| clmul64 | 254,400 | 12.91 |
| keccak_inc_absorb | 141,116 | 7.16 |
| keccak_inc_squeeze | 96,430 | 4.89 |
| gf_reduce | 80,541 | 4.09 |
| clmul_leaf_n8 | 75,036 | 3.81 |
| kmul_17 | 32,634 | 1.66 |
| kmul_18 | 31,680 | 1.61 |
| kmul_35 | 26,796 | 1.36 |
| gf_mul | 16,952 | 0.86 |
| compute_elp | 10,112 | 0.51 |
| support_contains_value | 8877 | 0.45 |
| kmul_69 | 8640 | 0.44 |
| fft_rec | 8632 | 0.44 |
| compute_error_values | 8110 | 0.41 |
| reed_solomon_encode | 8058 | 0.41 |
| memset | 6443 | 0.33 |
| vect_generate_random_support2 | 6156 | 0.31 |
| kmul_70 | 5916 | 0.30 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:45 | 114,324 | 5.80 |
| clmul64 at gf2x_clmul.c:46 | 114,324 | 5.80 |
| KeccakF1600_StatePermute at fips202.c:? | 98,548 | 5.00 |
| clmul_leaf at gf2x_clmul.c:491 | 88,992 | 4.52 |
| keccak_inc_absorb at fips202.c:438 | 85,750 | 4.35 |
| clmul_leaf at gf2x_clmul.c:492 | 79,104 | 4.01 |
| clmul_leaf at gf2x_clmul.c:493 | 79,104 | 4.01 |
| keccak_inc_squeeze at fips202.c:505 | 51,696 | 2.62 |
| clmul_leaf at gf2x_clmul.c:490 | 49,440 | 2.51 |
| keccak_inc_absorb at fips202.c:435 | 43,067 | 2.19 |
| clmul_leaf at gf2x_clmul.c:494 | 39,552 | 2.01 |
| gf_reduce at gf.c:82 | 38,151 | 1.94 |
| keccak_inc_squeeze at fips202.c:504 | 34,608 | 1.76 |
| gf_reduce at gf.c:80 | 29,673 | 1.51 |
| clmul_leaf at gf2x_clmul.c:518 | 29,664 | 1.51 |
| clmul64 at gf2x_clmul.c:? | 25,752 | 1.31 |
| clmul_leaf at gf2x_clmul.c:479 | 22,248 | 1.13 |
| clmul_leaf at gf2x_clmul.c:517 | 19,776 | 1.00 |
| clmul_leaf at gf2x_clmul.c:521 | 18,540 | 0.94 |
| clmul_leaf at gf2x_clmul.c:429 | 17,304 | 0.88 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 671,588 | 24.38 |
| xor | 509,339 | 18.49 |
| bnez | 400,820 | 14.55 |
| sd | 246,249 | 8.94 |
| clmul | 118,563 | 4.30 |
| clmulh | 114,324 | 4.15 |
| rori | 98,832 | 3.59 |
| andn | 85,200 | 3.09 |
| addi | 84,919 | 3.08 |
| add | 44,128 | 1.60 |
| slli | 42,141 | 1.53 |
| mv | 36,202 | 1.31 |
| andi | 27,964 | 1.01 |
| bne | 25,591 | 0.93 |
| sh3add | 25,519 | 0.93 |
| vle64.v | 21,204 | 0.77 |
| sub | 17,702 | 0.64 |
| zext.b | 14,991 | 0.54 |
| vxor.vv | 14,153 | 0.51 |
| bltu | 11,382 | 0.41 |

## 4. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 1,956,787 |
| total | 512 | 1,926,763 |
| total | 1024 | 1,922,144 |
| total | 2048 | 1,935,483 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 4,675,287 |
| total | 512 | 4,572,661 |
| total | 1024 | 4,550,103 |
| total | 2048 | 4,548,702 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 12,021,467 |
| total | 512 | 11,882,530 |
| total | 1024 | 11,838,941 |
| total | 2048 | 11,827,200 |

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

