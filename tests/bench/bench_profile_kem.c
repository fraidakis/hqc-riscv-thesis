/* tests/bench/bench_profile_kem.c
 *
 * Harness-free profiling driver for full KEM operations.
 * NO perf_event_open, NO median/sort — just loops of keypair+encaps+decaps.
 * Designed to be profiled externally via:
 *   perf record -e task-clock -F 4999 --call-graph fp -- ./bench_profile_kem_...
 *
 * Compile with: -O3 -fno-omit-frame-pointer -g -static
 * Do NOT use -fno-inline — we want the same inlining as the real binary.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "api.h"
#include "parameters.h"

#ifndef N_PROFILE
#  define N_PROFILE 5000
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

/* KEM buffers */
static uint8_t pk[PUBLIC_KEY_BYTES];
static uint8_t sk[SECRET_KEY_BYTES];
static uint8_t ct[CIPHERTEXT_BYTES];
static uint8_t ss_enc[SHARED_SECRET_BYTES];
static uint8_t ss_dec[SHARED_SECRET_BYTES];

/* Anti-DCE */
static volatile uint8_t sink;

int main(void)
{
    /* Warm-up */
    for (int i = 0; i < 10; i++) {
        crypto_kem_keypair(pk, sk);
        crypto_kem_enc(ct, ss_enc, pk);
        crypto_kem_dec(ss_dec, ct, sk);
    }

    /* Main profiling loop */
    for (int i = 0; i < N_PROFILE; i++) {
        crypto_kem_keypair(pk, sk);
        crypto_kem_enc(ct, ss_enc, pk);
        crypto_kem_dec(ss_dec, ct, sk);
    }

    /* Anti-DCE */
    sink = ss_dec[0] ^ ss_enc[0] ^ pk[0] ^ ct[0];
    (void)sink;

    return 0;
}
