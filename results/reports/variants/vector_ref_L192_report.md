# HQC-RISC-V Analysis: `vector_ref`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
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
| schoolbook_mul | 335,930,436 | 98.80 |
| karatsuba_mul | 1,843,302 | 0.54 |
| KeccakF1600_StatePermute | 1,129,052 | 0.33 |
| keccak_inc_absorb | 279,125 | 0.08 |
| keccak_inc_squeeze | 148,582 | 0.04 |
| gf_reduce | 142,652 | 0.04 |
| find_peaks | 128,955 | 0.04 |
| memset | 118,217 | 0.03 |
| expand_and_sum | 60,704 | 0.02 |
| gf_mul | 31,553 | 0.01 |
| compute_syndromes | 18,122 | 0.01 |
| support_contains_value | 14,959 | 0.00 |
| code_decode | 13,870 | 0.00 |
| compute_elp | 13,329 | 0.00 |
| ?? ??:0 | 13,302 | 0.00 |
| reed_solomon_encode | 13,250 | 0.00 |
| fft_rec | 11,954 | 0.00 |
| hadamard | 11,318 | 0.00 |
| vect_write_support_to_vector | 9855 | 0.00 |
| vect_generate_random_support2 | 7734 | 0.00 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul at gf2x.c:55 | 131,416,992 | 38.65 |
| schoolbook_mul at gf2x.c:54 | 109,514,160 | 32.21 |
| schoolbook_mul at gf2x.c:53 | 68,163,984 | 20.05 |
| schoolbook_mul at gf2x.c:? | 7,717,248 | 2.27 |
| schoolbook_mul at gf2x.c:44 | 7,483,392 | 2.20 |
| schoolbook_mul at gf2x.c:43 | 4,988,928 | 1.47 |
| schoolbook_mul at gf2x.c:48 | 2,494,464 | 0.73 |
| schoolbook_mul at gf2x.c:47 | 2,455,488 | 0.72 |
| schoolbook_mul at gf2x.c:42 | 623,616 | 0.18 |
| schoolbook_mul at gf2x.c:49 | 445,872 | 0.13 |
| schoolbook_mul at gf2x.c:50 | 374,160 | 0.11 |
| schoolbook_mul at gf2x.c:41 | 247,758 | 0.07 |
| KeccakF1600_StatePermute at fips202.c:? | 192,238 | 0.06 |
| keccak_inc_absorb at fips202.c:438 | 178,780 | 0.05 |
| karatsuba_mul at gf2x.c:116 | 177,192 | 0.05 |
| karatsuba_mul at gf2x.c:115 | 165,432 | 0.05 |
| karatsuba_mul at gf2x.c:104 | 145,134 | 0.04 |
| karatsuba_mul at gf2x.c:108 | 144,870 | 0.04 |
| karatsuba_mul at gf2x.c:107 | 143,922 | 0.04 |
| karatsuba_mul at gf2x.c:119 | 142,968 | 0.04 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 69,396,641 | 20.35 |
| addi | 46,782,290 | 13.72 |
| xor | 44,585,335 | 13.07 |
| sd | 44,191,608 | 12.96 |
| and | 43,918,758 | 12.88 |
| srl | 24,411,018 | 7.16 |
| bne | 22,038,733 | 6.46 |
| sll | 21,922,412 | 6.43 |
| mv | 5,164,308 | 1.51 |
| slli | 2,724,110 | 0.80 |
| beq | 2,639,574 | 0.77 |
| j | 2,556,186 | 0.75 |
| beqz | 2,552,851 | 0.75 |
| sub | 2,508,358 | 0.74 |
| srai | 2,495,360 | 0.73 |
| add | 1,019,504 | 0.30 |
| bnez | 515,242 | 0.15 |
| rori | 192,792 | 0.06 |
| andn | 166,356 | 0.05 |
| vl2re64.v | 129,474 | 0.04 |

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

