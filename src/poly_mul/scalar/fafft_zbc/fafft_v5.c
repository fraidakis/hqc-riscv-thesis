#include "fafft_v5.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <riscv_vector.h>

#include "parameters.h"
#include "ref/reduce.h"

#ifndef VEC_N_SIZE_64
#define VEC_N_SIZE_64 ((PARAM_N + 63) / 64)
#endif
#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

/* Supports all three HQC security levels at 64-bit word packing. */
#define V5_MAX_WORDS 1024u

static inline uint64_t v5_bitmask_last(void)
{
    const unsigned shift = (unsigned)(PARAM_N & 0x3Fu);
    return (shift == 0u) ? ~0ULL : ((1ULL << shift) - 1ULL);
}

/* ================================================================
 * RVV vclmul.vv vectorised schoolbook: C[i..i+n_b] ^= a[i] * b[0..n_b-1]
 *
 * Accumulates the carry-less product of scalar `ai` with the window
 * b[0..n_b-1] into C[offset..offset+n_b] (lo) and C[offset+1..offset+n_b]
 * (hi).  C must be zeroed by the caller.
 * ================================================================ */
static void v5_schoolbook_row(uint64_t * __restrict__ C,
                               size_t offset,
                               uint64_t ai,
                               const uint64_t * __restrict__ b,
                               size_t n_b)
{
    size_t j = 0;
    while (j < n_b) {
        size_t vl = __riscv_vsetvl_e64m4(n_b - j);
        vuint64m4_t vb  = __riscv_vle64_v_u64m4(b + j, vl);
        vuint64m4_t va  = __riscv_vmv_v_x_u64m4(ai, vl);
        vuint64m4_t vlo = __riscv_vclmul_vv_u64m4(va, vb, vl);
        vuint64m4_t vhi = __riscv_vclmulh_vv_u64m4(va, vb, vl);
        vuint64m4_t vc_lo = __riscv_vle64_v_u64m4(C + offset + j, vl);
        __riscv_vse64_v_u64m4(C + offset + j,
            __riscv_vxor_vv_u64m4(vc_lo, vlo, vl), vl);
        vuint64m4_t vc_hi = __riscv_vle64_v_u64m4(C + offset + j + 1, vl);
        __riscv_vse64_v_u64m4(C + offset + j + 1,
            __riscv_vxor_vv_u64m4(vc_hi, vhi, vl), vl);
        j += vl;
    }
}

/* Full schoolbook of a[0..n_a-1] × b[0..n_b-1] into C[0..n_a+n_b-1].
 * C must be zeroed by the caller. */
static void v5_schoolbook(uint64_t * __restrict__ C,
                           const uint64_t * __restrict__ a, size_t n_a,
                           const uint64_t * __restrict__ b, size_t n_b)
{
    for (size_t i = 0; i < n_a; i++)
        v5_schoolbook_row(C, i, a[i], b, n_b);
}

/* XOR-accumulate schoolbook of a[0..n_a-1] × b[0..n_b-1] into C[shift..].
 * C is NOT zeroed; existing content is preserved and XOR'd. */
static void v5_schoolbook_xor_at(uint64_t * __restrict__ C, size_t shift,
                                   const uint64_t * __restrict__ a, size_t n_a,
                                   const uint64_t * __restrict__ b, size_t n_b)
{
    uint64_t tmp[2u * V5_MAX_WORDS + 2u];
    memset(tmp, 0, (n_a + n_b + 1u) * sizeof(uint64_t));
    v5_schoolbook(tmp, a, n_a, b, n_b);
    for (size_t k = 0; k < n_a + n_b; k++)
        C[shift + k] ^= tmp[k];
}

/* ================================================================
 * Ring reduction for non-HQC-128 fallback (64-bit version)
 * ================================================================ */
static void v5_reduce64(uint64_t * __restrict__ o,
                         const uint64_t * __restrict__ a,
                         size_t n)
{
    const int shift = (int)(PARAM_N & 0x3Fu);
    const int inv   = 64 - shift;
    for (size_t i = 0; i < n; i++) {
        uint64_t hi    = a[i + n - 1u] >> shift;
        uint64_t carry = (shift == 0) ? 0ULL : (a[i + n] << inv);
        o[i] = a[i] ^ hi ^ carry;
    }
    const uint64_t mask = (shift == 0) ? ~0ULL : ((1ULL << shift) - 1ULL);
    o[n - 1u] &= mask;
}

