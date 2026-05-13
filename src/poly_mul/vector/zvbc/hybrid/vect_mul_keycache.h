/* src/poly_mul/zvbc/hybrid/vect_mul_keycache.h
 *
 * Hybrid key-caching API.
 *
 * Declare vect_mul_r2_keycached, which computes both r2*h and r2*s in
 * a single call. In the current scalar-hybrid path it is implemented as
 * two independent multiplies; this header keeps the shared-call ABI stable.
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
