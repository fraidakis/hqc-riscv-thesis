# Full Analysis Report

- Variant: karatsuba_zvbc
- Level: 128
- Date: 2026-05-17 12:51:46

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 72644 | 10368 |
| 192 | vect_mul | 219338 | 11830 |
| 256 | vect_mul | 326260 | 13724 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 205093 | 44442 |  |
| 128 | encaps | 435089 | 44442 |  |
| 128 | decaps | 900906 | 44442 |  |
| 192 | keypair | 455602 | 47238 |  |
| 192 | encaps | 981874 | 47238 |  |
| 192 | decaps | 1851145 | 47238 |  |
| 256 | keypair | 697402 | 56140 |  |
| 256 | encaps | 1510311 | 56140 |  |
| 256 | decaps | 3045518 | 56140 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1307 | 104 | 1203 | 128 | 13084231 | 7469807 | 2062999 | 1247106 |
| 192 | 2988 | 258 | 2730 | 687 | 30773162 | 20705289 | 4643194 | 3184910 |
| 256 | 6948 | 1073 | 5875 | 3264 | 60449977 | 43376288 | 9113552 | 6646886 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 2243 | 2123 | 46491 | 38241 | 727 | 598 |
| 192 | 2646 | 2433 | 59015 | 49841 | 923 | 779 |
| 256 | 2642 | 2318 | 61865 | 45190 | 967 | 707 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute | 521424 | 26.53 |  |
| vclmul_leaf | 258732 | 13.16 |  |
| keccak_inc_absorb | 141116 | 7.18 |  |
| memcpy | 137922 | 7.02 |  |
| gf_reduce | 105600 | 5.37 |  |
| keccak_inc_squeeze | 96430 | 4.91 |  |
| compare_u32 | 83250 | 4.23 |  |
| vect_generate_random_support2 | 79260 | 4.03 |  |
| vect_generate_random_support1 | 49869 | 2.54 |  |
| kmul_18 | 49368 | 2.51 |  |
| kmul_35 | 49236 | 2.50 |  |
| kmul_17 | 39294 | 2.00 |  |
| memset | 33045 | 1.68 |  |
| vect_compare | 31058 | 1.58 |  |
| kmul_69 | 29520 | 1.50 |  |
| kmul_70 | 24048 | 1.22 |  |
| gf_mul | 23794 | 1.21 |  |
| reduce | 19974 | 1.02 |  |
| kmul_277 | 19752 | 1.00 |  |
| kmul_139 | 19692 | 1.00 |  |

### Line-Level Hotspots (Top 20 Rows)
| function | instructions | percentage |  |
| --- | --- | --- | --- |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:148 | 194256 | 9.88 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:143 | 170400 | 8.67 |  |
| compare_u32 at vector.c:43 | 83250 | 4.23 |  |
| memcpy at mem.c:21 | 82652 | 4.20 |  |
| memcpy at mem.c:22 | 54764 | 2.79 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:294 | 51696 | 2.63 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:? | 45156 | 2.30 |  |
| gf_reduce at gf.c:106 | 44712 | 2.27 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:259 | 43067 | 2.19 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:261 | 42875 | 2.18 |  |
| keccak_inc_absorb at fips202_zvbb_gather.c:260 | 42875 | 2.18 |  |
| keccak_inc_squeeze at fips202_zvbb_gather.c:293 | 34608 | 1.76 |  |
| vect_generate_random_support2 at vector.c:136 | 34188 | 1.74 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:145 | 34080 | 1.73 |  |
| gf_reduce at gf.c:105 | 33534 | 1.71 |  |
| vect_generate_random_support2 at vector.c:137 | 33300 | 1.69 |  |
| vect_generate_random_support1 at vector.c:106 | 32175 | 1.64 |  |
| KeccakF1600_StatePermute at fips202_zvbb_gather.c:178 | 27264 | 1.39 |  |
| memset at mem.c:60 | 19730 | 1.00 |  |
| vect_compare at vector.c:250 | 17732 | 0.90 |  |

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 620366 | 22.48 |  |
| bnez | 395196 | 14.32 |  |
| addi | 270978 | 9.82 |  |
| xor | 261664 | 9.48 |  |
| sd | 161874 | 5.87 |  |
| add | 103734 | 3.76 |  |
| bne | 91996 | 3.33 |  |
| slli | 77499 | 2.81 |  |
| vle64.v | 65324 | 2.37 |  |
| lbu | 48736 | 1.77 |  |
| sb | 47835 | 1.73 |  |
| vxor.vv | 46837 | 1.70 |  |
| vse64.v | 46056 | 1.67 |  |
| or | 43277 | 1.57 |  |
| subw | 34146 | 1.24 |  |
| andi | 33443 | 1.21 |  |
| lw | 24081 | 0.87 |  |
| vsetivli | 21707 | 0.79 |  |
| zext.b | 19956 | 0.72 |  |
| csrr | 19568 | 0.71 |  |

