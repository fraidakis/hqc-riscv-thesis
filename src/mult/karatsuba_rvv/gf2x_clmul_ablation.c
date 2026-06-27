/* src/mult/karatsuba_rvv/gf2x_clmul_ablation.c
 *
 * karatsuba_rvv ABLATION harness — Base-RVV reconstruction (Tier 2).
 *
 * Companion to src/mult/karatsuba_zbc/gf2x_clmul_ablation.c (scalar ablation).
 * This file isolates the three reconstruction optimizations described in
 *   docs/thesis/chapters/06-polymul-optimization.tex
 *   \section{Base-RVV Path: ...}  \label{sec:polymul-base-rvv}
 * so they can be toggled independently at compile time and benchmarked by
 * scripts/bench/run_ablation.sh --vector_rvv.
 *
 * To keep the three axes ORTHOGONAL and KAT-verifiable, every ablation
 * variant runs on the SAME fixed test bed: a generic n>>1 Karatsuba
 * recursion with the unchanged scalar clmul leaf (clmul_leaf).  Only the
 * reconstruction (recomposition + reduction) is varied.  The deployed
 * champion `vector_rvv` (specialized generated tree) is built from the
 * production file gf2x_clmul.c and is intentionally separate — exactly as the
 * scalar champion `scalar_zb` is built from its production gf2x_clmul.c while
 * scalar_ref/A/AB/AC are built from gf2x_clmul_ablation.c.
 *
 * ── Ablation flags (set by the build via -D...) ─────────────────────────
 *   ABLATION_RVV_DIRECTWRITE (D)  0/1  default 0
 *       Direct-write discipline + overlap-save combine (ssec:direct-write,
 *       ssec:overlap-save): z0/z2 are written straight into the output and
 *       the two RVV copy loops are eliminated.  D bundles overlap-save
 *       because direct-write without it is incorrect (aliasing).
 *   ABLATION_RVV_INLINE (I)       0/1  default 0
 *       Inlined RVV recomposition ops with per-operation tuned LMUL
 *       (ssec:inlined-rvv) vs. shared out-of-line helper functions.
 *   ABLATION_RVV_REDUCE (R)       0/1  default 0
 *       Vectorised modular reduction reduce_rvv() (ssec:reduce-rvv) vs the
 *       scalar reduce().
 *
 *   Variant -> flags (wired in the build, mirrors scalar A/B/C):
 *     vector_rvv_ref  : (none)             D=0 I=0 R=0
 *     vector_rvv_D    : D                   D=1 I=0 R=0
 *     vector_rvv_DI   : D + I               D=1 I=1 R=0
 *     vector_rvv_DIR  : D + I + R           D=1 I=1 R=1
 *
 * HARD CONSTRAINT (unchanged from production): only base `V` (RVV 1.0).
 *   NO Zvbc / vector-crypto.  Scalar leaf multiply is unchanged.
 *
 * NOTE: This harness is part of the build matrix only; it is NOT compiled
 * into shipping firmware.  It must be KAT-validated locally (no TeX/C engine
 * is available in the authoring sandbox).
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include <riscv_bitmanip.h>
#include "parameters.h"
#include "hqc_parameters.h"

#ifndef ABLATION_RVV_DIRECTWRITE
#define ABLATION_RVV_DIRECTWRITE 0
#endif
#ifndef ABLATION_RVV_INLINE
#define ABLATION_RVV_INLINE 0
#endif
#ifndef ABLATION_RVV_REDUCE
#define ABLATION_RVV_REDUCE 0
#endif

/* I axis: shared out-of-line helpers (I=0) vs forced-inline tuned-LMUL ops
 * inlined at the recomposition call sites (I=1).  The arithmetic is identical;
 * only call-overhead / inlining differs, which is exactly the ssec:inlined-rvv
 * delta in a generic test bed (the helpers already carry tuned LMUL). */
#if ABLATION_RVV_INLINE
#define RVV_RECOMP __attribute__((always_inline)) inline
#else
#define RVV_RECOMP inline
#endif

/* ── RISC-V carry-less multiply base case (unchanged from production) ──────*/
static __attribute__((always_inline)) inline
void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    *lo = __riscv_clmul_64(a, b);
    *hi = __riscv_clmulh_64(a, b);
}

#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD HQC_KARATSUBA_THRESHOLD_DEFAULT
#endif

/* Generic recursion buffer sizing: per level uses z0(2n)+z2(2n)+zmid(2n)+
 * ta(n)+tb(n) = 8n words before the shared child buffer; the geometric sum
 * over levels stays below 16n.  Reuse the shared project multiplier (=16). */
#define TMP_BUFFER_WORDS (HQC_POLY_TMP_BUFFER_MULTIPLIER * VEC_N_SIZE_64)

