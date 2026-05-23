/* src/mult/karatsuba_zvbc/gf2x_vclmul.c
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
#include "vlen_config.h"

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
 * Uses adaptive LMUL via gf2x_vec_t / gf2x_vsetvl from vlen_config.h.
 */
static inline void rvv_copy_u64(uint64_t * __restrict__ dst,
                                const uint64_t * __restrict__ src,
                                size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = gf2x_vsetvl(n);
        gf2x_vec_t v = gf2x_vle64(src, vl);
        gf2x_vse64(dst, v, vl);
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
        size_t vlz = gf2x_vsetvl(n);
        gf2x_vse64(r + n, gf2x_vmv_v_x(0, vlz), vlz);
    }

    /* ── i=0 peeled: lo uses direct store (r[0..n-1] uninitialised) ─── */
    {
        uint64_t a0 = a[0];
        size_t j = 0;
        while (j < n) {
            size_t vl = gf2x_vsetvl(n - j);
            gf2x_vec_t vb   = gf2x_vle64(b + j, vl);
            gf2x_vec_t va0  = gf2x_vmv_v_x(a0, vl);
            gf2x_vec_t vlo0 = gf2x_vclmul_vv(va0, vb, vl);
            gf2x_vec_t vhi0 = gf2x_vclmulh_vv(va0, vb, vl);
            /* lo: ASSIGN — r[j..j+vl-1] have not been written yet */
            gf2x_vse64(r + j, vlo0, vl);
            /* hi: load-XOR-store — r[j+1..j+vl-1] hold lo; r[j+vl] pre-zeroed */
            gf2x_vec_t vr = gf2x_vle64(r + j + 1, vl);
            vr = gf2x_vxor_vv(vr, vhi0, vl);
            gf2x_vse64(r + j + 1, vr, vl);
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
            size_t vl = gf2x_vsetvl(n - j);

            gf2x_vec_t vb = gf2x_vle64(b + j, vl);

            /* Strip 0: a[i] */
            gf2x_vec_t va0  = gf2x_vmv_v_x(a0, vl);
            gf2x_vec_t vlo0 = gf2x_vclmul_vv(va0, vb, vl);
            gf2x_vec_t vhi0 = gf2x_vclmulh_vv(va0, vb, vl);

            /* Strip 1: a[i+1] */
            gf2x_vec_t va1  = gf2x_vmv_v_x(a1, vl);
            gf2x_vec_t vlo1 = gf2x_vclmul_vv(va1, vb, vl);
            gf2x_vec_t vhi1 = gf2x_vclmulh_vv(va1, vb, vl);

            /* Strip 2: a[i+2] */
            gf2x_vec_t va2  = gf2x_vmv_v_x(a2, vl);
            gf2x_vec_t vlo2 = gf2x_vclmul_vv(va2, vb, vl);
            gf2x_vec_t vhi2 = gf2x_vclmulh_vv(va2, vb, vl);

            /* Strip 3: a[i+3] */
            gf2x_vec_t va3  = gf2x_vmv_v_x(a3, vl);
            gf2x_vec_t vlo3 = gf2x_vclmul_vv(va3, vb, vl);
            gf2x_vec_t vhi3 = gf2x_vclmulh_vv(va3, vb, vl);

            /* Accumulate strip 0 */
            gf2x_vec_t vr;
            vr = gf2x_vle64(r + (i + 0) + j, vl);
            vr = gf2x_vxor_vv(vr, vlo0, vl);
            gf2x_vse64(r + (i + 0) + j, vr, vl);
            vr = gf2x_vle64(r + (i + 0) + j + 1, vl);
            vr = gf2x_vxor_vv(vr, vhi0, vl);
            gf2x_vse64(r + (i + 0) + j + 1, vr, vl);

            /* Accumulate strip 1 */
            vr = gf2x_vle64(r + (i + 1) + j, vl);
            vr = gf2x_vxor_vv(vr, vlo1, vl);
            gf2x_vse64(r + (i + 1) + j, vr, vl);
            vr = gf2x_vle64(r + (i + 1) + j + 1, vl);
            vr = gf2x_vxor_vv(vr, vhi1, vl);
            gf2x_vse64(r + (i + 1) + j + 1, vr, vl);

            /* Accumulate strip 2 */
            vr = gf2x_vle64(r + (i + 2) + j, vl);
            vr = gf2x_vxor_vv(vr, vlo2, vl);
            gf2x_vse64(r + (i + 2) + j, vr, vl);
            vr = gf2x_vle64(r + (i + 2) + j + 1, vl);
            vr = gf2x_vxor_vv(vr, vhi2, vl);
            gf2x_vse64(r + (i + 2) + j + 1, vr, vl);

            /* Accumulate strip 3 */
            vr = gf2x_vle64(r + (i + 3) + j, vl);
            vr = gf2x_vxor_vv(vr, vlo3, vl);
            gf2x_vse64(r + (i + 3) + j, vr, vl);
            vr = gf2x_vle64(r + (i + 3) + j + 1, vl);
            vr = gf2x_vxor_vv(vr, vhi3, vl);
            gf2x_vse64(r + (i + 3) + j + 1, vr, vl);

            j += vl;
        }
    }

    /* ── Scalar tail: remaining 0..3 outer iterations ───────────────── */
    for (; i < n; i++) {
        uint64_t ai = a[i];
        size_t j = 0;
        while (j < n) {
            size_t vl = gf2x_vsetvl(n - j);

            gf2x_vec_t vai = gf2x_vmv_v_x(ai, vl);
            gf2x_vec_t vb  = gf2x_vle64(b + j, vl);

            gf2x_vec_t vlo = gf2x_vclmul_vv(vai, vb, vl);
            gf2x_vec_t vhi = gf2x_vclmulh_vv(vai, vb, vl);

            gf2x_vec_t vr0 = gf2x_vle64(r + i + j, vl);
            vr0 = gf2x_vxor_vv(vr0, vlo, vl);
            gf2x_vse64(r + i + j, vr0, vl);

            gf2x_vec_t vr1 = gf2x_vle64(r + i + j + 1, vl);
            vr1 = gf2x_vxor_vv(vr1, vhi, vl);
            gf2x_vse64(r + i + j + 1, vr1, vl);

            j += vl;
        }
    }
}

#include "karatsuba_zvbc_specialized_gen.h"

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

    switch (VEC_N_SIZE_64) {
    case 277u:
        kmul_277(unreduced, a, b, tmp_buf);
        break;
    case 561u:
        kmul_561(unreduced, a, b, tmp_buf);
        break;
    case 901u:
        kmul_901(unreduced, a, b, tmp_buf);
        break;
    default:
        __builtin_trap();
    }
    reduce(o, unreduced);
}
