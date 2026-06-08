# HQC-RISC-V Analysis: `vector_ref`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-08 14:02:16 |
| Commit | `216edab` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 18,527,409 | 3454 |
| 192 | vect_mul | 56,315,139 | 3466 |
| 256 | vect_mul | 139,115,660 | 3470 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 18,653,676 | 35,338 |
| 128 | encaps | 37,325,173 | 35,338 |
| 128 | decaps | 56,394,693 | 35,338 |
| 128 | **total** | 112,373,542 | 35,338 |
| 192 | keypair | 56,543,388 | 36,100 |
| 192 | encaps | 113,126,035 | 36,100 |
| 192 | decaps | 170,291,912 | 36,100 |
| 192 | **total** | 339,961,335 | 36,100 |
| 256 | keypair | 139,476,234 | 43,806 |
| 256 | encaps | 279,034,551 | 43,806 |
| 256 | decaps | 419,679,222 | 43,806 |
| 256 | **total** | 838,190,007 | 43,806 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1367 | 95 | 1272 | 127 | 373,234,422 | 237,247,271 | 46,885,132 | 32,205,808 |
| 192 | 3289 | 285 | 3004 | 854 | 1,125,080,405 | 719,595,427 | 141,027,031 | 97,395,455 |
| 256 | 7747 | 1064 | 6683 | 3888 | 2,810,237,752 | 1,824,062,682 | 351,937,225 | 240,788,788 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1891 | 2012 | 14,197 | 18,160 | 222 | 284 |
| 192 | 2079 | 2112 | 15,196 | 20,031 | 238 | 313 |
| 256 | 2144 | 2127 | 15,855 | 19,886 | 248 | 311 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul | 110,519,532 | 98.34 |
| karatsuba_mul | 604,860 | 0.54 |
| KeccakF1600_StatePermute | 578,792 | 0.51 |
| keccak_inc_absorb | 141,110 | 0.13 |
| gf_reduce | 106,191 | 0.09 |
| find_peaks | 105,965 | 0.09 |
| keccak_inc_squeeze | 90,932 | 0.08 |
| memset | 43,101 | 0.04 |
| expand_and_sum | 33,258 | 0.03 |
| gf_mul | 23,702 | 0.02 |
| compute_syndromes | 13,990 | 0.01 |
| code_decode | 11,493 | 0.01 |
| compute_elp | 11,470 | 0.01 |
| hadamard | 10,080 | 0.01 |
| fft_rec | 8798 | 0.01 |
| reed_solomon_encode | 8376 | 0.01 |
| compute_error_values | 8148 | 0.01 |
| support_contains_value | 7803 | 0.01 |
| ?? ??:0 | 7209 | 0.01 |
| vect_write_support_to_vector | 6495 | 0.01 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul at gf2x.c:55 | 43,214,472 | 38.45 |
| schoolbook_mul at gf2x.c:54 | 36,012,060 | 32.04 |
| schoolbook_mul at gf2x.c:53 | 22,419,936 | 19.95 |
| schoolbook_mul at gf2x.c:? | 2,554,200 | 2.27 |
| schoolbook_mul at gf2x.c:44 | 2,476,800 | 2.20 |
| schoolbook_mul at gf2x.c:43 | 1,651,200 | 1.47 |
| schoolbook_mul at gf2x.c:48 | 825,600 | 0.73 |
| schoolbook_mul at gf2x.c:47 | 812,700 | 0.72 |
| schoolbook_mul at gf2x.c:42 | 206,400 | 0.18 |
| schoolbook_mul at gf2x.c:49 | 142,368 | 0.13 |
| schoolbook_mul at gf2x.c:50 | 120,120 | 0.11 |
| KeccakF1600_StatePermute at fips202.c:? | 98,548 | 0.09 |
| keccak_inc_absorb at fips202.c:438 | 85,750 | 0.08 |
| schoolbook_mul at gf2x.c:41 | 82,218 | 0.07 |
| karatsuba_mul at gf2x.c:116 | 58,722 | 0.05 |
| karatsuba_mul at gf2x.c:115 | 54,294 | 0.05 |
| gf_reduce at gf.c:82 | 50,301 | 0.04 |
| keccak_inc_squeeze at fips202.c:505 | 49,428 | 0.04 |
| karatsuba_mul at gf2x.c:104 | 47,664 | 0.04 |
| karatsuba_mul at gf2x.c:108 | 47,580 | 0.04 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 23,167,828 | 20.44 |
| addi | 15,423,066 | 13.61 |
| xor | 14,784,943 | 13.05 |
| sd | 14,558,931 | 12.85 |
| and | 14,448,410 | 12.75 |
| srl | 8,036,646 | 7.09 |
| bne | 7,254,860 | 6.40 |
| sll | 7,212,529 | 6.36 |
| mv | 1,733,144 | 1.53 |
| slli | 929,630 | 0.82 |
| beq | 877,427 | 0.77 |
| j | 849,489 | 0.75 |
| beqz | 845,804 | 0.75 |
| sub | 834,450 | 0.74 |
| srai | 826,336 | 0.73 |
| bnez | 475,436 | 0.42 |
| add | 356,009 | 0.31 |
| rori | 98,832 | 0.09 |
| andn | 85,375 | 0.08 |
| andi | 43,277 | 0.04 |

## 4. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 112,373,542 |
| total | 512 | 113,023,742 |
| total | 1024 | 113,279,550 |
| total | 2048 | 113,424,749 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 339,961,335 |
| total | 512 | 341,922,085 |
| total | 1024 | 342,359,409 |
| total | 2048 | 342,761,464 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 838,190,007 |
| total | 512 | 841,658,820 |
| total | 1024 | 842,795,840 |
| total | 2048 | 843,492,312 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_vector | OK | 112,369,640 | 31,982 |
| clang | -O3 | ref_vector | OK | 112,373,542 | 35,336 |
| clang | -Os | ref_vector | OK | 112,375,828 | 31,212 |
| gcc | -O2 | ref_vector | OK | 110,643,374 | 31,212 |
| gcc | -O3 | ref_vector | OK | 110,639,142 | 31,674 |
| gcc | -Os | ref_vector | OK | 110,656,536 | 31,054 |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_vector | OK | 339,959,120 | 32,872 |
| clang | -O3 | ref_vector | OK | 339,961,335 | 36,098 |
| clang | -Os | ref_vector | OK | 339,967,720 | 32,102 |
| gcc | -O2 | ref_vector | OK | 334,666,352 | 33,448 |
| gcc | -O3 | ref_vector | OK | 334,660,464 | 33,910 |
| gcc | -Os | ref_vector | OK | 334,684,099 | 33,252 |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_vector | OK | 838,185,076 | 40,464 |
| clang | -O3 | ref_vector | OK | 838,190,007 | 43,804 |
| clang | -Os | ref_vector | OK | 838,197,160 | 39,696 |
| gcc | -O2 | ref_vector | OK | 828,410,451 | 42,040 |
| gcc | -O3 | ref_vector | OK | 828,402,171 | 42,502 |
| gcc | -Os | ref_vector | OK | 828,450,458 | 41,866 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

