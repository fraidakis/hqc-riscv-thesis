# Compiler Comparison (HQC-128 / fafft_zvbc)

| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |
|---|---:|---|---:|---:|---:|---|
| clang | -O2 | OK | 2083765 | 2083765 | 66020 |  |
| clang | -O3 | OK | 1731112 | 1731112 | 96032 |  |
| clang | -Os | OK | 2516200 | 2516200 | 62462 |  |
| gcc | -O2 | FAIL |  |  |  | build_failed |
| gcc | -O3 | FAIL |  |  |  | build_failed |
| gcc | -Os | FAIL |  |  |  | build_failed |

Best cycles (clang): 1731112

Best cycles (gcc): 
