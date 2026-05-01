/* src/poly_mul/fafft_zvbc/fafft_vclmul.c
 *
 * Phase 1+ (Track B) — FAFFT compiled for Zvbc; RVV-vectorised where
 * applicable.
 *
 * ── Architecture decision (Phase 1+ research finding) ───────────────────
 * The existing FAFFT (fafft_zbc) uses the Jang et al. additive-FFT kernel
 * with GF(2^32) ARITHMETIC ENCODED IN A BIT-SLICED TOWER-FIELD
 * REPRESENTATION (gf232v_mul from gfv_tower.h).  In this representation,
 * each uint32_t word carries ONE BIT from EACH of 32 parallel GF(2^32)
 * elements — the inner operations are AND and XOR on packed 32-bit words,
 * NOT carry-less polynomial multiplications.
 *
 * vclmul.vv computes carry-less products of 64-bit BINARY POLYNOMIALS.
 * This is semantically incompatible with the bit-sliced tower-field
 * arithmetic: applying vclmul.vv to the packed words would produce wrong
 * intermediate values and corrupt the result.
 *
 * Consequence:  vclmul.vv CANNOT replace gf232v_mul in the existing FAFFT.
 * The vectorisation axes available without restructuring the algorithm are:
 *
 *   Axis A — vxor.vv (LMUL=4) for the btfy butterfly XOR combines:
 *     btfy_layer's inner `for(j=0;j<32;j++) data[i+j] ^= ...` loop → 4
 *     vector instructions instead of 32 scalar word stores.  (This axis is
 *     already present in fafft_rvv.c for fafft_zbc; fafft_zvbc inherits it
 *     via the shared ref/ sources that are compiled with RVV-capable flags.)
 *
 *   Axis B — vxor.vv for the final modular reduction (reduce.c).
 *
 * This means fafft_zvbc ≈ fafft_zbc in instruction-count, confirming the
 * thesis research finding:
 *   "For HQC-128/192/256, vclmul.vv benefits Karatsuba (direct leaf
 *    speedup) but NOT tower-field FAFFT (incompatible arithmetic model).
 *    Vector Karatsuba retains the crown."
 *
 * Detected at compile time via HQC_POLYMUL_ZVBC=1 (set by CMakeLists.txt).
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include "parameters.h"

/* Ref FAFFT headers — from fafft_zbc/ref/. */
#include "ref/gfmul_fft.h"
#include "ref/reduce.h"

#ifndef HQC_POLYMUL_ZVBC
#  error "fafft_vclmul.c must be compiled with -DHQC_POLYMUL_ZVBC=1"
#endif

#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

/*
 * vect_mul — HQC polynomial multiplication in GF(2)[x]/(x^PARAM_N + 1).
 *
 * Structurally identical to fafft_zbc/fafft.c.  The speedup over
 * fafft_zbc (if any) comes from the RVV-vectorised btfy_layer XOR loops
 * (Axis A above) which are compiled into btfy_ffft.c at higher SIMD width
 * when -march includes the V extension.  The Zvbc extension itself does
 * not contribute additional speedup for this algorithm.
 *
 * Benchmarking on Spike (see analysis/benchmarks/phase1plus/) will confirm
 * whether the small RVV-XOR benefit is measurable vs fafft_zbc.
 */
void vect_mul(uint64_t *o, const uint64_t *v1, const uint64_t *v2)
{
#if defined(HQC_128)
    uint32_t prod[65536 / 32] = {0};
    bmul2_4096_to_8192((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce((uint32_t *)o, prod);
#else
    uint32_t prod[131072 / 32] = {0};
    bmul2_8192_to_16384((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce((uint32_t *)o, prod);
#endif
}
