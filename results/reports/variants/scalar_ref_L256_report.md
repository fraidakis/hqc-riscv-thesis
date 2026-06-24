# HQC-RISC-V Analysis: `scalar_ref`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-06-24 15:21:07 |
| Commit | `0eb2718` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 18,639,752 | 5216 |
| 192 | vect_mul | 56,702,057 | 5252 |
| 256 | vect_mul | 139,877,019 | 4466 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 18,886,091 | 29,486 |
| 128 | encaps | 37,748,533 | 29,486 |
| 128 | decaps | 57,361,651 | 29,486 |
| 128 | **total** | 113,996,275 | 29,486 |
| 192 | keypair | 57,173,317 | 32,072 |
| 192 | encaps | 114,325,122 | 32,072 |
| 192 | decaps | 172,613,387 | 32,072 |
| 192 | **total** | 344,111,826 | 32,072 |
| 256 | keypair | 140,633,544 | 36,580 |
| 256 | encaps | 281,227,941 | 36,580 |
| 256 | decaps | 423,881,598 | 36,580 |
| 256 | **total** | 845,743,083 | 36,580 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1349 | 87 | 1262 | 136 | 372,860,807 | 236,874,517 | 46,886,869 | 30,932,959 |
| 192 | 3298 | 291 | 3007 | 932 | 1,123,549,340 | 718,572,777 | 140,895,471 | 93,503,859 |
| 256 | 7593 | 1131 | 6462 | 3778 | 2,808,270,227 | 1,822,987,412 | 351,794,801 | 233,769,062 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1207 | 1060 | 7787 | 6970 | 122 | 109 |
| 192 | 1236 | 1131 | 7681 | 7358 | 121 | 115 |
| 256 | 1107 | 1041 | 6788 | 6825 | 107 | 107 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

> _No data available._

### Line-Level Hotspots (Top 20 Rows)

> _No data available._

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | ref_scalar | OK | 119,588,829 | 517,897 |
| gcc | -O3 | ref_scalar | OK | 70,994,731 | 531,865 |
| clang | -O2 | ref_scalar | OK | 82,622,747 | 533,315 |
| clang | -O3 | ref_scalar | OK | 82,540,770 | 547,303 |

### HQC-192

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | ref_scalar | OK | 360,726,712 | 519,209 |
| gcc | -O3 | ref_scalar | OK | 214,228,777 | 533,481 |
| clang | -O2 | ref_scalar | OK | 249,443,989 | 534,709 |
| clang | -O3 | ref_scalar | OK | 249,475,460 | 548,665 |

### HQC-256

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | ref_scalar | OK | 925,792,203 | 526,337 |
| gcc | -O3 | ref_scalar | OK | 533,883,002 | 537,857 |
| clang | -O2 | ref_scalar | OK | 611,671,813 | 541,505 |
| clang | -O3 | ref_scalar | OK | 611,456,033 | 552,753 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

