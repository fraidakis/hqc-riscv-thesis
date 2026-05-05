#include <stdint.h>
#include <string.h>

#include "parameters.h"
#include "ref/gfmul_fft.h"
#include "ref/reduce.h"
#include "ref/CRT_fft.h"
#include "fafft_v5.h"

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
 * Task A v3 (active): delegates to vect_mul_v3() — a Karatsuba binary-
 * polynomial multiplier using clmul/clmulh at the 64-bit leaf.  The v3
 * implementation lives in v3/fafft_zvc_v3.c and is compiled into the same
 * CMake target via the existing POLYMUL_SOURCES list that already includes
 * v3/fafft_zvc_v3.c.
 *
 * Original tower-field FAFFT path (preserved below for reference):
 *   bmul2_4096_to_8192 / bmul2_8192_to_16384 (bc_1_32768 / bc_1_65536 domains).
 *   Revert: comment out the vect_mul_v3 call and uncomment the #if/#else block.
 */
void vect_mul(uint64_t *o, const uint64_t *v1, const uint64_t *v2)
{
    /* Phase 1b v5 — active: paper-optimized CRT bypass implementation. */
    vect_mul_v5(o, v1, v2);

    /* Original tower-field FAFFT path (fallback — uncomment to revert):
#if defined(HQC_128)
    uint32_t prod[65536 / 32] = {0};
    bmul2_4096_to_8192((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce_strict_u32((uint32_t *)o, prod);
#else
    uint32_t prod[131072 / 32] = {0};
    bmul2_8192_to_16384((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce_strict_u32((uint32_t *)o, prod);
#endif
    */
}
