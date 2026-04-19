/* src/poly_mul/karatsuba_zbc/gf2x_clmul.c
 *
 * Phase 1a — Karatsuba polynomial multiplication over GF(2)[x]
 * accelerated with the RISC-V Zbc carry-less multiply extension.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * Optimisation axes (to be implemented):
 *   1. clmul + clmulh base case: 128-bit carry-less product in 2 instructions
 *      (replaces ~30 AND/XOR/shift instructions on clmul-free platforms).
 *   2. Recursion depth sweep: empirically determine d* that minimises total
 *      cycle count for each security level (n = 277 / 561 / 901 64-bit words).
 *   3. Lazy reduction: defer modular reduction until the entire Karatsuba tree
 *      completes; saves 2-4 XOR/shift per clmul pair (~14 KB intermediate
 *      buffer for HQC-256, fits in L1 cache).
 *   4. 31-GPR register scheduling: keep sub-products and XOR-combine operands
 *      live in registers for d <= 3, eliminating intermediate spills.
 *
 * Verification: KAT vectors must pass after every change (run_kat.sh).
 *
 * TODO: implement
 */

#include <stdint.h>
#include "parameters.h"

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b) {
    (void)o; (void)a; (void)b;
    /* TODO: Phase 1a implementation */
    __builtin_trap();
}
