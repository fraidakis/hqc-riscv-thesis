/* src/keccak/rvv/fips202_rvv.c
 *
 * Phase 2 — RVV-vectorised Keccak-f[1600] and HQC-specific SHAKE-256 wrappers.
 *
 * This file replaces vendor/hqc-ref/lib/fips202/fips202.c when
 * HQC_KECCAK_IMPL=rvv is selected (cmake -DHQC_KECCAK_IMPL=rvv).
 *
 * Source of the RVV Keccak core:
 *   Zhang et al., "Keccak on RVV" (ePrint 2023/xxx).
 *   Multi-instance vectorised Keccak-f[1600] with 4-way interleaving.
 *
 * HQC-specific observations that make RVV Keccak attractive here:
 *   1. seed_expand() (PRF for sparse vector generation) calls SHAKE-256 with a
 *      fresh 32-byte seed + 1-byte domain separator, producing 32–64 bytes of
 *      output per call.  This pattern repeats ≥2×(HQC_N / 8) times per KEM
 *      operation — ideal for 4-way interleaving (4 seeds processed in parallel
 *      across the vector lanes of one logical Keccak-f[1600] evaluation).
 *   2. prng_update() calls SHA3-256 for DRBG reseed — single-call, not in a hot
 *      loop; RVV overhead here is noise.
 *
 * This file provides:
 *   shake256()              — same signature as fips202.h
 *   shake256_4way()         — 4-instance interleaved entry point for seed_expand
 *
 * To be included: Zhang et al.'s Keccak-f[1600] RVV round function and the
 * theta/rho/pi/chi/iota step assembly or C-with-builtins port.
 *
 * TODO: import Zhang et al. core, adapt to HQC API (shake256 must match
 *       fips202.h exactly), add 4-way wrapper for seed_expand.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <riscv_vector.h>
#include "fips202.h"

void shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen)
{
    (void)out; (void)outlen; (void)in; (void)inlen;
    /* TODO: Phase 2 implementation */
    __builtin_trap();
}
