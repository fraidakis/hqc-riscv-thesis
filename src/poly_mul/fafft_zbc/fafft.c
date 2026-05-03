#include <stdint.h>
#include <string.h>

#include "parameters.h"
#include "ref/gfmul_fft.h"
#include "ref/reduce.h"
#include "ref/CRT_fft.h"

#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

static inline void reduce_strict_u32(uint32_t *o, const uint32_t *a)
{
    reduce(o, a);
    o[VEC_N_SIZE_32 - 1] &= ((PARAM_N & 0x1F) == 0)
                           ? 0xFFFFFFFFu
                           : ((1u << (PARAM_N & 0x1F)) - 1u);
}

/*
 * vect_mul — HQC polynomial multiplication in GF(2)[x]/(x^PARAM_N + 1).
 *
 * Iter 1a (CRT) — REVERTED: CRT_fft_HQC1 / CRT_fft_HQC3 require a correct
 * bmul2_4096_to_4096 / bmul2_8192_to_8192 kernel (half-domain product in the
 * FAFFT ring).  The current stub just truncates the full product, which is
 * algebraically wrong — KAT fails at 0/100.  True half-domain kernels need
 * the original Jang et al. source (bc_1_16384 limit: 512 words < 553 needed).
 *
 * Iter 1b (TFAFFT) — NOT APPLICABLE: bc_1 mixes all polynomial bits before
 * the butterfly, so zero-padded branches cannot be pruned without rewriting
 * the combined bc_1 + btfy pipeline.
 *
 * Active path: direct full-product kernels (Phase 1b starting state).
 * RVV optimisation (Iter 2/3) is applied in btfy_ffft.c.
 */
void vect_mul(uint64_t *o, const uint64_t *v1, const uint64_t *v2)
{
#if defined(HQC_128)
    uint32_t prod[65536 / 32] = {0};
    bmul2_4096_to_8192((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce_strict_u32((uint32_t *)o, prod);
#else
    uint32_t prod[131072 / 32] = {0};
    bmul2_8192_to_16384((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce_strict_u32((uint32_t *)o, prod);
#endif
}