/* ── RVV recomposition helpers (tuned LMUL, from production) ───────────────*/
static RVV_RECOMP void rvv_copy_u64(uint64_t * __restrict__ dst,
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

static RVV_RECOMP void rvv_xor_u64(uint64_t * __restrict__ dst,
                                   const uint64_t * __restrict__ src1,
                                   const uint64_t * __restrict__ src2,
                                   size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m4(n);
        vuint64m4_t v1 = __riscv_vle64_v_u64m4(src1, vl);
        vuint64m4_t v2 = __riscv_vle64_v_u64m4(src2, vl);
        __riscv_vse64_v_u64m4(dst, __riscv_vxor_vv_u64m4(v1, v2, vl), vl);
        src1 += vl;
        src2 += vl;
        dst  += vl;
        n    -= vl;
    }
}

/* dst[i] ^= zmid[i] ^ z0[i] ^ z2[i]; NULL source pointer means 0. (LMUL=m2) */
static RVV_RECOMP void rvv_xor3_u64(uint64_t       * __restrict__ dst,
                                    const uint64_t * __restrict__ src_zmid,
                                    const uint64_t * __restrict__ src_z0,
                                    const uint64_t * __restrict__ src_z2,
                                    size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m2(n);
        vuint64m2_t vdst = __riscv_vle64_v_u64m2(dst,      vl);
        vuint64m2_t vmid = __riscv_vle64_v_u64m2(src_zmid, vl);
        vuint64m2_t acc  = vmid;
        if (src_z0 != (const uint64_t *)0) {
            vuint64m2_t vz0 = __riscv_vle64_v_u64m2(src_z0, vl);
            acc = __riscv_vxor_vv_u64m2(acc, vz0, vl);
        }
        if (src_z2 != (const uint64_t *)0) {
            vuint64m2_t vz2 = __riscv_vle64_v_u64m2(src_z2, vl);
            acc = __riscv_vxor_vv_u64m2(acc, vz2, vl);
        }
        vdst = __riscv_vxor_vv_u64m2(vdst, acc, vl);
        __riscv_vse64_v_u64m2(dst, vdst, vl);
        dst      += vl;
        src_zmid += vl;
        if (src_z0 != (const uint64_t *)0) src_z0 += vl;
        if (src_z2 != (const uint64_t *)0) src_z2 += vl;
        n        -= vl;
    }
}

/* ── Scalar leaf multiply (UNCHANGED — clmul cannot be vectorised w/o Zvbc) ─*/
static void clmul_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    uint64_t lo0, hi0, lo1, hi1, lo2, hi2, lo3, hi3;

    if (__builtin_expect(n == 1, 0)) {
        clmul64(a[0], b[0], &lo0, &hi0);
        r[0] = lo0;
        r[1] = hi0;
        return;
    }

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

    for (size_t i = 1; i < n; i++) {
        uint64_t *rp = r + i;
        j = 0;
        for (; j + 3 < n - 1; j += 4) {
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            clmul64(a[i], b[j + 2], &lo2, &hi2);
            clmul64(a[i], b[j + 3], &lo3, &hi3);
            rp[j]     ^= lo0;
            rp[j + 1] ^= hi0 ^ lo1;
            rp[j + 2] ^= hi1 ^ lo2;
            rp[j + 3] ^= hi2 ^ lo3;
            rp[j + 4] ^= hi3;
        }
        for (; j + 1 < n - 1; j += 2) {
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            rp[j]     ^= lo0;
            rp[j + 1] ^= hi0 ^ lo1;
            rp[j + 2] ^= hi1;
        }
        if (j < n - 1) {
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            rp[j]     ^= lo0;
            rp[j + 1] ^= hi0 ^ lo1;
            r[i + n]      = hi1;
        } else {
            clmul64(a[i], b[j], &lo0, &hi0);
            rp[j]  ^= lo0;
            r[i + n]   = hi0;
        }
    }
}

/* ── Generic Karatsuba recursion (fixed test bed) ─────────────────────────
 *
 * Convention (matches scalar gf2x_clmul_ablation.c): m = n>>1, low half
 * n0 = m, high half n1 = n - m  (so n1 >= n0).  tmp layout is allocated
 * scalar-style for buffer safety regardless of D:
 *   z0[2n] z2[2n] zmid[2n] ta[n] tb[n] | child(tmp+8n)
 */
