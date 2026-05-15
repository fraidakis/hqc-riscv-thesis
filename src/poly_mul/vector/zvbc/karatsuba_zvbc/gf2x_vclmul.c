/* src/poly_mul/karatsuba_zvbc/gf2x_vclmul_c3.c
 *
 * C-3a sandbox: assign-first-write discipline in vclmul_leaf.
 * Derived from gf2x_vclmul.c — only vclmul_leaf is modified.
 *
 * Change: replace memset(r, 0, 2*n) with:
 *   1. RVV zero of upper half only: vse64(r+n, vmv.v.x(0)) [1 instruction]
 *   2. Peeled i=0 outer iteration: lo uses direct vse64 (assign) since
 *      r[0..n-1] are uninitialised at that point.
 *
 * Correctness argument:
 *   r[0..n-1]: first written by i=0 lo-assign. No prior read. Safe.
 *   r[n..2n-1]: first-write always arrives from a hi strip which XOR-
 *               accumulates. Pre-zeroing these positions with one RVV
 *               store makes the XOR-accumulate behave as assignment on
 *               first touch. Correct.
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include "parameters.h"

#ifndef HQC_POLYMUL_ZVBC
#  error "gf2x_vclmul.c must be compiled with -DHQC_POLYMUL_ZVBC=1"
#endif

/* ── Karatsuba threshold ──────────────────────────────────────────────────
 * Same value as karatsuba_zbc; keep the same recursion depth.
 */
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 16
#endif

/* ── Temporary buffer sizing ─────────────────────────────────────────────
 * Identical formula to karatsuba_zbc.
 */
#define TMP_BUFFER_WORDS (16 * VEC_N_SIZE_64)

/* ── RVV helpers: Assign-on-First-Write memory discipline ────────────────
 *
 * Transplanted from karatsuba_zbc_rvv (Track B) to eliminate the recursive
 * memset() bottleneck in karatsuba_vclmul.  Together they implement the
 * zero-free Karatsuba combine (Step 5) without pre-zeroing r[].
 */

/* rvv_copy_u64: Direct-assign n_words uint64_t words from src → dst.
 * Used for Loop 1 (r = z0) and Loop 2 (r+2m = z2) in Karatsuba assemble.
 * LMUL=m4 → 16 uint64_t per iteration at VLEN=256.
 */
static inline void rvv_copy_u64(uint64_t * __restrict__ dst,
                                const uint64_t * __restrict__ src,
                                size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m4(n);
        vuint64m4_t v = __riscv_vle64_v_u64m4(src, vl);
        __riscv_vse64_v_u64m4(dst, v, vl);
        src += vl;
        dst += vl;
        n   -= vl;
    }
}

/* rvv_xor3_u64: dst[i] ^= zmid[i] ^ (z0 ? z0[i] : 0) ^ z2[i].
 * Used for Loop 3A (src_z0 != NULL) and Loop 3B (src_z0 == NULL, odd n).
 * LMUL=m2 accommodates four register groups (dst, zmid, z0, z2) within
 * the 32-register file.  At VLEN=256: 8 uint64_t per iteration.
 */
static inline void rvv_xor3_u64(uint64_t       * __restrict__ dst,
                                const uint64_t * __restrict__ src_zmid,
                                const uint64_t * __restrict__ src_z0,   /* NULL → 0 */
                                const uint64_t * __restrict__ src_z2,
                                size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m2(n);
        vuint64m2_t vdst = __riscv_vle64_v_u64m2(dst,      vl);
        vuint64m2_t vmid = __riscv_vle64_v_u64m2(src_zmid, vl);
        vuint64m2_t vz2  = __riscv_vle64_v_u64m2(src_z2,   vl);
        vuint64m2_t acc;
        if (src_z0 != (const uint64_t *)0) {
            vuint64m2_t vz0 = __riscv_vle64_v_u64m2(src_z0, vl);
            acc = __riscv_vxor_vv_u64m2(vmid, vz0, vl);
        } else {
            acc = vmid;
        }
        acc  = __riscv_vxor_vv_u64m2(acc,  vz2,  vl);
        vdst = __riscv_vxor_vv_u64m2(vdst, acc,  vl);
        __riscv_vse64_v_u64m2(dst, vdst, vl);
        dst      += vl;
        src_zmid += vl;
        if (src_z0 != (const uint64_t *)0) src_z0 += vl;
        src_z2   += vl;
        n        -= vl;
    }
}

