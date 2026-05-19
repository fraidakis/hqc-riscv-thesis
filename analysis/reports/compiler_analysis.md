# Compiler Showdown Analysis

Derived from compare_compilers CSVs under analysis/benchmarks/compiler_compare.
Scanned CSV files: 74

| Variant | Level | Best Clang | Best GCC | Winner | Margin |
|---|---:|---|---|---|---:|
| ref_scalar | 128 | -O2 (19,214,276) | -O3 (18,635,851) | GCC | 3.1% |
| ref_scalar | 192 | -O2 (58,433,896) | -O3 (56,690,930) | GCC | 3.1% |
| ref_scalar | 256 | -O2 (142,641,044) | -O3 (139,866,200) | GCC | 2.0% |
| ref_vector | 128 | -O2 (18,556,379) | -O2 (18,322,987) | GCC | 1.3% |
| ref_vector | 192 | -O2 (56,404,468) | -O2 (55,705,794) | GCC | 1.3% |
| ref_vector | 256 | -O2 (139,018,509) | -O2 (138,089,559) | GCC | 0.7% |
| karatsuba_zbc | 128 | -O2 (210,091) | -O3 (195,038) | GCC | 7.7% |
| karatsuba_zbc | 192 | -O2 (639,600) | -O3 (602,988) | GCC | 6.1% |
| karatsuba_zbc | 256 | -O2 (1,893,210) | -O3 (1,308,092) | GCC | 44.7% |
| karatsuba_rvv | 128 | -O2 (169,283) | -O2 (183,500) | Clang | 8.4% |
| karatsuba_rvv | 192 | -O2 (520,792) | -O2 (562,463) | Clang | 8.0% |
| karatsuba_rvv | 256 | -O2 (1,671,733) | -O2 (1,206,910) | GCC | 38.5% |
| karatsuba_zvbc | 128 | -O2 (100,231) | -O2 (135,275) | Clang | 35.0% |
| karatsuba_zvbc | 192 | -O2 (307,906) | -O2 (425,883) | Clang | 38.3% |
| karatsuba_zvbc | 256 | -O2 (485,437) | -O2 (712,042) | Clang | 46.7% |
