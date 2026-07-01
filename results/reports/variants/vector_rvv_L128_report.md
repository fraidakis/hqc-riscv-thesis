# HQC-RISC-V Analysis: `vector_rvv`

| Property | Value |
| --- | --- |
| Track | vector |
| Security levels | 128 · 192 · 256 |
| Report level (profiles) | 128 |
| Generated | 2026-07-01 11:48:45 |
| Commit | `cc8054d` |

---

## 1. Cycle Counts

### Polymul Isolated

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | vect_mul | 148,479 | 9606 |
| 192 | vect_mul | 449,808 | 10,796 |
| 256 | vect_mul | 1,043,726 | 13,834 |

### Full KEM Suite

| Level | Operation | Instructions | Code (B) |
| ---: | --- | ---: | ---: |
| 128 | keypair | 294,501 | 29,776 |
| 128 | encaps | 589,121 | 29,776 |
| 128 | decaps | 1,139,356 | 29,776 |
| 128 | **total** | 2,022,978 | 29,776 |
| 192 | keypair | 710,531 | 34,124 |
| 192 | encaps | 1,435,368 | 34,124 |
| 192 | decaps | 2,579,762 | 34,124 |
| 192 | **total** | 4,725,661 | 34,124 |
| 256 | keypair | 1,461,824 | 42,402 |
| 256 | encaps | 2,948,854 | 42,402 |
| 256 | decaps | 5,274,100 | 42,402 |
| 256 | **total** | 9,684,778 | 42,402 |

## 2. Hotspots

### Function-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| KeccakF1600_StatePermute | 591,004 | 29.02 |
| clmul_leaf_n9 | 420,324 | 20.64 |
| keccak_inc_absorb | 151,064 | 7.42 |
| __riscv_clmul_64 | 118,563 | 5.82 |
| __riscv_clmulh_64 | 115,116 | 5.65 |
| keccak_inc_squeeze | 103,803 | 5.10 |
| gf_reduce | 72,063 | 3.54 |
| clmul_leaf_n8 | 60,828 | 2.99 |
| kmul_18 | 58,608 | 2.88 |
| kmul_17 | 47,730 | 2.34 |
| memset | 44,514 | 2.19 |
| vect_compare | 31,052 | 1.52 |
| rvv_xor3_u64 | 23,952 | 1.18 |
| compute_error_values | 18,405 | 0.90 |
| reduce | 18,312 | 0.90 |
| gf_mul | 16,952 | 0.83 |
| reed_solomon_encode | 14,950 | 0.73 |
| compute_elp | 14,765 | 0.72 |
| rvv_xor_u64 | 13,992 | 0.69 |
| vect_generate_random_support2 | 10,050 | 0.49 |

### Line-Level Hotspots (Top 20 Rows)

| Function | Instructions | % Total |
| --- | ---: | ---: |
| __riscv_clmulh_64 at riscv_bitmanip.h:221 | 216,444 | 10.88 |
| __riscv_clmul_64 at riscv_bitmanip.h:214 | 195,483 | 9.83 |
| keccak_inc_absorb at fips202.c:438 | 94,325 | 4.74 |
| keccak_inc_squeeze at fips202.c:505 | 68,928 | 3.47 |
| gf_reduce at gf.c:80 | 33,912 | 1.71 |
| gf_reduce at gf.c:82 | 33,912 | 1.71 |
| keccak_inc_absorb at fips202.c:435 | 25,865 | 1.30 |
| memset at mem.c:85 | 20,508 | 1.03 |
| vect_compare at vector.c:282 | 17,732 | 0.89 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:113 | 15,048 | 0.76 |
| kmul_18 at karatsuba_rvv_specialized_gen.h:123 | 13,200 | 0.66 |
| kmul_17 at karatsuba_rvv_specialized_gen.h:51 | 12,654 | 0.64 |
| KeccakF1600_StatePermute at fips202.c:110 | 11,928 | 0.60 |
| KeccakF1600_StatePermute at fips202.c:114 | 11,928 | 0.60 |
| KeccakF1600_StatePermute at fips202.c:208 | 11,928 | 0.60 |
| KeccakF1600_StatePermute at fips202.c:134 | 11,502 | 0.58 |
| KeccakF1600_StatePermute at fips202.c:229 | 11,502 | 0.58 |
| kmul_17 at karatsuba_rvv_specialized_gen.h:61 | 10,434 | 0.52 |
| memset at mem.c:84 (discriminator 3) | 10,239 | 0.51 |
| memset at mem.c:84 (discriminator 1) | 10,239 | 0.51 |

## 3. VLEN Scaling Sweep

### VLEN Sweep — HQC-128

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 2,022,977 |
| total | 512 | 2,005,816 |
| total | 1024 | 1,999,752 |

### VLEN Sweep — HQC-192

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 4,725,660 |
| total | 512 | 4,669,939 |
| total | 1024 | 4,650,975 |

### VLEN Sweep — HQC-256

| Operation | VLEN | Instructions |
| --- | ---: | ---: |
| total | 256 | 9,684,777 |
| total | 512 | 9,594,709 |
| total | 1024 | 9,566,883 |

## 4. Compiler Comparison

### HQC-128

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | vector_rvv | OK | 2,667,282 | 531,235 |
| gcc | -O3 | vector_rvv | OK | 1,585,434 | 541,883 |
| clang | -O2 | vector_rvv | OK | 1,712,970 | 544,425 |
| clang | -O3 | vector_rvv | OK | 1,769,249 | 550,849 |

### HQC-192

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | vector_rvv | OK | 6,726,919 | 533,787 |
| gcc | -O3 | vector_rvv | OK | 3,592,013 | 546,763 |
| clang | -O2 | vector_rvv | OK | 4,039,884 | 547,601 |
| clang | -O3 | vector_rvv | OK | 4,145,892 | 554,665 |

### HQC-256

| Compiler | Opt | Variant | Status | cycles | Text (B) |
| --- | --- | --- | --- | ---: | ---: |
| gcc | -O2 | vector_rvv | OK | 15,911,458 | 540,867 |
| gcc | -O3 | vector_rvv | OK | 6,848,116 | 555,067 |
| clang | -O2 | vector_rvv | OK | 9,756,962 | 554,225 |
| clang | -O3 | vector_rvv | OK | 9,757,317 | 561,785 |

## 5. Cross-Variant Reports

The following reports aggregate data across all variants and are regenerated
each time a full analysis runs:

- Speedup summary: `results/reports/summaries/speedups_simulation.md`
- Compiler showdown: `results/reports/summaries/compiler_analysis.md`

