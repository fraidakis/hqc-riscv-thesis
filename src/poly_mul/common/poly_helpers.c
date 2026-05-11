#include "poly_helpers.h"

/* Clear a polynomial buffer. */
void hqc_poly_zero(uint64_t *poly, size_t words)
{
    for (size_t i = 0; i < words; i++) {
        poly[i] = 0ULL;
    }
}

/* Build the identity polynomial (constant 1). */
void hqc_poly_identity(uint64_t *poly, size_t words)
{
    hqc_poly_zero(poly, words);
    if (words > 0u) {
        poly[0] = 1ULL;
    }
}

/* GF(2) polynomial addition is a word-wise XOR. */
void hqc_poly_xor(uint64_t *dst, const uint64_t *src, size_t words)
{
    for (size_t i = 0; i < words; i++) {
        dst[i] ^= src[i];
    }
}

/* Scan for any non-zero coefficient word. */
bool hqc_poly_is_zero(const uint64_t *poly, size_t words)
{
    for (size_t i = 0; i < words; i++) {
        if (poly[i] != 0ULL) {
            return false;
        }
    }
    return true;
}

/* Check canonical identity encoding: 1 in bit 0, zero elsewhere. */
bool hqc_poly_is_identity(const uint64_t *poly, size_t words)
{
    if (words == 0u) {
        return false;
    }
    if (poly[0] != 1ULL) {
        return false;
    }
    for (size_t i = 1; i < words; i++) {
        if (poly[i] != 0ULL) {
            return false;
        }
    }
    return true;
}
