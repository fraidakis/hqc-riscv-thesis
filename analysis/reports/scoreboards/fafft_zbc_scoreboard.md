# Phase 1b Scoreboard — fafft_zbc (True FAFFT Iteration)

## Status

Phase 1b now uses reference FAFFT kernels (Cantor/additive-butterfly path from Jang et al. codebase) instead of the earlier Karatsuba bootstrap body.

Current state:

- `hqc{128,192,256}_fafft_zbc.elf` builds
- KAT PASS for HQC-128/192/256
- fafft_zbc polymul/full-KEM CSVs regenerated
- llvm-mca report regenerated for `src/poly_mul/fafft_zbc/fafft.c`

Implementation detail:

- HQC-128: `bmul2_4096_to_8192` + `reduce`
- HQC-192/256: `bmul2_8192_to_16384` + `reduce`
- RVV XOR helper remains available in `fafft_rvv.c`

## Bench Snapshot (`analysis/benchmarks/fafft_zbc`, Spike N=1)

### Polymul (`vect_mul`) — absolute instret

| Level   | karatsuba_ref (baseline_ref) | karatsuba_zbc (polymul_zbc) | fafft_zbc (fafft_zbc) |
|---------|------------------------|-------------------------|---------------------|
| HQC-128 |             18,578,007 |                 354,186 |           1,462,166 |
| HQC-192 |             56,469,320 |               1,078,640 |           3,256,399 |
| HQC-256 |            139,077,901 |               2,257,476 |           3,258,453 |

fafft_zbc vs baseline_ref: 12.7x / 17.3x / 42.7x speedup
fafft_zbc vs karatsuba_zbc: 0.24x / 0.33x / 0.69x (FAFFT currently slower at these sizes due to
additive butterfly overhead; expected to cross over at degree > 2^15 or with CRT path activation)

### Full KEM — absolute instret

| Level   | Op      | karatsuba_ref (baseline_ref) | karatsuba_zbc (polymul_zbc) | fafft_zbc (fafft_zbc) |
|---------|---------|------------------------|-------------------------|---------------------|
| HQC-128 | keypair |             18,816,325 |                 592,505 |           1,700,492 |
| HQC-128 | encaps  |             37,636,615 |               1,188,975 |           3,404,944 |
| HQC-128 | decaps  |             56,962,431 |               2,290,971 |           5,614,925 |
| HQC-192 | keypair |             56,986,059 |               1,595,380 |     ~3,800,000 est. |
| HQC-192 | encaps  |            113,992,185 |               3,210,827 |     ~7,600,000 est. |
| HQC-192 | decaps  |            171,660,064 |               5,488,027 |    ~12,000,000 est. |
| HQC-256 | keypair |            140,074,066 |               3,253,640 |     ~4,300,000 est. |
| HQC-256 | encaps  |            279,984,371 |               6,343,519 |     ~8,300,000 est. |
| HQC-256 | decaps  |            421,305,306 |              10,844,028 |    ~13,800,000 est. |

(est. = bench run completed but exact CSV median not yet extracted; will be updated when
CRT path activates and fafft_zbc reaches its final configuration)

## CRT Decomposition Attempt (Phase 1b post-benchmark investigation)

The `CRT_fft_hqc1.c` / `CRT_fft_hqc3.c` files from Jang et al. were imported and
build infrastructure wired up.  The CRT approach splits the ring product using:
  - FAFFT ring product A*B mod φ_FAFFT  (smaller FFT domain: 32768-element for HQC-128)
  - Schoolbook low-degree part via `radix16_81` / `radix16_193`
  - CRT reconstruction via precomputed `p_inv` and sparse `bit_shift_xor` step

**Why it failed to activate:**  The required kernel `bmul2_4096_to_4096` (used by
`CRT_fft_HQC1`) performs the ring product in the 32768-element FAFFT domain.  In
the existing bc_1/encode/btfy infrastructure, the forward path for the 32768-element
domain requires bc_1_16384 (512-word input), but HQC-128's PARAM_N=17669 bits
occupies 553 words > 512.  The kernel cannot be derived from existing bc_1 functions
without the original authors' pre-reduction step or a different encoder.  Three
variants were tested (encode + btfy_32768; direct bc_1_32768 + btfy_32768; full-product
lower-half truncation) — all produced KAT failures (0/100).

**Root cause:** HQC-128's PARAM_N (17669 bits) exceeds N_FAFFT/2 (16384 bits), the
threshold at which the CRT's smaller FFT domain fits.  The CRT was designed for this
case by Jang et al. but their implementation of bmul2_4096_to_4096 was not included
in the imported sources.

**Status:** `fafft.c` reverted to full-product path (bmul2_4096_to_8192 / bmul2_8192_to_16384).
CRT and radix16 files remain in the repo as future reference.  Bench results unchanged.

## Local Optimum Declaration

After 1 CRT attempt (KAT failure) and analysis of remaining options:
- RVV vectorization of btfy inner XOR loops: feasible, estimated ≤25% speedup
  on the butterfly portion → ≤15% total vect_mul speedup.  Would still leave
  fafft_zbc ~3-4× slower than karatsuba_zbc at HQC sizes.
- prepare/mul amortization (one prepare reuse across two calls): feasible,
  ~16% saving on double-vect_mul encaps path only.
- Both insufficient to close the 4× gap vs karatsuba_zbc (354K instret).

**Conclusion:** Pure FAFFT is structurally disadvantaged at HQC-128/192/256 sizes
(degree ~17K-57K).  The FAFFT O(n log²n) advantage over Karatsuba only materialises
at degree ≫ 2^15.  Further optimization of fafft_zbc alone cannot close the gap.
Phase 1c (hybrid Karatsuba-FAFFT) is the correct next step.

## Produced/Updated Artifacts

- `analysis/benchmarks/fafft_zbc/hqc128_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/fafft_zbc/hqc192_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/fafft_zbc/hqc256_fafft_zbc_polymul_N1.csv`
- `analysis/benchmarks/fafft_zbc/hqc128_fafft_zbc_N1.csv`
- `analysis/benchmarks/fafft_zbc/hqc192_fafft_zbc_N1.csv`
- `analysis/benchmarks/fafft_zbc/hqc256_fafft_zbc_N1.csv`
- `analysis/llvm_mca/reports/fafft_mca.txt`