/* ── Vectorised leaf multiply: r = a × b (unreduced, 2n words) ──────────
 *
 * Iter 1 (Advanced): Outer-loop register blocking (4x unroll).
 * Process 4 outer iterations at a time: load a[i..i+3] into scalar
 * registers, then for each j-block splat all four scalars and issue 4×2
 * vclmul/vclmulh pairs against the same b[j..j+vl-1] vector.  This
 * saturates the dual-issue pipeline and hides memory-access latency by
 * keeping 8 vector multiply results in flight simultaneously.
 *
 * Scalar tail: processes the remaining (n % 4) outer iterations one at
 * a time using the original single-strip loop.
 *
 * Correctness: same as the non-unrolled version — each contribution
 * lo(a[i],b[j]) → r[i+j], hi(a[i],b[j]) → r[i+j+1] is XOR-accumulated
 * independently; the unrolled strips are independent.
 */
static void vclmul_leaf(uint64_t * __restrict__ r,
                        const uint64_t * __restrict__ a,
                        const uint64_t * __restrict__ b,
                        size_t n)
{
    /* ── C-3a: Assign-First-Write ──────────────────────────────────────
     * Lower half r[0..n-1]: initialised by direct assign in the peeled
     *   i=0 lo strip below.  No pre-zeroing needed.
     * Upper half r[n..2n-1]: first-write always arrives from a hi strip
     *   (XOR-accumulate).  Pre-zero with one RVV vse64.v instead of the
     *   full scalar memset byte-loop (saves ~2n scalar instructions).
     */
    {
        size_t vlz = __riscv_vsetvl_e64m4(n);
        __riscv_vse64_v_u64m4(r + n, __riscv_vmv_v_x_u64m4(0, vlz), vlz);
    }

    /* ── i=0 peeled: lo uses direct store (r[0..n-1] uninitialised) ─── */
    {
        uint64_t a0 = a[0];
        size_t j = 0;
        while (j < n) {
            size_t vl = __riscv_vsetvl_e64m4(n - j);
            vuint64m4_t vb   = __riscv_vle64_v_u64m4(b + j, vl);
            vuint64m4_t va0  = __riscv_vmv_v_x_u64m4(a0, vl);
            vuint64m4_t vlo0 = __riscv_vclmul_vv_u64m4(va0, vb, vl);
            vuint64m4_t vhi0 = __riscv_vclmulh_vv_u64m4(va0, vb, vl);
            /* lo: ASSIGN — r[j..j+vl-1] have not been written yet */
            __riscv_vse64_v_u64m4(r + j, vlo0, vl);
            /* hi: load-XOR-store — r[j+1..j+vl-1] hold lo; r[j+vl] pre-zeroed */
            vuint64m4_t vr = __riscv_vle64_v_u64m4(r + j + 1, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vhi0, vl);
            __riscv_vse64_v_u64m4(r + j + 1, vr, vl);
            j += vl;
        }
    }

    size_t i = 1;  /* i=0 already handled above */

    /* ── 4x unrolled outer loop ─────────────────────────────────────── */
    for (; i + 3 < n; i += 4) {
        uint64_t a0 = a[i];
        uint64_t a1 = a[i + 1];
        uint64_t a2 = a[i + 2];
        uint64_t a3 = a[i + 3];

        size_t j = 0;
        while (j < n) {
            size_t vl = __riscv_vsetvl_e64m4(n - j);

            vuint64m4_t vb = __riscv_vle64_v_u64m4(b + j, vl);

            /* Strip 0: a[i] */
            vuint64m4_t va0  = __riscv_vmv_v_x_u64m4(a0, vl);
            vuint64m4_t vlo0 = __riscv_vclmul_vv_u64m4(va0, vb, vl);
            vuint64m4_t vhi0 = __riscv_vclmulh_vv_u64m4(va0, vb, vl);

            /* Strip 1: a[i+1] */
            vuint64m4_t va1  = __riscv_vmv_v_x_u64m4(a1, vl);
            vuint64m4_t vlo1 = __riscv_vclmul_vv_u64m4(va1, vb, vl);
            vuint64m4_t vhi1 = __riscv_vclmulh_vv_u64m4(va1, vb, vl);

            /* Strip 2: a[i+2] */
            vuint64m4_t va2  = __riscv_vmv_v_x_u64m4(a2, vl);
            vuint64m4_t vlo2 = __riscv_vclmul_vv_u64m4(va2, vb, vl);
            vuint64m4_t vhi2 = __riscv_vclmulh_vv_u64m4(va2, vb, vl);

            /* Strip 3: a[i+3] */
            vuint64m4_t va3  = __riscv_vmv_v_x_u64m4(a3, vl);
            vuint64m4_t vlo3 = __riscv_vclmul_vv_u64m4(va3, vb, vl);
            vuint64m4_t vhi3 = __riscv_vclmulh_vv_u64m4(va3, vb, vl);

            /* Accumulate strip 0 */
            vuint64m4_t vr;
            vr = __riscv_vle64_v_u64m4(r + (i + 0) + j, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vlo0, vl);
            __riscv_vse64_v_u64m4(r + (i + 0) + j, vr, vl);
            vr = __riscv_vle64_v_u64m4(r + (i + 0) + j + 1, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vhi0, vl);
            __riscv_vse64_v_u64m4(r + (i + 0) + j + 1, vr, vl);

            /* Accumulate strip 1 */
            vr = __riscv_vle64_v_u64m4(r + (i + 1) + j, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vlo1, vl);
            __riscv_vse64_v_u64m4(r + (i + 1) + j, vr, vl);
            vr = __riscv_vle64_v_u64m4(r + (i + 1) + j + 1, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vhi1, vl);
            __riscv_vse64_v_u64m4(r + (i + 1) + j + 1, vr, vl);

            /* Accumulate strip 2 */
            vr = __riscv_vle64_v_u64m4(r + (i + 2) + j, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vlo2, vl);
            __riscv_vse64_v_u64m4(r + (i + 2) + j, vr, vl);
            vr = __riscv_vle64_v_u64m4(r + (i + 2) + j + 1, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vhi2, vl);
            __riscv_vse64_v_u64m4(r + (i + 2) + j + 1, vr, vl);

            /* Accumulate strip 3 */
            vr = __riscv_vle64_v_u64m4(r + (i + 3) + j, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vlo3, vl);
            __riscv_vse64_v_u64m4(r + (i + 3) + j, vr, vl);
            vr = __riscv_vle64_v_u64m4(r + (i + 3) + j + 1, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vhi3, vl);
            __riscv_vse64_v_u64m4(r + (i + 3) + j + 1, vr, vl);

            j += vl;
        }
    }

    /* ── Scalar tail: remaining 0..3 outer iterations ───────────────── */
    for (; i < n; i++) {
        uint64_t ai = a[i];
        size_t j = 0;
        while (j < n) {
            size_t vl = __riscv_vsetvl_e64m4(n - j);

            vuint64m4_t vai = __riscv_vmv_v_x_u64m4(ai, vl);
            vuint64m4_t vb  = __riscv_vle64_v_u64m4(b + j, vl);

            vuint64m4_t vlo = __riscv_vclmul_vv_u64m4(vai, vb, vl);
            vuint64m4_t vhi = __riscv_vclmulh_vv_u64m4(vai, vb, vl);

            vuint64m4_t vr0 = __riscv_vle64_v_u64m4(r + i + j, vl);
            vr0 = __riscv_vxor_vv_u64m4(vr0, vlo, vl);
            __riscv_vse64_v_u64m4(r + i + j, vr0, vl);

            vuint64m4_t vr1 = __riscv_vle64_v_u64m4(r + i + j + 1, vl);
            vr1 = __riscv_vxor_vv_u64m4(vr1, vhi, vl);
            __riscv_vse64_v_u64m4(r + i + j + 1, vr1, vl);

            j += vl;
        }
    }
}

