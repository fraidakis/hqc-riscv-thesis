# Compiler Comparison (HQC-128 / fafft_zbc)

| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |
|---|---:|---|---:|---:|---:|---|
| clang | -O2 | OK | 198455 | 198455 | 1744 |  |
| clang | -O3 | OK | 198455 | 198455 | 1738 |  |
| clang | -Os | OK | 198456 | 198456 | 1738 |  |
| gcc | -O2 | FAIL |  |  |  | build_failed |
| gcc | -O3 | FAIL |  |  |  | build_failed |
| gcc | -Os | FAIL |  |  |  | build_failed |

Best cycles (clang): 198455

Best cycles (gcc): 
