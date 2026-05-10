#ifndef HQC_POLY_HELPERS_H
#define HQC_POLY_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void hqc_poly_zero(uint64_t *poly, size_t words);
void hqc_poly_identity(uint64_t *poly, size_t words);
void hqc_poly_xor(uint64_t *dst, const uint64_t *src, size_t words);
bool hqc_poly_is_zero(const uint64_t *poly, size_t words);
bool hqc_poly_is_identity(const uint64_t *poly, size_t words);

#endif
