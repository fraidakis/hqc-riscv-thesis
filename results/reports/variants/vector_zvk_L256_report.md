# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 256 |
| Generated | 2026-07-01 11:48:46 |
| Commit | `cc8054d` |

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
| KeccakF1600_StatePermute | 1,752,202 | 34.04 |
| vclmul_leaf | 1,101,198 | 21.39 |
| keccak_inc_absorb | 473,564 | 9.20 |
| kmul_28 | 340,380 | 6.61 |
| keccak_inc_squeeze | 300,162 | 5.83 |
| kmul_29 | 202,752 | 3.94 |
| gf_reduce | 126,038 | 2.45 |
| memset | 112,827 | 2.19 |
| vect_compare | 100,968 | 1.96 |
| rvv_xor3_u64 | 98,082 | 1.91 |
| compute_error_values_zvbc | 92,328 | 1.79 |
| reduce | 59,502 | 1.16 |
| reed_solomon_encode | 56,680 | 1.10 |
| rvv_xor_u64 | 53,406 | 1.04 |
| gf_mul | 31,724 | 0.62 |
| support_contains_value | 23,697 | 0.46 |
| rvv_copy_u64 | 20,472 | 0.40 |
| vect_generate_random_support2 | 19,822 | 0.39 |
| expand_and_sum | 15,033 | 0.29 |
| ?? ??:0 | 14,471 | 0.28 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| keccak_inc_absorb at fips202.c:438 | 315,953 | 6.49 |
| keccak_inc_squeeze at fips202.c:505 | 212,416 | 4.36 |
| keccak_inc_absorb at fips202.c:435 | 86,615 | 1.78 |
| gf_reduce at gf.c:80 | 59,312 | 1.22 |
| gf_reduce at gf.c:82 | 59,312 | 1.22 |
| vect_compare at vector.c:282 | 57,684 | 1.18 |
| vclmul_leaf at gf2x_vclmul.c:218 | 52,488 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:226 | 52,488 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:234 | 52,488 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:242 | 52,488 | 1.08 |
| memset at mem.c:85 | 52,384 | 1.08 |
| vclmul_leaf at gf2x_vclmul.c:215 | 39,366 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:110 | 35,364 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:114 | 35,364 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:208 | 35,364 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:134 | 34,101 | 0.70 |
| KeccakF1600_StatePermute at fips202.c:229 | 34,101 | 0.70 |
| vclmul_leaf at gf2x_vclmul.c:193 | 30,618 | 0.63 |
| KeccakF1600_StatePermute at fips202.c:113 | 30,312 | 0.62 |
| KeccakF1600_StatePermute at fips202.c:111 | 30,312 | 0.62 |

## 3. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 1,491,853 |
| total | 512 | 1,474,358 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 3,192,573 |
| total | 512 | 3,136,500 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 5,138,767 |

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

