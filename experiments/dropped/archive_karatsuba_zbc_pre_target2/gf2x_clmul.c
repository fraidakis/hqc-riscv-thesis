/* src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c
 *
 * Official karatsuba_zbc baseline implementation.
 *
 * Polynomial multiplication over GF(2)[x] using recursive Karatsuba with
 * RISC-V Zbc clmul/clmulh leaves, plus a memset-free output assembly/write
 * discipline to avoid redundant zeroing in hot recursion paths.
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

/* ── Leaf multiply: no memset ────────────────────────────────────────────
 *
 * Correctness argument for the init-free approach:
 *
 *  i=0 pass (first outer-loop row):
 *    - (i=0, j=0): r[0] = lo; r[1] = hi;          ← first writes, assign
 *    - (i=0, j=k), k≥1: r[k] ^= lo;               ← r[k] was written as 'hi'
 *                        r[k+1] = hi;              ←   at step k-1 (assign)
 *    After this pass r[0..n] are fully initialised.
 *
 *  i≥1 pass (remaining outer-loop rows), split at j=n-1:
 *    - j=0..n-2: r[i+j] ^= lo; r[i+j+1] ^= hi;
 *      All accessed positions are in [i, i+n-1] ⊆ [1, 2n-2].
 *      r[i..i+n-1] were initialised by the i=0 pass (r[0..n]) or by the
 *      j=n-1 step of the previous i row (r[n+1..n+i-1]).
 *    - j=n-1: r[i+n-1] ^= lo;  (already initialised as above)
 *             r[i+n]    = hi;   ← first ever write to r[n+i], assign
 *
 *  After all rows: r[0..2n-1] fully correct, zero bytes read uninitialised.
 */
static void clmul_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    uint64_t lo, hi;

    /* i=0: assign on first write per slot */
    clmul64(a[0], b[0], &lo, &hi);
    r[0] = lo;
    r[1] = hi;
    for (size_t j = 1; j < n; j++) {
        clmul64(a[0], b[j], &lo, &hi);
        r[j]     ^= lo;   /* r[j] already set as 'hi' by step j-1 */
        r[j + 1]  = hi;   /* r[j+1]: first write */
    }

    /* i≥1: XOR into already-initialised slots; assign on r[n+i] */
    for (size_t i = 1; i < n; i++) {
        for (size_t j = 0; j < n - 1; j++) {
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
        /* j = n-1: hi part lands on r[n+i], a position not yet touched */
        clmul64(a[i], b[n - 1], &lo, &hi);
        r[i + n - 1] ^= lo;
        r[i + n]      = hi;   /* first write to r[n+i] */
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
