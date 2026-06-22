/* tests/bench/bench_mul.c
 *
 * Unified polynomial-multiply micro-benchmark for HQC on RISC-V.
 *
 * Benchmarks vect_mul() in isolation (one call, no keypair/encaps/decaps
 * overhead, no Keccak).  This is the dominant cost in HQC (>90% of
 * runtime), giving a fast iteration loop when tuning gf2x_mul.
 *
 * Platform selection:
 *   - Without HQC_PLATFORM_HW: bare-metal Spike ISS (sim)
 *     Uses hqc_read_mcycle/hqc_read_minstret, single iteration, deterministic.
 *   - With -DHQC_PLATFORM_HW=1: Linux userspace on real hardware (BPI-F3)
 *     Uses perf_event_open for cycle/instret counting, many iterations.
 *
 * Input polynomials:
 *   poly_a — dense pseudo-random (mimics HQC public key / h vector)
 *   poly_b — sparse with exactly PARAM_OMEGA_R bits set, evenly distributed
 *            (mimics HQC error/r-vector with the correct Hamming weight)
 *
 * Output CSV format (simulation):
 *   # level=<L>,polymul=<P>,n_iter=<N>,codesize_text=<bytes>
 *   op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *   vect_mul,<N>,<min_c>,<med_c>,<max_c>,<min_i>,<med_i>,<max_i>
 *
 * Output CSV format (hardware):
 *   # level=<L>,polymul=<P>,n_iter=<N>,n_warmup=<W>,platform=hw
 *   op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *   vect_mul,...
 *
 * Compile-time knobs (set by CMakeLists.txt / Makefile.hw):
 *   SECURITY_LEVEL   128 | 192 | 256
 *   POLYMUL_NAME     string literal e.g. "karatsuba_ref"
 *   N_ITER           (optional, default N_ITER_DEFAULT from bench_common.h)
 *   N_WARMUP         (optional, default N_WARMUP_DEFAULT from bench_common.h)
 *   HQC_SILENT       suppresses HTIF metadata headers in normal Spike bench runs
 *   HQC_TRACE_MODE   trace/hist-only mode that compiles out un-timed warm-up calls
 *   HQC_PLATFORM_HW  selects hardware path (perf_event_open)
 *
 * Requirements: 5.1, 5.2, 5.4
 */

#include "bench_common.h"

#include "gf2x.h"        /* vect_mul() */
#include "parameters.h"  /* VEC_N_SIZE_64, PARAM_N, PARAM_OMEGA_R */

#ifndef N_ITER
#  define N_ITER N_ITER_DEFAULT
#endif

#ifndef N_WARMUP
#  define N_WARMUP N_WARMUP_DEFAULT
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

/* ── Static buffers ──────────────────────────────────────────────────────── */
static uint64_t poly_a[VEC_N_SIZE_64];   /* dense, mimics HQC public-key element */
static uint64_t poly_b[VEC_N_SIZE_64];   /* sparse weight-PARAM_OMEGA_R, mimics r-vector */
static uint64_t poly_out[VEC_N_SIZE_64];

/* ══════════════════════════════════════════════════════════════════════════
 * Hardware-specific path (perf_event_open, many iterations, statistics)
 * ══════════════════════════════════════════════════════════════════════════ */
#ifdef HQC_PLATFORM_HW

/* ── Timing sample storage (heap-allocated for large N_ITER) ─────────────── */
static uint64_t *samples_cyc;
static uint64_t *samples_ins;

#ifdef PERF_HW_EXTENDED
/* Extended PMU sample arrays */
static uint64_t *samples_l1d_miss;
static uint64_t *samples_l1d_access;
static uint64_t *samples_dtlb_miss;
static uint64_t *samples_br_mispred;
static uint64_t *samples_stall_fe;
static uint64_t *samples_stall_be;
static uint64_t *samples_vec_inst;
static uint64_t *samples_vpu_stall;
#endif

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

    BENCH_PRINTF("%s,%d,"
                 "%lu,%lu,%lu,%.1f,%.1f,"
                 "%lu,%lu,%lu,%.1f,%.1f",
                 op, n,
                 (unsigned long)samples_cyc[0],
                 (unsigned long)perf_hw_median(samples_cyc, n),
                 (unsigned long)samples_cyc[n - 1],
                 mean_c, std_c,
                 (unsigned long)samples_ins[0],
                 (unsigned long)perf_hw_median(samples_ins, n),
                 (unsigned long)samples_ins[n - 1],
                 mean_i, std_i);

