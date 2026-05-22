/* src/poly_mul/scalar/karatsuba_zbc_exp/gf2x_clmul.c
 *
 * Experimental: Target 3 — Final Scalar Sweep applied on top of Target 4 Track A.
 *
 * Optimisations layered on the promoted karatsuba_zbc baseline:
 *   - memset-free output assembly/write discipline (Target 1, promoted)
 *   - 2-way software pipeline + hi⊕lo fusion in clmul_leaf (Target 4 Track A, promoted)
 *   - 4-way loop unrolling of the three Karatsuba assembly loops (Target 3 Track A, NEW)
 *
 * The 4-way unroll targets the three loops in step 5 of karatsuba_clmul:
 *   Loop 1: r[0..2n0-1] = z0[i]                      (low-half assign)
 *   Loop 2: r[2m..2m+2n1-1] = z2[i]                  (high-half assign)
 *   Loop 3: r[m+i] ^= zmid[i] ^ z0[i] ^ z2[i]        (middle XOR accumulate)
 *
 * These three loops appear at every interior recursion node and their combined
 * loop-control overhead (branch + addi per element) is eliminated for groups
 * of 4.  Tail loops handle the remaining 0-3 elements.
 *
 * Correctness is identical to the baseline: the same words are written in the
 * same order; only 4 iterations are folded per branch.
 *
 * The gf_mul() optimisation (native clmul) lives in the companion local gf.c.
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
 * Unchanged from the promoted karatsuba_zbc baseline (Target 4 Track A).
 * All zero-free correctness invariants are preserved.
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

/* ── Recursive Karatsuba with 4-way unrolled assembly loops ──────────────
 *
 * Step 5 (assemble r) uses three loops, each 4-way unrolled.
 *
 * Loop 1: r[0..2n0-1] = z0[i]             (low-half, direct assign)
 * Loop 2: r[2m..2m+2n1-1] = z2[i]         (high-half, direct assign)
 * Loop 3: r[m+i] ^= zmid[i]^z0i^z2[i]    (middle XOR)
 *
 * Loop 3 is split at index 2*n0: below it z0i=z0[i]; above it z0i=0
 * (at most 1 element since n1-n0 ∈ {0,1}).
 *
 * Correctness: identical writes in identical order to the baseline.
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

    /* 5) Assemble r — no pre-zero, 4-way unrolled ───────────────────────*/

    /* Loop 1: r[0..2n0-1] = z0[i] */
    {
        size_t len = 2 * n0;
        size_t i = 0;
        for (; i + 4 <= len; i += 4) {
            r[i]     = z0[i];
            r[i + 1] = z0[i + 1];
            r[i + 2] = z0[i + 2];
            r[i + 3] = z0[i + 3];
        }
        for (; i < len; i++) r[i] = z0[i];
    }

    /* Loop 2: r[2m..2m+2n1-1] = z2[i] */
    {
        size_t len  = 2 * n1;
        uint64_t *rh = r + 2 * m;
        size_t i = 0;
        for (; i + 4 <= len; i += 4) {
            rh[i]     = z2[i];
            rh[i + 1] = z2[i + 1];
            rh[i + 2] = z2[i + 2];
            rh[i + 3] = z2[i + 3];
        }
        for (; i < len; i++) rh[i] = z2[i];
    }

    /* Loop 3: r[m+i] ^= zmid[i] ^ z0i ^ z2[i]
     * Split at split=2*n0: below → z0i=z0[i]; above → z0i=0 */
    {
        uint64_t *rm  = r + m;
        size_t split  = 2 * n0;
        size_t i = 0;
        /* Sub-range A: [0, split) — 4-way unroll */
        for (; i + 4 <= split; i += 4) {
            rm[i]     ^= zmid[i]     ^ z0[i]     ^ z2[i];
            rm[i + 1] ^= zmid[i + 1] ^ z0[i + 1] ^ z2[i + 1];
            rm[i + 2] ^= zmid[i + 2] ^ z0[i + 2] ^ z2[i + 2];
            rm[i + 3] ^= zmid[i + 3] ^ z0[i + 3] ^ z2[i + 3];
        }
        for (; i < split; i++) rm[i] ^= zmid[i] ^ z0[i] ^ z2[i];
        /* Sub-range B: [split, 2*n1) — at most 1 element (n odd only) */
        for (; i < 2 * n1; i++) rm[i] ^= zmid[i] ^ z2[i];
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
