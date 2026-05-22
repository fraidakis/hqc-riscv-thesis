# EXP-decode-rs-zvbc

**Status:** active  
**Track:** AP-3 (Decode hotspot vectorization)  
**Started:** 2026-05-22

## Hypothesis

`gf_reduce` and scalar GF(2^8) multiplications inside the Reed-Solomon decode phases
consume ~8.85% of total KEM instructions (HQC-128, `karatsuba_zvbc` baseline).  
Replacing the sequential scalar inner loops in Axis B (Berlekamp-Massey ELP update)
and Axis D (Forney error-value evaluation) with Zvbc lane-packed vector operations
will reduce that share significantly.

## Implementation

Source file: `reed_solomon_zvbc.c`

Axes vectorised relative to `src/decode/rvv/reed_solomon_rvv.c` (Axis A already done):

| Axis | Function              | Status    | Strategy |
|------|-----------------------|-----------|----------|
| A    | `compute_syndromes`   | inherited | Zvbc vclmul.vv lane-packing (existing) |
| B    | `compute_elp`         | **NEW**   | Vectorise three inner loops: sigma-SAXPY, X_sigma_p-blend, discrepancy dot-product |
| C    | `compute_roots`       | scalar    | Additive FFT (unchanged) |
| D    | `compute_error_values`| **NEW**   | Vectorise inv-power dot product and inner gf_mul batch |
| -    | `correct_errors`      | scalar    | Trivial XOR loop (unchanged) |

### Lane-Packing Strategy

- One GF(2^8) element (8-bit) placed in bits [7:0] of a 64-bit lane (upper 56 bits zero).
- `vclmul.vv` (SEW=64) computes the 14-bit carry-less product in bits [14:0].
- Inline two-pass Barrett-style reduction folds bits [14:8] back modulo
  `PARAM_GF_POLY = 0x11D` (x^8 + x^4 + x^3 + x^2 + 1) using vsrl/vand/vxor.
- LMUL=4, SEW=64: at VLEN=256 → vl=16 lanes.  Covers DELTA=15,16 in **1 pass**;
  DELTA=29 in **2 passes**.  VLA — works at any VLEN ≥ 64.

### Axis B — compute_elp vectorisation

Three inner loops per Berlekamp-Massey iteration are vectorised:

1. **Sigma SAXPY** (`sigma[1..n] ^= dd * X_sigma_p[1..n]`):
   vmv_v_x broadcast + vclmul.vv + inline reduction + vnsrl narrow-store.

2. **X_sigma_p blend** (`X_sigma_p[1..n] = mask12 ? sigma_copy[0..n-1] : X_sigma_p[0..n-1]`):
   Scalar-broadcast mask12 via vmv_v_x + bitwise AND/XOR blend (u16m1 SIMD).

3. **Discrepancy dot-product** (`d ^= sum_i gf_mul(sigma[i], syndromes[mu+1-i])`):
   vlse16 negative-stride load + vclmul.vv + inline reduction + vredxor.

### Axis D — compute_error_values vectorisation

Two inner structures are vectorised:

1. **Forney numerator dot-product** (`tmp1 = XOR_j  inv^j * z[j]`):
   Scalar precompute of `inv_pow[1..DELTA]` (unavoidable sequential chain), then
   vectorised dot-product over `inv_pow` vs `z[1..DELTA]` via vle16/vzext/vclmul/vredxor.

2. **Inner inverse-beta batch** (`inner_k = gf_mul(inverse, beta_j[(i+k)%DELTA])` for all k):
   Two contiguous-segment loads (wrap-around split), vclmul.vv + inline reduction,
   then vnsrl narrow-store into a scratch array; scalar product chain over
   `tmp2 = gf_mul(tmp2, 1 ^ inner_k[k])` remains sequential.

## Build (isolated)

```bash
# Activate the experimental decode source instead of production reed_solomon_rvv.c
cmake -S . -B build -DHQC_DECODE_IMPL=rs_zvbc_exp
make -C build -j$(nproc) kat_hqc128_karatsuba_zvbc kat_hqc192_karatsuba_zvbc kat_hqc256_karatsuba_zvbc
```

Or via the scripts:
```bash
source toolchain/toolchain.env
./scripts/test/run_kat.sh --mul karatsuba_zvbc --keccak opt --decode rs_zvbc_exp
./scripts/bench/run_benchmarks.sh --mode kem --mul karatsuba_zvbc --decode rs_zvbc_exp
```

## Expected Gains

| Level   | Baseline decaps instret | Target (estimate) |
|---------|------------------------|-------------------|
| HQC-128 | ~900 906               | ≤ 870 000         |
| HQC-192 | ~1 851 145             | ≤ 1 790 000       |
| HQC-256 | ~3 045 518             | ≤ 2 940 000       |
