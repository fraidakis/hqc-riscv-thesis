#include "poly_helpers.h"

void hqc_poly_zero(uint64_t *poly, size_t words)
{
    for (size_t i = 0; i < words; i++) {
        poly[i] = 0ULL;
    }
}

void hqc_poly_identity(uint64_t *poly, size_t words)
{
    hqc_poly_zero(poly, words);
    if (words > 0u) {
        poly[0] = 1ULL;
    }
}

void hqc_poly_xor(uint64_t *dst, const uint64_t *src, size_t words)
{
    for (size_t i = 0; i < words; i++) {
        dst[i] ^= src[i];
    }
}

bool hqc_poly_is_zero(const uint64_t *poly, size_t words)
{
    for (size_t i = 0; i < words; i++) {
        if (poly[i] != 0ULL) {
            return false;
        }
    }
    return true;
}

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
