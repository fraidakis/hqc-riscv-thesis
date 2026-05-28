/* tests/bench/bench_hw_mul.c
 *
 * Polynomial multiplication micro-benchmark for real RISC-V hardware.
 *
 * Mirrors bench_mul.c but uses perf_event_open instead of bare-metal rdcycle.
 * Benchmarks vect_mul() in isolation — no KEM, no Keccak.
 *
 * Output format (compatible with scripts/report/extract_results.py):
 *   # level=<L>,polymul=<P>,n_iter=<N>,n_warmup=<W>,platform=hw
 *   op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *   vect_mul,...
 *
 * Compile (native on board):
 *   gcc -O3 -static -march=rv64imafdcv_zba_zbb_zbc_zbs \
 *       -DSECURITY_LEVEL=128 -DHQC_128 -DHQC_BENCH_MODE=1 \
 *       -DPOLYMUL_NAME=\"karatsuba_zbc\" -DN_ITER=10000 -DN_WARMUP=100 \
 *       -I../../vendor/hqc-ref/src/common/hqc-1 \
 *       -I../../vendor/hqc-ref/src/common \
 *       -I../../vendor/hqc-ref/src/ref/hqc-1 \
 *       -I../../vendor/hqc-ref/src/ref \
 *       -I../../src/common \
 *       bench_hw_mul.c \
 *       <POLYMUL_SOURCE> \
 *       -lm -o bench_hw_mul
 *
 * Run:
 *   sudo nice -n -20 taskset -c 7 ./bench_hw_mul
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gf2x.h"
#include "parameters.h"
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
static uint64_t poly_a[VEC_N_SIZE_64];   /* dense, mimics HQC public-key element */
static uint64_t poly_b[VEC_N_SIZE_64];   /* sparse weight-PARAM_OMEGA_R */
static uint64_t poly_out[VEC_N_SIZE_64];

/* ── Timing sample storage (heap-allocated) ──────────────────────────────── */
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

    /* ── Initialize test polynomials ──────────────────────────────────── */
    /* poly_a: dense pseudo-random (mimics HQC h public-key element) */
    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        poly_a[i] = 0xA5A5A5A5A5A5A5A5ULL ^ ((uint64_t)i * 0x9e3779b97f4a7c15ULL);
    }

    /* poly_b: sparse with PARAM_OMEGA_R set bits (mimics HQC r/e vector) */
    memset(poly_b, 0, sizeof(poly_b));
    for (size_t i = 0; i < (size_t)PARAM_OMEGA_R; i++) {
        size_t bit_pos = (uint64_t)i * PARAM_N / PARAM_OMEGA_R;
        poly_b[bit_pos / 64] |= (uint64_t)1 << (bit_pos % 64);
    }

    /* ── Initialize perf counters ─────────────────────────────────────── */
    perf_hw_ctx ctx;
    if (perf_hw_init(&ctx) != 0) {
        fprintf(stderr, "ERROR: cannot initialize perf counters\n");
        return 1;
    }

    /* ── Warm-up passes (prime i-cache, branch predictor) ─────────────── */
    for (int i = 0; i < N_WARMUP; i++) {
        vect_mul(poly_out, poly_a, poly_b);
    }

    /* ── Emit metadata header ─────────────────────────────────────────── */
    printf("# level=%d,polymul=%s,n_iter=%d,n_warmup=%d,platform=hw\n",
           SECURITY_LEVEL, POLYMUL_NAME, N_ITER, N_WARMUP);
    printf("op,iterations,"
           "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
           "min_instret,median_instret,max_instret,mean_instret,stddev_instret\n");

    /* ── Timed passes ────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        vect_mul(poly_out, poly_a, poly_b);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
    emit_csv_row("vect_mul", N_ITER);

    /* ── Anti-DCE ─────────────────────────────────────────────────────── */
    if (poly_out[0] == 0xdeadbeefdeadbeefULL)
        printf("# (dead code marker)\n");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    perf_hw_close(&ctx);
    free(samples_cyc);
    free(samples_ins);

    return 0;
}
