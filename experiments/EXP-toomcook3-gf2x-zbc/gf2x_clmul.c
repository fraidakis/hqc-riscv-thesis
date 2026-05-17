/* experiments/EXP-toomcook3-gf2x-zbc/gf2x_clmul.c
 *
 * Experimental Toom-Cook 3-way multiplication over GF(2)[x], evaluated on
 * GF(4) points {0, 1, omega, omega^2, infinity}.
 *
 * Base-case CLMUL code paths (clmul64 / clmul_leaf_n8 / clmul_leaf / reduce)
 * are inherited from the scalar karatsuba_zbc champion.
 */

#include <stdint.h>
#include <string.h>
#include "parameters.h"
#include "hqc_parameters.h"

/* ── RISC-V Zbc carry-less multiply base case ─────────────────────────────
 * Computes the full 128-bit carry-less product of two 64-bit words.
 */
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

#ifndef TC3_THRESHOLD
#define TC3_THRESHOLD 8u
#endif

/* Conservative stack scratch bound for TC3 experiment builds. */
#ifndef TC3_TMP_BUFFER_MULTIPLIER
#define TC3_TMP_BUFFER_MULTIPLIER 30u
#endif

#define TMP_BUFFER_WORDS (TC3_TMP_BUFFER_MULTIPLIER * VEC_N_SIZE_64)

