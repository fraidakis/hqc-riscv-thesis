# HQC-RISC-V Analysis: `scalar_ref`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-30 23:42:27 |
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
| schoolbook_mul | 329,577,096 | 96.46 |
| memset | 5,871,210 | 1.72 |
| karatsuba_mul | 2,868,510 | 0.84 |
| KeccakF1600_StatePermute | 1,152,874 | 0.34 |
| support_contains_value | 324,300 | 0.09 |
| keccak_inc_absorb | 316,045 | 0.09 |
| hadamard | 279,496 | 0.08 |
| memcpy | 275,464 | 0.08 |
| expand_and_sum | 246,064 | 0.07 |
| keccak_inc_squeeze | 203,516 | 0.06 |
| gf_reduce | 127,636 | 0.04 |
| find_peaks | 86,883 | 0.03 |
| vect_compare | 62,873 | 0.02 |
| reduce | 37,056 | 0.01 |
| vect_add | 35,920 | 0.01 |
| gf_mul | 31,553 | 0.01 |
| ?? ??:0 | 25,118 | 0.01 |
| reed_solomon_encode | 23,814 | 0.01 |
| compute_error_values | 21,064 | 0.01 |
| compute_syndromes | 17,853 | 0.01 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul at gf2x.c:55 | 131,416,992 | 38.46 |
| schoolbook_mul at gf2x.c:54 | 109,514,160 | 32.05 |
| schoolbook_mul at gf2x.c:53 (discriminator 1) | 65,708,496 | 19.23 |
| schoolbook_mul at gf2x.c:47 | 7,370,838 | 2.16 |
| schoolbook_mul at gf2x.c:44 | 5,027,904 | 1.47 |
| schoolbook_mul at gf2x.c:48 | 2,494,464 | 0.73 |
| schoolbook_mul at gf2x.c:43 (discriminator 1) | 2,459,862 | 0.72 |
| schoolbook_mul at gf2x.c:43 (discriminator 2) | 2,455,488 | 0.72 |
| memset at mem.c:84 (discriminator 1) | 1,968,230 | 0.58 |
| memset at mem.c:85 | 1,948,142 | 0.57 |
| memset at mem.c:84 | 1,948,142 | 0.57 |
| schoolbook_mul at gf2x.c:50 | 1,738,320 | 0.51 |
| schoolbook_mul at gf2x.c:49 (discriminator 1) | 1,042,992 | 0.31 |
| karatsuba_mul at gf2x.c:120 | 552,354 | 0.16 |
| karatsuba_mul at gf2x.c:116 (discriminator 1) | 346,728 | 0.10 |
| karatsuba_mul at gf2x.c:115 (discriminator 1) | 222,444 | 0.07 |
| support_contains_value at vector.c:81 | 214,240 | 0.06 |
| karatsuba_mul at gf2x.c:114 | 210,546 | 0.06 |
| karatsuba_mul at gf2x.c:114 (discriminator 1) | 208,458 | 0.06 |
| karatsuba_mul at gf2x.c:107 | 205,452 | 0.06 |

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

