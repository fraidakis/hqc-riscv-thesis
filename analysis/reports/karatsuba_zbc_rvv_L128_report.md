# Full Analysis Report

- Variant: karatsuba_zbc_rvv
- Level: 128
- Date: 2026-05-15 18:30:21

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 270021 | 2892 |
| 192 | vect_mul | 823102 | 2924 |
| 256 | vect_mul | 1857509 | 2914 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 412915 | 38710 |  |
| 128 | encaps | 849252 | 38710 |  |
| 128 | decaps | 1745308 | 38710 |  |
| 192 | keypair | 1076893 | 39742 |  |
| 192 | encaps | 2224468 | 39742 |  |
| 192 | decaps | 4072134 | 39742 |  |
| 256 | keypair | 2256676 | 46740 |  |
| 256 | encaps | 4626491 | 46740 |  |
| 256 | decaps | 8295093 | 46740 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1293 | 88 | 1205 | 115 | 13387997 | 6525701 | 1977327 | 1162866 |
| 192 | 2965 | 309 | 2656 | 672 | 31557336 | 17884479 | 4446213 | 2857976 |
| 256 | 7012 | 1137 | 5875 | 3314 | 60547383 | 35227574 | 8393925 | 5386242 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1352 | 1360 | 8549 | 12075 | 134 | 189 |
| 192 | 1485 | 1386 | 9183 | 13269 | 144 | 208 |
| 256 | 1481 | 1363 | 9163 | 13162 | 144 | 206 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
No data found: /tmp/tmp.Xj6RiRNAGf

### Line-Level Hotspots (Top 20 Rows)
No data found: /tmp/tmp.OS5iO3C1Fd

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 767680 | 16.16 |  |
| addi | 615173 | 12.95 |  |
| xor | 470770 | 9.91 |  |
| bnez | 416663 | 8.77 |  |
| sd | 218129 | 4.59 |  |
| bne | 206382 | 4.34 |  |
| add | 168168 | 3.54 |  |
| sb | 137793 | 2.90 |  |
| clmul | 119322 | 2.51 |  |
| clmulh | 114324 | 2.41 |  |
| lh | 110766 | 2.33 |  |
| rori | 98832 | 2.08 |  |
| slli | 98387 | 2.07 |  |
| mv | 92882 | 1.96 |  |
| vsetivli | 89171 | 1.88 |  |
| bltu | 86498 | 1.82 |  |
| andn | 85375 | 1.80 |  |
| sh | 62575 | 1.32 |  |
| vle64.v | 61458 | 1.29 |  |
| subw | 54766 | 1.15 |  |

