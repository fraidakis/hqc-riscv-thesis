/* src/poly_mul/vector/zvbc/fafft_zvbc/fafft_vclmul.c
 *
 * ZVBC polynomial multiplication — promoted winner from the FAFFT showdown.
 *
 * Algorithm: pure RVV vclmul.vv schoolbook with ring reduction.
 * Origin:    experiments/EXP-fafft-v4-task-pipeline (task4)
 * Promoted:  2026-05-13
 *
 * L128 polymul_instructions: 193,177
 *   vs fafft_zvbc v0 (tower-field FAFFT): 1,731,112  (8.97x faster)
 *   vs fafft_zvbc v5 (CRT+vclmul):          198,453  (2.7% faster)
 *
 * ── Algorithm ───────────────────────────────────────────────────────────
 * For each 64-bit word a[i], broadcast it into a vector register
 * (vmv.v.x) and multiply against a window of b[j..j+vl-1] using
 * vclmul.vv (low 64 bits) and vclmulh.vv (high 64 bits).  Results are
 * XOR-accumulated into a double-width buffer.  Final ring reduction folds
 * the 2n-word product modulo x^PARAM_N + 1.
 *
 * Provides: void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include "parameters.h"

#ifndef HQC_POLYMUL_ZVBC
#  error "fafft_vclmul.c must be compiled with -DHQC_POLYMUL_ZVBC=1"
#endif

#ifndef VEC_N_SIZE_64
#  define VEC_N_SIZE_64 ((PARAM_N + 63) / 64)
#endif

/* Maximum supported words across all three HQC levels (256 at 64-bit packing). */
#define VCLMUL_MAX_WORDS 1024u

/* Ring reduction: fold the 2n-word unreduced product mod x^PARAM_N + 1. */
static void vclmul_reduce(uint64_t * __restrict__ o,
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

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    const size_t n = VEC_N_SIZE_64;

    /* Double-width accumulation buffer for the unreduced product. */
    uint64_t C[2u * VCLMUL_MAX_WORDS + 2u];
    memset(C, 0, (2u * n + 2u) * sizeof(uint64_t));

    for (size_t i = 0; i < n; i++) {
        uint64_t ai = a[i];
        size_t j = 0;
        while (j < n) {
            size_t vl = __riscv_vsetvl_e64m4(n - j);
            vuint64m4_t vb  = __riscv_vle64_v_u64m4(b + j, vl);
            vuint64m4_t va  = __riscv_vmv_v_x_u64m4(ai, vl);
            vuint64m4_t vlo = __riscv_vclmul_vv_u64m4(va, vb, vl);
            vuint64m4_t vhi = __riscv_vclmulh_vv_u64m4(va, vb, vl);
            /* Accumulate carry-less product lo-words into C[i+j]. */
            vuint64m4_t vc_lo = __riscv_vle64_v_u64m4(C + i + j, vl);
            __riscv_vse64_v_u64m4(C + i + j,
                __riscv_vxor_vv_u64m4(vc_lo, vlo, vl), vl);
            /* Accumulate carry-less product hi-words into C[i+j+1]. */
            vuint64m4_t vc_hi = __riscv_vle64_v_u64m4(C + i + j + 1, vl);
            __riscv_vse64_v_u64m4(C + i + j + 1,
                __riscv_vxor_vv_u64m4(vc_hi, vhi, vl), vl);
            j += vl;
        }
    }

    vclmul_reduce(o, C, n);
}