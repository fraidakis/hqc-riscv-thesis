#ifndef FAFFT_ZVC_V3_H
#define FAFFT_ZVC_V3_H

#include <stdint.h>

#define HQC_FOUR_RUSSIANS_CHUNK_SIZE 16u
#define HQC_FOUR_RUSSIANS_KEEP 60u

/* GF(2^32) multiply reduced by g(x)=x^32 + x^22 + x^2 + x + 1. */
uint32_t gf2_32_mul_zbc(uint32_t a, uint32_t b);

/* HQC-128 fixed split: 553 words -> 512-word power-of-two block + 41-word tail. */
void hqc128_crt_split(uint32_t *poly_17669,
					  uint32_t *poly_power2,
					  uint32_t *poly_remainder);

/* Branch-free reconstruction into GF(2)[x]/(x^17669 - 1). */
void hqc128_crt_reconstruct(uint32_t *poly_17669,
							const uint32_t *poly_power2,
							const uint32_t *poly_remainder);

/* 4096-bit (128-word) branch-free PWM block multiply helper. */
void hqc_bmul2_4096_to_4096(uint32_t out[128],
							const uint32_t A[128],
							const uint32_t B[128]);

#define BMUL2_4096(out, A, B) hqc_bmul2_4096_to_4096((out), (A), (B))

typedef struct {
	uint32_t cantor_s2i[16];
} hqc_cantor_tables_t;

typedef struct {
	uint16_t t1[HQC_FOUR_RUSSIANS_CHUNK_SIZE];
	uint16_t t2[HQC_FOUR_RUSSIANS_CHUNK_SIZE];
	uint16_t t3[HQC_FOUR_RUSSIANS_CHUNK_SIZE];
	uint16_t keep_indices[HQC_FOUR_RUSSIANS_KEEP];
	uint16_t keep_values[HQC_FOUR_RUSSIANS_KEEP];
} hqc_four_russians_t;

void hqc_cantor_init_tables(hqc_cantor_tables_t *tbl);
void BasisCvt(uint32_t *dst, const uint32_t *src, uint32_t n_words, const hqc_cantor_tables_t *tbl);
void hqc_four_russians_generate(uint16_t beta27, uint16_t V12, hqc_four_russians_t *out);

/*
 * Task A v3 — Binary-polynomial Karatsuba with clmul at the 64-bit leaf.
 *
 * Drop-in replacement for the tower-field FAFFT vect_mul.  Computes
 *   o = a * b  mod (x^PARAM_N − 1)  over GF(2)[x]
 * using recursive Karatsuba with clmul/clmulh at the base case.
 * Declared here so fafft.c can call it; implementation in fafft_zvc_v3.c.
 */
void vect_mul_v3(uint64_t *o, const uint64_t *a, const uint64_t *b);

#endif
