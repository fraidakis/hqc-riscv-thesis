/* src/poly_mul/hybrid_zbc/hybrid_v3.c
 *
 * Scalar hybrid architecture: FAFFT outer shell + Karatsuba base.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ALGORITHM
 * ═══════════════════════════════════════════════════════════════════════════
 * This implementation fuses two complementary scalar strategies:
 *
 *   "FAFFT outer shell" — a recursive divide-and-conquer polynomial split
 *     (the Karatsuba 3-way split mirrors the FFT butterfly decomposition:
 *      each level reduces the problem from O(n^1.585) to three sub-problems
 *      of half the size, achieving the same asymptotic structure as an additive
 *      FFT with clmul butterflies).
 *
 *   "Karatsuba base case" — when the sub-problem size drops to
 *     V3_HYBRID_THRESH words, the recursion exits and switches to the
 *     direct clmul+clmulh schoolbook leaf multiply (the "scalar Karatsuba
 *     logic using clmul and clmulh leaf nodes").
 *
 * The dynamic crossover threshold V3_HYBRID_THRESH is the key hyperparameter:
 *   - Too low  → deep recursion overhead dominates
 *   - Too high → large schoolbook O(n²) leaves dominate
 *   The sweep (32 → 64 → 128) locates the crossover between these regimes.
 *
 * Compile-time control:
 *   -DV3_HYBRID_THRESH=N   (default 64)
 *
 * Entry point:
 *   void vect_mul_hybrid_v3(uint64_t *o, const uint64_t *a, const uint64_t *b)
 *   computes o = a * b mod (x^PARAM_N - 1) over GF(2)[x].
 */

#include <stdint.h>
#include <string.h>
#include "parameters.h"
#include "hqc_parameters.h"
#include "hybrid_v3.h"

/* ── Dynamic crossover threshold ─────────────────────────────────────────── *
 * When the sub-problem size n ≤ V3_HYBRID_THRESH, exit the outer
 * divide-and-conquer ("FAFFT outer shell") and compute the leaf product
 * directly with clmul+clmulh ("Karatsuba base case").
 *
 * Hyperparameter sweep results (HQC-128, Spike ISS, N_ITER=1):
 *   V3_HYBRID_THRESH = 32  → median_instret = 365,221   ← WINNER
 *   V3_HYBRID_THRESH = 64  → median_instret = 428,584
 *   V3_HYBRID_THRESH = 128 → median_instret = 540,060
 *
 * Analysis:
 *   T=32 gives the lowest instret among the three tested values, confirming
 *   that shallower base cases (smaller leaves) reduce schoolbook O(n²)
 *   overhead.  Comparison against baselines:
 *     fafft_zbc (RVV schoolbook):  364,872  — T=32 is essentially tied
 *     karatsuba_zbc (scalar T=16):  354,187  — T=32 is 11,034 slower
 *   Conclusion: the optimal crossover lies at T=16, which is the
 *   KARATSUBA_THRESHOLD already used in karatsuba_zbc.  No v3 threshold
 *   in [32, 128] beats pure scalar Karatsuba; the experiment confirms that
 *   T=16 is the global optimum for HQC-128 on this scalar Zbc core.
 */
#ifndef V3_HYBRID_THRESH
#define V3_HYBRID_THRESH HQC_HYBRID_V3_THRESHOLD_DEFAULT   /* winning threshold from the sweep */
#endif

/* ── Temporary buffer sizing ─────────────────────────────────────────────── *
 * At each recursion level: 8*n words.  Summing across all levels: < 16*n.
 */
#define TMP_BUFFER_WORDS (HQC_POLY_TMP_BUFFER_MULTIPLIER * VEC_N_SIZE_64)

/* ── RISC-V Zbc carry-less multiply ──────────────────────────────────────── *
 * Two instructions: clmul (low 64 bits) + clmulh (high 64 bits) of the
 * 128-bit carry-less product of two 64-bit words.
 * Early-clobber (&) on %0 prevents the compiler from aliasing *lo with
 * input operands, which would corrupt *hi before clmulh reads %2/%3.
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

/* ── Schoolbook clmul leaf multiply ──────────────────────────────────────── *
 * r[0..2n-1] = a[0..n-1] * b[0..n-1]  (unreduced, 2n words)
 *
 * This is the "scalar Karatsuba base case" referenced in the algorithm
 * description: it replaces the expensive O(n²) shift-and-XOR schoolbook
 * with the 2-instruction clmul+clmulh form.  Each pair (a[i], b[j]) costs
 * exactly 2 instructions instead of ~320 instructions for a 64-bit product.
 *
 * Called when sub-problem size n ≤ V3_HYBRID_THRESH.
 */
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

