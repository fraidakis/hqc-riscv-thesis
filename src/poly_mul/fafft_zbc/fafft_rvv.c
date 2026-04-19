/* src/poly_mul/fafft_zbc/fafft_rvv.c
 *
 * Phase 1b (RVV axes) — RVV-vectorised helper routines for the FAFFT.
 *
 * This file implements the three RVV vectorisation axes for the FAFFT that
 * do NOT require vclmul.vv (Zvbc).  Compiled for rv64gcv_zbc.
 *
 * Axis 1 — Bulk XOR combine phases:
 *   The FAFFT at each stage XORs two coefficient arrays of ~277 / 561 / 901
 *   64-bit words.  Vectorise with:
 *     vle64.v → vxor.vv → vse64.v   using LMUL=4 (16 elements per group).
 *   Replaces a scalar XOR loop over N words with ceil(N/16) vector iterations.
 *
 * Axis 2 — Tower-field table lookup via vluxei8.v:
 *   At deeper FAFFT levels (small fields m <= 8), butterfly multiplications
 *   use precomputed 256-byte tables.  vluxei8.v performs 32 simultaneous
 *   GF(2^8) table lookups per instruction (VLEN=256).
 *
 * Axis 3 — Butterfly-order memory permutation:
 *   vrgatherei16.v for stride-friendly RVV streaming (applied once before
 *   the first FAFFT stage and once after the last).
 *
 * TODO: implement after fafft.c (Axis 1 first — it is the simplest and has
 *       the most isolated speedup; Axes 2 and 3 follow).
 */

#include <stdint.h>
#include <riscv_vector.h>
#include "parameters.h"

/*
 * hqc_vxor_words — XOR two arrays of 64-bit words using RVV vxor.vv LMUL=4.
 * o[0..n-1] ^= a[0..n-1]
 * Called from the FAFFT combine phases in fafft.c.
 */
void hqc_vxor_words(uint64_t * restrict o,
                    const uint64_t * restrict a,
                    size_t n_words)
{
    /* TODO: Phase 1b Axis 1 implementation */
    (void)o; (void)a; (void)n_words;
    __builtin_trap();
}
