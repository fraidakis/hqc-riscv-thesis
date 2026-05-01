#include "parameters.h"
#include <stddef.h>
#include <stdint.h>

#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

#define REDUCTION_MASKING (1<<(PARAM_N&0x1F))-1

void reduce(uint32_t *o, const uint32_t *a) {
	uint32_t r;
	uint32_t carry;

	for (size_t i = 0; i < VEC_N_SIZE_32; ++i) {
		r = a[i + VEC_N_SIZE_32 - 1] >> (PARAM_N & 0x1F);
		carry = a[i + VEC_N_SIZE_32] << (32 - (PARAM_N & 0x1F));
		o[i] = a[i] ^ r ^ carry;
	}

	o[VEC_N_SIZE_32 - 1] &= REDUCTION_MASKING;
}