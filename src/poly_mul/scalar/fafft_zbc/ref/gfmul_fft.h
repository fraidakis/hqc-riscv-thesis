#ifndef _GFMUL_FFT_H_
#define _GFMUL_FFT_H_

#include <stdint.h>

/* Full-product kernels (input X bytes -> output 2X bytes, full degree) */
void bmul2_4096_to_8192_prepare(uint32_t *a_out, const uint8_t *a_in);
void bmul2_4096_to_8192_mul(uint8_t *c, const uint32_t *a, const uint32_t *b);
void bmul2_4096_to_8192(uint8_t *c, const uint8_t *a, const uint8_t *b);

void bmul2_8192_to_16384_prepare(uint32_t *a_out, const uint8_t *a_in);
void bmul2_8192_to_16384_mul(uint8_t *c, const uint32_t *a, const uint32_t *b);
void bmul2_8192_to_16384(uint8_t *c, const uint8_t *a, const uint8_t *b);

/* Same-domain CRT kernels (product in the smaller FAFFT ring; used by CRT_fft_HQC1/3) */
void bmul2_4096_to_4096_prepare(uint32_t *a_out, const uint8_t *a_in);
void bmul2_4096_to_4096_mul(uint8_t *c, const uint32_t *a, const uint32_t *b);
void bmul2_4096_to_4096(uint8_t *c, const uint8_t *a, const uint8_t *b);

void bmul2_8192_to_8192_prepare(uint32_t *a_out, const uint8_t *a_in);
void bmul2_8192_to_8192_mul(uint8_t *c, const uint32_t *a, const uint32_t *b);
void bmul2_8192_to_8192(uint8_t *c, const uint8_t *a, const uint8_t *b);

#endif