#ifdef PERF_HW_EXTENDED
    perf_hw_sort_u64(samples_l1d_miss, n);
    perf_hw_sort_u64(samples_l1d_access, n);
    perf_hw_sort_u64(samples_dtlb_miss, n);
    perf_hw_sort_u64(samples_br_mispred, n);
    perf_hw_sort_u64(samples_stall_fe, n);
    perf_hw_sort_u64(samples_stall_be, n);
    perf_hw_sort_u64(samples_vec_inst, n);
    perf_hw_sort_u64(samples_vpu_stall, n);

    uint64_t med_l1d_miss  = perf_hw_median(samples_l1d_miss, n);
    uint64_t med_l1d_acc   = perf_hw_median(samples_l1d_access, n);
    uint64_t med_dtlb      = perf_hw_median(samples_dtlb_miss, n);
    uint64_t med_br        = perf_hw_median(samples_br_mispred, n);
    uint64_t med_fe        = perf_hw_median(samples_stall_fe, n);
    uint64_t med_be        = perf_hw_median(samples_stall_be, n);
    uint64_t med_vec       = perf_hw_median(samples_vec_inst, n);
    uint64_t med_vpu       = perf_hw_median(samples_vpu_stall, n);

    double miss_rate = (med_l1d_acc > 0)
        ? 100.0 * (double)med_l1d_miss / (double)med_l1d_acc
        : 0.0;

    BENCH_PRINTF(",%lu,%lu,%.2f%%,%lu,%lu,%lu,%lu,%lu,%lu",
           (unsigned long)med_l1d_miss,
           (unsigned long)med_l1d_acc,
           miss_rate,
           (unsigned long)med_dtlb,
           (unsigned long)med_br,
           (unsigned long)med_fe,
           (unsigned long)med_be,
           (unsigned long)med_vec,
           (unsigned long)med_vpu);
#endif /* PERF_HW_EXTENDED */

    BENCH_PRINTF("\n");
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

