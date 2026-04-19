/* src/poly_mul/fafft_zbc/fafft.c
 *
 * Phase 1b — Frobenius Additive FFT (FAFFT) for HQC polynomial multiplication
 * over GF(2)[x]/(x^n - 1), scalar component.
 *
 * Based on: Jang et al., "Efficient Polynomial Multiplication for HQC on
 * ARM Cortex-M4" (ePrint).  Algorithm structure (FAFFT recursion, Cantor
 * basis conversion, CRT decomposition) is ported from Jang's Cortex-M4
 * implementation to plain C targeting RV64.
 *
 * GF(2^m) butterfly multiplications in this file use RISC-V Zbc instructions
 * (clmul / clmulh) with level-specific Barrett reduction.  For small fields
 * (m <= 8), precomputed 256-byte lookup tables are used instead.
 *
 * See also: fafft_rvv.c for the RVV-vectorised XOR combine phases.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * TODO: implement — port Jang et al. C-level FAFFT logic, replace ARM asm
 *       GF(2^m) butterfly with clmul + Barrett reduction sequences.
 */

#include <stdint.h>
#include "parameters.h"

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b) {
    (void)o; (void)a; (void)b;
    /* TODO: Phase 1b implementation */
    __builtin_trap();
}
