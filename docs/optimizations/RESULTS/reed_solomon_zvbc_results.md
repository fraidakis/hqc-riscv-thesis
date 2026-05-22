### Reed-Solomon Decoder Zvbc Vectorization Results
- **Changes applied:** Vectorized Axes B (Berlekamp-Massey compute_elp) and D (compute_error_values) using Zvbc vclmul.vv lane-packing (1 element per 64-bit lane) and inline vector reduction loops.
- **Analysis:** Keypair and encaps are completely unchanged, confirming isolation. HQC-256 showed the highest performance scaling because its parameter profile (PARAM_DELTA=29) yields longer inner loop dimensions, fully amplifying vector instruction throughput.

| Security Level | Operation | Baseline instret | Optimized instret | Delta instret | Delta % |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **HQC-128** | decaps | 521,613 | 485,507 | -36,106 | -6.9% |
| **HQC-128** | total KEM | 890,168 | 854,062 | -36,106 | -4.0% |
| **HQC-192** | decaps | 1,150,818 | 1,110,836 | -39,982 | -3.5% |
| **HQC-192** | total KEM | 2,075,628 | 2,035,646 | -39,982 | -1.9% |
| **HQC-256** | decaps | 1,861,032 | 1,726,034 | -134,998 | -7.3% |
| **HQC-256** | total KEM | 3,271,977 | 3,136,979 | -134,998 | -4.1% |
