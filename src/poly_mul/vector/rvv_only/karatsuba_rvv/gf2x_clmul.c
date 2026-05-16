/* src/poly_mul/vector/rvv_only/karatsuba_rvv/gf2x_clmul.c
 *
 * karatsuba_rvv — Track B standard RVV variant.
 *
 * Forked from src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c
 * (scalar baseline with Target 1/3/4 Track A applied).
 *
 * Track B additions layered on top of the full scalar baseline:
 *
 *   RVV-1: Replace the 4-way unrolled Karatsuba assembly loops (step 5) with
 *           standard RVV `vle64.v` / `vse64.v` / `vxor.vv` loops.
 *           Extensions: V (RVV 1.0), LMUL=m4 for XOR accumulation.
 *
 *   RVV-2: (No residual memset calls remain — Target 1 Track A already
 *           eliminated all memset() calls via the direct-assign write
 *           discipline.  The `tmp_buf` scratch space is fully initialised
 *           by recursive direct-assign calls before any read.)
 *
 * HARD CONSTRAINT: Only base `V` extension (RVV 1.0) is used.
 *   - NO Zvbc (`vclmul`, `vclmulh`)
 *   - NO vector-crypto sub-extensions
 *   - Intrinsics via <riscv_vector.h>
 *
 * The scalar leaf multiply (`clmul_leaf`) is unchanged — the `clmul`/`clmulh`
 * instructions cannot be vectorised without Zvbc.
 *
 * KAT validated: all three security levels HQC-{128,192,256}.
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
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

/* ── RVV helpers ──────────────────────────────────────────────────────────
 *
 * rvv_zero_u64: Zero n_words uint64_t elements using LMUL=m8 VMV + VSE.
 *   Replaces scalar memset() for bulk zeroing of tmp buffers.
 *   Spike instret: ceil(n/8) vs n scalar stores — ~8× cheaper per element.
 */
static inline void rvv_zero_u64(uint64_t *dst, size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m8(n);
        vuint64m8_t zero = __riscv_vmv_v_x_u64m8(0, vl);
        __riscv_vse64_v_u64m8(dst, zero, vl);
        dst += vl;
        n   -= vl;
    }
}

/* rvv_copy_u64: Copy n_words uint64_t elements using LMUL=m4 VLE + VSE.
 *   Used for the direct-assign loops (Loop 1 and Loop 2) in Karatsuba assembly.
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

/* rvv_xor3_u64: Compute dst[i] ^= src0[i] ^ src1[i] ^ src2[i] using LMUL=m2.
 *   Used for the middle XOR accumulation loop (Loop 3) in Karatsuba assembly.
 *   LMUL=m2 allows loading three source groups (m2 each = 6 registers) plus
 *   one destination group (m2 = 2 registers) within the 32-register file.
 *
 *   For sub-range B (i >= 2*n0, z0i = 0), src0 pointer is NULL and we skip
 *   loading it (replaced with zero).
 */
static inline void rvv_xor3_u64(uint64_t       * __restrict__ dst,
                                  const uint64_t * __restrict__ src_zmid,
                                  const uint64_t * __restrict__ src_z0,   /* NULL means 0 */
                                  const uint64_t * __restrict__ src_z2,
                                  size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m2(n);
        vuint64m2_t vdst  = __riscv_vle64_v_u64m2(dst,      vl);
        vuint64m2_t vmid  = __riscv_vle64_v_u64m2(src_zmid, vl);
        vuint64m2_t vz2   = __riscv_vle64_v_u64m2(src_z2,   vl);
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

/* ── Leaf multiply: 2-way software pipeline + hi⊕lo fusion ──────────────
 *
 * Unchanged from the scalar baseline (Target 4 Track A).
 * The clmul/clmulh instructions cannot be vectorised without Zvbc.
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

    /* ── i=0 row ─────────────────────────────────────────────────────────*/
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

    /* ── i≥1 rows ────────────────────────────────────────────────────────*/
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

/* ── Recursive Karatsuba with RVV assembly loops ─────────────────────────
 *
 * Track B replaces the three scalar assembly loops (step 5) with:
 *   Loop 1: rvv_copy_u64(r,         z0,   2*n0)         — direct assign
 *   Loop 2: rvv_copy_u64(r + 2*m,   z2,   2*n1)         — direct assign
 *   Loop 3A: rvv_xor3_u64(r+m, zmid, z0, z2, split)     — XOR with z0
 *   Loop 3B: rvv_xor3_u64(r+m+split, zmid+split, NULL, z2+split, rem) — XOR no z0
 *
 * The scalar `memset(r, 0, ...)` calls from early baseline versions were
 * already eliminated by Target 1 Track A; no memset calls remain here.
 * The `tmp_buf` on the outermost stack frame is zero-initialised by the
 * C runtime (stack); internal recursion re-uses it with direct assignment.
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

    /* 5) Assemble r — RVV vectorised ─────────────────────────────────────
     *
     * Loop 1: r[0..2n0-1] = z0[i]             (low-half direct assign)
     * Loop 2: r[2m..2m+2n1-1] = z2[i]          (high-half direct assign)
     * Loop 3: r[m+i] ^= zmid[i] ^ z0i ^ z2[i]  (middle XOR accumulate)
     *   Sub-range A [0, 2n0): z0i = z0[i]
     *   Sub-range B [2n0, 2n1): z0i = 0  (at most 1 word for odd n)
     */

    /* Loop 1: r[0..2n0-1] = z0 */
    rvv_copy_u64(r, z0, 2 * n0);

    /* Loop 2: r[2m..2m+2n1-1] = z2 */
    rvv_copy_u64(r + 2 * m, z2, 2 * n1);

    /* Loop 3A: r[m..m+2n0-1] ^= zmid ^ z0 ^ z2 */
    size_t split = 2 * n0;
    if (split > 0) {
        rvv_xor3_u64(r + m, zmid, z0, z2, split);
    }

    /* Loop 3B: r[m+2n0..m+2n1-1] ^= zmid ^ z2  (z0i = 0, n odd only) */
    size_t rem = 2 * n1 - split;
    if (rem > 0) {
        rvv_xor3_u64(r + m + split,
                     zmid + split,
                     (const uint64_t *)0,
                     z2   + split,
                     rem);
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

    /* No pre-zeroing needed: Karatsuba write discipline (direct assigns at
     * every recursion level) ensures every word of tmp_buf is written before
     * it is read.  The scalar baseline carries no memset here. */
    karatsuba_clmul(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
