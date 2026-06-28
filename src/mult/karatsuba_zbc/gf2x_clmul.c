/* src/mult/karatsuba_zbc/gf2x_clmul.c
 *
 * Official karatsuba_zbc implementation — the "scalar_zb" champion.
 *
 * Optimisations:
 *   - A: Word-level carry-less multiply (Zbc clmul/clmulh)
 *   - B: Assign-on-first-write tree recomposition (noinit gen header)
 *   - C: Compile-time specialized recursion tree (gen_karatsuba_specialized.py)
 *   - D: Fixed-size hybrid leaf specializations (clmul_leaf_n9, clmul_leaf_n8)
 *        eliminating memset via assign-on-first-write; compact inner loops
 *        for I-cache efficiency on in-order cores
 *
 * The recursion tree for HQC-128 (n=277, T=16) produces only n=9 and n=8
 * leaves.  Specialized leaves use assign-on-first-write discipline with
 * compact inner loops for I-cache-friendly execution on in-order cores.
 *
 * KAT validated: 300/300 vectors across HQC-{128,192,256}.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
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

/* ── Karatsuba threshold ──────────────────────────────────────────────────
 * Default: HQC_KARATSUBA_THRESHOLD_DEFAULT (=16), which is the V2-A1 sweep
 * winner.  Override via -DKARATSUBA_THRESHOLD=4|8|16 at compile time for
 * experimental builds (karatsuba_zbc_exp via -DHQC_KARATSUBA_THRESHOLD=N).
 */
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD HQC_KARATSUBA_THRESHOLD_DEFAULT
#endif

/* ── Temporary buffer sizing ─────────────────────────────────────────────*/
#define TMP_BUFFER_WORDS (HQC_POLY_TMP_BUFFER_MULTIPLIER * VEC_N_SIZE_64)

/* ── Fixed-size leaf specializations (D optimization) ────────────────────
 * Hybrid n=8 and n=9 leaves: row-0 unrolled (assign-on-first-write) +
 * compact inner loops for remaining rows (I-cache friendly).
 */
#include "clmul_leaf_specialized.h"

/* ── Generic leaf multiply: memset + XOR-accumulate schoolbook ────────────
 * Fallback for leaf sizes not covered by fixed specializations (n=14, 15).
 * Uses memset to zero the output buffer, then XOR-accumulates products.
 *
 * Despite higher instruction count vs the 4-way pipelined variant, the
 * compact loop body (~6 instructions) stays resident in I-cache across all
 * 729 leaf calls at L256.  On the in-order X60 with its small I-cache this
 * yields ~6% fewer cycles than the unrolled alternative (empirical).
 */
static void clmul_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    uint64_t lo, hi;
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            clmul64(a[i], b[j], &lo, &hi);
            r[i+j]   ^= lo;
            r[i+j+1] ^= hi;
        }
    }
}

#include "karatsuba_specialized_gen.h"

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
