# Compiler Comparison (HQC-128 / fafft_zbc)

| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |
|---|---:|---|---:|---:|---:|---|
| clang | -O2 | OK | 198455 | 198455 | 1744 |  |
| clang | -O3 | OK | 198455 | 198455 | 1738 |  |
| clang | -Os | OK | 198456 | 198456 | 1738 |  |
| gcc | -O2 | OK | 230319 | 230319 | 1676 |  |
| gcc | -O3 | OK | 226439 | 226439 | 1838 |  |
| gcc | -Os | OK | 240040 | 240042 | 1602 |  |

Best cycles (clang): 198455

Best cycles (gcc): 226439