/* ── Fixed-size leaf: n=8 fully unrolled (64 clmul64, zero branches) ─────
 *
 * Same zero-free write discipline as clmul_leaf_n4 extended to n=8.
 *
 * Structure mirrors clmul_leaf with n=8 unrolled:
 *   Row i=0: init pair(j=0,1) + loop pairs(j=2,3),(j=4,5),(j=6,7)
 *            → assigns r[0..8]
 *   Row i≥1: main pairs(j=0,1),(j=2,3),(j=4,5) + even-tail pair(j=6,7)
 *            → XOR into r[i..i+7]; fresh assign to r[i+n]=r[i+8]
 *
 * First-write proof sketch:
 *   r[0..8]  : first-written by row 0 (row 0 init/loop pairs)
 *   r[9]     : first-written by row 1 even-tail (r[1+8]=r[9])
 *   r[k]     : first-written by row k-8 even-tail for k = 9..15
 *   r[i+0..i+7] for i≥1 : all have been written by row 0 or prior rows ✓
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n8(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo0, hi0, lo1, hi1;

    /* ── Row i=0 ──────────────────────────────────────────────────────── */

    /* Init pair (j=0, j=1) */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;
    r[2] = hi1;

    /* Loop pair (j=2, j=3) */
    clmul64(a[0], b[2], &lo0, &hi0);
    clmul64(a[0], b[3], &lo1, &hi1);
    r[2] ^= lo0;
    r[3]  = hi0 ^ lo1;
    r[4]  = hi1;

    /* Loop pair (j=4, j=5) */
    clmul64(a[0], b[4], &lo0, &hi0);
    clmul64(a[0], b[5], &lo1, &hi1);
    r[4] ^= lo0;
    r[5]  = hi0 ^ lo1;
    r[6]  = hi1;

    /* Loop pair (j=6, j=7) */
    clmul64(a[0], b[6], &lo0, &hi0);
    clmul64(a[0], b[7], &lo1, &hi1);
    r[6] ^= lo0;
    r[7]  = hi0 ^ lo1;
    r[8]  = hi1;          /* r[0+n]=r[8]: fresh assign (ends row 0) */

    /* ── Row i=1 ──────────────────────────────────────────────────────── */

    /* Main loop pair (j=0, j=1): j+1=1 < n-1=7 → all three outputs XOR */
    clmul64(a[1], b[0], &lo0, &hi0);
    clmul64(a[1], b[1], &lo1, &hi1);
    r[1] ^= lo0;
    r[2] ^= hi0 ^ lo1;
    r[3] ^= hi1;

    /* Main loop pair (j=2, j=3): j+1=3 < 7 */
    clmul64(a[1], b[2], &lo0, &hi0);
    clmul64(a[1], b[3], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    /* Main loop pair (j=4, j=5): j+1=5 < 7 */
    clmul64(a[1], b[4], &lo0, &hi0);
    clmul64(a[1], b[5], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[1+8]=r[9] is fresh assign */
    clmul64(a[1], b[6], &lo0, &hi0);
    clmul64(a[1], b[7], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9]  = hi1;

    /* ── Row i=2 ──────────────────────────────────────────────────────── */

    clmul64(a[2], b[0], &lo0, &hi0);
    clmul64(a[2], b[1], &lo1, &hi1);
    r[2] ^= lo0;
    r[3] ^= hi0 ^ lo1;
    r[4] ^= hi1;

    clmul64(a[2], b[2], &lo0, &hi0);
    clmul64(a[2], b[3], &lo1, &hi1);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1;

    clmul64(a[2], b[4], &lo0, &hi0);
    clmul64(a[2], b[5], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[2+8]=r[10] is fresh assign */
    clmul64(a[2], b[6], &lo0, &hi0);
    clmul64(a[2], b[7], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10]  = hi1;

    /* ── Row i=3 ──────────────────────────────────────────────────────── */

    clmul64(a[3], b[0], &lo0, &hi0);
    clmul64(a[3], b[1], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    clmul64(a[3], b[2], &lo0, &hi0);
    clmul64(a[3], b[3], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[3], b[4], &lo0, &hi0);
    clmul64(a[3], b[5], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[3+8]=r[11] is fresh assign */
    clmul64(a[3], b[6], &lo0, &hi0);
    clmul64(a[3], b[7], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11]  = hi1;

    /* ── Row i=4 ──────────────────────────────────────────────────────── */

    clmul64(a[4], b[0], &lo0, &hi0);
    clmul64(a[4], b[1], &lo1, &hi1);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1;

    clmul64(a[4], b[2], &lo0, &hi0);
    clmul64(a[4], b[3], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    clmul64(a[4], b[4], &lo0, &hi0);
    clmul64(a[4], b[5], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[4+8]=r[12] is fresh assign */
    clmul64(a[4], b[6], &lo0, &hi0);
    clmul64(a[4], b[7], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12]  = hi1;

    /* ── Row i=5 ──────────────────────────────────────────────────────── */

    clmul64(a[5], b[0], &lo0, &hi0);
    clmul64(a[5], b[1], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[5], b[2], &lo0, &hi0);
    clmul64(a[5], b[3], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    clmul64(a[5], b[4], &lo0, &hi0);
    clmul64(a[5], b[5], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[5+8]=r[13] is fresh assign */
    clmul64(a[5], b[6], &lo0, &hi0);
    clmul64(a[5], b[7], &lo1, &hi1);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13]  = hi1;

    /* ── Row i=6 ──────────────────────────────────────────────────────── */

    clmul64(a[6], b[0], &lo0, &hi0);
    clmul64(a[6], b[1], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    clmul64(a[6], b[2], &lo0, &hi0);
    clmul64(a[6], b[3], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    clmul64(a[6], b[4], &lo0, &hi0);
    clmul64(a[6], b[5], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[6+8]=r[14] is fresh assign */
    clmul64(a[6], b[6], &lo0, &hi0);
    clmul64(a[6], b[7], &lo1, &hi1);
    r[12] ^= lo0;
    r[13] ^= hi0 ^ lo1;
    r[14]  = hi1;

    /* ── Row i=7 ──────────────────────────────────────────────────────── */

    clmul64(a[7], b[0], &lo0, &hi0);
    clmul64(a[7], b[1], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    clmul64(a[7], b[2], &lo0, &hi0);
    clmul64(a[7], b[3], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    clmul64(a[7], b[4], &lo0, &hi0);
    clmul64(a[7], b[5], &lo1, &hi1);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[7+8]=r[15] is fresh assign */
    clmul64(a[7], b[6], &lo0, &hi0);
    clmul64(a[7], b[7], &lo1, &hi1);
    r[13] ^= lo0;
    r[14] ^= hi0 ^ lo1;
    r[15]  = hi1;
}

/* ── Leaf multiply: 2-way software pipeline + hi⊕lo fusion ──────────────
 * General fallback for n ∉ {4, 8}.  Unchanged from stable champion.
 */
static void clmul_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    uint64_t lo0, hi0, lo1, hi1;

    /* n=1 guard: avoid reading b[1] that doesn't exist */
    if (__builtin_expect(n == 1, 0)) {
        clmul64(a[0], b[0], &lo0, &hi0);
        r[0] = lo0;
        r[1] = hi0;
        return;
    }

    /* ── i=0 row ─────────────────────────────────────────────────────────
     * All writes in this pass are fresh assigns (or fused assigns).
     * The interim assign to r[j+2] is updated by the XOR of the next pair.
     */

    /* First pair (j=0, j=1): issue both clmul64 before consuming results */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;   /* fused assign: hi from pair 0, lo from pair 1 */
    r[2] = hi1;          /* interim assign; will be XOR'd by next pair's lo */

    /* Main unrolled loop: pairs starting at j=2 */
    size_t j = 2;
    for (; j + 1 < n; j += 2) {
        clmul64(a[0], b[j],     &lo0, &hi0);
        clmul64(a[0], b[j + 1], &lo1, &hi1);
        r[j]     ^= lo0;          /* XOR: r[j] held interim-hi from prev pair */
        r[j + 1]  = hi0 ^ lo1;   /* fused assign */
        r[j + 2]  = hi1;          /* interim assign */
    }

    /* Scalar tail: handles last element when n is odd (j == n-1) */
    if (j < n) {
        clmul64(a[0], b[j], &lo0, &hi0);
        r[j]     ^= lo0;
        r[j + 1]  = hi0;
    }

    /* ── i≥1 rows ────────────────────────────────────────────────────────
     * All writes are XOR into already-initialised positions, except for
     * the very last write of each row: r[i+n] = hi (first write, assign).
     *
     * Main loop: pairs where both j and j+1 are in [0, n-2].
     * After the loop:
     *   n even → j == n-2 → handle "last pair" (j=n-2, j+1=n-1) where
     *            the third output r[i+n] is a fresh assign.
     *   n odd  → j == n-1 → handle "single tail" (j=n-1) similarly.
     */
    for (size_t i = 1; i < n; i++) {
        j = 0;

        /* Main loop: both elements of each pair are in [0, n-2] */
        for (; j + 1 < n - 1; j += 2) {
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            r[i + j]     ^= lo0;
            r[i + j + 1] ^= hi0 ^ lo1;  /* fused XOR */
            r[i + j + 2] ^= hi1;
        }

        /* Handle remaining element(s) */
        if (j < n - 1) {
            /* n even: j == n-2; pair (n-2, n-1); r[i+n] is the fresh assign */
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            r[i + j]     ^= lo0;
            r[i + j + 1] ^= hi0 ^ lo1;  /* fused XOR */
            r[i + n]      = hi1;          /* first write to r[n+i]: assign */
        } else {
            /* n odd: j == n-1; single tail */
            clmul64(a[i], b[j], &lo0, &hi0);
            r[i + j]  ^= lo0;
            r[i + n]   = hi0;             /* first write to r[n+i]: assign */
        }
    }
}

/* Recursive 6-product 3-way Karatsuba over GF(2)[x]. */
static void tc3_clmul(uint64_t * __restrict__ r,
                      const uint64_t * __restrict__ a,
                      const uint64_t * __restrict__ b,
                      size_t n,
                      uint64_t * __restrict__ tmp)
{
    if (n <= TC3_THRESHOLD) {
        if (n == 8u) {
            clmul_leaf_n8(r, a, b);
        } else {
            clmul_leaf(r, a, b, n);
        }
        return;
    }

    size_t k = (n + 2u) / 3u;
    size_t n2 = n - (2u * k);
    size_t pk = 2u * k;
    size_t p2k = 2u * n2;

    const uint64_t *a0 = a;
    const uint64_t *a1 = a + k;
    const uint64_t *a2 = a + (2u * k);
    const uint64_t *b0 = b;
    const uint64_t *b1 = b + k;
    const uint64_t *b2 = b + (2u * k);

    uint64_t *ea01 = tmp;           /* k */
    uint64_t *ea02 = ea01 + k;      /* k */
    uint64_t *ea12 = ea02 + k;      /* k */
    uint64_t *eb01 = ea12 + k;      /* k */
    uint64_t *eb02 = eb01 + k;      /* k */
    uint64_t *eb12 = eb02 + k;      /* k */

    uint64_t *p0 = eb12 + k;        /* 2k */
    uint64_t *p1 = p0 + pk;         /* 2k */
    uint64_t *p2 = p1 + pk;         /* 2k (only first 2*n2 valid) */
    uint64_t *p01 = p2 + pk;        /* 2k */
    uint64_t *p02 = p01 + pk;       /* 2k */
    uint64_t *p12 = p02 + pk;       /* 2k */

    uint64_t *child_buf = p12 + pk;

    for (size_t i = 0; i < k; i++) {
        uint64_t a2i = (i < n2) ? a2[i] : 0;
        uint64_t b2i = (i < n2) ? b2[i] : 0;

        ea01[i] = a0[i] ^ a1[i];
        ea02[i] = a0[i] ^ a2i;
        ea12[i] = a1[i] ^ a2i;

        eb01[i] = b0[i] ^ b1[i];
        eb02[i] = b0[i] ^ b2i;
        eb12[i] = b1[i] ^ b2i;
    }

    tc3_clmul(p0, a0, b0, k, child_buf);
    tc3_clmul(p1, a1, b1, k, child_buf);
    memset(p2, 0, pk * sizeof(uint64_t));
    tc3_clmul(p2, a2, b2, n2, child_buf);
    tc3_clmul(p01, ea01, eb01, k, child_buf);
    tc3_clmul(p02, ea02, eb02, k, child_buf);
    tc3_clmul(p12, ea12, eb12, k, child_buf);

    memset(r, 0, (2u * n) * sizeof(uint64_t));

    for (size_t i = 0; i < pk; i++) {
        size_t idx = i;
        if (idx < 2u * n) {
            r[idx] ^= p0[i];
        }
    }

    for (size_t i = 0; i < pk; i++) {
        size_t idx = k + i;
        if (idx < 2u * n) {
            r[idx] ^= (p0[i] ^ p1[i] ^ p01[i]);
        }
    }

    for (size_t i = 0; i < pk; i++) {
        uint64_t p2i = (i < p2k) ? p2[i] : 0;
        size_t idx = (2u * k) + i;
        if (idx < 2u * n) {
            r[idx] ^= (p0[i] ^ p1[i] ^ p2i ^ p02[i]);
        }
    }

    for (size_t i = 0; i < pk; i++) {
        uint64_t p2i = (i < p2k) ? p2[i] : 0;
        size_t idx = (3u * k) + i;
        if (idx < 2u * n) {
            r[idx] ^= (p1[i] ^ p2i ^ p12[i]);
        }
    }

    for (size_t i = 0; i < p2k; i++) {
        size_t idx = (4u * k) + i;
        if (idx < 2u * n) {
            r[idx] ^= p2[i];
        }
    }
}

/* ── Modular reduction mod (x^PARAM_N - 1) ──────────────────────────────*/
static void reduce(uint64_t * __restrict__ o, const uint64_t * __restrict__ a)
{
    int shift = (int)HQC_PARAM_N_MOD_64;
    int inv   = 64 - shift;

    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t r  = a[i + VEC_N_SIZE_64 - 1] >> shift;
        uint64_t carry = (shift == 0) ? 0 : (a[i + VEC_N_SIZE_64] << inv);
        o[i] = a[i] ^ r ^ carry;
    }
    o[VEC_N_SIZE_64 - 1] &= HQC_PARAM_LAST_WORD_MASK_64;
}

/* ── Public entry point ──────────────────────────────────────────────────*/
void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

    tc3_clmul(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
