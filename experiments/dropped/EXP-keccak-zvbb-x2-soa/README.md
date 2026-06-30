# EXP-keccak-zvbb-x2-soa (Archived)

## Summary

Track C2: 2-way SoA (Structure-of-Arrays) vectorized Keccak-f[1600] backend
using Zvbb (vror.vx, vandn.vv) and LMUL=8 vrgather.vv for the Pi permutation.
Included both single-stream fips202 API and paired-stream x2 wrappers
(shake256_x2_*) used by HQC keygen fast path.

## Why Archived

Experimental evaluation on real hardware (BPI-F3, SpacemiT X60, VLEN=256)
demonstrated that vectorized Keccak-f[1600] in any form (LMUL=8 or LMUL=1 SoA)
cannot outperform the scalar Zbb implementation on in-order dual-issue cores:

- **LMUL=8 vrgather approach:** 8.1× slower than scalar (CPI=16.9 vs 0.67)
  due to serial vrgather.vv execution, vector register spills, and single-pipe
  vslidedown/vslide1up.
- **2-way SoA LMUL=1 approach:** 2.3× slower than two sequential scalar calls
  due to vl=2 providing insufficient data-level parallelism.

The scalar Zbb path (rori for ρ-rotations, andn for χ) remains the optimal
choice across all security levels on VLEN=256 hardware.

See thesis §7.1 (Επανασχεδιασμός Keccak / SHA-3) for full analysis.

## Previously Located At

- `src/keccak/fips202_zvbb_x2.c`

## Build (historical)

```bash
cmake -B build -DHQC_KECCAK_IMPL=zvbb_x2_soa \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake
```

Required Clang (uses `__attribute__((riscv_rvv_vector_bits(N)))`).

## Date Archived

2026-06-30
