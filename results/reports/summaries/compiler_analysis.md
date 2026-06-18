# Compiler Showdown Analysis

Derived from compare_compilers CSVs under results/simulation/benchmarks/compiler_compare.
Scanned CSV files: 5

| Variant | Level | Best Clang | Best GCC | Winner | Margin |
|---|---:|---|---|---|---:|
| scalar_ref | 128 | -O3 (117,344,266) | -O3 (113,958,344) | GCC | 3.0% |
| scalar_ref | 192 | -O3 (354,302,605) | -O3 (344,003,671) | GCC | 3.0% |
| scalar_ref | 256 | -O3 (861,974,446) | -O3 (845,613,464) | GCC | 1.9% |
| scalar_zb | 128 | -O2 (3,449,795) | -O2 (3,277,538) | GCC | 5.3% |
| scalar_zb | 192 | -O2 (7,811,056) | -O2 (7,448,827) | GCC | 4.9% |
| scalar_zb | 256 | -O2 (17,551,791) | -O2 (14,317,378) | GCC | 22.6% |
| vector_ref | 128 | -O2 (112,369,640) | -O3 (110,639,142) | GCC | 1.6% |
| vector_ref | 192 | -O2 (339,959,120) | -O3 (334,660,464) | GCC | 1.6% |
| vector_ref | 256 | -O2 (838,185,076) | -O3 (828,402,171) | GCC | 1.2% |
| vector_rvv | 128 | -O2 (1,975,424) | -O2 (2,029,880) | Clang | 2.8% |
| vector_rvv | 192 | -O2 (4,730,302) | -O2 (4,792,531) | Clang | 1.3% |
| vector_rvv | 256 | -O2 (12,135,502) | -O2 (9,732,294) | GCC | 24.7% |
| vector_zvk | 128 | -O2 (702,755) | N/A | Clang | N/A |
| vector_zvk | 192 | -O2 (1,653,804) | N/A | Clang | N/A |
| vector_zvk | 256 | -O2 (2,544,133) | N/A | Clang | N/A |
