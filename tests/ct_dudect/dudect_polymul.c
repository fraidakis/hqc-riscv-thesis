/* tests/ct_dudect/dudect_polymul.c
 *
 * Dudect constant-time verification harness for vect_mul (polynomial
 * multiplication) on RISC-V hardware (Banana Pi BPI-F3).
 *
 * Implements the dudect methodology (Reparaz et al., 2016):
 *   - Two input classes: fixed (class A) vs random (class B) operands
 *   - Timing via perf_event_open (hardware cycle counters)
 *   - Streaming Welch's t-test (first-order + second-order percentile-cropped)
 *   - Auto-termination: LEAKAGE if |t| > T_THRESHOLD; PASS if n >= N_MAX
 *   - CSV output for thesis convergence plots
 *
 * Measurement methodology (prepare-then-measure):
 *   Phase 1 — Prepare: Generate ALL inputs for the batch into pre-allocated
 *             buffers.  Both classes write to the same buffer structure so
 *             cache state is symmetric.
 *   Phase 2 — Measure: Tight loop over the batch. Each iteration reads from
 *             the pre-filled input buffer and times only the target function.
 *             No PRNG calls, no class-dependent branching inside the timed
 *             region.
 *
 * This two-phase design eliminates the cache-state asymmetry that occurs
 * when Class B inputs are generated immediately before measurement (warming
 * the operand cache lines) while Class A operands may be cold.
 *
 * Usage:
 *   ./dudect_polymul_hqc128_karatsuba_zbc [--outdir /path/to/results]
 *
 * Build with Makefile.hw:
 *   make -f Makefile.hw TARGET=polymul LEVEL=128 POLYMUL=karatsuba_zbc
 *
 * Requirements: 1.1, 1.2, 1.3, 1.4, 3.1, 3.2, 3.3, 3.4, 5.1, 5.2,
 *               6.1, 6.2, 6.3, 6.4, 7.1, 7.2, 7.3, 8.1, 8.2, 8.3, 8.4,
 *               9.1, 9.2, 9.3, 9.4, 10.1, 10.2, 10.3, 12.2, 12.3,
 *               15.1, 15.2, 15.3
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* HQC target function and parameters */
#include "gf2x.h"
#include "parameters.h"

/* Dudect infrastructure */
#include "perf_hw.h"
#include "dudect_ttest.h"
#include "dudect_percentile.h"
#include "dudect_prng.h"

/* ── Compile-time configuration ──────────────────────────────────────────── */

#ifndef N_MAX
#  define N_MAX 100000
#endif

#ifndef BATCH_SIZE
#  define BATCH_SIZE 5000
#endif

#ifndef T_THRESHOLD
#  define T_THRESHOLD 4.5
#endif

#ifndef PERCENTILE_CROP
#  define PERCENTILE_CROP 95
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

#ifndef SECURITY_LEVEL
#  define SECURITY_LEVEL 128
#endif

/* ── Exit codes ──────────────────────────────────────────────────────────── */

#define EXIT_PASS     0
#define EXIT_LEAKAGE  1
#define EXIT_ERROR    2

/* ── CSV Reporter ────────────────────────────────────────────────────────── */

typedef struct {
    FILE *fp;
    int batch_num;
} csv_reporter_t;

static int csv_reporter_open(csv_reporter_t *r, const char *path)
{
    r->fp = fopen(path, "w");
    if (!r->fp) {
        fprintf(stderr, "[dudect] ERROR: cannot open CSV file: %s (%s)\n",
                path, strerror(errno));
        return -1;
    }
    fprintf(r->fp, "batch,n_measurements,t_statistic_first_order,t_statistic_second_order\n");
    fflush(r->fp);
    r->batch_num = 0;
    return 0;
}

static void csv_reporter_write_batch(csv_reporter_t *r,
                                     int64_t n_total,
                                     double t_first, double t_second)
{
    r->batch_num++;
    fprintf(r->fp, "%d,%ld,%.6f,%.6f\n",
            r->batch_num, (long)n_total, t_first, t_second);
    fflush(r->fp);
}

static void csv_reporter_close(csv_reporter_t *r)
{
    if (r->fp) {
        fclose(r->fp);
        r->fp = NULL;
    }
}

/* ── Dudect state ────────────────────────────────────────────────────────── */

