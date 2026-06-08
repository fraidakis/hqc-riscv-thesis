# HQC-RISC-V Analysis: `vector_ref`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
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

> _No data available._

### Line-Level Hotspots (Top 20 Rows)

> _No data available._

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 173,822,530 | 20.71 |
| addi | 117,169,572 | 13.96 |
| xor | 113,330,921 | 13.50 |
| sd | 112,687,122 | 13.43 |
| and | 112,391,207 | 13.39 |
| srl | 59,951,634 | 7.14 |
| bne | 56,503,426 | 6.73 |
| sll | 55,970,266 | 6.67 |
| mv | 8,212,470 | 0.98 |
| slli | 4,359,988 | 0.52 |
| beq | 4,168,639 | 0.50 |
| j | 4,079,786 | 0.49 |
| beqz | 4,068,915 | 0.48 |
| sub | 4,002,961 | 0.48 |
| srai | 3,988,128 | 0.48 |
| add | 1,480,257 | 0.18 |
| bnez | 538,297 | 0.06 |
| rori | 293,016 | 0.03 |
| andn | 252,829 | 0.03 |
| vl2re64.v | 224,718 | 0.03 |

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

