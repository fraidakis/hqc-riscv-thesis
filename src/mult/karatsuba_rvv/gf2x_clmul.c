/* src/mult/karatsuba_rvv/gf2x_clmul.c
 *
 * karatsuba_rvv — Track B standard RVV variant.
 *
 * Forked from src/mult/karatsuba_zbc/gf2x_clmul.c
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
#include <riscv_bitmanip.h>
#include "parameters.h"
#include "hqc_parameters.h"

/* ── RISC-V Zbc carry-less multiply base case ─────────────────────────────
 * Computes the full 128-bit carry-less product of two 64-bit words.
 * Uses compiler intrinsics instead of inline asm — allows the compiler to
 * freely schedule, allocate registers, and optimise across call sites.
 */
static __attribute__((always_inline)) inline
void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    *lo = __riscv_clmul_64(a, b);
    *hi = __riscv_clmulh_64(a, b);
}

/* ── Karatsuba threshold ──────────────────────────────────────────────────*/
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD HQC_KARATSUBA_THRESHOLD_DEFAULT
#endif

/* ── Temporary buffer sizing ─────────────────────────────────────────────
 *
 * With RVV direct-write discipline, z0 and z2 are written directly to
 * the output buffer r (not into tmp).  The tmp layout per recursion level is:
 *
 *   zmid:      2*n0 words
 *   ta:        n0 words
 *   tb:        n0 words
 *   child_buf: recursive (shared among 3 sequential child calls)
 *
 * Per-level cost: 4*n0 ≈ 2*n + 2 words.
 * Summing the geometric series across recursion levels:
 *   4*n0 + 4*(n0/2) + 4*(n0/4) + ... < 4*n0 * 2 = 8*n0 ≈ 4*n
 *
 * Empirical worst case (HQC-192, n=561): 3.9643 * n.
 * Multiplier of 4 provides sufficient headroom for all security levels.
 *
 * Stack budget verification (unreduced[2*n] + tmp_buf[4*n], in bytes):
 *   HQC-128 (n=277): (554 + 1108) * 8 =  13,296 B ≈  13.0 KB  ✓
 *   HQC-192 (n=561): (1122 + 2244) * 8 = 26,928 B ≈  26.3 KB  ✓
 *   HQC-256 (n=901): (1802 + 3604) * 8 = 43,248 B ≈  42.2 KB  ✓
 *
 * All well under the 512 KB limit (Requirement 9.3).
 */
#ifdef RVV_ABLATION_HEADER
/* Ablation variants may use non-direct-write layout requiring more tmp space */
#define TMP_BUFFER_WORDS (16u * VEC_N_SIZE_64)
#else
#define TMP_BUFFER_WORDS (4u * VEC_N_SIZE_64)
#endif

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

/* rvv_xor_u64: dst[i] = src1[i] ^ src2[i] for n_words elements.
 *   Used for the ta/tb preparation in Karatsuba split.
 *   LMUL=m4 → 16 uint64_t per iteration at VLEN=256.
 */
static inline void rvv_xor_u64(uint64_t * __restrict__ dst,
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
                                  const uint64_t * __restrict__ src_z2,   /* NULL means 0 */
                                  size_t n_words)
{
    size_t n = n_words;
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e64m2(n);
        vuint64m2_t vdst  = __riscv_vle64_v_u64m2(dst,      vl);
        vuint64m2_t vmid  = __riscv_vle64_v_u64m2(src_zmid, vl);
        vuint64m2_t acc = vmid;
        if (src_z0 != (const uint64_t *)0) {
            vuint64m2_t vz0 = __riscv_vle64_v_u64m2(src_z0, vl);
            acc = __riscv_vxor_vv_u64m2(acc, vz0, vl);
        }
        if (src_z2 != (const uint64_t *)0) {
            vuint64m2_t vz2 = __riscv_vle64_v_u64m2(src_z2, vl);
            acc = __riscv_vxor_vv_u64m2(acc, vz2, vl);
        }
        vdst = __riscv_vxor_vv_u64m2(vdst, acc,  vl);
        __riscv_vse64_v_u64m2(dst, vdst, vl);
        dst      += vl;
        src_zmid += vl;
        if (src_z0 != (const uint64_t *)0) src_z0 += vl;
        if (src_z2 != (const uint64_t *)0) src_z2 += vl;
        n        -= vl;
    }
}

