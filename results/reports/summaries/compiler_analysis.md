# Compiler Showdown Analysis

Derived from hardware compiler compare CSVs under results/hardware/benchmarks/compiler_compare.
Scanned CSV files: 3

| Variant | Level | Best Clang | Best GCC | Winner | Margin |
|---|---:|---|---|---|---:|
| scalar_ref | 128 | -O3 (82,540,770) | -O3 (70,994,731) | GCC | 16.3% |
| scalar_ref | 192 | -O2 (249,443,989) | -O3 (214,228,777) | GCC | 16.4% |
| scalar_ref | 256 | -O3 (611,456,033) | -O3 (533,883,002) | GCC | 14.5% |
| scalar_zb | 128 | -O2 (2,151,335) | -O3 (1,902,665) | GCC | 13.1% |
| scalar_zb | 192 | -O2 (4,874,759) | -O3 (4,223,873) | GCC | 15.4% |
| scalar_zb | 256 | -O3 (11,144,875) | -O3 (8,106,449) | GCC | 37.5% |
| vector_rvv | 128 | -O2 (1,712,970) | -O3 (1,585,434) | GCC | 8.0% |
| vector_rvv | 192 | -O2 (4,039,884) | -O3 (3,592,013) | GCC | 12.5% |
| vector_rvv | 256 | -O2 (9,756,962) | -O3 (6,848,116) | GCC | 42.5% |
