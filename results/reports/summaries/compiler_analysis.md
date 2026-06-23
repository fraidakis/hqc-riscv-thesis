# Compiler Showdown Analysis

Derived from hardware compiler compare CSVs under results/hardware/benchmarks/compiler_compare.
Scanned CSV files: 3

| Variant | Level | Best Clang | Best GCC | Winner | Margin |
|---|---:|---|---|---|---:|
| scalar_ref | 128 | -O3 (82,540,770) | -O3 (70,994,731) | GCC | 16.3% |
| scalar_ref | 192 | -O2 (249,443,989) | -O3 (214,228,777) | GCC | 16.4% |
| scalar_ref | 256 | -O3 (611,456,033) | -O3 (533,883,002) | GCC | 14.5% |
| scalar_zb | 128 | -O3 (2,158,898) | -O3 (2,032,805) | GCC | 6.2% |
| scalar_zb | 192 | -O2 (4,897,752) | -O3 (4,621,302) | GCC | 6.0% |
| scalar_zb | 256 | -O3 (11,360,831) | -O3 (8,958,436) | GCC | 26.8% |
| vector_rvv | 128 | -O3 (1,660,813) | -O3 (1,673,598) | Clang | 0.8% |
| vector_rvv | 192 | -O3 (3,954,544) | -O3 (3,975,160) | Clang | 0.5% |
| vector_rvv | 256 | -O3 (9,366,365) | -O3 (7,558,296) | GCC | 23.9% |
