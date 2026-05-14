/* src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c
 *
 * Official karatsuba_zbc implementation — Target 4 Track A applied.
 *
 * Polynomial multiplication over GF(2)[x] using recursive Karatsuba with
 * RISC-V Zbc clmul/clmulh leaves.  Incorporates all prior optimisations:
 *   - memset-free output assembly/write discipline (Target 1)
 *   - 2-way software pipeline + hi⊕lo fusion in clmul_leaf (Target 4 Track A)
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

/* ── Karatsuba threshold ──────────────────────────────────────────────────*/
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD HQC_KARATSUBA_THRESHOLD_DEFAULT
#endif

/* ── Temporary buffer sizing ─────────────────────────────────────────────*/
#define TMP_BUFFER_WORDS (HQC_POLY_TMP_BUFFER_MULTIPLIER * VEC_N_SIZE_64)

/* ── Leaf multiply: 2-way software pipeline + hi⊕lo fusion ──────────────
 *
 * Zero-free correctness invariants from karatsuba_zbc are preserved.
 * The optimisations only reorder and fuse the arithmetic; the set of writes
 * to each output word is identical to the scalar baseline.
 *
 *  i=0 row:
 *    First pair (j=0, j=1): r[0]=lo0 (assign), r[1]=hi0^lo1 (fused assign),
 *    r[2]=hi1 (interim assign, updated by next pair's lo).
 *    Pairs (j=2,4,...): r[j] ^= lo0 (XOR into interim-hi), r[j+1]=hi0^lo1
 *    (fused assign), r[j+2]=hi1 (interim assign).
 *    Scalar tail (odd n): r[n-1] ^= lo, r[n] = hi.
 *
 *  i≥1 rows:
 *    Pairs (j=0,1), (j=2,3),... while j+1 ≤ n-2:
 *      r[i+j] ^= lo0; r[i+j+1] ^= (hi0^lo1); r[i+j+2] ^= hi1.
 *    Last pair when n is even (j=n-2): as above, but r[i+n] = hi1 (assign).
 *    Single tail when n is odd (j=n-1): r[i+n-1] ^= lo, r[i+n] = hi (assign).
 *
 *  n=1 guard: trivial single-product case, no pairing needed.
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

/* ── Recursive Karatsuba with clmul leaves and no memset ─────────────────
 *
 * Assembly step (step 5) replaces
 *     memset(r, 0, 2n) + three XOR loops
 * with direct-assign loops for z0 and z2 followed by an XOR loop for zmid.
 *
 * Correctness: after loop-1 (z0→r[0..2n0-1]) and loop-2 (z2→r[2m..2m+2n1-1])
 * every position in r[0..2n-1] has been assigned exactly once
 * (loop-1 covers the low half, loop-2 the high half; they are adjacent and
 * non-overlapping because 2n0=2m and loop-2 starts at 2m).
 * Loop-3 then XORs zmid^z0^z2 into the already-initialised middle segment
 * r[m..m+2n1-1].
 */
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

    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;

    uint64_t *z0        = tmp;
    uint64_t *z2        = z0  + 2 * n;
    uint64_t *zmid      = z2  + 2 * n;
    uint64_t *ta        = zmid + 2 * n;
    uint64_t *tb        = ta  + n;
    uint64_t *child_buf = tmp + 8 * n;

    /* 1) z0 = a_lo * b_lo */
    karatsuba_clmul(z0, a, b, n0, child_buf);

    /* 2) z2 = a_hi * b_hi */
    karatsuba_clmul(z2, a + m, b + m, n1, child_buf);

    /* 3) ta = a_lo XOR a_hi,  tb = b_lo XOR b_hi */
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }

    /* 4) zmid = ta * tb */
    karatsuba_clmul(zmid, ta, tb, n1, child_buf);

    /* 5) Assemble r = z0 + (zmid^z0^z2)*x^m + z2*x^(2m)  — no pre-zero */

    /* Low half: r[0..2n0-1] ← z0 (direct assign, first writes) */
    for (size_t i = 0; i < 2 * n0; i++) r[i] = z0[i];

    /* High half: r[2m..2m+2n1-1] ← z2 (direct assign, first writes) */
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i] = z2[i];

    /* Middle: r[m..m+2n1-1] ^= zmid^z0^z2 (positions already initialised) */
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        r[m + i] ^= zmid[i] ^ z0i ^ z2[i];
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

    karatsuba_clmul(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
