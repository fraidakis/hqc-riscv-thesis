# Full Analysis Report

- Variant: karatsuba_rvv
- Level: 128
- Date: 2026-05-16 17:18:13

## 1. Cycle Counts

### Polymul Isolated
| level | op | instructions | codesize |
| --- | --- | --- | --- |
| 128 | vect_mul | 270021 | 2932 |
| 192 | vect_mul | 823102 | 2964 |
| 256 | vect_mul | 1857509 | 2954 |

### Full KEM Suite
| level | variant | op | instructions | codesize |
| --- | --- | --- | --- | --- |
| 128 | keypair | 411289 | 41634 |  |
| 128 | encaps | 846696 | 41634 |  |
| 128 | decaps | 1740894 | 41634 |  |
| 192 | keypair | 1075457 | 42666 |  |
| 192 | encaps | 2222011 | 42666 |  |
| 192 | decaps | 4067779 | 42666 |  |
| 256 | keypair | 2254438 | 49664 |  |
| 256 | encaps | 4622790 | 49664 |  |
| 256 | decaps | 8288769 | 49664 |  |

## 2. Memory & Cache

### Cache Misses (Top 10 Rows)
| level | dc_misses | dc_read_misses | dc_write_misses | dc_writebacks | dc_bytes_read | dc_bytes_written | dc_read_accesses | dc_write_accesses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 1286 | 82 | 1204 | 109 | 13282215 | 6525647 | 1964099 | 1005438 |
| 192 | 3021 | 323 | 2698 | 740 | 31569024 | 17884479 | 4447674 | 2537306 |
| 256 | 6887 | 1122 | 5765 | 3203 | 60436777 | 35227520 | 8380094 | 4896482 |

### Memory Footprint
| level | load_instructions | store_instructions | load_bytes | store_bytes | est_cache_lines_load | est_cache_lines_store |
| --- | --- | --- | --- | --- | --- | --- |
| 128 | 1342 | 1356 | 8539 | 12225 | 134 | 192 |
| 192 | 1581 | 1507 | 10007 | 14433 | 157 | 226 |
| 256 | 1569 | 1466 | 9937 | 14126 | 156 | 221 |

## 3. Hotspots

### Function-Level Hotspots (Top 20 Rows)
No data found: /tmp/tmp.5xfIR5HZBg

### Line-Level Hotspots (Top 20 Rows)
No data found: /tmp/tmp.dal6OjyhHN

### Basic Block Histogram (Top 20)
| instruction | count | percentage |  |
| --- | --- | --- | --- |
| ld | 753636 | 16.92 |  |
| addi | 525188 | 11.79 |  |
| xor | 470770 | 10.57 |  |
| bnez | 403187 | 9.05 |  |
| sd | 218123 | 4.90 |  |
| add | 168464 | 3.78 |  |
| clmul | 119322 | 2.68 |  |
| bne | 116430 | 2.61 |  |
| clmulh | 114324 | 2.57 |  |
| lh | 110766 | 2.49 |  |
| rori | 98832 | 2.22 |  |
| slli | 98763 | 2.22 |  |
| mv | 92876 | 2.08 |  |
| vsetivli | 89141 | 2.00 |  |
| bltu | 86492 | 1.94 |  |
| andn | 85375 | 1.92 |  |
| sh | 62575 | 1.40 |  |
| vle64.v | 61458 | 1.38 |  |
| subw | 54754 | 1.23 |  |
| vxor.vv | 54313 | 1.22 |  |

