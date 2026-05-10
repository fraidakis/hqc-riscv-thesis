# Compiler Comparison (HQC-128 / karatsuba_zvbc)

| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |
|---|---:|---|---:|---:|---:|---|
| clang | -O2 | OK | 425352 | 425352 | 2179 |  |
| clang | -O3 | OK | 425352 | 425352 | 2173 |  |
| clang | -Os | OK | 426970 | 426970 | 2171 |  |
| gcc | -O2 | FAIL |  |  |  | build_failed |
| gcc | -O3 | FAIL |  |  |  | build_failed |
| gcc | -Os | FAIL |  |  |  | build_failed |

Best cycles (clang): 425352

Best cycles (gcc): 
