/* src/poly_mul/hybrid_zbc/hybrid.c
 *
 * Phase 1c — Hybrid Karatsuba-FAFFT polynomial multiplication.
 *
 * Algorithm:
 *   Apply HYBRID_KARATSUBA_DEPTH levels of Karatsuba splitting first, then
 *   at the leaf level use the FAFFT full-product kernel (bmul2_4096_to_8192
 *   for HQC-128, bmul2_8192_to_16384 for HQC-192/256) to compute each
 *   sub-multiplication as an unreduced binary polynomial product.
 *   One modular reduction (mod x^PARAM_N - 1) is applied at the very end.
 *
 * Depth semantics:
 *   depth=0 → pure FAFFT  (equivalent to fafft_zbc, useful as d=0 baseline)
 *   depth=1 → 1 Karatsuba level, then FAFFT on 3 sub-problems of size n/2
 *   depth=2 → 2 levels, 9 sub-problems of size n/4, etc.
 *   depth→∞ → equivalent to pure karatsuba_zbc (falls back to clmul_leaf)
 *
 * Compile-time control:
 *   -DHYBRID_KARATSUBA_DEPTH=N   (default 1)
 *   -DKARATSUBA_THRESHOLD=N      (default 16, same as phase1a)
 *
 * FAFFT input buffer constraint:
 *   bmul2_*_prepare() reads exactly VEC_N_SIZE_64*8 bytes from its input.
 *   For sub-problems (n < VEC_N_SIZE_64) we zero-pad a VEC_N_SIZE_64*8-byte
 *   local buffer and copy the sub-polynomial into its low bytes.
 */

#include <stdint.h>
#include <string.h>
#include "parameters.h"

/* FAFFT full-product kernels */
#include "../fafft_zbc/ref/gfmul_fft.h"

/* ── Compile-time depth parameter ────────────────────────────────────── */
#ifndef HYBRID_KARATSUBA_DEPTH
#define HYBRID_KARATSUBA_DEPTH 0
#endif

#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 16
#endif

/* ── Temporary buffer (covers all Karatsuba levels) ─────────────────── */
#define TMP_BUFFER_WORDS (16 * VEC_N_SIZE_64)

/* ── Zbc carry-less multiply base case ──────────────────────────────── */
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

/* ── Schoolbook clmul leaf multiply: r = a * b (2n words, unreduced) ── */
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
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
    }
}

/* ── FAFFT unreduced leaf multiply: r = a * b (2n words, unreduced) ─── *
 *
 * Calls the smallest fitting FAFFT kernel for the sub-problem size n.
 *
 * Kernel selection (adaptive based on n):
 *   n <= HQC128_N_WORDS (= 277): use bmul2_4096_to_8192
 *     - kernel reads 2216 bytes (= 277*8) from input; safe when n ≤ 277
 *     - output: 8192 bytes; product of degree ≤ n*64-1 fits in 65536 bits ✓
 *   n >  HQC128_N_WORDS:          use bmul2_8192_to_16384
 *     - kernel reads VEC_N_SIZE_64*8 bytes; handles full HQC-192/256 poly
 *
 * This enables a domain-size reduction for HQC-192/256 when Karatsuba
 * has reduced sub-problems to ≤ 277 words (depth ≥ 2 for HQC-192/256):
 *   HQC-192 full (561 words) → bmul2_8192_to_16384 (131072-bit domain)
 *   HQC-192 d=2 (~141 words) → bmul2_4096_to_8192  ( 65536-bit domain) ← 2×
 *
 * Buffer layout:
 *   a_buf / b_buf: VEC_N_SIZE_64 * 8 bytes, zero-filled.  Sub-polynomial
 *   occupies bytes 0..n*8-1; bytes n*8..end are zero (zero coefficients).
 *   bmul2_4096_to_8192_prepare reads bytes 0..2215 (correct since n ≤ 277).
 *   bmul2_8192_to_16384_prepare reads bytes 0..VEC_N_SIZE_64*8-1.
 */

/* HQC-128's VEC_N_SIZE_64: threshold for the smaller FAFFT kernel */
#define HQC128_N_WORDS  ((17669 + 63) / 64)   /* = 277 */

