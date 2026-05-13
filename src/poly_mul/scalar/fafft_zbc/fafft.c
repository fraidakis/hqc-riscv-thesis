// Scalar Karatsuba baseline (from EXP-fafft-v3-zvc)
#include <stdint.h>
#include <string.h>
#include "parameters.h"

#ifndef VEC_N_SIZE_64
#define VEC_N_SIZE_64 ((PARAM_N + 63) / 64)
#endif

#define KARATSUBA_THRESH 16
#define TMP_WORDS  (16u * VEC_N_SIZE_64)

static __attribute__((always_inline)) inline
void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    __asm__ volatile(
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
    memset(r, 0, 2u * n * sizeof(uint64_t));
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            uint64_t lo, hi;
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
    }
}

static void karatsuba_poly(uint64_t * __restrict__ r,
                           const uint64_t * __restrict__ a,
                           const uint64_t * __restrict__ b,
                           size_t n,
                           uint64_t * __restrict__ tmp)
{
    if (n <= KARATSUBA_THRESH) {
        clmul_leaf(r, a, b, n);
        return;
    }

    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;

    uint64_t *z0    = tmp;
    uint64_t *z2    = z0   + 2u * n;
    uint64_t *zmid  = z2   + 2u * n;
    uint64_t *ta    = zmid + 2u * n;
    uint64_t *tb    = ta   + n;
    uint64_t *child = tmp  + 8u * n;

    karatsuba_poly(z0,   a,     b,     n0, child);
    karatsuba_poly(z2,   a + m, b + m, n1, child);
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0ULL) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0ULL) ^ b[m + i];
    }
    karatsuba_poly(zmid, ta, tb, n1, child);

    memset(r, 0, 2u * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2u * n0; i++) r[i]         ^= z0[i];
    for (size_t i = 0; i < 2u * n1; i++) r[2u*m + i]  ^= z2[i];
    for (size_t i = 0; i < 2u * n1; i++) {
        uint64_t z0i = (i < 2u*n0 ? z0[i] : 0ULL);
        r[m + i] ^= zmid[i] ^ z0i ^ z2[i];
    }
}

static void reduce_poly(uint64_t * __restrict__ o,
                        const uint64_t * __restrict__ a)
{
    const int shift = (int)(PARAM_N & 0x3Fu);
    const int inv   = 64 - shift;
    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t hi   = a[i + VEC_N_SIZE_64 - 1u] >> shift;
        uint64_t carry = (shift == 0) ? 0ULL : (a[i + VEC_N_SIZE_64] << inv);
        o[i] = a[i] ^ hi ^ carry;
    }
    const uint64_t mask = (shift == 0) ? ~0ULL : ((1ULL << shift) - 1ULL);
    o[VEC_N_SIZE_64 - 1u] &= mask;
}

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2u * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_WORDS];
    karatsuba_poly(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce_poly(o, unreduced);
}
