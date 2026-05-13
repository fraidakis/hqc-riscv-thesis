/* EXP-fafft-zvbc-v0-towerfield/fafft_vclmul_towerfield.c
 *
 * ZVBC tower-field FAFFT — archived as loser from the FAFFT showdown.
 *
 * L128 polymul_instructions: 1,731,112  (showdown date: 2026-05-13)
 * Defeated by: EXP-fafft-zvbc-v4 vclmul schoolbook (193,177 instructions)
 *
 * Original location: src/poly_mul/vector/zvbc/fafft_zvbc/fafft_vclmul.c
 * Reason for retirement: vclmul.vv is semantically incompatible with the
 * bit-sliced tower-field GF(2^32) arithmetic used by the FAFFT kernel.
 * The Zvbc extension provides no speedup over plain fafft_zbc here.
 * The direct vclmul.vv schoolbook (EXP-fafft-zvbc-v4) is 8.97x faster.
 */

/* src/poly_mul/fafft_zvbc/fafft_vclmul.c
 *
 * FAFFT compiled for Zvbc; RVV-vectorized where
 * applicable.
 *
 * ── Architecture decision ───────────────────────────────────────────────
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
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include "parameters.h"

#include "ref/gfmul_fft.h"
#include "ref/reduce.h"

#ifndef HQC_POLYMUL_ZVBC
#  error "fafft_vclmul.c must be compiled with -DHQC_POLYMUL_ZVBC=1"
#endif

#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

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
