# HQC-RISC-V Analysis: `vector_zvk`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
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
| KeccakF1600_StatePermute | 591,004 | 39.27 |
| vclmul_leaf | 232,710 | 15.46 |
| keccak_inc_absorb | 151,064 | 10.04 |
| keccak_inc_squeeze | 103,803 | 6.90 |
| kmul_18 | 62,304 | 4.14 |
| kmul_17 | 51,282 | 3.41 |
| memset | 46,358 | 3.08 |
| gf_reduce | 40,018 | 2.66 |
| vect_compare | 31,052 | 2.06 |
| rvv_xor3_u64 | 23,982 | 1.59 |
| reduce | 18,312 | 1.22 |
| compute_error_values_zvbc | 15,418 | 1.02 |
| reed_solomon_encode | 14,950 | 0.99 |
| rvv_xor_u64 | 13,998 | 0.93 |
| vect_generate_random_support2 | 10,050 | 0.67 |
| gf_mul | 8952 | 0.59 |
| support_contains_value | 8688 | 0.58 |
| kmul_35 | 7128 | 0.47 |
| vect_write_support_to_vector | 5868 | 0.39 |
| rvv_copy_u64 | 5430 | 0.36 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| keccak_inc_absorb at fips202.c:438 | 94,325 | 6.43 |
| keccak_inc_squeeze at fips202.c:505 | 68,928 | 4.70 |
| keccak_inc_absorb at fips202.c:435 | 25,865 | 1.76 |
| memset at mem.c:85 | 21,480 | 1.46 |
| gf_reduce at gf.c:80 | 18,832 | 1.28 |
| gf_reduce at gf.c:82 | 18,832 | 1.28 |
| vect_compare at vector.c:282 | 17,732 | 1.21 |
| KeccakF1600_StatePermute at fips202.c:110 | 11,928 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:114 | 11,928 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:208 | 11,928 | 0.81 |
| KeccakF1600_StatePermute at fips202.c:134 | 11,502 | 0.78 |
| KeccakF1600_StatePermute at fips202.c:229 | 11,502 | 0.78 |
| vclmul_leaf at gf2x_vclmul.c:218 | 10,776 | 0.73 |
| vclmul_leaf at gf2x_vclmul.c:226 | 10,776 | 0.73 |
| vclmul_leaf at gf2x_vclmul.c:234 | 10,776 | 0.73 |
| vclmul_leaf at gf2x_vclmul.c:242 | 10,776 | 0.73 |
| memset at mem.c:84 (discriminator 3) | 10,723 | 0.73 |
| memset at mem.c:84 (discriminator 1) | 10,723 | 0.73 |
| KeccakF1600_StatePermute at fips202.c:113 | 10,224 | 0.70 |
| KeccakF1600_StatePermute at fips202.c:111 | 10,224 | 0.70 |

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

