/* tests/ct/ct_harness.c
 *
 * Constant-time verification harness for HQC-RISC-V.
 *
 * Strategy (ctgrind / Valgrind Memcheck):
 *   1. Generate a keypair.  sk holds real, valid key material.
 *   2. Encapsulate against pk — produces (ct, ss_enc).
 *   3. Poison sk with VALGRIND_MAKE_MEM_UNDEFINED.
 *      Valgrind now propagates "uninitialised" taint through every
 *      computation that reads from sk.
 *   4. Call crypto_kem_dec(ss_dec, ct, sk).
 *      Any conditional branch or load/store address that depends on sk
 *      is a constant-time violation and is reported as an error.
 *   5. Mark sk and ss_dec as DEFINED before cleanup to suppress false
 *      positives from the zeroisation code itself.
 *
 * Build notes:
 *   - Compiled natively (x86-64) by scripts/ci/check_ct.sh.
 *   - This file provides randombytes() / randombytes_init() via getrandom(2).
 *     Do NOT link src/common/randombytes.c alongside this file.
 *   - Do NOT define HQC_KAT_MODE or HQC_BENCH_MODE.
 *   - Requires valgrind-dev (libvalgrind-dev on Debian/Ubuntu) for
 *     <valgrind/memcheck.h>.
 *
 * Usage:
 *   valgrind --tool=memcheck --error-exitcode=1 \
 *            --track-origins=yes --leak-check=no \
 *            ./build/ct/ct_hqc128_karatsuba_ref
 */

#define _DEFAULT_SOURCE   /* SYS_getrandom on glibc */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <valgrind/memcheck.h>

#include "api.h"
#include "parameters.h"
#include "crypto_memset.h"
#include "randombytes.h"   /* declarations only — definitions are below */

/* ── randombytes — provided by this file for native CT builds ─────────────── */

void randombytes_init(const uint8_t seed[48]) { (void)seed; /* no-op */ }

int randombytes(uint8_t *out, size_t len)
{
    size_t done = 0;
    while (done < len) {
        ssize_t n = syscall(SYS_getrandom, out + done, len - done, 0);
        if (n < 0) {
            perror("[ct] getrandom");
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

/* ── Single CT trial ─────────────────────────────────────────────────────── */

static int ct_trial(int trial)
{
    uint8_t pk[PUBLIC_KEY_BYTES];
    uint8_t sk[SECRET_KEY_BYTES];
    uint8_t ct[CIPHERTEXT_BYTES];
    uint8_t ss_enc[SHARED_SECRET_BYTES];
    uint8_t ss_dec[SHARED_SECRET_BYTES];

    /* (1) Key generation — sk is filled with real, valid key material. */
    if (crypto_kem_keypair(pk, sk) != 0) {
        fprintf(stderr, "[ct] trial %d: keypair() failed\n", trial);
        return 1;
    }

    /* (2) Encapsulation — uses only pk (public); no secret data touched. */
    if (crypto_kem_enc(ct, ss_enc, pk) != 0) {
        fprintf(stderr, "[ct] trial %d: enc() failed\n", trial);
        return 1;
    }

    /* (3) Poison the secret key.
     * From this point, every byte of sk is "uninitialised" in Valgrind's
     * view.  Any conditional branch or memory address derived from sk will
     * be flagged as a constant-time violation.
     */
    VALGRIND_MAKE_MEM_UNDEFINED(sk, SECRET_KEY_BYTES);

    /* (4) Decapsulation — the operation under scrutiny.
     * Ignore the return value: it is derived from sk and would itself be
     * tainted, causing a false-positive at the if-check in this harness.
     * All genuine violations are inside crypto_kem_dec().
     */
    (void) crypto_kem_dec(ss_dec, ct, sk);

    /* (5) Restore definedness before zeroisation so Valgrind does not
     * flag the cleanup writes as operating on tainted memory.
     */
    VALGRIND_MAKE_MEM_DEFINED(sk,     SECRET_KEY_BYTES);
    VALGRIND_MAKE_MEM_DEFINED(ss_dec, SHARED_SECRET_BYTES);

    memset_zero(sk,     SECRET_KEY_BYTES);
    memset_zero(ss_enc, SHARED_SECRET_BYTES);
    memset_zero(ss_dec, SHARED_SECRET_BYTES);

    return 0;
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

#ifndef CT_N_ITER
#  define CT_N_ITER 5
#endif

int main(void)
{
    fprintf(stderr, "[ct] HQC-%d (%s)  %d trial(s)\n",
            PARAM_SECURITY, CRYPTO_ALGNAME, CT_N_ITER);

    int rc = 0;
    for (int i = 0; i < CT_N_ITER; i++)
        rc |= ct_trial(i);

    fprintf(stderr, rc == 0 ? "[ct] PASS\n" : "[ct] FAIL (internal error)\n");
    return rc;
}
