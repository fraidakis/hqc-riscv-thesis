# HQC-RISC-V Analysis: `karatsuba_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-05-26 18:18:29 |
| Commit | `c56c136` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 157,377 | 10,442 |
| 192 | vect_mul | 481,065 | 11,262 |
| 256 | vect_mul | 1,608,062 | 9746 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 289,001 | 40,248 |
| 128 | encaps | 591,892 | 40,248 |
| 128 | decaps | 1,094,531 | 40,248 |
| 128 | **total** | 1,975,424 | 40,248 |
| 192 | keypair | 713,518 | 43,810 |
| 192 | encaps | 1,472,419 | 43,810 |
| 192 | decaps | 2,544,365 | 43,810 |
| 192 | **total** | 4,730,302 | 43,810 |
| 256 | keypair | 1,887,450 | 47,824 |
| 256 | encaps | 3,845,194 | 47,824 |
| 256 | decaps | 6,402,858 | 47,824 |
| 256 | **total** | 12,135,502 | 47,824 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1298 | 92 | 1206 | 110 | 11,395,191 | 5,947,015 | 1,728,213 | 1,041,077 |
| 192 | 3070 | 283 | 2787 | 769 | 25,517,826 | 15,781,137 | 3,691,257 | 2,426,191 |
| 256 | 6860 | 971 | 5889 | 3250 | 56,982,933 | 31,952,376 | 7,954,322 | 4,729,425 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1999 | 2206 | 26,936 | 23,788 | 421 | 372 |
| 192 | 2336 | 2365 | 35,940 | 29,174 | 562 | 456 |
| 256 | 2215 | 2319 | 31,216 | 26,885 | 488 | 421 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute | 578,792 | 29.09 |
| clmul_leaf | 490,692 | 24.66 |
| clmul64 | 254,178 | 12.78 |
| keccak_inc_absorb | 141,116 | 7.09 |
| keccak_inc_squeeze | 96,430 | 4.85 |
| gf_reduce | 80,541 | 4.05 |
| clmul_leaf_n8 | 73,482 | 3.69 |
| rvv_xor3_u64 | 50,814 | 2.55 |
| rvv_copy_u64 | 26,418 | 1.33 |
| rvv_xor_u64 | 22,320 | 1.12 |
| gf_mul | 16,952 | 0.85 |
| kmul_17 | 15,540 | 0.78 |
| compute_elp | 10,173 | 0.51 |
| kmul_35 | 9108 | 0.46 |
| support_contains_value | 8877 | 0.45 |
| kmul_18 | 8844 | 0.44 |
| fft_rec | 8632 | 0.43 |
| compute_error_values | 8112 | 0.41 |
| reed_solomon_encode | 8058 | 0.41 |
| memset | 6443 | 0.32 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul64 at gf2x_clmul.c:45 | 114,324 | 5.75 |
| clmul64 at gf2x_clmul.c:46 | 114,324 | 5.75 |
| KeccakF1600_StatePermute at fips202.c:0 | 98,548 | 4.95 |
| clmul_leaf at gf2x_clmul.c:468 | 88,992 | 4.47 |
| keccak_inc_absorb at fips202.c:438 | 85,750 | 4.31 |
| clmul_leaf at gf2x_clmul.c:469 | 79,104 | 3.98 |
| clmul_leaf at gf2x_clmul.c:470 | 79,104 | 3.98 |
| keccak_inc_squeeze at fips202.c:505 | 51,696 | 2.60 |
| clmul_leaf at gf2x_clmul.c:467 | 49,440 | 2.48 |
| keccak_inc_absorb at fips202.c:435 | 43,067 | 2.16 |
| clmul_leaf at gf2x_clmul.c:471 | 39,552 | 1.99 |
| gf_reduce at gf.c:82 | 38,151 | 1.92 |
| keccak_inc_squeeze at fips202.c:504 | 34,608 | 1.74 |
| gf_reduce at gf.c:80 | 29,673 | 1.49 |
| clmul_leaf at gf2x_clmul.c:495 | 29,664 | 1.49 |
| clmul64 at gf2x_clmul.c:0 | 25,530 | 1.28 |
| clmul_leaf at gf2x_clmul.c:456 | 22,248 | 1.12 |
| clmul_leaf at gf2x_clmul.c:494 | 19,776 | 0.99 |
| clmul_leaf at gf2x_clmul.c:498 | 18,540 | 0.93 |
| clmul_leaf at gf2x_clmul.c:406 | 17,304 | 0.87 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 671,724 | 24.21 |
| xor | 509,459 | 18.36 |
| bnez | 404,820 | 14.59 |
| sd | 245,797 | 8.86 |
| clmul | 118,563 | 4.27 |
| clmulh | 114,324 | 4.12 |
| rori | 98,832 | 3.56 |
| addi | 85,548 | 3.08 |
| andn | 85,200 | 3.07 |
| add | 44,626 | 1.61 |
| slli | 42,237 | 1.52 |
| mv | 35,053 | 1.26 |
| sh3add | 29,089 | 1.05 |
| andi | 27,964 | 1.01 |
| bne | 25,825 | 0.93 |
| vle64.v | 21,852 | 0.79 |
| sub | 21,032 | 0.76 |
| zext.b | 14,991 | 0.54 |
| vxor.vv | 13,673 | 0.49 |
| vsetvli | 12,889 | 0.46 |

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

- Speedup summary: `analysis/reports/speedups.md`
- Compiler showdown: `analysis/reports/compiler_analysis.md`

