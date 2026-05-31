/* tests/bench/bench_hw_decode.c
 *
 * Concatenated Reed-Muller + Reed-Solomon decode micro-benchmark for HQC
 * on real RISC-V hardware (Linux userspace).
 *
 * Mirrors bench_decode.c but uses perf_event_open instead of bare-metal rdcycle.
 * Benchmarks the full decode pipeline that runs inside decaps:
 *   reed_muller_decode(rm_msg, rm_cdw);    -- RM(1,7) list decoder
 *   reed_solomon_decode(rs_msg, rm_msg);   -- RS(n1,k) error corrector
 *
 * Use this alongside bench_hw_mul.c and bench_hw_keccak.c to assemble an
 * Amdahl's Law breakdown of the three main bottlenecks in HQC decaps.
 *
 * Output format (compatible with scripts/report/extract_results.py):
 *   # level=<L>,decode=ref,n_iter=<N>,n_warmup=<W>,platform=hw
 *   op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,
 *   min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *   decode,...
 *
 * Compile (native on board via Makefile.hw):
 *   make -f Makefile.hw LEVEL=128 MODE=publish decode
 *
 * Run:
 *   sudo nice -n -20 taskset -c 7 ./bench_hw_decode_hqc128
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reed_muller.h"
#include "reed_solomon.h"
#include "parameters.h"
#include "perf_hw.h"

#ifndef N_ITER
#  define N_ITER 10000
#endif

#ifndef N_WARMUP
#  define N_WARMUP 100
#endif

#ifndef DECODE_NAME
#  define DECODE_NAME "ref"
#endif

/* ── Static buffers ──────────────────────────────────────────────────────── */
/* RM codeword: VEC_N1N2_SIZE_64 64-bit words covering PARAM_N1N2 bits. */
static uint64_t rm_cdw[VEC_N1N2_SIZE_64];
/* RM decode output / RS decode input: VEC_N1_SIZE_64 words = PARAM_N1 bytes */
static uint64_t rm_msg[VEC_N1_SIZE_64];
/* RS decoded message: CEIL_DIVIDE(PARAM_K, 8) 64-bit words = PARAM_K bytes */
static uint64_t rs_msg[CEIL_DIVIDE(PARAM_K, 8)];

/* ── Timing sample storage (heap-allocated) ──────────────────────────────── */
static uint64_t *samples_cyc;
static uint64_t *samples_ins;

/* ── Raw dump file path (NULL = no dump) ─────────────────────────────────── */
static const char *raw_dump_path = NULL;

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

/* ── Raw per-iteration dump ──────────────────────────────────────────────── */
static void dump_raw(int n)
{
    if (!raw_dump_path)
        return;
    FILE *f = fopen(raw_dump_path, "w");
    if (!f) {
        fprintf(stderr, "WARNING: cannot open raw dump file: %s\n", raw_dump_path);
        return;
    }
    for (int i = 0; i < n; i++) {
        fprintf(f, "%lu %lu\n",
                (unsigned long)samples_cyc[i],
                (unsigned long)samples_ins[i]);
    }
    fclose(f);
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    /* Parse optional --raw-dir argument */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--raw-dir") == 0 && i + 1 < argc) {
            static char path_buf[512];
            snprintf(path_buf, sizeof(path_buf), "%s/decode_%d_cycles.txt",
                     argv[i + 1], SECURITY_LEVEL);
            raw_dump_path = path_buf;
            i++;
        }
    }

    /* ── System checks ────────────────────────────────────────────────── */
    perf_hw_check_system();

    /* ── Allocate sample arrays ───────────────────────────────────────── */
    samples_cyc = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_ins = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_cyc || !samples_ins) {
        fprintf(stderr, "ERROR: failed to allocate sample arrays for %d iterations\n", N_ITER);
        return 1;
    }

    /* ── Initialize test buffers ──────────────────────────────────────── */
    /* Zero all buffers — deterministic input for the decoders.
     * Performance is representative because both decoders are
     * data-independent in their loop structure. */
    memset(rm_cdw, 0, sizeof(rm_cdw));
    memset(rm_msg, 0, sizeof(rm_msg));
    memset(rs_msg, 0, sizeof(rs_msg));

    /* ── Initialize perf counters ─────────────────────────────────────── */
    perf_hw_ctx ctx;
    if (perf_hw_init(&ctx) != 0) {
        fprintf(stderr, "ERROR: cannot initialize perf counters\n");
        return 1;
    }

    /* ── Warm-up passes (prime i-cache, branch predictor) ─────────────── */
    for (int i = 0; i < N_WARMUP; i++) {
        reed_muller_decode(rm_msg, rm_cdw);
        reed_solomon_decode(rs_msg, rm_msg);
    }

    /* ── Emit metadata header ─────────────────────────────────────────── */
    printf("# level=%d,decode=%s,n_iter=%d,n_warmup=%d,platform=hw\n",
           SECURITY_LEVEL, DECODE_NAME, N_ITER, N_WARMUP);
    printf("op,iterations,"
           "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
           "min_instret,median_instret,max_instret,mean_instret,stddev_instret\n");

    /* ── Timed passes ────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        reed_muller_decode(rm_msg, rm_cdw);
        reed_solomon_decode(rs_msg, rm_msg);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }

    /* ── Raw dump (unsorted, original order — must happen before sort) ── */
    dump_raw(N_ITER);

    emit_csv_row("decode", N_ITER);

    /* ── Anti-DCE ─────────────────────────────────────────────────────── */
    if (rs_msg[0] == 0xdeadbeefdeadbeefULL)
        printf("# (dead code marker)\n");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    perf_hw_close(&ctx);
    free(samples_cyc);
    free(samples_ins);

    return 0;
}
