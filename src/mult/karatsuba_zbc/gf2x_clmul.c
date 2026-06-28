/* src/mult/karatsuba_zbc/gf2x_clmul.c
 *
 * Official karatsuba_zbc implementation — the "scalar_zb" champion.
 *
 * Optimisations (applied in order A → C → B):
 *   - A: Word-level carry-less multiply (Zbc clmul/clmulh)
 *   - C: Compile-time specialized recursion tree (gen_karatsuba_specialized.py)
 *   - B: Assign-on-first-write policy — both tree recomposition (direct-write
 *        discipline: z0/z2 written directly to output r[], overlap-save combine,
 *        eliminating two copy loops per recursion node) and fixed-size hybrid
 *        leaf specializations (clmul_leaf_n8, clmul_leaf_n9, clmul_leaf_n14,
 *        clmul_leaf_n15) eliminating memset via assign-on-first-write; compact
 *        inner loops for I-cache efficiency on in-order cores
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

/* ── Temporary buffer sizing ─────────────────────────────────────────────
 * With direct-write discipline, z0 and z2 are written directly to the
 * output buffer r (not into tmp).  The tmp layout per recursion level is:
 *   zmid (2*n0 words) + ta (n0 words) + tb (n0 words) + child_buf
 * Per-level cost: ~4*n0. Summing geometric series: < 4*n total.
 */
#define TMP_BUFFER_WORDS (4u * VEC_N_SIZE_64)

/* ── Fixed-size leaf specializations (B optimization — leaf level) ────────
 * Hybrid n=8, n=9, n=14, n=15 leaves: row-0 unrolled (assign-on-first-write)
 * + compact inner loops for remaining rows (I-cache friendly).
 * Covers ALL leaf sizes produced by the T=16 recursion tree for HQC-{128,192,256}.
 * Part of the assign-on-first-write (B) policy applied at the leaf level.
 */
#include "clmul_leaf_specialized.h"

/* ── Scalar helpers: direct-write Karatsuba combine ──────────────────────
 *
 * Called by the generated karatsuba_specialized_gen.h for tree-level
 * copy/XOR operations.  Plain scalar loops — no vector instructions.
 */

/* scalar_copy_u64: dst[i] = src[i] for n_words elements. */
static __attribute__((always_inline)) inline
void scalar_copy_u64(uint64_t * __restrict__ dst,
                     const uint64_t * __restrict__ src,
                     size_t n_words)
{
    for (size_t i = 0; i < n_words; i++)
        dst[i] = src[i];
}

/* scalar_xor_u64: dst[i] = src1[i] ^ src2[i] for n_words elements. */
static __attribute__((always_inline)) inline
void scalar_xor_u64(uint64_t * __restrict__ dst,
                    const uint64_t * __restrict__ src1,
                    const uint64_t * __restrict__ src2,
                    size_t n_words)
{
    for (size_t i = 0; i < n_words; i++)
        dst[i] = src1[i] ^ src2[i];
}

/* scalar_xor3_u64: dst[i] ^= zmid[i] ^ (z0 ? z0[i] : 0) ^ (z2 ? z2[i] : 0).
 * NULL z0 or z2 means that component is absent (treated as 0).
 */
static __attribute__((always_inline)) inline
void scalar_xor3_u64(uint64_t       * __restrict__ dst,
                     const uint64_t * __restrict__ src_zmid,
                     const uint64_t * __restrict__ src_z0,
                     const uint64_t * __restrict__ src_z2,
                     size_t n_words)
{
    if (src_z0 && src_z2) {
        for (size_t i = 0; i < n_words; i++)
            dst[i] ^= src_zmid[i] ^ src_z0[i] ^ src_z2[i];
    } else if (src_z0) {
        for (size_t i = 0; i < n_words; i++)
            dst[i] ^= src_zmid[i] ^ src_z0[i];
    } else {
        for (size_t i = 0; i < n_words; i++)
            dst[i] ^= src_zmid[i];
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
