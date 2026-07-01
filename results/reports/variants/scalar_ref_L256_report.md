# HQC-RISC-V Analysis: `scalar_ref`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-07-01 11:48:42 |
| Commit | `cc8054d` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 18,504,302 | 5054 |
| 192 | vect_mul | 56,292,809 | 5090 |
| 256 | vect_mul | 139,222,953 | 4298 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 18,750,499 | 29,438 |
| 128 | encaps | 37,477,397 | 29,438 |
| 128 | decaps | 56,937,074 | 29,438 |
| 128 | **total** | 113,164,970 | 29,438 |
| 192 | keypair | 56,763,859 | 32,024 |
| 192 | encaps | 113,506,273 | 32,024 |
| 192 | decaps | 171,349,089 | 32,024 |
| 192 | **total** | 341,619,221 | 32,024 |
| 256 | keypair | 139,979,206 | 36,530 |
| 256 | encaps | 279,919,351 | 36,530 |
| 256 | decaps | 421,860,950 | 36,530 |
| 256 | **total** | 841,759,507 | 36,530 |

## 2. Hotspots

### Function-Level Hotspots (Top 20 Rows)

> _No data available._

### Line-Level Hotspots (Top 20 Rows)

> _No data available._

## 4. Compiler Comparison

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

## 5. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

