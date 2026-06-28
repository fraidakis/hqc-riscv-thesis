/* src/mult/karatsuba_zbc/gf2x_clmul_ablation.c
 *
 * Parametric ablation file for the scalar Karatsuba Zbc path.
 *
 * Compile-time flags select which optimizations are active:
 *
 *   ABLATION_WORDLEVEL  (A) : 1 = clmul64 word-level leaves
 *                             0 = bit-serial schoolbook (reference behaviour)
 *
 *   ABLATION_NOINIT     (B) : 1 = assign-on-first-write at tree recomposition
 *                             0 = memset(r, 0, ...) + XOR-accumulate recomposition
 *                             NOTE: applies to tree-level only; leaf always uses
 *                             the simple memset + XOR-accumulate pattern unless
 *                             leaf specialization is enabled.
 *
 *   ABLATION_AP2        (C) : 1 = compile-time specialized recursion tree
 *                             0 = generic recursive Karatsuba with n>>1 split
 *
 *   ABLATION_LEAF_SPEC  (D) : 1 = fixed-size hybrid leaf specializations
 *                                 (clmul_leaf_n8, clmul_leaf_n9) with
 *                                 assign-on-first-write + compact inner loops
 *                             0 = generic memset + XOR-accumulate leaf loop
 *                             NOTE: requires A=1 and C=1 (word-level clmul and
 *                             known fixed leaf sizes from the specialized tree).
 *
 * Intended variant matrix (orthogonal axes A, B, C, D):
 *   scalar_zb_A   : A=1, B=0, C=0, D=0  (word-level only)
 *   scalar_zb_AB  : A=1, B=1, C=0, D=0  (word-level + noinit recomposition)
 *   scalar_zb_AC  : A=1, B=0, C=1, D=0  (word-level + specialized tree, generic leaves)
 *   scalar_zb_ACB : A=1, B=1, C=1, D=0  (word-level + specialized tree + noinit)
 *   scalar_zb_ACD : A=1, B=0, C=1, D=1  (word-level + specialized tree + hybrid leaves)
 *   scalar_zb     : A=1, B=1, C=1, D=1  (all four — champion)
 *
 * Orthogonality guarantee:
 *   - A controls the leaf multiply primitive (clmul vs bit-serial)
 *   - B controls tree-level recomposition style (direct-write vs memset+XOR)
 *   - C controls tree structure (specialized compile-time tree with known
 *     fixed leaf sizes vs generic runtime recursion with n>>1 split)
 *   - D controls leaf specialization (hybrid fixed-size leaves with compact
 *     loops vs generic loop-based leaves). Requires C=1.
 *   Each axis (A, B, C) is independent; D depends on C.
 *
 * KAT validated by running all ablation variants against known-answer vectors.
 */

#include <stdint.h>
#include <string.h>
#include <riscv_bitmanip.h>
#include "parameters.h"
#include "hqc_parameters.h"

/* ── Default flags if not set at compile time ────────────────────────────*/
#ifndef ABLATION_WORDLEVEL
#define ABLATION_WORDLEVEL 1
#endif
#ifndef ABLATION_NOINIT
#define ABLATION_NOINIT 0
#endif
#ifndef ABLATION_AP2
#define ABLATION_AP2 0
#endif
#ifndef ABLATION_LEAF_SPEC
#define ABLATION_LEAF_SPEC 0
#endif
#ifndef ABLATION_SYMMETRIC_TREE
#define ABLATION_SYMMETRIC_TREE 0
#endif

/* ── Threshold and buffer sizing ─────────────────────────────────────────*/
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD HQC_KARATSUBA_THRESHOLD_DEFAULT
#endif

#define TMP_BUFFER_WORDS (HQC_POLY_TMP_BUFFER_MULTIPLIER * VEC_N_SIZE_64)

/* ═══════════════════════════════════════════════════════════════════════════
 * LEAF MULTIPLICATION
 * ═══════════════════════════════════════════════════════════════════════════*/

#if ABLATION_WORDLEVEL

/* ── Word-level leaf using Zbc clmul ─────────────────────────────────────*/
static __attribute__((always_inline)) inline
void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    *lo = __riscv_clmul_64(a, b);
    *hi = __riscv_clmulh_64(a, b);
}

#if ABLATION_LEAF_SPEC
/* ── Fixed-size leaf specializations (D optimization) ────────────────────
 * Hybrid leaves for the hot-path sizes (n=8, n=9) that the specialized
 * recursion tree (C) produces.  Row 0 unrolled (assign-on-first-write),
 * remaining rows use compact inner loops for I-cache efficiency.
 * Requires A=1 (word-level clmul) and C=1 (specialized tree for fixed sizes).
 */
#include "clmul_leaf_specialized.h"
#endif /* ABLATION_LEAF_SPEC */

/* Simple memset + XOR-accumulate leaf — used as generic fallback.
 * When D=1, this is only called for leaf sizes not covered by specializations
 * (n=14, n=15 in HQC-192/256 recursion trees).
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

#else /* !ABLATION_WORDLEVEL — bit-serial schoolbook from reference */

