/* src/poly_mul/karatsuba_zvbc/gf2x_vclmul.c
 *
 * Vectorized Karatsuba with vclmul.vv / vclmulh.vv.
 *
 * This is the Zvbc-vectorised version of karatsuba_zbc/gf2x_clmul.c.
 * Two optimisation axes are applied:
 *
 *   Iter 1 — Vectorise leaf nodes:
 *     Replace scalar `clmul`+`clmulh` double-loop with `vclmul.vv` +
 *     `vclmulh.vv` using a splat (vmv.v.x) + vector-loop over b[].
 *     For THRESHOLD=16 and VLEN=256 (LMUL=4 → 16 uint64_t per group),
 *     the entire inner j-loop collapses to ONE vector iteration:
 *       scalar: 16×16 × ~8 instr ≈ 2048 instr/leaf
 *       vector: 16 × ~10 instr    ≈  160 instr/leaf  (~12.8× on leaf)
 *
 *   Iter 2 — Vectorise Karatsuba XOR combines:
 *     The XOR accumulation in the combine phase (z0, zmid, z2 arrays)
 *     uses vxor.vv with LMUL=4 (16 uint64_t per iteration), replacing
 *     scalar word-by-word loops.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * Requires: -march includes zvbc (ratified RISC-V vector carry-less multiply).
 * Detected at compile time via HQC_POLYMUL_ZVBC=1 (set by CMakeLists.txt).
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

/* ── RVV helper: dst[0..n-1] ^= src[0..n-1]  (vxor.vv, LMUL=4) ─────────
 * Used for the Karatsuba combine phases.
 * With VLEN=256: 16 uint64_t per vector iteration (LMUL=4).
 */
static void vxor_words(uint64_t * __restrict__ dst,
                       const uint64_t * __restrict__ src,
                       size_t n)
{
    size_t i = 0;
    while (i < n) {
        size_t vl = __riscv_vsetvl_e64m4(n - i);
        vuint64m4_t vd = __riscv_vle64_v_u64m4(dst + i, vl);
        vuint64m4_t vs = __riscv_vle64_v_u64m4(src + i, vl);
        vd = __riscv_vxor_vv_u64m4(vd, vs, vl);
        __riscv_vse64_v_u64m4(dst + i, vd, vl);
        i += vl;
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
    memset(r, 0, 2 * n * sizeof(uint64_t));

    size_t i = 0;

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

    /* 5) Assemble result using vectorised XOR (Iter 2 advanced):
     *      r = z0  +  (zmid ^ z0 ^ z2) * x^m  +  z2 * x^(2m)
     *
     * Vectorise the combine loop:
     *   for i in [0, 2*n0):  r[m+i] ^= zmid[i] ^ z0[i] ^ z2[i]
     *   for i in [2*n0, 2*n1): r[m+i] ^= zmid[i] ^ z2[i]
     *             (z0 is zero outside [0, 2*n0))
     * Both segments use vxor_words (LMUL=4, 16 elements/iter). No scalar
     * fallback, no branch per element.
     */
    memset(r, 0, 2 * n * sizeof(uint64_t));

    /* r[0..2*n0-1] ^= z0[0..2*n0-1] */
    vxor_words(r, z0, 2 * n0);

    /* r[2*m..2*m+2*n1-1] ^= z2[0..2*n1-1] */
    vxor_words(r + 2 * m, z2, 2 * n1);

    /* Vectorised combine: first segment [0, 2*n0) — zmid ^ z0 ^ z2 */
    {
        /* Reuse a temporary to form (zmid ^ z0 ^ z2) for i in [0, 2*n0). */
        /* We XOR directly into r[m..] in two vectorised sweeps.           */
        size_t len0 = 2 * n0;   /* z0 is valid here */
        size_t len1 = 2 * n1 - len0; /* remainder, z0 treated as 0 */

        /* Segment A: r[m+i] ^= zmid[i] ^ z0[i] ^ z2[i], i in [0, len0) */
        size_t off = 0;
        while (off < len0) {
            size_t vl = __riscv_vsetvl_e64m4(len0 - off);
            vuint64m4_t vm = __riscv_vle64_v_u64m4(zmid + off, vl);
            vuint64m4_t vz0 = __riscv_vle64_v_u64m4(z0   + off, vl);
            vuint64m4_t vz2 = __riscv_vle64_v_u64m4(z2   + off, vl);
            vuint64m4_t vr  = __riscv_vle64_v_u64m4(r + m + off, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vm,  vl);
            vr = __riscv_vxor_vv_u64m4(vr, vz0, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vz2, vl);
            __riscv_vse64_v_u64m4(r + m + off, vr, vl);
            off += vl;
        }

        /* Segment B: r[m+i] ^= zmid[i] ^ z2[i], i in [len0, 2*n1) */
        /* Only exists when n is odd (n1 = n0 + 1, len1 = 2). */
        off = len0;
        while (off < 2 * n1) {
            size_t vl = __riscv_vsetvl_e64m4(2 * n1 - off);
            vuint64m4_t vm  = __riscv_vle64_v_u64m4(zmid + off, vl);
            vuint64m4_t vz2 = __riscv_vle64_v_u64m4(z2   + off, vl);
            vuint64m4_t vr  = __riscv_vle64_v_u64m4(r + m + off, vl);
            vr = __riscv_vxor_vv_u64m4(vr, vm,  vl);
            vr = __riscv_vxor_vv_u64m4(vr, vz2, vl);
            __riscv_vse64_v_u64m4(r + m + off, vr, vl);
            off += vl;
        }
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
