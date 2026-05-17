/* src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c
 *
 * Official karatsuba_zbc implementation — V2-A3 ablation applied (2026-05-17).
 *
 * Optimisations layered on the prior karatsuba_zbc baseline:
 *   - memset-free output assembly/write discipline (Target 1, promoted)
 *   - 2-way software pipeline + hi⊕lo fusion in clmul_leaf (Target 4 Track A, promoted)
 *   - 4-way loop unrolling of the three Karatsuba assembly loops (Target 3 Track A, promoted)
 *   - fixed-size leaf specialization: clmul_leaf_n8 (V2-A3, promoted 2026-05-15)
 *   - dead-code ablation: clmul_leaf_n4 + case 4 dispatch removed (2026-05-17)
 *
 * Ablation study (EXP-karatsuba-ablation, 2026-05-17):
 *   At T=16 the recursion tree for HQC-128 (n=277) never produces n=4 leaves.
 *   clmul_leaf_n4 was dead code.  Removing it and its case 4 switch arm also
 *   eliminates the dead comparison on every leaf call, yielding a further
 *   435-instruction reduction:
 *     V2-A3 champion (with clmul_leaf_n8): 274,131 instret
 *
 * Full optimisation history vs original baseline (360,961 instret):
 *   Total reduction: 360,961 → 274,131 instret  (−86,830, −24.05%)
 *
 * KAT validated: 300/300 vectors across HQC-{128,192,256} (2026-05-17).
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

/* ── Karatsuba threshold ──────────────────────────────────────────────────
 * Default: HQC_KARATSUBA_THRESHOLD_DEFAULT (=16), which is the V2-A1 sweep
 * winner.  Override via -DKARATSUBA_THRESHOLD=4|8|16 at compile time for
 * experimental builds (karatsuba_zbc_exp via -DHQC_KARATSUBA_THRESHOLD=N).
 */
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD HQC_KARATSUBA_THRESHOLD_DEFAULT
#endif

/* ── Temporary buffer sizing ─────────────────────────────────────────────*/
#define TMP_BUFFER_WORDS (HQC_POLY_TMP_BUFFER_MULTIPLIER * VEC_N_SIZE_64)

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

/* ── Recursive Karatsuba with fixed-size leaf dispatch ───────────────────
 *
 * Ablation variant: n=4 specialization removed (dead code at T=16).
 * Switch dispatches n=8 → clmul_leaf_n8 only; all other n fall to the
 * general clmul_leaf fallback.  KARATSUBA_THRESHOLD = 16 (default).
 *
 * The 4-way unrolled assembly loops (Target 3 Track A) are preserved.
 */
static void karatsuba_clmul(uint64_t * __restrict__ r,
                             const uint64_t * __restrict__ a,
                             const uint64_t * __restrict__ b,
                             size_t n,
                             uint64_t * __restrict__ tmp)
{
    if (n <= KARATSUBA_THRESHOLD) {
        switch (n) {
        case 8: clmul_leaf_n8(r, a, b); return;
        default: clmul_leaf(r, a, b, n); return;
        }
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
