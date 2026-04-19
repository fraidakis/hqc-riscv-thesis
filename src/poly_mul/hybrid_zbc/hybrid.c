/* src/poly_mul/hybrid_zbc/hybrid.c
 *
 * Phase 1c — Hybrid Karatsuba-FAFFT polynomial multiplication.
 *
 * Strategy: apply 1–2 levels of Karatsuba splitting to reduce the problem to
 * 3–9 sub-multiplications of ~280–300 words each, then solve each sub-problem
 * with the FAFFT (fafft.c).  Two goals:
 *
 *   1. Eliminate FAFFT padding waste: n=17669 pads to 32768 directly, but
 *      two Karatsuba sub-problems of ~8835 each pad to 16384 — halving wasted
 *      work.  Alternative: CRT decomposition (Jang et al.) for a cleaner split.
 *
 *   2. Inherit FAFFT's O(n log^2 n) asymptotic advantage for the inner multiplies
 *      while incurring only O(n) Karatsuba combine overhead at the outer level.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * TODO: implement after Phase 1b (fafft_zbc) is benchmarked — the outer
 *       Karatsuba levels reuse the fafft_zbc vect_mul internally.
 */

#include <stdint.h>
#include "parameters.h"

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b) {
    (void)o; (void)a; (void)b;
    /* TODO: Phase 1c implementation */
    __builtin_trap();
}
