#include <stdint.h>
#include <string.h>

#include "parameters.h"
#include "ref/gfmul_fft.h"
#include "ref/reduce.h"

#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

void vect_mul(uint64_t *o, const uint64_t *v1, const uint64_t *v2)
{
#if defined(HQC_128)
    uint32_t prod[65536 / 32] = {0};
    bmul2_4096_to_8192((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce((uint32_t *)o, prod);
#else
    uint32_t prod[131072 / 32] = {0};
    bmul2_8192_to_16384((uint8_t *)prod, (const uint8_t *)v1, (const uint8_t *)v2);
    reduce((uint32_t *)o, prod);
#endif
}
