# Compiler Comparison (HQC-128 / karatsuba_zbc)

| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |
|---|---:|---|---:|---:|---:|---|
| clang | -O2 | OK | 697526 | 697526 | 1776 |  |
| clang | -O3 | OK | 697526 | 697526 | 1770 |  |
| clang | -Os | OK | 702501 | 702501 | 1766 |  |
| gcc | -O2 | OK | 689297 | 689297 | 1722 |  |
| gcc | -O3 | OK | 680659 | 680659 | 5656 |  |
| gcc | -Os | OK | 918105 | 918105 | 1682 |  |

Best cycles (clang): 697526

Best cycles (gcc): 680659