/* ================================================================
 * HQC-128 Paper-Optimised CRT Bypass
 *
 * Split point: X = x^{16384} = word offset 512 (32-bit) = offset 256 (64-bit)
 *
 *   A = A_L + A_H * x^{16384},   A_L: 256 words (64-bit), A_H: 22 words
 *   B = B_L + B_H * x^{16384},   B_L: 256 words,          B_H: 22 words
 *
 * A*B = C0 + cross*x^{16384} + C2*x^{32768}
 *   C0    = A_L * B_L            (511  64-bit words, fills r[0..510])
 *   cross = A_L*B_H + A_H*B_L   (≤277 64-bit words, fills r[256..532])
 *   C2    = A_H * B_H            (≤43  64-bit words, fills r[512..554])
 *
 * All three products fit within r[0..552] (64-bit) = r[0..1105] (32-bit).
 * Final reduce(o, r) gives A*B mod x^{17669}+1.
 *
 * CRITICAL NOTE: For HQC-128 PARAM_N=17669:
 *   VEC_N_SIZE_64 = 277 words = 256 lower + 21 upper (ceil(17669/64)-256=21)
 *   A_H has 277-256 = 21 words.
 *   cross has at most 256+21-1 = 276 words.
 *   C2 has at most 2*21-1 = 41 words.
 *   r[0..2*277-1] = r[0..553] (64-bit) = r[0..1105] (32-bit): all within bounds.
 * ================================================================ */
#if PARAM_N == 17669

/* Split dimensions (64-bit words): */
#define V5_N_L  256u   /* lower split size: 256 × 64 bits = 16384 bits */
#define V5_N_H  (VEC_N_SIZE_64 - V5_N_L)   /* 21 upper words          */

static void v5_mul_hqc128(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    const size_t n   = VEC_N_SIZE_64;   /* 277 */
    const size_t n_l = V5_N_L;          /* 256 */
    const size_t n_h = V5_N_H;          /* 21  */

    const uint64_t *A_L = a;
    const uint64_t *A_H = a + n_l;
    const uint64_t *B_L = b;
    const uint64_t *B_H = b + n_l;

    /* Double-width accumulation buffer (64-bit), zeroed. */
    uint64_t r64[2u * V5_MAX_WORDS + 4u];
    memset(r64, 0, (2u * n + 2u) * sizeof(uint64_t));

    /* ---- C0 = A_L * B_L → r64[0..511] (256×256 schoolbook) ---- */
    for (size_t i = 0; i < n_l; i++)
        v5_schoolbook_row(r64, i, A_L[i], B_L, n_l);

    /* ---- cross: A_L*B_H → r64[n_l+0 .. n_l+n_l+n_h-1] ---- */
    for (size_t i = 0; i < n_l; i++)
        v5_schoolbook_row(r64, n_l + i, A_L[i], B_H, n_h);

    /* ---- cross: A_H*B_L → r64[n_l+0 .. n_l+n_h+n_l-1] ---- */
    for (size_t i = 0; i < n_h; i++)
        v5_schoolbook_row(r64, n_l + i, A_H[i], B_L, n_l);

    /* ---- C2 = A_H * B_H → r64[2n_l+0 .. 2n_l+2n_h-1] ---- */
    for (size_t i = 0; i < n_h; i++)
        v5_schoolbook_row(r64, 2u * n_l + i, A_H[i], B_H, n_h);

    /* ---- ring reduction using the 32-bit reduce() from ref/ ---- */
    reduce((uint32_t *)o, (const uint32_t *)r64);
    o[n - 1u] &= v5_bitmask_last();
}

#endif /* PARAM_N == 17669 */

/* ================================================================
 * Public entry point
 * ================================================================ */
void vect_mul_v5(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
#if PARAM_N == 17669
    v5_mul_hqc128(o, a, b);
#else
    /*
     * Fallback for HQC-192 / HQC-256: full RVV vectorised schoolbook
     * with direct ring reduction (no CRT).
     */
    const size_t n = VEC_N_SIZE_64;
    uint64_t C[2u * V5_MAX_WORDS + 2u];
    memset(C, 0, (2u * n + 2u) * sizeof(uint64_t));
    v5_schoolbook(C, a, n, b, n);
    v5_reduce64(o, C, n);
#endif
}
