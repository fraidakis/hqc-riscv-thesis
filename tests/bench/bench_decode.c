/* tests/bench/bench_decode.c
 *
 * Concatenated Reed-Muller + Reed-Solomon decode micro-benchmark for HQC.
 *
 * Unified benchmark: compiles for both simulation (Spike ISS, bare-metal)
 * and hardware (BPI-F3, Linux userspace) via bench_common.h platform macros.
 *
 * Benchmarks the full decode pipeline that runs inside decaps:
 *   reed_muller_decode(rm_msg, rm_cdw);    -- RM(1,7) list decoder
 *   reed_solomon_decode(rs_msg, rm_msg);   -- RS(n1,k) error corrector
 *
 * Use this alongside bench_mul.c and bench_keccak.c to assemble an
 * Amdahl's Law breakdown of the three main bottlenecks in HQC decaps.
 *
 * Input buffers:
 *   rm_cdw — zeroed RM codeword of VEC_N1N2_SIZE_64 64-bit words
 *             (mimics the noisy received word passed to the RM decoder)
 *   rm_msg — output of RM decode / input to RS decode, VEC_N1_SIZE_64 words
 *   rs_msg — RS decoded message, CEIL_DIVIDE(PARAM_K, 8) 64-bit words
 *
 * Note: zeroed inputs produce deterministic (non-failing) execution paths
 * in the reference decoder.  Performance is representative because both
 * decoders are data-independent in their loop structure.
 *
 * Output CSV format:
 *   Simulation:
 *     # level=<L>,decode=<DECODE_NAME>,n_iter=<N>,codesize_text=<bytes>
 *     op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *     decode,<N>,<min_c>,<med_c>,<max_c>,<min_i>,<med_i>,<max_i>
 *
 *   Hardware:
 *     # level=<L>,decode=<DECODE_NAME>,n_iter=<N>,n_warmup=<W>,platform=hw
 *     op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,
 *       min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *     decode,...
 *
 * Compile-time knobs (set by CMakeLists.txt or Makefile.hw):
 *   SECURITY_LEVEL   128 | 192 | 256
 *   DECODE_NAME      string literal e.g. "ref" or "rvv"
 *   N_ITER           (optional, default from bench_common.h)
 *   N_WARMUP         (optional, default from bench_common.h, hw only)
 *   HQC_PLATFORM_HW  defined for hardware builds
 *   HQC_SILENT       suppresses HTIF metadata headers (sim only)
 *   HQC_TRACE_MODE   trace/hist-only mode (sim only)
 *
 * Requirements: 5.1, 5.2, 5.4
 */

#include <stdint.h>
#include <string.h>

#include "reed_muller.h"
#include "reed_solomon.h"
#include "bench_common.h"
#include "parameters.h"

#ifndef N_ITER
#  define N_ITER N_ITER_DEFAULT
#endif

#ifndef N_WARMUP
#  define N_WARMUP N_WARMUP_DEFAULT
#endif

#ifndef DECODE_NAME
#  define DECODE_NAME "ref"
#endif

/* ── Static buffers ──────────────────────────────────────────────────────── */
/* RM codeword: VEC_N1N2_SIZE_64 64-bit words covering PARAM_N1N2 bits.    */
static uint64_t rm_cdw[VEC_N1N2_SIZE_64];
/* RM decode output / RS decode input: VEC_N1_SIZE_64 words = PARAM_N1 bytes */
static uint64_t rm_msg[VEC_N1_SIZE_64];
/* RS decoded message: CEIL_DIVIDE(PARAM_K, 8) 64-bit words = PARAM_K bytes */
static uint64_t rs_msg[CEIL_DIVIDE(PARAM_K, 8)];

/* ── Timing sample storage ───────────────────────────────────────────────── */
#ifdef HQC_PLATFORM_HW
static uint64_t *samples_cyc;
static uint64_t *samples_ins;
static const char *raw_dump_path = NULL;
#else
static uint64_t samples_cyc[N_ITER];
static uint64_t samples_ins[N_ITER];
/* Linker symbols bracketing .text */
extern char _text_start[];
extern char _text_end[];
#endif

/* ── Sorting / statistics helpers ────────────────────────────────────────── */

#ifdef HQC_PLATFORM_HW

/* ── CSV emit helper (HW — extended statistics) ──────────────────────────── */
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

#else /* Simulation */

static void sort_u64(uint64_t *arr, int n)
{
    for (int i = 1; i < n; i++) {
        uint64_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && (int64_t)(arr[j] - key) > 0) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static uint64_t median_u64(const uint64_t *sorted, int n)
{
    if (n % 2 == 1)
        return sorted[n / 2];
    return (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
}

#endif /* HQC_PLATFORM_HW */

/* ── main ────────────────────────────────────────────────────────────────── */
#ifdef HQC_PLATFORM_HW
int main(int argc, char **argv)
#else
int main(void)
#endif
{
#ifdef HQC_PLATFORM_HW
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

    /* System checks */
    perf_hw_check_system();

    /* Allocate sample arrays */
    samples_cyc = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_ins = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_cyc || !samples_ins) {
        fprintf(stderr, "ERROR: failed to allocate sample arrays for %d iterations\n", N_ITER);
        return 1;
    }
#endif

    /* Zero all buffers — deterministic input for the decoders. */
    memset(rm_cdw, 0, sizeof(rm_cdw));
    memset(rm_msg, 0, sizeof(rm_msg));
    memset(rs_msg, 0, sizeof(rs_msg));

#ifdef HQC_PLATFORM_HW
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

#else /* Simulation */

    /* ── Emit metadata header ────────────────────────────────────────────── */
#ifndef HQC_SILENT
    uint64_t text_size = (uint64_t)(_text_end - _text_start);
    hqc_htif_puts("# level=");
    hqc_htif_putu64(SECURITY_LEVEL);
    hqc_htif_puts(",decode=" DECODE_NAME ",n_iter=");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",codesize_text=");
    hqc_htif_putu64(text_size);
    hqc_htif_puts("\n");
#endif

    /* ── Warm-up call ────────────────────────────────────────────────────── */
#ifndef HQC_TRACE_MODE
    reed_muller_decode(rm_msg, rm_cdw);
    reed_solomon_decode(rs_msg, rm_msg);
#endif

    /* ── Timed passes ────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = hqc_rdcycle();
        uint64_t t0_ins = hqc_rdinstret();
        reed_muller_decode(rm_msg, rm_cdw);
        reed_solomon_decode(rs_msg, rm_msg);
        uint64_t t1_cyc = hqc_rdcycle();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }

    sort_u64(samples_cyc, N_ITER);
    sort_u64(samples_ins, N_ITER);

    /* ── Emit CSV ────────────────────────────────────────────────────────── */
#ifndef HQC_SILENT
    hqc_htif_puts("op,iterations,min_cycles,median_cycles,max_cycles,"
                  "min_instret,median_instret,max_instret\n");
#endif
    hqc_htif_puts("decode,");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(median_u64(samples_cyc, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[N_ITER - 1]);
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(median_u64(samples_ins, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[N_ITER - 1]);
    hqc_htif_puts("\n");

    /* Use rs_msg to defeat dead-code elimination. */
    if (rs_msg[0] == 0xdeadbeefdeadbeefULL)
        hqc_htif_puts("# (dead code marker)\n");

    hqc_htif_exit(0);
#endif /* HQC_PLATFORM_HW */

    return 0;
}
