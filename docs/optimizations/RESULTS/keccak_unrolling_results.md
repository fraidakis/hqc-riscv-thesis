### Keccak Unrolling & Vector-Immediate Optimization Results
- **Changes applied:** Unrolled the Pi+Rho and Theta D-application loops into explicit named statements inside `KeccakF1600_StatePermute_x2`, enabling native `vror.vi` instruction emission and eliminating 1,200 loop-carried scalar loads.
- **Verification:** 300/300 Known Answer Tests passed successfully.

| Security Level | Baseline instret | After Optimization | Delta instret |
| :--- | :--- | :--- | :--- |
| **HQC-128** | 926,798 | 913,382 | -13,416 (-1.45%) |
| **HQC-192** | 2,147,193 | 2,121,408 | -25,785 (-1.20%) |
| **HQC-256** | 3,385,953 | 3,345,323 | -40,630 (-1.20%) |
