#include "parameters.h"
#include <stddef.h>
#include <stdint.h>

#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

#define REDUCTION_MASKING (((PARAM_N & 0x1F) == 0) ? 0xFFFFFFFFu : ((1u << (PARAM_N & 0x1F)) - 1u))

void reduce(uint32_t *o, const uint32_t *a) {
	const uint32_t shift = (PARAM_N & 0x1F);
	const uint32_t inv = 32U - shift;
	uint32_t r;
	uint32_t carry;

	for (size_t i = 0; i < VEC_N_SIZE_32; ++i) {
		r = (shift == 0U) ? a[i + VEC_N_SIZE_32 - 1]
		                  : (a[i + VEC_N_SIZE_32 - 1] >> shift);
		carry = (shift == 0U) ? 0U : (a[i + VEC_N_SIZE_32] << inv);
		o[i] = a[i] ^ r ^ carry;
	}

	o[VEC_N_SIZE_32 - 1] &= REDUCTION_MASKING;
}