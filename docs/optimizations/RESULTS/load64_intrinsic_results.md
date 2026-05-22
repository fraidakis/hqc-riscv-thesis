### Native 64-bit Unaligned Intrinsic Load Optimization Results
- **Changes applied:** Replaced explicit loop-based byte shifts with __builtin_memcpy(&r, x, sizeof(r)) inside load64_le and store64_le within fips202_zvbb_x2.c.
- **Why it works:** The previous standard memcpy attempt caused a regression because the build uses -ffreestanding with a custom mem.c providing a real, RVV-vectorized memcpy symbol—forcing a slow function call. Using the compiler intrinsic completely bypasses the custom mem.c function dispatch. At -O3 on little-endian rv64 without strict-alignment constraints, Clang lowers this fixed-size expression directly to a single native unaligned 'ld' or 'sd' instruction, removing 8 iterations of scalar shifting and bitwise OR chains.

| Security Level | Operation | Baseline instret | After Intrinsic Fix | Delta instret |
| :--- | :--- | :--- | :--- | :--- |
| **HQC-128** | keypair | 119,819 | 118,245 | -1,574 |
| **HQC-128** | encaps | 252,724 | 250,310 | -2,414 |
| **HQC-128** | decaps | 525,765 | 521,613 | -4,152 |
| **HQC-128** | total KEM | 898,308 | 890,168 | -8,140 |
| **HQC-192** | total KEM | 2,091,043 | 2,075,628 | -15,415 |
| **HQC-256** | total KEM | 3,296,601 | 3,271,977 | -24,624 |
| **Combined** | All Levels | 6,285,952 | 6,237,773 | -48,179 |

- **Verification:** 300/300 Known Answer Tests passed successfully.
