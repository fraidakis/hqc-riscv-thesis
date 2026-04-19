/* src/decode/rvv/reed_muller_rvv.c
 *
 * Phase 3 — RVV-vectorised 128-point Hadamard transform for RM decoding.
 *
 * This file replaces vendor/hqc-ref/src/ref/reed_muller.c when
 * HQC_DECODE_IMPL=rvv is selected (cmake -DHQC_DECODE_IMPL=rvv).
 *
 * The vendor scalar RM decoder performs a 128-point Fast Hadamard Transform
 * (FHT) over Z.  The RVV implementation vectorises the butterfly stages:
 *
 *   Stages 0–6 (128 = 2^7 points):  each stage processes 64 butterfly pairs.
 *   Each butterfly: {a, b} → {a+b, a-b}  (signed 32-bit for saturation safety).
 *
 * RVV strategy:
 *   Stage k processes pairs at stride 2^k:
 *     - Load  { a[2^k*i], a[2^k*i + 2^(k-1)] }  via vle32.v (stride-gather or
 *       sequential depending on stage)
 *     - Compute add/sub pair with vadd.vv / vsub.vv
 *     - Store back
 *   LMUL=4 → 32 butterfly pairs per strip-mine iteration.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/reed_muller.h:
 *   void reed_muller_decode(uint8_t *msg, const uint8_t *cdw);
 *
 * TODO: implement Phase 3 — start with vectorised innermost FHT stage (most
 *       regular memory pattern), then move to strided stages 0–5, measure
 *       cycle reduction per stage, document in analysis/benchmarks/.
 */

#include <stdint.h>
#include <riscv_vector.h>
#include "reed_muller.h"

void reed_muller_decode(uint8_t *msg, const uint8_t *cdw)
{
    (void)msg; (void)cdw;
    /* TODO: Phase 3 implementation */
    __builtin_trap();
}