int main(int argc, char **argv)
{
    /* Parse optional --raw-dir argument */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--raw-dir") == 0 && i + 1 < argc) {
            static char path_buf[512];
            snprintf(path_buf, sizeof(path_buf), "%s/%s_%d_mul_cycles.txt",
                     argv[i + 1], POLYMUL_NAME, SECURITY_LEVEL);
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

#ifdef PERF_HW_EXTENDED
    samples_l1d_miss  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_l1d_access= (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_dtlb_miss = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_br_mispred= (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_stall_fe  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_stall_be  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_vec_inst  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_vpu_stall = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_l1d_miss || !samples_l1d_access || !samples_dtlb_miss ||
        !samples_br_mispred || !samples_stall_fe || !samples_stall_be ||
        !samples_vec_inst || !samples_vpu_stall) {
        fprintf(stderr, "ERROR: failed to allocate extended sample arrays\n");
        return 1;
    }
#endif

    /* ── Initialize test polynomials ──────────────────────────────────── */
    bench_init_random_poly(poly_a, VEC_N_SIZE_64);
    bench_init_sparse_poly(poly_b, PARAM_N, PARAM_OMEGA_R);

    /* ── Initialize perf counters ─────────────────────────────────────── */
#ifdef PERF_HW_EXTENDED
    /* Two-pass approach: basic counters for clean cycle/instret measurement,
     * then extended counters for microarchitectural profiling.
     * This avoids PMU multiplexing overhead contaminating cycle counts. */
    perf_hw_ctx ctx_basic;
    if (perf_hw_init(&ctx_basic) != 0) {
        fprintf(stderr, "ERROR: cannot initialize basic perf counters\n");
        return 1;
    }
    perf_hw_ext_ctx ctx_ext;
    if (perf_hw_ext_init(&ctx_ext) != 0) {
        fprintf(stderr, "ERROR: cannot initialize extended perf counters\n");
        return 1;
    }
#else
    perf_hw_ctx ctx_basic;
    if (perf_hw_init(&ctx_basic) != 0) {
        fprintf(stderr, "ERROR: cannot initialize perf counters\n");
        return 1;
    }
#endif

    /* ── Warm-up passes (prime i-cache, branch predictor) ─────────────── */
    for (int i = 0; i < N_WARMUP; i++) {
        vect_mul(poly_out, poly_a, poly_b);
    }

    /* ── Emit metadata header ─────────────────────────────────────────── */
#ifdef PERF_HW_EXTENDED
    BENCH_PRINTF("# level=%d,polymul=%s,n_iter=%d,n_warmup=%d,platform=hw,extended=1\n",
                 SECURITY_LEVEL, POLYMUL_NAME, N_ITER, N_WARMUP);
    BENCH_PRINTF("op,iterations,"
                 "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
                 "min_instret,median_instret,max_instret,mean_instret,stddev_instret,"
                 "median_l1d_miss,median_l1d_access,l1d_miss_rate,"
                 "median_dtlb_miss,median_br_mispred,"
                 "median_stall_fe,median_stall_be,"
                 "median_vec_inst,median_vpu_stall\n");
#else
    BENCH_PRINTF("# level=%d,polymul=%s,n_iter=%d,n_warmup=%d,platform=hw\n",
                 SECURITY_LEVEL, POLYMUL_NAME, N_ITER, N_WARMUP);
    BENCH_PRINTF("op,iterations,"
                 "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
                 "min_instret,median_instret,max_instret,mean_instret,stddev_instret\n");
#endif

    /* ── Pass 1: Timed passes (cycles + instret only, no PMU noise) ───── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx_basic);
        vect_mul(poly_out, poly_a, poly_b);
        perf_hw_stop(&ctx_basic);
        perf_hw_read(&ctx_basic, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }

#ifdef PERF_HW_EXTENDED
    /* ── Pass 2: Extended counters (microarchitectural profiling) ──────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_ext_sample s;
        perf_hw_ext_start(&ctx_ext);
        vect_mul(poly_out, poly_a, poly_b);
        perf_hw_ext_stop(&ctx_ext);
        perf_hw_ext_read(&ctx_ext, &s);
        samples_l1d_miss[i]   = s.l1d_load_miss;
        samples_l1d_access[i] = s.l1d_access;
        samples_dtlb_miss[i]  = s.dtlb_load_miss;
        samples_br_mispred[i] = s.br_mispred;
        samples_stall_fe[i]   = s.stall_frontend;
        samples_stall_be[i]   = s.stall_backend;
        samples_vec_inst[i]   = s.vector_inst;
        samples_vpu_stall[i]  = s.vpu_stall;
    }
#endif

    /* ── Raw dump (unsorted, original order — must happen before sort) ── */
    dump_raw(N_ITER);

    emit_csv_row("vect_mul", N_ITER);

    /* ── Anti-DCE ─────────────────────────────────────────────────────── */
    bench_consume(poly_out, sizeof(poly_out));

    /* ── Cleanup ──────────────────────────────────────────────────────── */
#ifdef PERF_HW_EXTENDED
    perf_hw_ext_close(&ctx_ext);
    free(samples_l1d_miss);
    free(samples_l1d_access);
    free(samples_dtlb_miss);
    free(samples_br_mispred);
    free(samples_stall_fe);
    free(samples_stall_be);
    free(samples_vec_inst);
    free(samples_vpu_stall);
#endif
    perf_hw_close(&ctx_basic);
    free(samples_cyc);
    free(samples_ins);

    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Simulation-specific path (Spike ISS, deterministic, few iterations)
 * ══════════════════════════════════════════════════════════════════════════ */
#else /* !HQC_PLATFORM_HW */

/* Linker symbols bracketing .text.startup + .text */
extern char _text_start[];
extern char _text_end[];

/* ── Timing sample storage ───────────────────────────────────────────────── */
static uint64_t samples_cyc[N_ITER];
static uint64_t samples_ins[N_ITER];

int main(void)
{
    /* ── Initialize test polynomials ──────────────────────────────────── */
    bench_init_random_poly(poly_a, VEC_N_SIZE_64);
    bench_init_sparse_poly(poly_b, PARAM_N, PARAM_OMEGA_R);

    /* ── Emit metadata header ────────────────────────────────────────── */
#ifndef HQC_SILENT
    uint64_t text_size = (uint64_t)(_text_end - _text_start);
    hqc_htif_puts("# level=");
    hqc_htif_putu64(SECURITY_LEVEL);
    hqc_htif_puts(",polymul=" POLYMUL_NAME ",n_iter=");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",codesize_text=");
    hqc_htif_putu64(text_size);
    hqc_htif_puts("\n");
#endif

    /* ── Warm-up call (discards first-call latency, if any) ─────────────── */
#ifndef HQC_TRACE_MODE
    for (int i = 0; i < N_WARMUP; i++) {
        vect_mul(poly_out, poly_a, poly_b);
    }
#endif

    /* ── Timed passes ────────────────────────────────────────────────��───── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = BENCH_RDCYCLE();
        uint64_t t0_ins = hqc_read_minstret();
        vect_mul(poly_out, poly_a, poly_b);
        uint64_t t1_cyc = BENCH_RDCYCLE();
        uint64_t t1_ins = hqc_read_minstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }

    /* Sort for statistics */
    for (size_t i = 1; i < (size_t)N_ITER; i++) {
        uint64_t key_c = samples_cyc[i];
        uint64_t key_i = samples_ins[i];
        size_t j = i;
        while (j > 0 && samples_cyc[j - 1] > key_c) {
            samples_cyc[j] = samples_cyc[j - 1];
            j--;
        }
        samples_cyc[j] = key_c;
        /* Sort instret independently */
        j = i;
        while (j > 0 && samples_ins[j - 1] > key_i) {
            samples_ins[j] = samples_ins[j - 1];
            j--;
        }
        samples_ins[j] = key_i;
    }

    /* ── Emit CSV ────────────────────────────────────────────────────────── */
#ifndef HQC_SILENT
    hqc_htif_puts("op,iterations,min_cycles,median_cycles,max_cycles,"
                  "min_instret,median_instret,max_instret\n");
#endif
    hqc_htif_puts("vect_mul,");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(bench_median(samples_cyc, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[N_ITER - 1]);
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(bench_median(samples_ins, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[N_ITER - 1]);
    hqc_htif_puts("\n");

    /* ── Anti-DCE ─────────────────────────────────────────────────────── */
    bench_consume(poly_out, sizeof(poly_out));

    hqc_htif_exit(0);
    return 0;
}

#endif /* HQC_PLATFORM_HW */
