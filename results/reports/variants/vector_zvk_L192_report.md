# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 192 |
| Generated | 2026-06-30 23:42:29 |
| Commit | `bdd9cf4` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 70,070 | 6978 |
| 192 | vect_mul | 207,443 | 8074 |
| 256 | vect_mul | 318,972 | 9250 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 216,092 | 27,790 |
| 128 | encaps | 432,303 | 27,790 |
| 128 | decaps | 843,459 | 27,790 |
| 128 | **total** | 1,491,854 | 27,790 |
| 192 | keypair | 468,166 | 30,922 |
| 192 | encaps | 950,638 | 30,922 |
| 192 | decaps | 1,773,770 | 30,922 |
| 192 | **total** | 3,192,574 | 30,922 |
| 256 | keypair | 737,070 | 37,976 |
| 256 | encaps | 1,499,346 | 37,976 |
| 256 | decaps | 2,902,352 | 37,976 |
| 256 | **total** | 5,138,768 | 37,976 |

## 2. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute | 1,152,874 | 35.51 |
| vclmul_leaf | 697,026 | 21.47 |
| keccak_inc_absorb | 298,128 | 9.18 |
| kmul_18 | 252,048 | 7.76 |
| keccak_inc_squeeze | 203,516 | 6.27 |
| kmul_17 | 84,630 | 2.61 |
| rvv_xor3_u64 | 74,406 | 2.29 |
| vect_compare | 62,867 | 1.94 |
| gf_reduce | 53,856 | 1.66 |
| memset | 47,353 | 1.46 |
| rvv_xor_u64 | 43,308 | 1.33 |
| reduce | 37,056 | 1.14 |
| reed_solomon_encode | 23,814 | 0.73 |
| kmul_35 | 22,620 | 0.70 |
| compute_error_values_zvbc | 19,471 | 0.60 |
| rvv_copy_u64 | 16,596 | 0.51 |
| support_contains_value | 16,002 | 0.49 |
| vect_generate_random_support2 | 15,198 | 0.47 |
| gf_mul | 12,782 | 0.39 |
| ?? ??:0 | 9545 | 0.29 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| keccak_inc_absorb at fips202.c:438 | 196,658 | 6.27 |
| keccak_inc_squeeze at fips202.c:505 | 139,520 | 4.45 |
| keccak_inc_absorb at fips202.c:435 | 53,920 | 1.72 |
| vect_compare at vector.c:282 | 35,912 | 1.14 |
| vclmul_leaf at gf2x_vclmul.c:218 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:226 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:234 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:242 | 33,432 | 1.07 |
| vclmul_leaf at gf2x_vclmul.c:215 | 25,464 | 0.81 |
| gf_reduce at gf.c:80 | 25,344 | 0.81 |
| gf_reduce at gf.c:82 | 25,344 | 0.81 |
| vclmul_leaf at gf2x_vclmul.c:185 (discriminator 1) | 23,904 | 0.76 |
| KeccakF1600_StatePermute at fips202.c:110 | 23,268 | 0.74 |
| KeccakF1600_StatePermute at fips202.c:114 | 23,268 | 0.74 |
| KeccakF1600_StatePermute at fips202.c:208 | 23,268 | 0.74 |
| KeccakF1600_StatePermute at fips202.c:134 | 22,437 | 0.72 |
| KeccakF1600_StatePermute at fips202.c:229 | 22,437 | 0.72 |
| vclmul_leaf at gf2x_vclmul.c:193 | 21,090 | 0.67 |
| memset at mem.c:85 | 20,742 | 0.66 |
| KeccakF1600_StatePermute at fips202.c:113 | 19,944 | 0.64 |

## 3. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

> _No data rows._

### VLEN Sweep — HQC-192

> _No data rows._

### VLEN Sweep — HQC-256

> _No data rows._

## 4. Compiler Comparison

### HQC-128

> _No data available._

### HQC-192

> _No data available._

### HQC-256

> _No data available._

## 5. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

