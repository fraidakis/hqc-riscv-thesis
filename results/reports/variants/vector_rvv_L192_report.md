# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
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
| clmul_leaf | 1,581,648 | 33.45 |
| KeccakF1600_StatePermute | 1,129,052 | 23.88 |
| clmul64 | 739,788 | 15.64 |
| keccak_inc_absorb | 279,123 | 5.90 |
| keccak_inc_squeeze | 187,760 | 3.97 |
| clmul_leaf_n8 | 156,390 | 3.31 |
| kmul_18 | 128,544 | 2.72 |
| gf_reduce | 109,212 | 2.31 |
| kmul_35 | 77,610 | 1.64 |
| kmul_17 | 46,800 | 0.99 |
| kmul_70 | 28,290 | 0.60 |
| gf_mul | 22,753 | 0.48 |
| kmul_36 | 18,048 | 0.38 |
| support_contains_value | 16,295 | 0.34 |
| kmul_71 | 13,968 | 0.30 |
| reed_solomon_encode | 12,844 | 0.27 |
| compute_elp | 12,152 | 0.26 |
| kmul_140 | 12,120 | 0.26 |
| fft_rec | 11,794 | 0.25 |
| expand_and_sum | 10,976 | 0.23 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:45 | 347,664 | 7.35 |
| clmul64 at gf2x_clmul.c:46 | 347,664 | 7.35 |
| clmul_leaf at gf2x_clmul.c:491 | 286,848 | 6.07 |
| clmul_leaf at gf2x_clmul.c:492 | 254,976 | 5.39 |
| clmul_leaf at gf2x_clmul.c:493 | 254,976 | 5.39 |
| KeccakF1600_StatePermute at fips202.c:? | 192,238 | 4.07 |
| keccak_inc_absorb at fips202.c:438 | 178,780 | 3.78 |
| clmul_leaf at gf2x_clmul.c:490 | 159,360 | 3.37 |
| clmul_leaf at gf2x_clmul.c:494 | 127,488 | 2.70 |
| keccak_inc_squeeze at fips202.c:505 | 104,640 | 2.21 |
| clmul_leaf at gf2x_clmul.c:518 | 95,616 | 2.02 |
| keccak_inc_absorb at fips202.c:435 | 89,792 | 1.90 |
| clmul_leaf at gf2x_clmul.c:479 | 71,712 | 1.52 |
| keccak_inc_squeeze at fips202.c:504 | 70,034 | 1.48 |
| clmul_leaf at gf2x_clmul.c:517 | 63,744 | 1.35 |
| clmul_leaf at gf2x_clmul.c:521 | 59,760 | 1.26 |
| clmul_leaf at gf2x_clmul.c:429 | 55,776 | 1.18 |
| gf_reduce at gf.c:82 | 51,732 | 1.09 |
| clmul64 at gf2x_clmul.c:? | 44,460 | 0.94 |
| gf_reduce at gf.c:80 | 40,236 | 0.85 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| xor | 1,192,360 | 21.52 |
| ld | 1,131,376 | 20.42 |
| sd | 643,704 | 11.62 |
| bnez | 430,323 | 7.77 |
| clmul | 353,412 | 6.38 |
| clmulh | 347,664 | 6.28 |
| addi | 193,679 | 3.50 |
| rori | 192,792 | 3.48 |
| andn | 166,200 | 3.00 |
| mv | 90,772 | 1.64 |
| add | 83,689 | 1.51 |
| sh3add | 76,895 | 1.39 |
| vle64.v | 65,454 | 1.18 |
| slli | 62,152 | 1.12 |
| bne | 58,037 | 1.05 |
| andi | 55,249 | 1.00 |
| vxor.vv | 43,553 | 0.79 |
| sub | 41,549 | 0.75 |
| vsetvli | 25,574 | 0.46 |
| vse64.v | 23,256 | 0.42 |

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

