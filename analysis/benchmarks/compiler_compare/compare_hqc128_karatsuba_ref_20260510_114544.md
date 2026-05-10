# Compiler Comparison (HQC-128 / karatsuba_ref)

| Compiler | Opt | Status | Median Cycles | Median Instret | .text bytes | Error |
|---|---:|---|---:|---:|---:|---|
| clang | -O2 | OK | 18578007 | 18578008 | 4317 |  |
| clang | -O3 | OK | 18578006 | 18578007 | 4710 |  |
| clang | -Os | OK | 18973668 | 18973668 | 2316 |  |
| gcc | -O2 | OK | 19717046 | 19717046 | 1814 |  |
| gcc | -O3 | OK | 18635851 | 18635851 | 5374 |  |
| gcc | -Os | OK | 19939973 | 19939973 | 1750 |  |

Best cycles (clang): 18578006

Best cycles (gcc): 18635851