static void fafft_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    uint8_t a_buf[VEC_N_SIZE_64 * sizeof(uint64_t)];
    uint8_t b_buf[VEC_N_SIZE_64 * sizeof(uint64_t)];

    memset(a_buf, 0, VEC_N_SIZE_64 * sizeof(uint64_t));
    memset(b_buf, 0, VEC_N_SIZE_64 * sizeof(uint64_t));

    size_t copy_bytes = n * sizeof(uint64_t);
    memcpy(a_buf, a, copy_bytes);
    memcpy(b_buf, b, copy_bytes);

    /* Use the smaller kernel whenever the sub-problem fits. */
    if (n <= HQC128_N_WORDS) {
        /* bmul2_4096_to_8192: 65536-bit output domain, reads 2216 input bytes */
        uint8_t prod[65536 / 8];   /* 8192 bytes */
        bmul2_4096_to_8192(prod, a_buf, b_buf);
        memcpy(r, prod, 2 * n * sizeof(uint64_t));
    } else {
        /* bmul2_8192_to_16384: 131072-bit output domain */
        uint8_t prod[131072 / 8];  /* 16384 bytes */
        bmul2_8192_to_16384(prod, a_buf, b_buf);
        memcpy(r, prod, 2 * n * sizeof(uint64_t));
    }
}

/* ── Modular reduction mod (x^PARAM_N - 1) ──────────────────────────── *
 * Identical to karatsuba_zbc's static reduce (works on uint64_t arrays).
 */
static void reduce(uint64_t * __restrict__ o,
                   const uint64_t * __restrict__ a)
{
    int shift = PARAM_N & 0x3F;
    int inv   = 64 - shift;

    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t r  = a[i + VEC_N_SIZE_64 - 1] >> shift;
        uint64_t carry = (shift == 0) ? 0 : (a[i + VEC_N_SIZE_64] << inv);
        o[i] = a[i] ^ r ^ carry;
    }
    o[VEC_N_SIZE_64 - 1] &= BITMASK(PARAM_N, 64);
}

/* ── Recursive hybrid multiply ───────────────────────────────────────── *
 *
 * r (2*n words) = a (n words) * b (n words)  [unreduced product]
 *
 * depth: remaining Karatsuba levels before switching to FAFFT leaf.
 *   depth == 0 → call fafft_leaf
 *   depth  > 0 AND n > KARATSUBA_THRESHOLD → one Karatsuba split, recurse
 *   n <= KARATSUBA_THRESHOLD → call clmul_leaf (avoids FAFFT overhead
 *       on tiny sub-problems that may arise at large depths)
 */
static void hybrid_mul(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n,
                       int depth,
                       uint64_t * __restrict__ tmp)
{
    /* Leaf cases */
    if (depth == 0) {
        fafft_leaf(r, a, b, n);
        return;
    }
    if (n <= (size_t)KARATSUBA_THRESHOLD) {
        clmul_leaf(r, a, b, n);
        return;
    }

    /* One Karatsuba level */
    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;   /* n1 == m or m+1 when n is odd */

    /* Slice tmp (8*n words at this level, rest for children) */
    uint64_t *z0   = tmp;             /* 2*n: a_lo * b_lo   */
    uint64_t *z2   = z0   + 2 * n;   /* 2*n: a_hi * b_hi   */
    uint64_t *zmid = z2   + 2 * n;   /* 2*n: (a_lo^a_hi)*(b_lo^b_hi) */
    uint64_t *ta   = zmid + 2 * n;   /* n1:  a_lo XOR a_hi */
    uint64_t *tb   = ta   + n;       /* n1:  b_lo XOR b_hi */
    uint64_t *child_buf = tmp + 8 * n;

    /* z0 = a_lo * b_lo */
    hybrid_mul(z0, a, b, n0, depth - 1, child_buf);

    /* z2 = a_hi * b_hi */
    hybrid_mul(z2, a + m, b + m, n1, depth - 1, child_buf);

    /* ta = a_lo XOR a_hi,  tb = b_lo XOR b_hi */
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }

    /* zmid = ta * tb */
    hybrid_mul(zmid, ta, tb, n1, depth - 1, child_buf);

    /* Assemble r = z0 + (zmid ^ z0 ^ z2)*x^m + z2*x^(2m) */
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2 * n0; i++) r[i]           ^= z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i]   ^= z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        uint64_t z2i = z2[i];
        r[m + i] ^= zmid[i] ^ z0i ^ z2i;
    }
}

/* ── Public entry point ──────────────────────────────────────────────── */
void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

    hybrid_mul(unreduced, a, b, VEC_N_SIZE_64, HYBRID_KARATSUBA_DEPTH, tmp_buf);
    reduce(o, unreduced);
}
