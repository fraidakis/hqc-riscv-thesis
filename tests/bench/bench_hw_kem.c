/* tests/bench/bench_hw_kem.c
 *
 * Full KEM benchmark for HQC on real RISC-V hardware (Linux userspace).
 *
 * Mirrors bench_kem.c but uses perf_event_open instead of bare-metal rdcycle.
 * Designed for native compilation on BPI-F3 (SpacemiT X60, GCC 14.2).
 *
 * Output format (compatible with scripts/report/extract_results.py):
 *   # level=<L>,polymul=<P>,n_iter=<N>,n_warmup=<W>,platform=hw
 *   op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *   keypair,...
 *   encaps,...
 *   decaps,...
 *
 * Compile (native on board):
 *   gcc -O3 -static -march=rv64imafdcv_zba_zbb_zbc_zbs \
 *       -DSECURITY_LEVEL=128 -DHQC_128 -DHQC_BENCH_MODE=1 \
 *       -DPOLYMUL_NAME=\"karatsuba_zbc\" -DN_ITER=10000 -DN_WARMUP=100 \
 *       -I../../vendor/hqc-ref/src/common/hqc-1 \
 *       -I../../vendor/hqc-ref/src/common \
 *       -I../../vendor/hqc-ref/src/ref/hqc-1 \
 *       -I../../vendor/hqc-ref/src/ref \
 *       -I../../vendor/hqc-ref/lib/fips202 \
 *       -I../../src/common \
 *       bench_hw_kem.c \
 *       ../../src/hqc.c \
 *       ../../vendor/hqc-ref/src/common/code.c \
 *       ../../vendor/hqc-ref/src/common/symmetric.c \
 *       ../../vendor/hqc-ref/src/ref/reed_muller.c \
 *       ../../vendor/hqc-ref/src/ref/reed_solomon.c \
 *       ../../vendor/hqc-ref/src/common/fft.c \
 *       ../../vendor/hqc-ref/lib/fips202/fips202.c \
 *       ../../src/vector.c \
 *       ../../src/common/randombytes.c \
 *       <POLYMUL_SOURCE> \
 *       -lm -o bench_hw_kem
 *
 * Run:
 *   sudo nice -n -20 taskset -c 7 ./bench_hw_kem
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

#include "api.h"
#include "parameters.h"
#include "symmetric.h"  /* prng_init */
#include "perf_hw.h"

#ifndef N_ITER
#  define N_ITER 10000
#endif

#ifndef N_WARMUP
#  define N_WARMUP 100
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

/* ── Static buffers ──────────────────────────────────────────────────────── */
static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss_enc[CRYPTO_BYTES];
static uint8_t ss_dec[CRYPTO_BYTES];

/* ── Timing sample storage (heap-allocated for large N_ITER) ─────────────── */
static uint64_t *samples_cyc;
static uint64_t *samples_ins;

/* ── CSV emit helper ─────────────────────────────────────────────────────── */
static void emit_csv_row(const char *op, int n)
{
    perf_hw_sort_u64(samples_cyc, n);
    perf_hw_sort_u64(samples_ins, n);

    double mean_c = perf_hw_mean(samples_cyc, n);
    double std_c  = perf_hw_stddev(samples_cyc, n, mean_c);
    double mean_i = perf_hw_mean(samples_ins, n);
    double std_i  = perf_hw_stddev(samples_ins, n, mean_i);

    printf("%s,%d,"
           "%lu,%lu,%lu,%.1f,%.1f,"
           "%lu,%lu,%lu,%.1f,%.1f\n",
           op, n,
           (unsigned long)samples_cyc[0],
           (unsigned long)perf_hw_median(samples_cyc, n),
           (unsigned long)samples_cyc[n - 1],
           mean_c, std_c,
           (unsigned long)samples_ins[0],
           (unsigned long)perf_hw_median(samples_ins, n),
           (unsigned long)samples_ins[n - 1],
           mean_i, std_i);
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* ── System checks ────────────────────────────────────────────────── */
    perf_hw_check_system();

    /* ── Allocate sample arrays ───────────────────────────────────────── */
    samples_cyc = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_ins = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_cyc || !samples_ins) {
        fprintf(stderr, "ERROR: failed to allocate sample arrays for %d iterations\n", N_ITER);
        return 1;
    }

    /* ── Initialize perf counters ─────────────────────────────────────── */
    perf_hw_ctx ctx;
    if (perf_hw_init(&ctx) != 0) {
        fprintf(stderr, "ERROR: cannot initialize perf counters\n");
        return 1;
    }

    /* ── Seed the SHAKE-256 PRNG (required by kem.c) ─────────────────── */
    static const uint8_t dummy_seed[48] = { 0 };
    prng_init((uint8_t *)dummy_seed, NULL, 48, 0);

    /* ── Warm-up passes (prime i-cache, branch predictor, PRNG state) ── */
    for (int i = 0; i < N_WARMUP; i++) {
        crypto_kem_keypair(pk, sk);
        crypto_kem_enc(ct, ss_enc, pk);
        crypto_kem_dec(ss_dec, ct, sk);
    }

    /* ── Correctness check ────────────────────────────────────────────── */
    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, ss_enc, pk);
    crypto_kem_dec(ss_dec, ct, sk);
    if (memcmp(ss_enc, ss_dec, CRYPTO_BYTES) != 0) {
        fprintf(stderr, "ERROR: ss_enc != ss_dec — decaps failure!\n");
        perf_hw_close(&ctx);
        return 1;
    }

    /* ── Emit metadata header ─────────────────────────────────────────── */
    printf("# level=%d,polymul=%s,n_iter=%d,n_warmup=%d,platform=hw\n",
           SECURITY_LEVEL, POLYMUL_NAME, N_ITER, N_WARMUP);
    printf("op,iterations,"
           "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
           "min_instret,median_instret,max_instret,mean_instret,stddev_instret\n");

    /* ── KeyGen ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        crypto_kem_keypair(pk, sk);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
    emit_csv_row("keypair", N_ITER);

    /* ── Encaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        crypto_kem_enc(ct, ss_enc, pk);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
    emit_csv_row("encaps", N_ITER);

    /* ── Decaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        crypto_kem_dec(ss_dec, ct, sk);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
    emit_csv_row("decaps", N_ITER);

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    perf_hw_close(&ctx);
    free(samples_cyc);
    free(samples_ins);

    return 0;
}
