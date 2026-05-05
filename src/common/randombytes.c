/* src/common/randombytes.c
 *
 * Mode-switch wrapper for randombytes().
 *
 * HQC_KAT_MODE=1 (default):
 *   randombytes() delegates only to the AES-CTR DRBG (aes_ctr_drbg.c).
 *   randombytes_init(seed) must be called before the first randombytes() call.
 *   This is the only cryptographically valid path in this wrapper and it
 *   produces KAT-compatible output identical to the NIST rng.c.
 *
 * HQC_BENCH_MODE=1:
 *   randombytes_init() is a no-op.
 *   randombytes() fills output with a fixed repeating pattern seeded from
 *   BENCH_SEED (default 0xAB).  No AES overhead — the benchmark measures only
 *   the KEM arithmetic, not entropy generation. This path is deliberately not
 *   a CSPRNG and must never be used for KATs, tests that assert cryptographic
 *   behavior, or any production/security-sensitive build.
 *
 * Exactly one of HQC_KAT_MODE or HQC_BENCH_MODE must be defined by the build
 * system; CMakeLists.txt ensures this via target_compile_definitions().
 */

#include "randombytes.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(HQC_KAT_MODE) && defined(HQC_BENCH_MODE)
#  error "HQC_KAT_MODE and HQC_BENCH_MODE are mutually exclusive"
#endif
#if !defined(HQC_KAT_MODE) && !defined(HQC_BENCH_MODE)
#  error "Define exactly one of HQC_KAT_MODE or HQC_BENCH_MODE"
#endif

/* ── KAT mode: route exclusively to the AES-CTR DRBG CSPRNG ──────────────── */
#if defined(HQC_KAT_MODE)

#include "rng/aes_ctr_drbg.h"

/* randombytes_init/randombytes are provided directly by aes_ctr_drbg.c via
 * aes_ctr_drbg.h. Keeping KAT mode as a direct include prevents accidental
 * fallthrough to the non-cryptographic bench stub. */

/* ── BENCH mode: non-cryptographic fixed-pattern fill for throughput only ── */
#elif defined(HQC_BENCH_MODE)

#ifndef BENCH_SEED
#  define BENCH_SEED 0xABu
#endif

void randombytes_init(const uint8_t seed[48])
{
    (void)seed;   /* no-op in bench mode */
}

int randombytes(uint8_t *out, size_t len)
{
    /* Fill with a simple deterministic pattern.
     * Not cryptographically meaningful — only used to avoid branch variance
     * in KEM due to all-zero inputs. */
    for (size_t i = 0; i < len; i++)
        out[i] = (uint8_t)(BENCH_SEED ^ (i & 0xFF));
    return 0;
}

#endif /* HQC_KAT_MODE / HQC_BENCH_MODE */
