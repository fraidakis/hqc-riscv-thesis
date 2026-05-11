/* src/poly_mul/hybrid_zbc/hybrid_v3.h
 *
 * Scalar hybrid FAFFT+Karatsuba polynomial multiplication.
 *
 * Public API: vect_mul_hybrid_v3
 *   Computes o = a * b mod (x^PARAM_N - 1) over GF(2)[x].
 */
#ifndef HYBRID_V3_H
#define HYBRID_V3_H

#include <stdint.h>

void vect_mul_hybrid_v3(uint64_t *o, const uint64_t *a, const uint64_t *b);

#endif /* HYBRID_V3_H */