static void karatsuba_mul_generic(uint64_t * __restrict__ r,
                                  const uint64_t * __restrict__ a,
                                  const uint64_t * __restrict__ b,
                                  size_t n,
                                  uint64_t *tmp_buffer)
{
    if (n <= KARATSUBA_THRESHOLD) {
        clmul_leaf(r, a, b, n);
        return;
    }

    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;

    uint64_t *z0   = tmp_buffer;
    uint64_t *z2   = z0 + 2 * n;
    uint64_t *zmid = z2 + 2 * n;
    uint64_t *ta   = zmid + 2 * n;
    uint64_t *tb   = ta + n;
    uint64_t *child_buffer = tmp_buffer + 8 * n;

#if ABLATION_RVV_DIRECTWRITE
    /* ── Direct-write + overlap-save (D=1) ────────────────────────────────
     * z0 -> r[0 .. 2*n0-1], z2 -> r[2*n0 .. 2*n0+2*n1-1] (no copy loops).
     */
    (void)z0; (void)z2;
    karatsuba_mul_generic(r,         a,     b,     n0, child_buffer);
    karatsuba_mul_generic(r + 2 * m, a + m, b + m, n1, child_buffer);

    /* ta = aL ^ aH, tb = bL ^ bH  (length n1; low half zero-padded past n0) */
    rvv_xor_u64(ta, a, a + m, n0);
    rvv_xor_u64(tb, b, b + m, n0);
    for (size_t i = n0; i < n1; i++) { ta[i] = a[m + i]; tb[i] = b[m + i]; }
    karatsuba_mul_generic(zmid, ta, tb, n1, child_buffer);

    /* Overlap-save: the middle accumulation writes r[m .. m+2*n1-1] and would
     * clobber the z0 tail r[m .. 2*n0-1] (n0 words) before it is read.  Save
     * it into ta (free now that zmid is computed). */
    rvv_copy_u64(ta, r + m, n0);

    /* Middle accumulate: r[m+i] ^= zmid[i] ^ z0[i] ^ z2[i].
     *   z0[i] = r[i]      for i in [0, m)      (untouched low part)
     *         = ta[i-m]   for i in [m, 2*n0)   (saved overlap)
     *   z2[i] = r[2*n0+i] for i in [0, 2*n1)   (always read ahead of write)
     *   z0[i] = 0         for i in [2*n0, 2*n1) */
    rvv_xor3_u64(r + m,            zmid,            r,        r + 2 * n0, m);
    rvv_xor3_u64(r + m + m,        zmid + m,        ta,       r + 2 * n0 + m, m);
    rvv_xor3_u64(r + m + 2 * n0,   zmid + 2 * n0,   (const uint64_t *)0,
                 r + 2 * n0 + 2 * n0, (2 * n1 - 2 * n0));
#else
    /* ── Copy-loop reconstruction (D=0, reference Base-RVV path) ──────────
     * Children land in intermediate buffers; two RVV copy loops move z0/z2
     * into the output, then the middle is XOR-accumulated. */
    karatsuba_mul_generic(z0, a,     b,     n0, child_buffer);
    karatsuba_mul_generic(z2, a + m, b + m, n1, child_buffer);

    rvv_xor_u64(ta, a, a + m, n0);
    rvv_xor_u64(tb, b, b + m, n0);
    for (size_t i = n0; i < n1; i++) { ta[i] = a[m + i]; tb[i] = b[m + i]; }
    karatsuba_mul_generic(zmid, ta, tb, n1, child_buffer);

    /* Copy loops: z0 -> r[0..2n0-1], z2 -> r[2n0..2n0+2n1-1] (tile all of r). */
    rvv_copy_u64(r,         z0, 2 * n0);
    rvv_copy_u64(r + 2 * m, z2, 2 * n1);

    /* Middle accumulate from the intermediate buffers.
     *   i in [0, 2*n0)  : z0[i] and z2[i] both present
     *   i in [2*n0,2*n1): z0[i] = 0 (NULL) */
    rvv_xor3_u64(r + m,          zmid,          z0, z2,          2 * n0);
    rvv_xor3_u64(r + m + 2 * n0, zmid + 2 * n0, (const uint64_t *)0,
                 z2 + 2 * n0, (2 * n1 - 2 * n0));
#endif
}

/* ── Modular reduction mod (x^PARAM_N - 1) ────────────────────────────────*/
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

#if ABLATION_RVV_REDUCE
static void reduce_rvv(uint64_t * __restrict__ o, const uint64_t * __restrict__ a)
{
    const int shift = (int)HQC_PARAM_N_MOD_64;
    const int inv   = 64 - shift;
    size_t n = VEC_N_SIZE_64;
    size_t i = 0;

    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m8(n);
        vuint64m8_t va   = __riscv_vle64_v_u64m8(a + i, vl);
        vuint64m8_t vhi  = __riscv_vle64_v_u64m8(a + i + VEC_N_SIZE_64 - 1, vl);
        vuint64m8_t vnxt = __riscv_vle64_v_u64m8(a + i + VEC_N_SIZE_64, vl);

        vuint64m8_t vr   = __riscv_vsrl_vx_u64m8(vhi, (size_t)shift, vl);
        vuint64m8_t vc   = (shift == 0)
                         ? __riscv_vmv_v_x_u64m8(0, vl)
                         : __riscv_vsll_vx_u64m8(vnxt, (size_t)inv, vl);

        vuint64m8_t vout = __riscv_vxor_vv_u64m8(va, vr, vl);
        vout = __riscv_vxor_vv_u64m8(vout, vc, vl);

        __riscv_vse64_v_u64m8(o + i, vout, vl);
        i += vl;
        n -= vl;
    }
    o[VEC_N_SIZE_64 - 1] &= HQC_PARAM_LAST_WORD_MASK_64;
}
#endif

/* ── Public entry point ───────────────────────────────────────────────────*/
void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64] __attribute__((aligned(32)));
    uint64_t tmp_buf[TMP_BUFFER_WORDS]    __attribute__((aligned(32)));

    karatsuba_mul_generic(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);

#if ABLATION_RVV_REDUCE
    reduce_rvv(o, unreduced);
#else
    reduce(o, unreduced);
#endif
}
