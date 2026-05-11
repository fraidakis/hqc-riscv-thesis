#ifndef HQC_POLY_HELPERS_H
#define HQC_POLY_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Shared helpers for dense binary polynomials represented as uint64_t arrays.
 *
 * Representation contract:
 * - poly[0] holds coefficients for x^0..x^63 (least-significant bit first).
 * - poly[i] holds the next 64 coefficients.
 * - callers provide the exact logical length in 64-bit words.
 */

/* Set all words to zero. */
void hqc_poly_zero(uint64_t *poly, size_t words);

/* Set polynomial to multiplicative identity: 1 + 0*x + ... */
void hqc_poly_identity(uint64_t *poly, size_t words);

/* In-place XOR: dst[i] ^= src[i] for i in [0, words). */
void hqc_poly_xor(uint64_t *dst, const uint64_t *src, size_t words);

/* True iff every word is zero. */
bool hqc_poly_is_zero(const uint64_t *poly, size_t words);

/* True iff poly == 1 (first word is 1, all remaining words are zero). */
bool hqc_poly_is_identity(const uint64_t *poly, size_t words);

#endif
