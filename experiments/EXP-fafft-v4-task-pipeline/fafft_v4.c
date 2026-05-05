#include "fafft_v4.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <riscv_vector.h>

#include "parameters.h"
#include "../ref/bitconversion.h"
#include "../ref/btfy_ffft.h"
#include "../ref/encode.h"
#include "../ref/gfv_tower.h"

#ifndef VEC_N_SIZE_64
#define VEC_N_SIZE_64 ((PARAM_N + 63) / 64)
#endif
#ifndef VEC_N_SIZE_32
#define VEC_N_SIZE_32 ((PARAM_N + 31) / 32)
#endif

/* Supports all HQC levels in this repo (max <= 1024 words at 64-bit packing). */
#define V4_MONO_MAX_WORDS 1024u

/* ===============================================================
 * Shared helpers
 * =============================================================== */

static inline uint64_t bitmask_last_word(void)
{
    const unsigned shift = (unsigned)(PARAM_N & 0x3Fu);
    return (shift == 0u) ? ~0ULL : ((1ULL << shift) - 1ULL);
}

/* ===============================================================
 * Task 1 (preserved): truncated FAFFT
 * =============================================================== */

void vect_mul_v4_task1(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint32_t a0[512] = {0};
    uint32_t b0[512] = {0};

    const uint32_t *a32 = (const uint32_t *)a;
    const uint32_t *b32 = (const uint32_t *)b;
    size_t copy_words = VEC_N_SIZE_32 < 512u ? VEC_N_SIZE_32 : 512u;
    memcpy(a0, a32, copy_words * sizeof(uint32_t));
    memcpy(b0, b32, copy_words * sizeof(uint32_t));

    bc_1_16384(a0);
    bc_1_16384(b0);

    uint32_t ae[32768 / 32];
    uint32_t be[32768 / 32];
    memset(ae, 0, sizeof(ae));
    memset(be, 0, sizeof(be));

    {
        uint32_t temp[16];
        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 16; j++) temp[j] = a0[32 * j + i];
            encode_trunc_fft(ae + i * 32, temp);
            for (int j = 0; j < 16; j++) temp[j] = b0[32 * j + i];
            encode_trunc_fft(be + i * 32, temp);
        }
    }

    btfy_32768(ae);
    btfy_32768(be);

    uint32_t ce[32768 / 32];
    memset(ce, 0, sizeof(ce));
    for (int i = 0; i < (32768 / 32); i += 32) {
        gf232v_mul(ce + i, ae + i, be + i);
    }

    ibtfy_32768(ce);

    uint32_t c0[512] = {0};
    {
        uint32_t temp[32];
        for (int i = 0; i < (32768 / 32); i += 32) {
            decode_trunc_fft(temp, ce + i);
            for (int j = 0; j < 32; j++) c0[32 * j + i / 32] = temp[j];
        }
    }
    ibc_1_16384(c0);

    memset(o, 0, VEC_N_SIZE_64 * sizeof(uint64_t));
    memcpy(o, c0, sizeof(c0));
    o[VEC_N_SIZE_64 - 1u] &= bitmask_last_word();
}

/* Deprecated entries left as aliases to keep old symbols resolvable. */
void vect_mul_v4_task2(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    vect_mul_v4_task1(o, a, b);
}

void vect_mul_v4_task3(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    vect_mul_v4_task1(o, a, b);
}

/* ===============================================================
 * Task 4: RVV vectorized binary polynomial multiply
 * ===============================================================
 *
 * Uses vclmul.vv + vclmulh.vv to execute VLEN/64 * LMUL carry-less
 * 64-bit multiplications per instruction, replacing the scalar clmul
 * inner loop.  For each coefficient word a[i] we broadcast it via
 * vmv.v.x then multiply against a full window of b[j..j+vl-1] in a
 * single vclmul.vv / vclmulh.vv pair.  The 128-bit unreduced products
 * are XOR-accumulated into a double-width buffer and then ring-reduced
 * modulo x^PARAM_N + 1.
 *
 * NOTE: GF(2)[x] polynomial multiplication requires the *unreduced*
 * 128-bit carry-less product of each word pair.  A field-reduced
 * (mod g(x)) multiply silently truncates cross-term bits that carry
 * across word boundaries — this is why the previous GF(2^64)-FFT
 * approach produced KAT FAIL 0/100.
 */

/*
 * Ring reduction: fold the (2n)-word unreduced product into the
 * n-word output modulo x^PARAM_N + 1.
 */
static void v4_reduce(uint64_t * __restrict__ o,
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

void vect_mul_v4_task4_fafft(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    const size_t n = VEC_N_SIZE_64;

    /* Double-width accumulation buffer for the unreduced product. */
    uint64_t C[2u * V4_MONO_MAX_WORDS + 2u];
    memset(C, 0, (2u * n + 2u) * sizeof(uint64_t));

    /*
     * For each word a[i], broadcast it into a vector register (vmv.v.x)
     * and multiply against a window of b[j..j+vl-1] using vclmul.vv
     * (low 64 bits) and vclmulh.vv (high 64 bits).  XOR-accumulate lo
     * into C[i+j] and hi into C[i+j+1].
     *
     * The overlap at C[i+j+1]: within a single j iteration, lo writes
     * C[i+j+1] (for position j+1) and hi writes C[i+j+1] (for position j).
     * These are independent contributions (XOR is commutative) so reading
     * the partially-updated value in the hi accumulation is correct.
     */
    for (size_t i = 0; i < n; i++) {
        uint64_t ai = a[i];
        size_t j = 0;
        while (j < n) {
            size_t vl = __riscv_vsetvl_e64m4(n - j);
            vuint64m4_t vb  = __riscv_vle64_v_u64m4(b + j, vl);
            vuint64m4_t va  = __riscv_vmv_v_x_u64m4(ai, vl);
            vuint64m4_t vlo = __riscv_vclmul_vv_u64m4(va, vb, vl);
            vuint64m4_t vhi = __riscv_vclmulh_vv_u64m4(va, vb, vl);
            /* Accumulate low words into C[i+j .. i+j+vl-1]. */
            vuint64m4_t vc_lo = __riscv_vle64_v_u64m4(C + i + j, vl);
            __riscv_vse64_v_u64m4(C + i + j,
                __riscv_vxor_vv_u64m4(vc_lo, vlo, vl), vl);
            /* Accumulate high words into C[i+j+1 .. i+j+vl]. */
            vuint64m4_t vc_hi = __riscv_vle64_v_u64m4(C + i + j + 1, vl);
            __riscv_vse64_v_u64m4(C + i + j + 1,
                __riscv_vxor_vv_u64m4(vc_hi, vhi, vl), vl);
            j += vl;
        }
    }

    /* Reduce the double-width product modulo x^PARAM_N + 1. */
    v4_reduce(o, C, n);
}

void vect_mul_v4(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    vect_mul_v4_task4_fafft(o, a, b);
}
