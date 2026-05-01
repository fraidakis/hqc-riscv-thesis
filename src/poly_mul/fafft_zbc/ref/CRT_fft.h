#ifndef CRT_FFT_H
#define CRT_FFT_H

#include <stdint.h>

void CRT_fft_HQC1(uint32_t *res, const uint32_t *A, const uint32_t *B);
void CRT_fft_HQC3(uint32_t *res, const uint32_t *A, const uint32_t *B);

#endif
