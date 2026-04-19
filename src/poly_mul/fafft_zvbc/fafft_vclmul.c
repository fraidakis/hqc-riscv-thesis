/* src/poly_mul/fafft_zvbc/fafft_vclmul.c
 *
 * Phase 1+ (Zvbc upper bound) — FAFFT with vclmul.vv / vclmulh.vv vectorised
 * butterfly multiplications.
 *
 * This tier requires the RISC-V Zvbc extension.  At the time of writing,
 * Zvbc is ratified but not yet available in production silicon.  Benchmarking
 * is therefore done on Spike only:
 *   spike --isa=rv64gcv_zvbc ...
 *
 * The purpose of this tier is to QUANTIFY the incentive for vendors to ship
 * Zvbc for post-quantum cryptography.  The result (Tier 4 vs Tier 2 speedup)
 * directly answers the question: "how much does vclmul.vv help over scalar
 * clmul for HQC?"
 *
 * vclmul.vv processes VLEN/64 butterfly GF(2^m) multiplications per
 * instruction — 4× per instruction for VLEN=256 vs scalar clmul.  Combined
 * with vectorised Barrett reduction (vxor.vv / vsll.vi / vsrl.vi), this
 * replaces the scalar clmul loop from fafft_zbc entirely.
 *
 * Detected at compile time via HQC_POLYMUL_ZVBC=1 (set by CMakeLists.txt).
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * TODO: implement after fafft_zbc is complete — replace scalar clmul butterfly
 *       loop with vclmul.vv + vectorised Barrett, keeping the FAFFT structure.
 */

#include <stdint.h>
#include <riscv_vector.h>
#include "parameters.h"

#ifndef HQC_POLYMUL_ZVBC
#  error "fafft_vclmul.c must be compiled with -DHQC_POLYMUL_ZVBC=1"
#endif

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b) {
    (void)o; (void)a; (void)b;
    /* TODO: Phase 1+ implementation */
    __builtin_trap();
}
