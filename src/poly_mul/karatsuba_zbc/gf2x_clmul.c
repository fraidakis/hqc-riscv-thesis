/* src/poly_mul/karatsuba_zbc/gf2x_clmul.c
 *
 * Phase 1a — Karatsuba polynomial multiplication over GF(2)[x]
 * accelerated with the RISC-V Zbc carry-less multiply extension.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * Optimisation axes applied:
 *   1. clmul + clmulh base case: 128-bit carry-less product in 2 instructions
 *      (replaces the schoolbook shift-and-XOR loop, ~320 instr → 2 instr).
 *   2. KARATSUBA_THRESHOLD sweep: compile-time tunable cutoff (default = 8 words
 *      based on empirical sweep for HQC-128; override via -DKARATSUBA_THRESHOLD=N).
 *   3. Lazy reduction: multiplication result kept in 2*n unreduced buffer;
 *      reduction mod x^PARAM_N-1 done once at the end (matches reference).
 *
 * Verification: KAT vectors must pass after every change (run_kat.sh).
 */

#include <stdint.h>
#include <string.h>
#include "parameters.h"

/* ── RISC-V Zbc carry-less multiply base case ─────────────────────────────
 * Computes the full 128-bit carry-less product of two 64-bit words.
 * Uses inline asm to guarantee exactly clmul + clmulh back-to-back.
 * -march must include _zbc (enforced by the CMake toolchain flags).
 */
static __attribute__((always_inline)) inline
void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    /* Early-clobber (&) on %0 prevents the compiler from allocating the
     * same register for *lo and the input operands a (%2) or b (%3).
     * Without &, clmul may overwrite a before clmulh reads it, corrupting
     * the high half of the 128-bit product.
     */
    __asm__ volatile (
        "clmul  %0, %2, %3\n\t"
        "clmulh %1, %2, %3\n\t"
        : "=&r"(*lo), "=r"(*hi)
        : "r"(a), "r"(b)
    );
}

/* ── Karatsuba threshold ──────────────────────────────────────────────────
 * For n <= threshold words, use a direct clmul-based leaf multiply.
 * Empirical sweep for HQC-128 (n=277 words):
 *   T=1: 1,809,335   T=2: 1,094,119   T=4: 677,836
 *   T=8:   450,644   T=12-16: 354,187 (optimal, ≡ for this recursion tree)
 *   T=18:  360,287   T=32:    360,287
 * T=16 gives the optimal balance of Karatsuba reduce and leaf clmul cost.
 * Override at compile time: -DKARATSUBA_THRESHOLD=N
 */
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 16
#endif

/* ── Temporary buffer sizing (same formula as reference) ─────────────────
 * At each level: 8*n words.  Summing across levels: < 16*n total.
 */
#define TMP_BUFFER_WORDS (16 * VEC_N_SIZE_64)

/* ── Leaf multiply: n words × n words → 2n words using clmul ────────────
 * For small n (≤ KARATSUBA_THRESHOLD), performs a "schoolbook clmul" where
 * each 64×64 sub-multiplication is handled with clmul+clmulh (2 instructions)
 * instead of the shift-and-XOR loop (~320 instructions).
 *
 * Instruction scheduling: the sequential loop is already near-optimal for
 * instruction count (Spike measures instret).  A 2-wide dispatch combining
 * hi0 ^ lo1 was benchmarked but adds loop-overhead instructions (+9% instret).
 * On real OoO hardware the 2-wide form would win via ILP; for this simulator
 * baseline the sequential version is retained.
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

/* ── Recursive Karatsuba with clmul leaves ───────────────────────────────
 * Identical structure to the reference karatsuba_mul; only the base case
 * is replaced: schoolbook_mul → clmul_leaf.
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
    size_t n1 = n - m;  /* n1 == m or m+1 when n is odd */

    /* Slice temporary storage (8*n words at this level, rest for children) */
    uint64_t *z0   = tmp;           /* 2*n words: low × low   */
    uint64_t *z2   = z0 + 2 * n;   /* 2*n words: high × high  */
    uint64_t *zmid = z2 + 2 * n;   /* 2*n words: mid × mid    */
    uint64_t *ta   = zmid + 2 * n; /* n1 words: a_lo XOR a_hi */
    uint64_t *tb   = ta + n;       /* n1 words: b_lo XOR b_hi */
    uint64_t *child_buf = tmp + 8 * n;

    /* 1) z0 = a_lo * b_lo */
    karatsuba_clmul(z0, a, b, n0, child_buf);

    /* 2) z2 = a_hi * b_hi */
    karatsuba_clmul(z2, a + m, b + m, n1, child_buf);

    /* 3) ta = a_lo XOR a_hi,  tb = b_lo XOR b_hi  (handles odd n1) */
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }

    /* 4) zmid = ta * tb */
    karatsuba_clmul(zmid, ta, tb, n1, child_buf);

    /* 5) Assemble r = z0  +  (zmid - z0 - z2) * x^m  +  z2 * x^(2m) */
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2 * n0; i++) r[i]           ^= z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i]   ^= z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i  = (i < 2 * n0 ? z0[i]  : 0);
        uint64_t z2i  = z2[i];
        r[m + i] ^= zmid[i] ^ z0i ^ z2i;
    }
}

/* ── Modular reduction mod (x^PARAM_N - 1) ──────────────────────────────
 * Identical to reference: folds the high half back into the low half.
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

/* ── Public entry point ──────────────────────────────────────────────────
 * Computes o = a * b mod (x^PARAM_N - 1) over GF(2)[x].
 */
void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

    karatsuba_clmul(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