/* ── Recursive Karatsuba with vectorised leaf and combines ───────────────
 * Structure identical to karatsuba_zbc; only the base case (vclmul_leaf)
 * and the combine XOR loops (vxor_words) differ.
 */
static void karatsuba_vclmul(uint64_t * __restrict__ r,
                              const uint64_t * __restrict__ a,
                              const uint64_t * __restrict__ b,
                              size_t n,
                              uint64_t * __restrict__ tmp)
{
    if (n <= KARATSUBA_THRESHOLD) {
        vclmul_leaf(r, a, b, n);
        return;
    }

    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;   /* n1 == m or m+1 when n is odd */

    /* Slice temporary storage (8*n words at this level). */
    uint64_t *z0   = tmp;
    uint64_t *z2   = z0 + 2 * n;
    uint64_t *zmid = z2 + 2 * n;
    uint64_t *ta   = zmid + 2 * n;
    uint64_t *tb   = ta + n;
    uint64_t *child_buf = tmp + 8 * n;

    /* 1) z0 = a_lo * b_lo */
    karatsuba_vclmul(z0, a, b, n0, child_buf);

    /* 2) z2 = a_hi * b_hi */
    karatsuba_vclmul(z2, a + m, b + m, n1, child_buf);

    /* 3) ta = a_lo XOR a_hi,  tb = b_lo XOR b_hi  (handles odd n1). */
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }

    /* 4) zmid = ta * tb */
    karatsuba_vclmul(zmid, ta, tb, n1, child_buf);

    /* 5) Assemble result — zero-free Assign-on-First-Write discipline:
     *      r = z0  +  (zmid ^ z0 ^ z2) * x^m  +  z2 * x^(2m)
     *
     * Loop 1: r[0..2*n0-1]           = z0          (direct assign)
     * Loop 2: r[2*m..2*m+2*n1-1]     = z2          (direct assign)
     * Loop 3A: r[m+i] ^= zmid[i] ^ z0[i] ^ z2[i]   i in [0, 2*n0)
     * Loop 3B: r[m+i] ^= zmid[i] ^ z2[i]            i in [2*n0, 2*n1)
     *          (z0 is zero outside [0, 2*n0); Loop 3B only for odd n)
     *
     * No memset: Loops 1+2 together cover all 2*n words of r before any
     * read in Loops 3A/3B — same write-before-read guarantee as Track B.
     */

    /* Loop 1: r[0..2*n0-1] = z0 */
    rvv_copy_u64(r, z0, 2 * n0);

    /* Loop 2: r[2*m..2*m+2*n1-1] = z2 */
    rvv_copy_u64(r + 2 * m, z2, 2 * n1);

    /* Loop 3A: r[m..m+2*n0-1] ^= zmid ^ z0 ^ z2 */
    size_t split = 2 * n0;
    if (split > 0) {
        rvv_xor3_u64(r + m, zmid, z0, z2, split);
    }

    /* Loop 3B: r[m+2*n0..m+2*n1-1] ^= zmid ^ z2  (odd n only, len=2) */
    size_t rem = 2 * n1 - split;
    if (rem > 0) {
        rvv_xor3_u64(r + m + split,
                     zmid + split,
                     (const uint64_t *)0,
                     z2   + split,
                     rem);
    }
}

/* ── Modular reduction mod (x^PARAM_N - 1) ──────────────────────────────
 * Kept scalar: vectorizing the sliding-window pattern (each output element
 * combines a[i], a[i+N-1], a[i+N] across a 2N-word array) requires two
 * overlapping VLE64 windows that share elements with adjacent output
 * positions, and the resulting code produced KAT failures for HQC-256.
 * Its share of total instret is < 5%, so the scalar version is kept
 * and the optimization budget is better spent on the leaf and combine.
 */
static void reduce(uint64_t * __restrict__ o,
                   const uint64_t * __restrict__ a)
{
    int shift = PARAM_N & 0x3F;
    int inv   = 64 - shift;

    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t r     = a[i + VEC_N_SIZE_64 - 1] >> shift;
        uint64_t carry = (shift == 0) ? 0 : (a[i + VEC_N_SIZE_64] << inv);
        o[i] = a[i] ^ r ^ carry;
    }
    o[VEC_N_SIZE_64 - 1] &= BITMASK(PARAM_N, 64);
}

/* ── Public entry point ──────────────────────────────────────────────────
 * Identical interface to vendor/hqc-ref/src/ref/gf2x.c and karatsuba_zbc.
 */
void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

    karatsuba_vclmul(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