static void schoolbook_mul(uint64_t *r, const uint64_t *a, const uint64_t *b, size_t n) {
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < n; i++) {
        uint64_t ai = a[i];
        for (int bit = 0; bit < 64; bit++) {
            uint64_t mask = -((ai >> bit) & 1ULL);
            size_t base = i;
            int sh = bit;
            int inv = 64 - sh;
            if (sh == 0) {
                for (size_t j = 0; j < n; j++) {
                    r[base + j] ^= b[j] & mask;
                }
            } else {
                for (size_t j = 0; j < n; j++) {
                    r[base + j] ^= (b[j] << sh) & mask;
                    r[base + j + 1] ^= (b[j] >> inv) & mask;
                }
            }
        }
    }
}

#endif /* ABLATION_WORDLEVEL */


/* ═══════════════════════════════════════════════════════════════════════════
 * KARATSUBA RECURSION
 * ═══════════════════════════════════════════════════════════════════════════*/

#if ABLATION_AP2

/* Compile-time specialized recursion tree.
 * B selects the recomposition style:
 *   B=1 (noinit) → gen header with direct-write recomposition
 *   B=0 (memset) → gen header with memset + XOR-accumulate recomposition
 * D selects leaf dispatch (via LEAF_CALL_N8/N9 macros in gen header):
 *   D=1 → hybrid clmul_leaf_n8/n9 (from clmul_leaf_specialized.h)
 *   D=0 → generic clmul_leaf(..., 8u/9u)
 * Both call clmul_leaf() for sizes not covered by specializations (n=14, 15).
 */

#if ABLATION_NOINIT
#include "karatsuba_specialized_gen.h"
#else
#include "karatsuba_specialized_memset_gen.h"
#endif

#elif ABLATION_SYMMETRIC_TREE

/* Symmetric (n>>1) specialized recursion tree — compile-time unrolled.
 * Same B-conditional header selection as ABLATION_AP2.
 */

#if ABLATION_NOINIT
#include "karatsuba_specialized_symmetric_gen.h"
#else
#include "karatsuba_specialized_symmetric_memset_gen.h"
#endif

#else /* !ABLATION_AP2 && !ABLATION_SYMMETRIC_TREE — generic recursion */

static void karatsuba_mul_generic(uint64_t *r, const uint64_t *a, const uint64_t *b,
                                  size_t n, uint64_t *tmp_buffer)
{
#if ABLATION_WORDLEVEL
    if (n <= KARATSUBA_THRESHOLD) {
        clmul_leaf(r, a, b, n);
        return;
    }
#else
    if (n <= KARATSUBA_THRESHOLD) {
        schoolbook_mul(r, a, b, n);
        return;
    }
#endif

    /* Reference split: n>>1 */
    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;

    uint64_t *z0   = tmp_buffer;
    uint64_t *z2   = z0 + 2 * n;
    uint64_t *zmid = z2 + 2 * n;
    uint64_t *ta   = zmid + 2 * n;
    uint64_t *tb   = ta + n;
    uint64_t *child_buffer = tmp_buffer + 8 * n;

    karatsuba_mul_generic(z0, a, b, n0, child_buffer);
    karatsuba_mul_generic(z2, a + m, b + m, n1, child_buffer);

    for (size_t i = 0; i < n1; i++) {
        uint64_t loa = (i < n0 ? a[i] : 0);
        uint64_t lob = (i < n0 ? b[i] : 0);
        ta[i] = loa ^ a[m + i];
        tb[i] = lob ^ b[m + i];
    }
    karatsuba_mul_generic(zmid, ta, tb, n1, child_buffer);

#if ABLATION_NOINIT
    /* Direct-write recomposition (no memset) — B optimization */
    for (size_t i = 0; i < 2 * n0; i++) r[i] = z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i] = z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        uint64_t z2i = (i < 2 * n1 ? z2[i] : 0);
        uint64_t mid = zmid[i] ^ z0i ^ z2i;
        r[m + i] ^= mid;
    }
#else
    /* Memset + XOR-accumulate recomposition (reference style) */
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2 * n0; i++) r[i] ^= z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i] ^= z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        uint64_t z2i = (i < 2 * n1 ? z2[i] : 0);
        uint64_t mid = zmid[i] ^ z0i ^ z2i;
        r[m + i] ^= mid;
    }
#endif
}

#endif /* ABLATION_AP2 */


/* ═══════════════════════════════════════════════════════════════════════════
 * REDUCTION AND PUBLIC ENTRY POINT
 * ═══════════════════════════════════════════════════════════════════════════*/

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

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

#if ABLATION_AP2 || ABLATION_SYMMETRIC_TREE
    /* Compile-time specialized tree */
    switch (VEC_N_SIZE_64) {
    case 277u: kmul_277(unreduced, a, b, tmp_buf); break;
    case 561u: kmul_561(unreduced, a, b, tmp_buf); break;
    case 901u: kmul_901(unreduced, a, b, tmp_buf); break;
    default:   __builtin_trap();
    }
#else
    /* Generic n>>1 recursion */
    karatsuba_mul_generic(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
#endif

    reduce(o, unreduced);
}
