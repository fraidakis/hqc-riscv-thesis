#include <stdint.h>
#include <string.h>

#include "parameters.h"
#include "ref/gfmul_fft.h"
#include "ref/reduce.h"

#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

/*
 * vect_mul — HQC polynomial multiplication in GF(2)[x]/(x^PARAM_N + 1).
 *
 * Uses the FAFFT full-product kernels from Jang et al. (FAFFT_LJKH25):
 *   HQC-128: bmul2_4096_to_8192  (65536-element additive FFT domain)
 *   HQC-192/256: bmul2_8192_to_16384 (131072-element domain)
 *
 * CRT decomposition path (attempted, Phase 1b iteration):
 *   CRT_fft_HQC1 / CRT_fft_HQC3 are imported but INACTIVE because
 *   bmul2_4096_to_4096 / bmul2_8192_to_8192 (the same-domain ring-product
 *   kernels they require) cannot be derived from the existing bc_1/btfy
 *   infrastructure: for HQC-128 the input polynomial has 553 words which
 *   exceeds the 512-word limit of bc_1_16384.  The missing kernels need
 *   the original Jang et al. source.  Noted as future work.
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
