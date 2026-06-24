/* tests/bench/bench_profile_mul.c
 *
 * Harness-free profiling driver for polynomial multiplication.
 * NO perf_event_open, NO median/sort — just a tight loop calling vect_mul().
 * Designed to be profiled externally via:
 *   perf record -e task-clock -F 4999 --call-graph fp -- ./bench_profile_mul_...
 *
 * Compile with: -O3 -fno-omit-frame-pointer -g -static
 * Do NOT use -fno-inline — we want the same inlining as the real binary.
 *
 * The iteration count (N_PROFILE) controls run duration. Target ~2-5 seconds
 * to get >5000 samples at 4999 Hz.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "gf2x.h"
#include "parameters.h"

#ifndef N_PROFILE
#  define N_PROFILE 50000
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

/* Static buffers — same structure as bench_mul.c */
static uint64_t poly_a[VEC_N_SIZE_64];
static uint64_t poly_b[VEC_N_SIZE_64];
static uint64_t poly_out[VEC_N_SIZE_64];

/* Simple PRNG for initialization (no dependency on randombytes) */
static uint64_t xorshift64_state = 0xdeadbeefcafe1234ULL;
static uint64_t xorshift64(void) {
    uint64_t x = xorshift64_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    xorshift64_state = x;
    return x;
}

static void init_random_poly(uint64_t *p, size_t n64) {
    for (size_t i = 0; i < n64; i++)
        p[i] = xorshift64();
}

static void init_sparse_poly(uint64_t *p, size_t n_bits, size_t weight) {
    memset(p, 0, (n_bits + 63) / 64 * sizeof(uint64_t));
    size_t step = n_bits / weight;
    for (size_t i = 0; i < weight; i++) {
        size_t pos = i * step + (xorshift64() % step);
        if (pos >= n_bits) pos = n_bits - 1;
        p[pos / 64] |= (1ULL << (pos % 64));
    }
}

/* Anti-DCE: prevent compiler from optimizing away the result */
static volatile uint64_t sink;

int main(void)
{
    /* Initialize polynomials */
    init_random_poly(poly_a, VEC_N_SIZE_64);
    init_sparse_poly(poly_b, PARAM_N, PARAM_OMEGA_R);

    /* Warm-up (prime i-cache, branch predictor) */
    for (int i = 0; i < 100; i++) {
        vect_mul(poly_out, poly_a, poly_b);
    }

    /* Main profiling loop — no measurement overhead */
    for (int i = 0; i < N_PROFILE; i++) {
        vect_mul(poly_out, poly_a, poly_b);
    }

    /* Anti-DCE */
    sink = poly_out[0] ^ poly_out[VEC_N_SIZE_64 - 1];
    (void)sink;

    return 0;
}
