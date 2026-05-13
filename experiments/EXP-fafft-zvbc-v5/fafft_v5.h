#ifndef FAFFT_V5_H
#define FAFFT_V5_H

#include <stdint.h>

/*
 * Paper-optimized CRT bypass implementation.
 *
 * For HQC-128 (PARAM_N == 17669):
 *   Implements the CRT-based polynomial multiplication from the reference
 *   algorithm in ref/CRT_fft_hqc1.c, but replaces the slow
 *   bmul2_4096_to_4096 FAFFT stub with the v4 RVV vclmul.vv vectorised
 *   schoolbook accumulation.  The CRT correction (radix16_81 + sparse P_15
 *   multiply) is kept exactly as in the paper.
 *
 * For all other HQC levels:
 *   Falls back to the v4 RVV vclmul.vv vectorised schoolbook with direct
 *   ring reduction.
 */
void vect_mul_v5(uint64_t *o, const uint64_t *a, const uint64_t *b);

#endif /* FAFFT_V5_H */