/* ── Fixed-size leaf: n=9 fully unrolled (81 clmul64, zero branches) ─────
 *
 * Same zero-free write discipline extended to n=9 (odd).
 *
 * Structure mirrors clmul_leaf with n=9 unrolled:
 *   Row i=0: init pair(j=0,1) + loop pairs(j=2,3),(j=4,5),(j=6,7) + tail(j=8)
 *            → assigns r[0..9]
 *   Row i≥1: main pairs(j=0..6) + odd-tail single(j=8)
 *            → XOR into r[i..i+8]; fresh assign to r[i+n]=r[i+9]
 *
 * First-write proof sketch:
 *   r[0..9]  : first-written by row 0
 *   r[k] for k=10..17 : first-written by row k-9 odd-tail
 *   r[i+0..i+8] for i≥1 : all written by prior rows ✓
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n9(uint64_t * __restrict__ r,
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
    r[8]  = hi1;

    /* Odd tail (j=8): single element */
    clmul64(a[0], b[8], &lo0, &hi0);
    r[8] ^= lo0;
    r[9]  = hi0;

    /* ── Row i=1 ──────────────────────────────────────────────────────── */

    clmul64(a[1], b[0], &lo0, &hi0);
    clmul64(a[1], b[1], &lo1, &hi1);
    r[1] ^= lo0;
    r[2] ^= hi0 ^ lo1;
    r[3] ^= hi1;

    clmul64(a[1], b[2], &lo0, &hi0);
    clmul64(a[1], b[3], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    clmul64(a[1], b[4], &lo0, &hi0);
    clmul64(a[1], b[5], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[1], b[6], &lo0, &hi0);
    clmul64(a[1], b[7], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    /* Odd-tail (j=8): r[1+9]=r[10] is fresh assign */
    clmul64(a[1], b[8], &lo0, &hi0);
    r[9]  ^= lo0;
    r[10]  = hi0;

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

    clmul64(a[2], b[6], &lo0, &hi0);
    clmul64(a[2], b[7], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    /* Odd-tail (j=8): r[2+9]=r[11] is fresh assign */
    clmul64(a[2], b[8], &lo0, &hi0);
    r[10] ^= lo0;
    r[11]  = hi0;

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

    clmul64(a[3], b[6], &lo0, &hi0);
    clmul64(a[3], b[7], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    /* Odd-tail (j=8): r[3+9]=r[12] is fresh assign */
    clmul64(a[3], b[8], &lo0, &hi0);
    r[11] ^= lo0;
    r[12]  = hi0;

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

    clmul64(a[4], b[6], &lo0, &hi0);
    clmul64(a[4], b[7], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1;

    /* Odd-tail (j=8): r[4+9]=r[13] is fresh assign */
    clmul64(a[4], b[8], &lo0, &hi0);
    r[12] ^= lo0;
    r[13]  = hi0;

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

    clmul64(a[5], b[6], &lo0, &hi0);
    clmul64(a[5], b[7], &lo1, &hi1);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13] ^= hi1;

    /* Odd-tail (j=8): r[5+9]=r[14] is fresh assign */
    clmul64(a[5], b[8], &lo0, &hi0);
    r[13] ^= lo0;
    r[14]  = hi0;

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

    clmul64(a[6], b[6], &lo0, &hi0);
    clmul64(a[6], b[7], &lo1, &hi1);
    r[12] ^= lo0;
    r[13] ^= hi0 ^ lo1;
    r[14] ^= hi1;

    /* Odd-tail (j=8): r[6+9]=r[15] is fresh assign */
    clmul64(a[6], b[8], &lo0, &hi0);
    r[14] ^= lo0;
    r[15]  = hi0;

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

    clmul64(a[7], b[6], &lo0, &hi0);
    clmul64(a[7], b[7], &lo1, &hi1);
    r[13] ^= lo0;
    r[14] ^= hi0 ^ lo1;
    r[15] ^= hi1;

    /* Odd-tail (j=8): r[7+9]=r[16] is fresh assign */
    clmul64(a[7], b[8], &lo0, &hi0);
    r[15] ^= lo0;
    r[16]  = hi0;

    /* ── Row i=8 ──────────────────────────────────────────────────────── */

    clmul64(a[8], b[0], &lo0, &hi0);
    clmul64(a[8], b[1], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    clmul64(a[8], b[2], &lo0, &hi0);
    clmul64(a[8], b[3], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1;

    clmul64(a[8], b[4], &lo0, &hi0);
    clmul64(a[8], b[5], &lo1, &hi1);
    r[12] ^= lo0;
    r[13] ^= hi0 ^ lo1;
    r[14] ^= hi1;

    clmul64(a[8], b[6], &lo0, &hi0);
    clmul64(a[8], b[7], &lo1, &hi1);
    r[14] ^= lo0;
    r[15] ^= hi0 ^ lo1;
    r[16] ^= hi1;

    /* Odd-tail (j=8): r[8+9]=r[17] is fresh assign */
    clmul64(a[8], b[8], &lo0, &hi0);
    r[16] ^= lo0;
    r[17]  = hi0;
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
    uint64_t lo0, hi0, lo1, hi1, lo2, hi2, lo3, hi3;

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
        uint64_t *rp = r + i;
        j = 0;

        /* Main 4-way block: four consecutive multiplies (j..j+3). */
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

        /* 2-way tail: keeps the fused hi0^lo1 update. */
        for (; j + 1 < n - 1; j += 2) {
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            rp[j]     ^= lo0;
            rp[j + 1] ^= hi0 ^ lo1;  /* fused XOR */
            rp[j + 2] ^= hi1;
        }

        /* Handle remaining element(s) */
        if (j < n - 1) {
            /* n even: j == n-2; pair (n-2, n-1); r[i+n] is the fresh assign */
            clmul64(a[i], b[j],     &lo0, &hi0);
            clmul64(a[i], b[j + 1], &lo1, &hi1);
            rp[j]     ^= lo0;
            rp[j + 1] ^= hi0 ^ lo1;  /* fused XOR */
            r[i + n]      = hi1;          /* first write to r[n+i]: assign */
        } else {
            /* n odd: j == n-1; single tail */
            clmul64(a[i], b[j], &lo0, &hi0);
            rp[j]  ^= lo0;
            r[i + n]   = hi0;             /* first write to r[n+i]: assign */
        }
    }
}

#ifdef RVV_ABLATION_HEADER
#include RVV_ABLATION_HEADER
#else
#include "karatsuba_rvv_specialized_gen.h"
#endif

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

/* ── Vectorized modular reduction mod (x^PARAM_N - 1) ───────────────────
 *
 * Replaces the scalar reduce() with RVV vector operations:
 *   - vsrl.vx for the right-shift by (PARAM_N mod 64)
 *   - vsll.vx for the left-shift by (64 - PARAM_N mod 64)
 *   - vxor.vv for the XOR accumulation
 *
 * Uses LMUL=m8 for maximum throughput (8 elements per iteration at VLEN=256).
 * Tail elements are handled naturally by the VL-bounded vsetvl loop.
 */
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

/* ── Public entry point ──────────────────────────────────────────────────*/
void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64] __attribute__((aligned(32)));
    uint64_t tmp_buf[TMP_BUFFER_WORDS] __attribute__((aligned(32)));

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
    reduce_rvv(o, unreduced);
}
