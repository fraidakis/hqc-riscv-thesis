/* src/decode/rvv/reed_solomon_rvv.c
 *
 * Phase 3 — RVV-accelerated Reed-Solomon decoder for HQC.
 *
 * This file replaces vendor/hqc-ref/src/ref/reed_solomon.c when
 * HQC_DECODE_IMPL=rvv is selected.
 *
 * The vendor scalar RS decoder executes three GF(2^8) polynomial operations:
 *   1. Syndrome computation  — 28 evaluations of a 45-term polynomial
 *   2. Berlekamp-Massey (BM) — sequential, data-dependent, hard to vectorise
 *   3. Chien search          — 256 polynomial evaluations over GF(2^8)
 *   4. Forney formula        — 28 GF(2^8) field inversions + multiplications
 *
 * RVV vectorisation axes:
 *   Axis A — Syndrome computation (§1):
 *     vluxei8.v for 32 simultaneous GF(2^8) multiplications via precomputed
 *     256-byte log/antilog tables.  Reduces syndrome loop from 45×28 scalar
 *     multiplications to ceil(45/32) × 28 vector passes.
 *
 *   Axis B — Chien search (§3):
 *     Parallelise across 256 candidate error positions using vle8.v + GF mul
 *     tables.  All 256 evaluations independent → natural SIMD parallelism.
 *
 *   Axis C — Berlekamp-Massey: NOT vectorised (sequential by design).
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/reed_solomon.h:
 *   void reed_solomon_decode(uint64_t *msg, uint64_t *cdw);
 *
 * TODO: implement Phase 3 — Axis A first (cleanest gain), then Axis B.
 */

#include <stdint.h>
#include <riscv_vector.h>
#include "reed_solomon.h"

void reed_solomon_decode(uint64_t *msg, uint64_t *cdw)
{
    (void)msg; (void)cdw;
    /* TODO: Phase 3 implementation */
    __builtin_trap();
}
