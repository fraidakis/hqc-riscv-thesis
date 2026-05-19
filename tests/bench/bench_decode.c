/* tests/bench/bench_decode.c
 *
 * Concatenated Reed-Muller + Reed-Solomon decode micro-benchmark for HQC
 * on bare-metal RISC-V.
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
 * Output CSV format (identical to bench_mul.c):
 *   # level=<L>,decode=<DECODE_NAME>,n_iter=<N>,codesize_text=<bytes>
 *   op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *   decode,<N>,<min_c>,<med_c>,<max_c>,<min_i>,<med_i>,<max_i>
 *
 * Compile-time knobs (set by CMakeLists.txt):
 *   SECURITY_LEVEL  128 | 192 | 256
 *   DECODE_NAME     string literal e.g. "ref" or "rvv"
 *   N_ITER          (optional, default 1)
 *   HQC_SILENT      suppresses HTIF metadata headers
 */

#include <stdint.h>
#include <string.h>

#include "reed_muller.h"
#include "reed_solomon.h"
#include "bench.h"
#include "parameters.h"

#ifndef N_ITER
#  define N_ITER 1
#endif

#ifndef DECODE_NAME
#  define DECODE_NAME "ref"
#endif

/* Linker symbols bracketing .text */
extern char _text_start[];
extern char _text_end[];

/* ── Static buffers ──────────────────────────────────────────────────────── */
/* RM codeword: VEC_N1N2_SIZE_64 64-bit words covering PARAM_N1N2 bits.    */
static uint64_t rm_cdw[VEC_N1N2_SIZE_64];
/* RM decode output / RS decode input: VEC_N1_SIZE_64 words = PARAM_N1 bytes */
static uint64_t rm_msg[VEC_N1_SIZE_64];
/* RS decoded message: CEIL_DIVIDE(PARAM_K, 8) 64-bit words = PARAM_K bytes */
static uint64_t rs_msg[CEIL_DIVIDE(PARAM_K, 8)];

/* ── Timing sample storage ───────────────────────────────────────────────── */
static uint64_t samples_cyc[N_ITER];
static uint64_t samples_ins[N_ITER];

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

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Zero all buffers — deterministic input for the decoders. */
    memset(rm_cdw, 0, sizeof(rm_cdw));
    memset(rm_msg, 0, sizeof(rm_msg));
    memset(rs_msg, 0, sizeof(rs_msg));

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
    return 0;
}
