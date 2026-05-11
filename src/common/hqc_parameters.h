#ifndef HQC_LOCAL_PARAMETERS_H
#define HQC_LOCAL_PARAMETERS_H

#include "parameters.h"

#define HQC_WORD_BITS_32 32u
#define HQC_WORD_BITS_64 64u
#define HQC_WORD_BYTES_64 8u

/* Number of machine words required to store PARAM_N bits. */
#define HQC_VEC_N_SIZE_32 ((PARAM_N + (HQC_WORD_BITS_32 - 1u)) / HQC_WORD_BITS_32)
#define HQC_VEC_N_SIZE_64 ((PARAM_N + (HQC_WORD_BITS_64 - 1u)) / HQC_WORD_BITS_64)

#define HQC_FAFFT_BITS_4096 4096u
#define HQC_FAFFT_BITS_8192 8192u
#define HQC_FAFFT_BITS_16384 16384u
#define HQC_FAFFT_BITS_32768 32768u
#define HQC_FAFFT_BITS_65536 65536u
#define HQC_FAFFT_BITS_131072 131072u

#define HQC_FAFFT_TRUNC_FFT_LANES 16u
#define HQC_FAFFT_VECTOR_CHUNK_U32 32u

#define HQC_FAFFT_HQC128_SPLIT_BITS HQC_FAFFT_BITS_16384
#define HQC_FAFFT_HQC128_SPLIT_WORDS_32 (HQC_FAFFT_HQC128_SPLIT_BITS / HQC_WORD_BITS_32)
#define HQC_FAFFT_HQC128_SPLIT_WORDS_64 (HQC_FAFFT_HQC128_SPLIT_BITS / HQC_WORD_BITS_64)

#define HQC_KARATSUBA_THRESHOLD_DEFAULT 16u
#define HQC_HYBRID_V3_THRESHOLD_DEFAULT 32u
#define HQC_POLY_TMP_BUFFER_MULTIPLIER 16u

/* Remainder bits in the top 64-bit word of an N-bit polynomial vector. */
#define HQC_PARAM_N_MOD_64 ((unsigned)(PARAM_N & 0x3Fu))

/*
 * Mask for the last 64-bit word of an N-bit polynomial:
 * - If N is a multiple of 64, keep all bits (~0ULL).
 * - Otherwise keep only the low (N mod 64) bits and clear the padding bits.
 *
 * Example: if N mod 64 = 13, the mask is 0x1FFF and bits [63:13] are cleared.
 */
#define HQC_PARAM_LAST_WORD_MASK_64 \
	((HQC_PARAM_N_MOD_64 == 0u) ? ~0ULL : ((1ULL << HQC_PARAM_N_MOD_64) - 1ULL))

/* Unit-conversion helpers used by FAFFT and buffer-size calculations. */
#define HQC_BITS_TO_U32_WORDS(bits) ((bits) / HQC_WORD_BITS_32)
#define HQC_BITS_TO_BYTES(bits) ((bits) / 8u)

#endif
