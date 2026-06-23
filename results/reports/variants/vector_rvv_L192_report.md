# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-23 14:21:01 |
| Commit | `d38ed14` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 142,197 | 6430 |
| 192 | vect_mul | 441,263 | 7608 |
| 256 | vect_mul | 1,014,499 | 7064 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 288,219 | 30,680 |
| 128 | encaps | 576,557 | 30,680 |
| 128 | decaps | 1,120,509 | 30,680 |
| 128 | **total** | 1,985,285 | 30,680 |
| 192 | keypair | 701,986 | 35,028 |
| 192 | encaps | 1,418,278 | 35,028 |
| 192 | decaps | 2,554,126 | 35,028 |
| 192 | **total** | 4,674,390 | 35,028 |
| 256 | keypair | 1,432,597 | 39,730 |
| 256 | encaps | 2,890,400 | 39,730 |
| 256 | decaps | 5,186,418 | 39,730 |
| 256 | **total** | 9,509,415 | 39,730 |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)

| Level | D$ Misses | D$ Read Miss | D$ Write Miss | D$ Writebacks | D$ Bytes Read | D$ Bytes Written | D$ Read Acc. | D$ Write Acc. |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 878 | 73 | 805 | 57 | 10,377,481 | 4,752,239 | 1,576,639 | 778,058 |
| 192 | 1889 | 176 | 1713 | 310 | 22,901,560 | 12,700,829 | 3,314,695 | 1,880,346 |
| 256 | 3689 | 547 | 3142 | 1087 | 39,538,591 | 25,888,840 | 5,703,723 | 3,732,609 |

### Memory Footprint

| Level | Load Insns | Store Insns | Load Bytes | Store Bytes | CL Load | CL Store |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 1844 | 1728 | 26,585 | 17,670 | 416 | 277 |
| 192 | 1889 | 1651 | 29,575 | 18,076 | 463 | 283 |
| 256 | 2394 | 2184 | 33,019 | 22,153 | 516 | 347 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| clmul_leaf | 1,474,080 | 31.16 |
| KeccakF1600_StatePermute | 1,152,874 | 24.37 |
| __riscv_clmul_64 | 372,132 | 7.87 |
| __riscv_clmulh_64 | 367,944 | 7.78 |
| keccak_inc_absorb | 298,128 | 6.30 |
| keccak_inc_squeeze | 203,516 | 4.30 |
| kmul_18 | 146,316 | 3.09 |
| gf_reduce | 97,716 | 2.07 |
| kmul_35 | 81,120 | 1.71 |
| vect_compare | 62,867 | 1.33 |
| clmul_leaf_n8 | 54,210 | 1.15 |
| kmul_17 | 47,580 | 1.01 |
| memset | 47,560 | 1.01 |
| kmul_70 | 29,754 | 0.63 |
| reed_solomon_encode | 23,814 | 0.50 |
| gf_mul | 22,753 | 0.48 |
| compute_error_values | 21,956 | 0.46 |
| kmul_36 | 18,624 | 0.39 |
| compute_elp | 16,692 | 0.35 |
| support_contains_value | 16,002 | 0.34 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 372,132 | 7.87 |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 367,944 | 7.78 |
| clmul_leaf at gf2x_clmul.c:491 | 254,976 | 5.39 |
| clmul_leaf at gf2x_clmul.c:493 | 254,976 | 5.39 |
| clmul_leaf at gf2x_clmul.c:492 | 254,976 | 5.39 |
| keccak_inc_absorb at fips202.c:438 | 196,658 | 4.16 |
| clmul_leaf at gf2x_clmul.c:490 | 159,360 | 3.37 |
| keccak_inc_squeeze at fips202.c:505 | 139,520 | 2.95 |
| clmul_leaf at gf2x_clmul.c:494 | 127,488 | 2.70 |
| clmul_leaf at gf2x_clmul.c:479 (discriminator 1) | 95,616 | 2.02 |
| clmul_leaf at gf2x_clmul.c:517 | 63,744 | 1.35 |
| keccak_inc_absorb at fips202.c:435 | 53,920 | 1.14 |
| clmul_leaf at gf2x_clmul.c:425 | 47,808 | 1.01 |
| gf_reduce at gf.c:80 | 45,984 | 0.97 |
| gf_reduce at gf.c:82 | 45,984 | 0.97 |
| clmul_leaf at gf2x_clmul.c:521 | 39,840 | 0.84 |
| vect_compare at vector.c:282 | 35,912 | 0.76 |
| clmul_leaf at gf2x_clmul.c:485 | 31,872 | 0.67 |
| clmul_leaf at gf2x_clmul.c:518 | 31,872 | 0.67 |
| clmul_leaf at gf2x_clmul.c:457 | 23,904 | 0.51 |

### Basic Block Histogram (Top 20)

| instruction | count | % Total |
| --- | ---: | ---: |
| xor | 1,203,494 | 21.72 |
| ld | 1,082,905 | 19.55 |
| sd | 553,333 | 9.99 |
| bnez | 430,454 | 7.77 |
| clmul | 353,412 | 6.38 |
| clmulh | 347,664 | 6.28 |
| addi | 208,566 | 3.76 |
| rori | 192,792 | 3.48 |
| andn | 166,200 | 3.00 |
| add | 110,329 | 1.99 |
| sh3add | 77,078 | 1.39 |
| andi | 75,877 | 1.37 |
| slliw | 71,906 | 1.30 |
| bne | 71,264 | 1.29 |
| vle64.v | 65,454 | 1.18 |
| vxor.vv | 43,122 | 0.78 |
| mv | 41,646 | 0.75 |
| lbu | 40,166 | 0.73 |
| bltu | 33,666 | 0.61 |
| sb | 31,319 | 0.57 |

## 4. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 2,018,612 |
| total | 512 | 1,986,842 |
| total | 1024 | 1,979,136 |
| total | 2048 | 1,975,917 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 4,773,181 |
| total | 512 | 4,669,568 |
| total | 1024 | 4,643,585 |
| total | 2048 | 4,633,345 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 12,241,069 |
| total | 512 | 12,099,878 |
| total | 1024 | 12,054,204 |
| total | 2048 | 12,040,736 |

## 5. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_rvv | OK | 1,820,793 | 529,971 |
| gcc | -O3 | karatsuba_rvv | OK | 1,673,598 | 537,187 |
| clang | -O2 | karatsuba_rvv | OK | 1,684,379 | 542,297 |
| clang | -O3 | karatsuba_rvv | OK | 1,660,813 | 546,833 |

### HQC-192

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_rvv | OK | 4,430,532 | 532,515 |
| gcc | -O3 | karatsuba_rvv | OK | 3,975,160 | 541,955 |
| clang | -O2 | karatsuba_rvv | OK | 3,958,739 | 546,945 |
| clang | -O3 | karatsuba_rvv | OK | 3,954,544 | 550,985 |

### HQC-256

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | karatsuba_rvv | OK | 9,429,684 | 538,579 |
| gcc | -O3 | karatsuba_rvv | OK | 7,558,296 | 546,827 |
| clang | -O2 | karatsuba_rvv | OK | 9,629,208 | 549,753 |
| clang | -O3 | karatsuba_rvv | OK | 9,366,365 | 553,481 |

## 6. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

