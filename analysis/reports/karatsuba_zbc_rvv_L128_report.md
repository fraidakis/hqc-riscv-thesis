# Full Analysis Report

- Variant: karatsuba_zbc_rvv
- Level: 128
- Date: 2026-05-14 20:18:30

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 286173 | 2892 |
| 192 | vect_mul | 873410 | 2924 |
| 256 | vect_mul | 1922492 | 2914 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 426449 | 38710 |  |
| 128 | encaps | 878608 | 38710 |  |
| 128 | decaps | 1795968 | 38710 |  |
| 192 | keypair | 1126534 | 39742 |  |
| 192 | encaps | 2328680 | 39742 |  |
| 192 | decaps | 4232479 | 39742 |  |
| 256 | keypair | 2322245 | 46740 |  |
| 256 | encaps | 4765727 | 46740 |  |
| 256 | decaps | 8508365 | 46740 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1326 | 106 | 1220 | 144 | 13368125 | 6521893 | 1971567 | 1159058 |
| 192 | 3008 | 294 | 2714 | 726 | 31547768 | 17878399 | 4439809 | 2851896 |
| 256 | 6908 | 1167 | 5741 | 3227 | 60566967 | 35227574 | 8396373 | 5386242 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1352 | 1360 | 8549 | 12075 | 134 | 189 |
| 192 | 1485 | 1386 | 9183 | 13269 | 144 | 208 |
| 256 | 1481 | 1363 | 9163 | 13162 | 144 | 206 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
No data found: /tmp/tmp.XMSGSBMvaC

### Line-Level Hotspots (Top 20 Rows)
No data found: /tmp/tmp.Q7sOAvzC0y

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 766672 | 15.94 |  |
| addi | 615173 | 12.79 |  |
| xor | 470770 | 9.79 |  |
| bnez | 420821 | 8.75 |  |
| sd | 218129 | 4.53 |  |
| bne | 206382 | 4.29 |  |
| add | 183672 | 3.82 |  |
| sb | 137793 | 2.86 |  |
| clmul | 119322 | 2.48 |  |
| clmulh | 114324 | 2.38 |  |
| lh | 110766 | 2.30 |  |
| slli | 103553 | 2.15 |  |
| rori | 98832 | 2.05 |  |
| mv | 92882 | 1.93 |  |
| vsetivli | 89171 | 1.85 |  |
| bltu | 86498 | 1.80 |  |
| andn | 85375 | 1.77 |  |
| vle64.v | 74382 | 1.55 |  |
| sh | 62575 | 1.30 |  |
| vxor.vv | 62071 | 1.29 |  |

## 4. VLEN Scaling Analysis

### VLEN Instruction Scaling (HQC-128 vect_mul)
*Note: Tested directly via Spike utilizing the `zvl<VLEN>b` ISA flags.*

| Variant / Width | vect_mul instructions | Delta vs Scalar |
| --- | --- | --- |
| karatsuba_zbc (scalar baseline) | 296,703 | baseline |
| karatsuba_zbc_rvv (VLEN=256) | 286,173 | -3.55% |
| karatsuba_zbc_rvv (VLEN=512) | 271,589 | -8.46% |
| karatsuba_zbc_rvv (VLEN=1024) | 275,105 | -7.28% |

**Architectural Note:** At VLEN=256, `LMUL=m4` processes exactly 4 elements per iteration, matching our 4-way scalar unroll, meaning vector setup overhead cancels out loop-control savings. At VLEN=512, the overhead is amortized across 8 elements per iteration, resulting in the peak performance gain for this specific algorithm. VLEN=1024 sees slight degradation likely due to register-group pressure.

