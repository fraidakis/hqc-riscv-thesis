# Compiler Comparison (HQC-128 / karatsuba_zvbc)

| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |
|---|---:|---|---:|---:|---:|---|
| clang | -O2 | OK | 425352 | 425352 | 2179 |  |
| clang | -O3 | OK | 425352 | 425352 | 2173 |  |
| clang | -Os | OK | 426970 | 426970 | 2171 |  |
| gcc | -O2 | OK | 420556 | 420556 | 2248 |  |
| gcc | -O3 | OK | 419360 | 419360 | 5660 |  |
| gcc | -Os | OK | 626963 | 626965 | 2168 |  |

Best cycles (clang): 425352

Best cycles (gcc): 419360
