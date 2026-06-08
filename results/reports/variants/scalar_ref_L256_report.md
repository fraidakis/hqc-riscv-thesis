# HQC-RISC-V Analysis: `scalar_ref`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-08 14:02:16 |
| Commit | `216edab` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 19,214,277 | 2012 |
| 192 | vect_mul | 58,433,896 | 2016 |
| 256 | vect_mul | 142,641,043 | 2026 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 19,454,626 | 32,176 |
| 128 | encaps | 38,875,925 | 32,176 |
| 128 | decaps | 59,013,743 | 32,176 |
| 128 | **total** | 117,344,294 | 32,176 |
| 192 | keypair | 58,895,392 | 33,930 |
| 192 | encaps | 117,744,966 | 33,930 |
| 192 | decaps | 177,662,495 | 33,930 |
| 192 | **total** | 354,302,853 | 33,930 |
| 256 | keypair | 143,382,635 | 41,392 |
| 256 | encaps | 286,701,285 | 41,392 |
| 256 | decaps | 431,890,526 | 41,392 |
| 256 | **total** | 861,974,446 | 41,392 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1358 | 87 | 1271 | 125 | 372,774,966 | 237,223,951 | 46,807,726 | 30,974,223 |
| 192 | 3325 | 287 | 3038 | 906 | 1,123,633,067 | 719,436,907 | 140,822,271 | 93,607,383 |
| 256 | 7878 | 1136 | 6742 | 4027 | 2,808,117,202 | 1,823,990,638 | 351,646,625 | 233,895,589 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1643 | 2131 | 7715 | 10,629 | 121 | 167 |
| 192 | 1810 | 2240 | 8127 | 10,776 | 127 | 169 |
| 256 | 1829 | 2226 | 7659 | 9993 | 120 | 157 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

> _No data available._

### Line-Level Hotspots (Top 20 Rows)

> _No data available._

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 175,652,911 | 20.35 |
| addi | 122,622,780 | 14.21 |
| xor | 114,531,101 | 13.27 |
| sd | 113,557,696 | 13.16 |
| and | 112,812,885 | 13.07 |
| bne | 60,455,584 | 7.01 |
| srl | 59,953,202 | 6.95 |
| sll | 55,970,266 | 6.49 |
| mv | 15,999,117 | 1.85 |
| slli | 4,296,395 | 0.50 |
| beq | 4,231,972 | 0.49 |
| j | 4,072,713 | 0.47 |
| beqz | 3,998,713 | 0.46 |
| srai | 3,988,128 | 0.46 |
| sub | 3,959,794 | 0.46 |
| sb | 3,235,166 | 0.37 |
| add | 971,687 | 0.11 |
| bnez | 518,414 | 0.06 |
| rori | 293,016 | 0.03 |
| andn | 254,482 | 0.03 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_scalar | OK | 117,418,020 | 23,972 |
| clang | -O3 | ref_scalar | OK | 117,344,266 | 32,158 |
| clang | -Os | ref_scalar | OK | 117,459,473 | 18,264 |
| gcc | -O2 | ref_scalar | OK | 120,486,098 | 17,834 |
| gcc | -O3 | ref_scalar | OK | 113,958,344 | 26,270 |
| gcc | -Os | ref_scalar | OK | 122,366,611 | 16,888 |

### HQC-192

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_scalar | OK | 354,434,380 | 25,844 |
| clang | -O3 | ref_scalar | OK | 354,302,605 | 33,916 |
| clang | -Os | ref_scalar | OK | 354,513,619 | 19,574 |
| gcc | -O2 | ref_scalar | OK | 363,797,867 | 18,942 |
| gcc | -O3 | ref_scalar | OK | 344,003,671 | 28,980 |
| gcc | -Os | ref_scalar | OK | 369,265,318 | 17,786 |

### HQC-256

| Compiler | Opt | Variant | Status | Instructions | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| clang | -O2 | ref_scalar | OK | 862,204,770 | 31,746 |
| clang | -O3 | ref_scalar | OK | 861,974,446 | 41,390 |
| clang | -Os | ref_scalar | OK | 862,317,151 | 26,516 |
| gcc | -O2 | ref_scalar | OK | 898,156,204 | 25,928 |
| gcc | -O3 | ref_scalar | OK | 845,613,464 | 33,532 |
| gcc | -Os | ref_scalar | OK | 907,517,922 | 24,678 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