typedef struct {
    /* Statistical engines */
    ttest_ctx_t ttest_first_order;
    ttest_ctx_t ttest_second_order;

    /* PRNG */
    dudect_prng_t prng;

    /* Measurement infrastructure */
    perf_hw_ctx perf;

    /* Batch buffers (reused each batch) */
    uint64_t *batch_times;
    int      *batch_classes;

    /* Pre-allocated input buffers for prepare-then-measure pattern.
     * Each measurement i reads from input_a[i] and input_b[i].
     * Both classes are written to the same buffer structure so
     * there is no cache asymmetry. */
    uint64_t *input_a;   /* BATCH_SIZE * VEC_N_SIZE_64 uint64s */
    uint64_t *input_b;   /* BATCH_SIZE * VEC_N_SIZE_64 uint64s */

    /* Cumulative state */
    int64_t total_measurements;
    int     batch_count;

    /* Output */
    csv_reporter_t csv;

    /* Fixed operands (Class A) */
    uint64_t fixed_a[VEC_N_SIZE_64];
    uint64_t fixed_b[VEC_N_SIZE_64];
} dudect_state_t;

/* ── Directory creation (mkdir -p equivalent) ────────────────────────────── */

static int mkdirp(const char *path)
{
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* ── Startup checks ─────────────────────────────────────────────────────── */

/**
 * Check CPU frequency governor and warn if not 'performance'.
 * (Req 12.2)
 */
static void check_governor(void)
{
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (f) {
        char gov[64] = {0};
        if (fgets(gov, sizeof(gov), f)) {
            char *nl = strchr(gov, '\n');
            if (nl) *nl = '\0';
            if (strcmp(gov, "performance") != 0) {
                fprintf(stderr, "[dudect] WARNING: CPU governor is '%s', not 'performance'.\n"
                        "  Run: echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor\n",
                        gov);
            } else {
                fprintf(stderr, "[dudect] CPU governor: performance ✓\n");
            }
        }
        fclose(f);
    } else {
        fprintf(stderr, "[dudect] WARNING: cannot read CPU governor (file not found)\n");
    }
}

/**
 * Validate perf counters return non-zero cycle counts.
 * (Req 12.3)
 */
static int validate_perf_counters(void)
{
    perf_hw_ctx test_ctx;
    if (perf_hw_init(&test_ctx) != 0) {
        fprintf(stderr, "[dudect] ERROR: perf_event_open failed. "
                "Check /proc/sys/kernel/perf_event_paranoid\n");
        return -1;
    }

    perf_hw_start(&test_ctx);
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) x += i;
    perf_hw_stop(&test_ctx);

    perf_hw_sample s;
    perf_hw_read(&test_ctx, &s);
    perf_hw_close(&test_ctx);

    (void)x;

    if (s.cycles == 0) {
        fprintf(stderr, "[dudect] ERROR: perf counters return 0 cycles. "
                "Check /proc/sys/kernel/perf_event_paranoid\n");
        return -1;
    }

    fprintf(stderr, "[dudect] perf counters validated (test: %lu cycles)\n",
            (unsigned long)s.cycles);
    return 0;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    /* Default output directory */
    const char *outdir = "results/ct_verify/dudect";

    /* Parse --outdir argument (Req 15.3) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--outdir") == 0 && i + 1 < argc) {
            outdir = argv[i + 1];
            i++;
        }
    }

    /* ── Print target info ────────────────────────────────────────────── */
    fprintf(stderr, "[dudect] target=vect_mul variant=%s level=%d\n",
            POLYMUL_NAME, SECURITY_LEVEL);
    fprintf(stderr, "[dudect] N_MAX=%d BATCH_SIZE=%d T_THRESHOLD=%.1f PERCENTILE=%d\n",
            N_MAX, BATCH_SIZE, T_THRESHOLD, PERCENTILE_CROP);

    /* ── Startup checks (Req 12.2, 12.3) ─────────────────────────────── */
    check_governor();
    if (validate_perf_counters() != 0)
        return EXIT_ERROR;

    /* ── Initialize state ─────────────────────────────────────────────── */
    dudect_state_t state;
    memset(&state, 0, sizeof(state));

    ttest_init(&state.ttest_first_order);
    ttest_init(&state.ttest_second_order);

    /* ── Initialize PRNG (Req 16.1, 16.2, 16.3) ──────────────────────── */
    if (dudect_prng_init(&state.prng) != 0)
        return EXIT_ERROR;

    /* ── Generate fixed operands — Class A (Req 1.2) ─────────────────── */
    dudect_prng_fill(&state.prng, (uint8_t *)state.fixed_a,
                     sizeof(state.fixed_a));
    dudect_prng_fill(&state.prng, (uint8_t *)state.fixed_b,
                     sizeof(state.fixed_b));

    /* ── Initialize perf counters (Req 3.1, 3.4) ─────────────────────── */
    if (perf_hw_init(&state.perf) != 0) {
        fprintf(stderr, "[dudect] ERROR: cannot initialize perf counters\n");
        return EXIT_ERROR;
    }

    /* ── Allocate batch buffers (Req 8.1) ─────────────────────────────── */
    state.batch_times = (uint64_t *)malloc((size_t)BATCH_SIZE * sizeof(uint64_t));
    state.batch_classes = (int *)malloc((size_t)BATCH_SIZE * sizeof(int));

    /* Allocate input buffers for prepare-then-measure pattern */
    size_t vec_bytes = (size_t)VEC_N_SIZE_64 * sizeof(uint64_t);
    state.input_a = (uint64_t *)malloc((size_t)BATCH_SIZE * vec_bytes);
    state.input_b = (uint64_t *)malloc((size_t)BATCH_SIZE * vec_bytes);

    if (!state.batch_times || !state.batch_classes ||
        !state.input_a || !state.input_b) {
        fprintf(stderr, "[dudect] ERROR: failed to allocate batch buffers\n");
        return EXIT_ERROR;
    }

    /* ── Create output directory (Req 15.1) ───────────────────────────── */
    if (mkdirp(outdir) != 0) {
        fprintf(stderr, "[dudect] ERROR: cannot create output directory: %s (%s)\n",
                outdir, strerror(errno));
        return EXIT_ERROR;
    }

    /* ── Open CSV reporter (Req 9.1, 9.2) ─────────────────────────────── */
    char csv_path[512];
    snprintf(csv_path, sizeof(csv_path), "%s/dudect_polymul_hqc%d_%s.csv",
             outdir, SECURITY_LEVEL, POLYMUL_NAME);

    if (csv_reporter_open(&state.csv, csv_path) != 0)
        return EXIT_ERROR;

    fprintf(stderr, "[dudect] CSV output: %s\n", csv_path);

    /* ── Output buffer (reused across all measurements) ───────────────── */
    uint64_t poly_out[VEC_N_SIZE_64];

    /* ── Warm-up round (Req 8.4) ──────────────────────────────────────── *
     * Run one full batch to warm up caches, TLB, branch predictor, and
     * page tables. Timing measurements are discarded — not fed into the
     * t-test accumulators. This eliminates cold-start transients that
     * would otherwise bias early batches asymmetrically.              */
    {
        size_t vec_bytes_wu = (size_t)VEC_N_SIZE_64 * sizeof(uint64_t);
        for (int i = 0; i < BATCH_SIZE; i++) {
            int class_id = dudect_prng_bit(&state.prng);
            uint64_t *dst_a = state.input_a + (size_t)i * VEC_N_SIZE_64;
            uint64_t *dst_b = state.input_b + (size_t)i * VEC_N_SIZE_64;
            if (class_id == 0) {
                memcpy(dst_a, state.fixed_a, vec_bytes_wu);
                memcpy(dst_b, state.fixed_b, vec_bytes_wu);
            } else {
                dudect_prng_fill(&state.prng, (uint8_t *)dst_a, vec_bytes_wu);
                dudect_prng_fill(&state.prng, (uint8_t *)dst_b, vec_bytes_wu);
            }
        }
        for (int i = 0; i < BATCH_SIZE; i++) {
            uint64_t *op_a = state.input_a + (size_t)i * VEC_N_SIZE_64;
            uint64_t *op_b = state.input_b + (size_t)i * VEC_N_SIZE_64;
            perf_hw_sample sample;
            perf_hw_start(&state.perf);
            vect_mul(poly_out, op_a, op_b);
            perf_hw_stop(&state.perf);
            perf_hw_read(&state.perf, &sample);
            __asm__ volatile("" : : "r"(poly_out) : "memory");
        }
        fprintf(stderr, "[dudect] warm-up batch completed (discarded)\n");
    }

    /* ── Main measurement loop ────────────────────────────────────────── */
    int verdict = -1; /* -1 = continue, 0 = pass, 1 = leakage */

    while (verdict < 0) {
        /* ══════════════════════════════════════════════════════════════════
         * PHASE 1: PREPARE — Generate all inputs for this batch.
         *
         * Both classes write to the same input_a/input_b buffer array,
         * ensuring symmetric cache behavior.  Class A copies the fixed
         * operands; Class B fills with fresh random data.
         * ══════════════════════════════════════════════════════════════════ */
        for (int i = 0; i < BATCH_SIZE; i++) {
            int class_id = dudect_prng_bit(&state.prng);
            state.batch_classes[i] = class_id;

            uint64_t *dst_a = state.input_a + (size_t)i * VEC_N_SIZE_64;
            uint64_t *dst_b = state.input_b + (size_t)i * VEC_N_SIZE_64;

            if (class_id == 0) {
                /* Class A: copy fixed operands into the per-measurement slot */
                memcpy(dst_a, state.fixed_a, vec_bytes);
                memcpy(dst_b, state.fixed_b, vec_bytes);
            } else {
                /* Class B: fill with fresh random data */
                dudect_prng_fill(&state.prng, (uint8_t *)dst_a, vec_bytes);
                dudect_prng_fill(&state.prng, (uint8_t *)dst_b, vec_bytes);
            }
        }

        /* ══════════════════════════════════════════════════════════════════
         * PHASE 2: MEASURE — Tight timing loop.
         *
         * No PRNG calls, no class-dependent branching. Each iteration
         * reads from input_a[i], input_b[i] → vect_mul → poly_out.
         * The perf counter region covers only the target function.
         * ══════════════════════════════════════════════════════════════════ */
        for (int i = 0; i < BATCH_SIZE; i++) {
            uint64_t *op_a = state.input_a + (size_t)i * VEC_N_SIZE_64;
            uint64_t *op_b = state.input_b + (size_t)i * VEC_N_SIZE_64;

            perf_hw_sample sample;
            perf_hw_start(&state.perf);
            vect_mul(poly_out, op_a, op_b);
            perf_hw_stop(&state.perf);
            perf_hw_read(&state.perf, &sample);

            /* Anti-DCE barrier */
            __asm__ volatile("" : : "r"(poly_out) : "memory");

            state.batch_times[i] = sample.cycles;
        }

        state.total_measurements += BATCH_SIZE;
        state.batch_count++;

        /* ── Feed measurements to first-order t-test (Req 5.1) ────────── */
        for (int i = 0; i < BATCH_SIZE; i++) {
            ttest_push(&state.ttest_first_order,
                       (double)state.batch_times[i],
                       state.batch_classes[i]);
        }

        /* ── Second-order: percentile cropping (Req 6.1, 6.2, 6.3, 6.4) ── */
        uint64_t *sorted_times = (uint64_t *)malloc((size_t)BATCH_SIZE * sizeof(uint64_t));
        if (sorted_times) {
            memcpy(sorted_times, state.batch_times, (size_t)BATCH_SIZE * sizeof(uint64_t));
            uint64_t threshold = percentile_threshold(sorted_times, BATCH_SIZE, PERCENTILE_CROP);
            free(sorted_times);

            for (int i = 0; i < BATCH_SIZE; i++) {
                if (state.batch_times[i] <= threshold) {
                    ttest_push(&state.ttest_second_order,
                               (double)state.batch_times[i],
                               state.batch_classes[i]);
                }
            }
        }

        /* ── Compute t-statistics (Req 5.2, 8.2) ─────────────────────── */
        double t_first = ttest_compute(&state.ttest_first_order);
        double t_second = ttest_compute(&state.ttest_second_order);

        /* ── Write CSV row (Req 9.3, 9.4) ─────────────────────────────── */
        csv_reporter_write_batch(&state.csv, state.total_measurements,
                                 t_first, t_second);

        /* ── Progress output (Req 10.3) ───────────────────────────────── */
        fprintf(stderr, "[dudect] batch %3d: n=%7ld  t1=%7.3f  t2=%7.3f\n",
                state.batch_count, (long)state.total_measurements,
                t_first, t_second);

        /* ── Termination check (Req 7.1, 7.2, 7.3) ───────────────────── */
        if (fabs(t_first) > T_THRESHOLD || fabs(t_second) > T_THRESHOLD) {
            verdict = 1; /* LEAKAGE */
        } else if (state.total_measurements >= N_MAX) {
            verdict = 0; /* PASS */
        }
    }

    /* ── Summary output (Req 10.1, 10.2) ──────────────────────────────── */
    double t_first_final = ttest_compute(&state.ttest_first_order);
    double t_second_final = ttest_compute(&state.ttest_second_order);

    if (verdict == 1) {
        fprintf(stderr, "\n[dudect] RESULT: LEAKAGE DETECTED "
                "(target=vect_mul variant=%s level=%d n=%ld t1=%.3f t2=%.3f)\n",
                POLYMUL_NAME, SECURITY_LEVEL,
                (long)state.total_measurements, t_first_final, t_second_final);
    } else {
        fprintf(stderr, "\n[dudect] RESULT: no leakage detected "
                "(target=vect_mul variant=%s level=%d n=%ld t1=%.3f t2=%.3f)\n",
                POLYMUL_NAME, SECURITY_LEVEL,
                (long)state.total_measurements, t_first_final, t_second_final);
    }

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    csv_reporter_close(&state.csv);
    perf_hw_close(&state.perf);
    free(state.batch_times);
    free(state.batch_classes);
    free(state.input_a);
    free(state.input_b);

    return (verdict == 1) ? EXIT_LEAKAGE : EXIT_PASS;
}
