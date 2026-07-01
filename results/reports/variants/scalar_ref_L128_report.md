# HQC-RISC-V Analysis: `scalar_ref`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-30 23:42:22 |
| Commit | `bdd9cf4` |

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
| 128 | keypair | 18,886,091 | 29,486 |
| 128 | encaps | 37,748,533 | 29,486 |
| 128 | decaps | 57,361,651 | 29,486 |
| 128 | **total** | 113,996,275 | 29,486 |

## 2. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul | 108,419,784 | 95.79 |
| memset | 1,994,589 | 1.76 |
| karatsuba_mul | 899,916 | 0.80 |
| KeccakF1600_StatePermute | 591,004 | 0.52 |
| hadamard | 229,586 | 0.20 |
| keccak_inc_absorb | 159,678 | 0.14 |
| support_contains_value | 140,436 | 0.12 |
| memcpy | 137,922 | 0.12 |
| expand_and_sum | 116,426 | 0.10 |
| keccak_inc_squeeze | 103,803 | 0.09 |
| gf_reduce | 95,013 | 0.08 |
| find_peaks | 71,293 | 0.06 |
| vect_compare | 31,058 | 0.03 |
| gf_mul | 23,702 | 0.02 |
| reduce | 18,312 | 0.02 |
| compute_error_values | 17,745 | 0.02 |
| vect_add | 17,744 | 0.02 |
| reed_solomon_encode | 14,966 | 0.01 |
| compute_syndromes | 13,708 | 0.01 |
| compute_elp | 13,413 | 0.01 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul at gf2x.c:55 | 43,214,472 | 38.18 |
| schoolbook_mul at gf2x.c:54 | 36,012,060 | 31.82 |
| schoolbook_mul at gf2x.c:53 (discriminator 1) | 21,607,236 | 19.09 |
| schoolbook_mul at gf2x.c:47 | 2,439,558 | 2.16 |
| schoolbook_mul at gf2x.c:44 | 1,664,100 | 1.47 |
| schoolbook_mul at gf2x.c:48 | 825,600 | 0.73 |
| schoolbook_mul at gf2x.c:43 (discriminator 1) | 814,158 | 0.72 |
| schoolbook_mul at gf2x.c:43 (discriminator 2) | 812,700 | 0.72 |
| memset at mem.c:84 (discriminator 1) | 668,693 | 0.59 |
| memset at mem.c:85 | 661,799 | 0.58 |
| memset at mem.c:84 | 661,799 | 0.58 |
| schoolbook_mul at gf2x.c:50 | 571,620 | 0.51 |
| schoolbook_mul at gf2x.c:49 (discriminator 1) | 342,972 | 0.30 |
| karatsuba_mul at gf2x.c:120 | 173,796 | 0.15 |
| karatsuba_mul at gf2x.c:116 (discriminator 1) | 106,512 | 0.09 |
| keccak_inc_absorb at fips202.c:438 | 94,325 | 0.08 |
| support_contains_value at vector.c:81 | 92,340 | 0.08 |
| hadamard at reed_muller.c:98 | 82,432 | 0.07 |
| hadamard at reed_muller.c:99 | 82,432 | 0.07 |
| karatsuba_mul at gf2x.c:115 (discriminator 1) | 70,338 | 0.06 |

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