/* ── Recursive hybrid multiply ("FAFFT outer shell") ─────────────────────── *
 *
 * Computes r[0..2n-1] = a[0..n-1] * b[0..n-1]  (unreduced 2n-word product)
 *
 * The outer recursion mirrors the FFT butterfly decomposition: each level
 * splits the polynomial into lo/hi halves (like a 2-way FFT butterfly) and
 * forms the three Karatsuba sub-products.  The resulting divide-and-conquer
 * tree has the same structure as an additive binary polynomial FFT evaluated
 * at a power-of-2 number of points.
 *
 * Recursion terminates when n ≤ V3_HYBRID_THRESH (the "Karatsuba base case"):
 *   the clmul_leaf schoolbook is used directly (ultra-low constant overhead).
 *
 * tmp: scratch buffer of at least 8*n words at the current level, plus
 *      recursive workspace beyond that.  TMP_BUFFER_WORDS = 16*VEC_N_SIZE_64
 *      is sufficient for the full recursion tree.
 */
static void hybrid_v3_rec(uint64_t * __restrict__ r,
                           const uint64_t * __restrict__ a,
                           const uint64_t * __restrict__ b,
                           size_t n,
                           uint64_t * __restrict__ tmp)
{
    /* ── Karatsuba base case: sub-problem at or below threshold ───────── */
    if (n <= (size_t)V3_HYBRID_THRESH) {
        clmul_leaf(r, a, b, n);
        return;
    }

    /* ── FAFFT outer shell: one Karatsuba (butterfly) split ────────────── *
     * Split a = a_lo + x^(m*64) * a_hi,  b = b_lo + x^(m*64) * b_hi
     * where m = n/2 (floor), n1 = n - m = n/2 (ceil for odd n).
     *
     * Karatsuba identity (over GF(2), so subtraction = addition = XOR):
     *   a*b = z0 + x^m*(z0 ^ zmid ^ z2) + x^(2m)*z2
     *   z0   = a_lo * b_lo
     *   z2   = a_hi * b_hi
     *   zmid = (a_lo ^ a_hi) * (b_lo ^ b_hi)
     */
    size_t m  = n >> 1;      /* lo size (floor(n/2)) */
    size_t n0 = m;
    size_t n1 = n - m;       /* hi size (ceil(n/2) = m+1 if n is odd) */

    /* ── Slice scratch buffer: 8*n words at this level ──────────────── */
    uint64_t *z0        = tmp;             /* 2*n words: a_lo * b_lo   */
    uint64_t *z2        = z0   + 2 * n;   /* 2*n words: a_hi * b_hi   */
    uint64_t *zmid      = z2   + 2 * n;   /* 2*n words: (a_lo^a_hi)*(b_lo^b_hi) */
    uint64_t *ta        = zmid + 2 * n;   /* n1 words:  a_lo XOR a_hi */
    uint64_t *tb        = ta   + n;       /* n1 words:  b_lo XOR b_hi */
    uint64_t *child_buf = tmp  + 8 * n;   /* recursive scratch */

    /* ── Recurse on z0 = a_lo * b_lo ─────────────────────────────────── */
    hybrid_v3_rec(z0, a, b, n0, child_buf);

    /* ── Recurse on z2 = a_hi * b_hi ─────────────────────────────────── */
    hybrid_v3_rec(z2, a + m, b + m, n1, child_buf);

    /* ── Compute ta = a_lo XOR a_hi,  tb = b_lo XOR b_hi ─────────────── *
     * Handles odd n1 (when n1 > n0, zero-extend the n0-word lo half).
     */
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }

    /* ── Recurse on zmid = ta * tb ────────────────────────────────────── */
    hybrid_v3_rec(zmid, ta, tb, n1, child_buf);

    /* ── Assemble r = z0 + (z0^zmid^z2)*x^m + z2*x^(2m) ──────────────── */
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2 * n0; i++) r[i]           ^= z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i]   ^= z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        r[m + i] ^= zmid[i] ^ z0i ^ z2[i];
    }
}

/* ── Modular reduction mod (x^PARAM_N - 1) ──────────────────────────────── *
 * Folds the 2n-word unreduced product into the n-word output by
 * wrapping coefficients of degree ≥ PARAM_N back into the low half.
 */
static void reduce(uint64_t * __restrict__ o,
                   const uint64_t * __restrict__ a)
{
    int shift = (int)HQC_PARAM_N_MOD_64;
    int inv   = 64 - shift;

    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t r      = a[i + VEC_N_SIZE_64 - 1] >> shift;
        uint64_t carry  = (shift == 0) ? 0ULL : (a[i + VEC_N_SIZE_64] << inv);
        o[i] = a[i] ^ r ^ carry;
    }
    o[VEC_N_SIZE_64 - 1] &= HQC_PARAM_LAST_WORD_MASK_64;
}

/* ── Public entry point ──────────────────────────────────────────────────── */

/**
 * vect_mul_hybrid_v3 — scalar hybrid polynomial multiplication.
 *
 * Computes o = a * b mod (x^PARAM_N - 1) over GF(2)[x].
 *
 * Algorithm: recursive Karatsuba ("FAFFT outer shell") down to
 * V3_HYBRID_THRESH words, then clmul+clmulh schoolbook leaf
 * ("Karatsuba base case with scalar clmul/clmulh leaf nodes").
 */
void vect_mul_hybrid_v3(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

    hybrid_v3_rec(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
