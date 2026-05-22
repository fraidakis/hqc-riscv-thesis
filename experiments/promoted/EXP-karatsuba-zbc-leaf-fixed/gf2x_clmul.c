/* experiments/EXP-karatsuba-zbc-leaf-fixed/gf2x_clmul.c
 *
 * EXP: V2-A3 Fixed-Size Leaf Specialization + V2-A1 Threshold Sweep
 *
 * Tracks applied (beyond the stable karatsuba_zbc champion):
 *   V2-A3 — fully unrolled fixed-size leaf functions clmul_leaf_n4 and
 *            clmul_leaf_n8; zero branches, zero loops, all 16 / 64
 *            clmul64 calls visible to the compiler for optimal scheduling.
 *   V2-A1 — default threshold lowered to 8 (was 16); sweep T∈{4,8,16}
 *            via -DKARATSUBA_THRESHOLD=N at compile time.
 *
 * Hypothesis: eliminating inner-loop control overhead (branch + addi per
 * row) in the leaf multiplier reduces instret for vect_mul HQC-128 below
 * the current champion baseline of 283,686.
 *
 * Promotion criterion: best instret < 283,686 AND all KATs pass.
 *
 * Baseline optimizations carried forward unchanged:
 *   - memset-free output write discipline (Target 1)
 *   - 2-way software pipeline + hi⊕lo fusion in clmul_leaf (Target 4 Track A)
 *   - 4-way loop unrolling of Karatsuba assembly loops (Target 3 Track A)
 *
 * Experiment id: EXP-karatsuba-zbc-leaf-fixed
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

/* ── Fixed-size leaf: n=4 fully unrolled (16 clmul64, zero branches) ─────
 *
 * Zero-free write discipline: first write to each output word uses plain
 * assign (=); all subsequent accumulations use XOR (^=).
 *
 * Structure mirrors clmul_leaf with n=4 unrolled:
 *   Row i=0: init pair(j=0,1) + loop pair(j=2,3)
 *            → assigns r[0..4]
 *   Row i≥1: main pair(j=0,1) + even-tail pair(j=2,3)
 *            → XOR into r[i..i+3]; fresh assign to r[i+n]=r[i+4]
 *
 * Verified correct: all first-writes are plain assigns; r[i+n] per row is
 * written exactly once (assign) since no earlier row touches that index.
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n4(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo0, hi0, lo1, hi1;

    /* ── Row i=0 ──────────────────────────────────────────────────────── */

    /* Init pair (j=0, j=1) */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;   /* fused assign */
    r[2] = hi1;          /* interim assign; XOR'd by next pair's lo */

    /* Loop pair (j=2, j=3) — only pair in main loop for n=4 */
    clmul64(a[0], b[2], &lo0, &hi0);
    clmul64(a[0], b[3], &lo1, &hi1);
    r[2] ^= lo0;
    r[3]  = hi0 ^ lo1;   /* fused assign */
    r[4]  = hi1;          /* r[0+n]=r[4]: fresh assign, not yet touched */

    /* ── Row i=1 ──────────────────────────────────────────────────────── */

    /* Main loop pair (j=0, j=1): j+1=1 < n-1=3 → both outputs XOR */
    clmul64(a[1], b[0], &lo0, &hi0);
    clmul64(a[1], b[1], &lo1, &hi1);
    r[1] ^= lo0;
    r[2] ^= hi0 ^ lo1;   /* fused XOR */
    r[3] ^= hi1;

    /* Even-tail pair (j=2, j=3): r[1+4]=r[5] is fresh assign */
    clmul64(a[1], b[2], &lo0, &hi0);
    clmul64(a[1], b[3], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;   /* fused XOR */
    r[5]  = hi1;          /* r[1+n]=r[5]: first and only write */

    /* ── Row i=2 ──────────────────────────────────────────────────────── */

    /* Main loop pair (j=0, j=1) */
    clmul64(a[2], b[0], &lo0, &hi0);
    clmul64(a[2], b[1], &lo1, &hi1);
    r[2] ^= lo0;
    r[3] ^= hi0 ^ lo1;
    r[4] ^= hi1;

    /* Even-tail pair (j=2, j=3): r[2+4]=r[6] is fresh assign */
    clmul64(a[2], b[2], &lo0, &hi0);
    clmul64(a[2], b[3], &lo1, &hi1);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6]  = hi1;          /* r[2+n]=r[6]: first and only write */

    /* ── Row i=3 ──────────────────────────────────────────────────────── */

    /* Main loop pair (j=0, j=1) */
    clmul64(a[3], b[0], &lo0, &hi0);
    clmul64(a[3], b[1], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    /* Even-tail pair (j=2, j=3): r[3+4]=r[7] is fresh assign */
    clmul64(a[3], b[2], &lo0, &hi0);
    clmul64(a[3], b[3], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7]  = hi1;          /* r[3+n]=r[7]: first and only write */
}

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
     */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;
    r[2] = hi1;

    size_t j = 2;
    for (; j + 1 < n; j += 2) {
        clmul64(a[0], b[j],     &lo0, &hi0);
        clmul64(a[0], b[j + 1], &lo1, &hi1);
        r[j]     ^= lo0;
        r[j + 1]  = hi0 ^ lo1;
        r[j + 2]  = hi1;
    }

    if (j < n) {
        clmul64(a[0], b[j], &lo0, &hi0);
        r[j]     ^= lo0;
        r[j + 1]  = hi0;
    }

    /* ── i≥1 rows ────────────────────────────────────────────────────────
     * All writes XOR into already-initialised positions, except r[i+n]
     * which is a fresh assign (first write to that index).
     */
    for (size_t i = 1; i < n; i++) {
        j = 0;

        for (; j + 1 < n - 1; j += 2) {
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            r[i + j]     ^= lo0;
            r[i + j + 1] ^= hi0 ^ lo1;
            r[i + j + 2] ^= hi1;
        }

        if (j < n - 1) {
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            r[i + j]     ^= lo0;
            r[i + j + 1] ^= hi0 ^ lo1;
            r[i + n]      = hi1;
        } else {
            clmul64(a[i], b[j], &lo0, &hi0);
            r[i + j]  ^= lo0;
            r[i + n]   = hi0;
        }
    }
}

/* ── Recursive Karatsuba with fixed-size leaf dispatch ───────────────────
 *
 * Identical to the stable champion except:
 *   1. The leaf dispatch uses a switch to route n=4 → clmul_leaf_n4 and
 *      n=8 → clmul_leaf_n8, eliminating all inner-loop control overhead
 *      for those sizes.
 *   2. Default KARATSUBA_THRESHOLD is 8 (was 16); sweep via -D flag.
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
        case 4: clmul_leaf_n4(r, a, b); return;
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
        for (; i + 4 <= split; i += 4) {
            rm[i]     ^= zmid[i]     ^ z0[i]     ^ z2[i];
            rm[i + 1] ^= zmid[i + 1] ^ z0[i + 1] ^ z2[i + 1];
            rm[i + 2] ^= zmid[i + 2] ^ z0[i + 2] ^ z2[i + 2];
            rm[i + 3] ^= zmid[i + 3] ^ z0[i + 3] ^ z2[i + 3];
        }
        for (; i < split; i++) rm[i] ^= zmid[i] ^ z0[i] ^ z2[i];
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
