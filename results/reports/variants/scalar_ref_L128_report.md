# HQC-RISC-V Analysis: `scalar_ref`

| Property | Value |
| --- | --- |
| Track | scalar |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-06-23 13:26:11 |
| Commit | `c828690` |

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

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul | 112,621,824 | 95.96 |
| memset | 1,995,963 | 1.70 |
| karatsuba_mul | 958,428 | 0.82 |
| KeccakF1600_StatePermute | 578,792 | 0.49 |
| hadamard | 186,162 | 0.16 |
| keccak_inc_absorb | 141,110 | 0.12 |
| support_contains_value | 139,683 | 0.12 |
| memcpy | 124,446 | 0.11 |
| gf_reduce | 106,191 | 0.09 |
| find_peaks | 105,965 | 0.09 |
| keccak_inc_squeeze | 95,567 | 0.08 |
| expand_and_sum | 90,206 | 0.08 |
| vect_compare | 31,058 | 0.03 |
| gf_mul | 23,702 | 0.02 |
| reduce | 19,997 | 0.02 |
| vect_add | 15,781 | 0.01 |
| compute_syndromes | 13,770 | 0.01 |
| compute_elp | 12,117 | 0.01 |
| vect_generate_random_support2 | 11,718 | 0.01 |
| code_decode | 11,387 | 0.01 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| schoolbook_mul at gf2x.c:55 | 43,214,472 | 36.82 |
| schoolbook_mul at gf2x.c:54 | 36,012,060 | 30.69 |
| schoolbook_mul at gf2x.c:53 | 22,419,936 | 19.10 |
| schoolbook_mul at gf2x.c:0 | 4,115,100 | 3.51 |
| schoolbook_mul at gf2x.c:44 | 2,476,800 | 2.11 |
| schoolbook_mul at gf2x.c:43 | 1,651,200 | 1.41 |
| memset at mem.c:84 | 1,331,280 | 1.13 |
| schoolbook_mul at gf2x.c:48 | 825,600 | 0.70 |
| schoolbook_mul at gf2x.c:47 | 812,700 | 0.69 |
| memset at mem.c:85 | 662,961 | 0.56 |
| schoolbook_mul at gf2x.c:50 | 571,620 | 0.49 |
| schoolbook_mul at gf2x.c:49 | 355,872 | 0.30 |
| karatsuba_mul at gf2x.c:115 | 159,510 | 0.14 |
| karatsuba_mul at gf2x.c:114 | 154,608 | 0.13 |
| schoolbook_mul at gf2x.c:42 | 103,200 | 0.09 |
| KeccakF1600_StatePermute at fips202.c:0 | 98,548 | 0.08 |
| support_contains_value at vector.c:81 | 92,340 | 0.08 |
| karatsuba_mul at gf2x.c:119 | 89,904 | 0.08 |
| karatsuba_mul at gf2x.c:116 | 89,904 | 0.08 |
| keccak_inc_absorb at fips202.c:438 | 85,750 | 0.07 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| ld | 23,497,739 | 19.86 |
| addi | 16,579,202 | 14.02 |
| xor | 15,005,645 | 12.68 |
| sd | 14,724,060 | 12.45 |
| and | 14,536,031 | 12.29 |
| bne | 8,116,107 | 6.86 |
| srl | 8,037,222 | 6.79 |
| sll | 7,212,529 | 6.10 |
| mv | 3,345,004 | 2.83 |
| slli | 918,451 | 0.78 |
| beq | 878,540 | 0.74 |
| j | 848,758 | 0.72 |
| beqz | 829,992 | 0.70 |
| srai | 826,336 | 0.70 |
| sub | 824,034 | 0.70 |
| sb | 700,036 | 0.59 |
| bnez | 470,006 | 0.40 |
| add | 232,454 | 0.20 |
| rori | 98,832 | 0.08 |
| andn | 85,723 | 0.07 |

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

