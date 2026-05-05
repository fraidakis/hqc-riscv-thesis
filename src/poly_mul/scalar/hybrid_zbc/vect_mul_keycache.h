/* src/poly_mul/hybrid_zbc/vect_mul_keycache.h
 *
 * Phase 1b-3 Iter 3 — Key Caching API
 *
 * Declare vect_mul_r2_keycached, which computes both r2*h and r2*s in
 * a single call, sharing the forward FAFFT transforms of r2.
 * Included by vendor/hqc-ref/src/ref/hqc.c when
 * HQC_POLYMUL_HYBRID_KEYCACHE is defined.
 */
#ifndef VECT_MUL_KEYCACHE_H
#define VECT_MUL_KEYCACHE_H

#include <stdint.h>

void vect_mul_r2_keycached(
        uint64_t * __restrict__ u,   /* output: r2 * h */
        uint64_t * __restrict__ v,   /* output: r2 * s */
        const uint64_t *r2,
        const uint64_t *h,
        const uint64_t *s);

#endif /* VECT_MUL_KEYCACHE_H */
