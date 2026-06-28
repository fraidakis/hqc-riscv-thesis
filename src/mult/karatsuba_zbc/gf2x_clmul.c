/* src/mult/karatsuba_zbc/gf2x_clmul.c
 *
 * Official karatsuba_zbc implementation — the "scalar_zb" champion.
 *
 * Optimisations (applied in order A → C → B):
 *   - A: Word-level carry-less multiply (Zbc clmul/clmulh)
 *   - C: Compile-time specialized recursion tree (gen_karatsuba_specialized.py)
 *   - B: Assign-on-first-write policy — both tree recomposition (noinit gen
 *        header) and fixed-size hybrid leaf specializations (clmul_leaf_n8,
 *        clmul_leaf_n9, clmul_leaf_n14, clmul_leaf_n15) eliminating memset
 *        via assign-on-first-write; compact inner loops for I-cache efficiency
 *        on in-order cores
 *
 * The recursion tree for HQC-128 (n=277, T=16) produces only n=9 and n=8
 * leaves; HQC-192 also produces n=8/n=9; HQC-256 (n=901, T=16) produces
 * n=14 and n=15 leaves.  All sizes have dedicated specializations.
 *
 * KAT validated: 300/300 vectors across HQC-{128,192,256}.
 */

#include <stdint.h>
#include <stddef.h>
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

/* ── Fixed-size leaf specializations (B optimization — leaf level) ────────
 * Hybrid n=8, n=9, n=14, n=15 leaves: row-0 unrolled (assign-on-first-write)
 * + compact inner loops for remaining rows (I-cache friendly).
 * Covers ALL leaf sizes produced by the T=16 recursion tree for HQC-{128,192,256}.
 * Part of the assign-on-first-write (B) policy applied at the leaf level.
 */
#include "clmul_leaf_specialized.h"

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
