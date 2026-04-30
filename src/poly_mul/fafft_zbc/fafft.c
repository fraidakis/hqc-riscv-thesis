/* src/poly_mul/fafft_zbc/fafft.c
 *
 * Phase 1b — Frobenius Additive FFT (FAFFT) for HQC polynomial multiplication
 * over GF(2)[x]/(x^n - 1), scalar component.
 *
 * Based on: Jang et al., "Efficient Polynomial Multiplication for HQC on
 * ARM Cortex-M4" (ePrint).  Algorithm structure (FAFFT recursion, Cantor
 * basis conversion, CRT decomposition) is ported from Jang's Cortex-M4
 * implementation to plain C targeting RV64.
 *
 * GF(2^m) butterfly multiplications in this file use RISC-V Zbc instructions
 * (clmul / clmulh) with level-specific Barrett reduction.  For small fields
 * (m <= 8), precomputed 256-byte lookup tables are used instead.
 *
 * See also: fafft_rvv.c for the RVV-vectorised XOR combine phases.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * TODO: implement — port Jang et al. C-level FAFFT logic, replace ARM asm
 *       GF(2^m) butterfly with clmul + Barrett reduction sequences.
 */

#include <stdint.h>
#include <string.h>
#include "parameters.h"

/*
 * Phase 1b bootstrap implementation:
 * - preserves the fafft_zbc integration path and removes traps
 * - uses the proven clmul Karatsuba core as the scalar backbone
 *
 * This keeps the variant correctness-stable while FAFFT-specific stages
 * (Cantor-basis transform + CRT decomposition + butterfly scheduling)
 * are iterated in subsequent commits.
 */

#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 16
#endif

#define TMP_BUFFER_WORDS (16 * VEC_N_SIZE_64)

static __attribute__((always_inline)) inline
void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    __asm__ volatile (
        "clmul  %0, %2, %3\n\t"
        "clmulh %1, %2, %3\n\t"
        : "=&r"(*lo), "=r"(*hi)
        : "r"(a), "r"(b)
    );
}

static void clmul_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            uint64_t lo, hi;
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j] ^= lo;
            r[i + j + 1] ^= hi;
        }
    }
}

static void karatsuba_clmul(uint64_t * __restrict__ r,
                            const uint64_t * __restrict__ a,
                            const uint64_t * __restrict__ b,
                            size_t n,
                            uint64_t * __restrict__ tmp)
{
    if (n <= KARATSUBA_THRESHOLD) {
        clmul_leaf(r, a, b, n);
        return;
    }

    size_t m = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;

    uint64_t *z0 = tmp;
    uint64_t *z2 = z0 + 2 * n;
    uint64_t *zmid = z2 + 2 * n;
    uint64_t *ta = zmid + 2 * n;
    uint64_t *tb = ta + n;
    uint64_t *child = tmp + 8 * n;

    karatsuba_clmul(z0, a, b, n0, child);
    karatsuba_clmul(z2, a + m, b + m, n1, child);

    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }

    karatsuba_clmul(zmid, ta, tb, n1, child);

    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2 * n0; i++) r[i] ^= z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i] ^= z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        r[m + i] ^= zmid[i] ^ z0i ^ z2[i];
    }
}

static void reduce(uint64_t * __restrict__ o,
                   const uint64_t * __restrict__ a)
{
    int shift = PARAM_N & 0x3F;
    int inv = 64 - shift;

    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t r = a[i + VEC_N_SIZE_64 - 1] >> shift;
        uint64_t carry = (shift == 0) ? 0 : (a[i + VEC_N_SIZE_64] << inv);
        o[i] = a[i] ^ r ^ carry;
    }
    o[VEC_N_SIZE_64 - 1] &= BITMASK(PARAM_N, 64);
}

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

    karatsuba_clmul(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
