/* tests/bench/bench_poly_mul.c
 *
 * Polynomial-multiply micro-benchmark for HQC on bare-metal RISC-V.
 *
 * Benchmarks vect_mul() in isolation (one call, no keypair/encaps/decaps
 * overhead, no Keccak).  This is the dominant cost in HQC (>90% of
 * runtime), giving a fast iteration loop when tuning gf2x_mul:
 *
 *   QEMU KAT       — correctness, ~10-68 s total for all three levels
 *   Spike polymul  — cycle + instret counts for vect_mul alone, ~20-60 min
 *   Spike KEM      — full end-to-end baseline, used only at milestones, ~2-20 h
 *
 * Input polynomials:
 *   poly_a — dense pseudo-random (mimics HQC public key / h vector)
 *   poly_b — sparse with exactly PARAM_OMEGA_R bits set, evenly distributed
 *             (mimics HQC error/r-vector with the correct Hamming weight)
 *
 * Output CSV format:
 *   # level=<L>,polymul=<P>,n_iter=<N>,codesize_text=<bytes>
 *   op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *   vect_mul,<N>,<min_c>,<med_c>,<max_c>,<min_i>,<med_i>,<max_i>
 *
 * Compile-time knobs (set by CMakeLists.txt):
 *   SECURITY_LEVEL  128 | 192 | 256
 *   POLYMUL_NAME    string literal e.g. "karatsuba_ref"
 *   N_ITER          (optional, default 1)
 *
 * Note: on Spike (deterministic ISS) N_ITER > 1 confirms cycle stability but
 * does not add statistical value.  For real hardware, set N_ITER >= 10.
 * Use on Spike only; QEMU rdcycle is a software counter, not wall-clock.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "gf2x.h"        /* vect_mul() */
#include "bench.h"
#include "parameters.h"  /* VEC_N_SIZE_64, PARAM_N, PARAM_OMEGA_R */

#ifndef N_ITER
#  define N_ITER 1
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "karatsuba_ref"
#endif

/* Linker symbols bracketing .text.startup + .text */
extern char _text_start[];
extern char _text_end[];

/* ── Static buffers ──────────────────────────────────────────────────────── */
static uint64_t poly_a[VEC_N_SIZE_64];   /* dense, mimics HQC public-key element */
static uint64_t poly_b[VEC_N_SIZE_64];   /* sparse weight-PARAM_OMEGA_R, mimics r-vector */
static uint64_t poly_out[VEC_N_SIZE_64];

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
    /* ── poly_a: dense pseudo-random (mimics the HQC h public-key element) ── */
    /* XOR with a Fibonacci hash constant so the pattern depends on i and the
     * compiler cannot constant-fold the multiply away. */
    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        poly_a[i] = (uint64_t)(0xA5A5A5A5A5A5A5A5ULL ^ (uint64_t)i * 0x9e3779b97f4a7c15ULL);
    }

    /* ── poly_b: sparse with PARAM_OMEGA_R set bits (mimics HQC r/e vector) ── */
    /* Positions are evenly distributed across [0, PARAM_N): the i-th set bit
     * is placed at floor(i * PARAM_N / PARAM_OMEGA_R).  This gives the correct
     * Hamming weight and a representative bit-distribution without needing the
     * SHAKE PRNG in this micro-benchmark binary. */
    memset(poly_b, 0, sizeof(poly_b));
    for (size_t i = 0; i < (size_t)PARAM_OMEGA_R; i++) {
        size_t bit_pos = (uint64_t)i * PARAM_N / PARAM_OMEGA_R;
        poly_b[bit_pos / 64] |= (uint64_t)1 << (bit_pos % 64);
    }

    /* ── Emit metadata header ────────────────────────────────────────────── */
    uint64_t text_size = (uint64_t)(_text_end - _text_start);
    hqc_htif_puts("# level=");
    hqc_htif_putu64(SECURITY_LEVEL);
    hqc_htif_puts(",polymul=" POLYMUL_NAME ",n_iter=");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",codesize_text=");
    hqc_htif_putu64(text_size);
    hqc_htif_puts("\n");

    /* ── Warm-up call (discards first-call latency, if any) ─────────────── */
    vect_mul(poly_out, poly_a, poly_b);

    /* ── Timed passes ────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = hqc_rdcycle();
        uint64_t t0_ins = hqc_rdinstret();
        vect_mul(poly_out, poly_a, poly_b);
        uint64_t t1_cyc = hqc_rdcycle();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }

    sort_u64(samples_cyc, N_ITER);
    sort_u64(samples_ins, N_ITER);

    /* ── Emit CSV ────────────────────────────────────────────────────────── */
    hqc_htif_puts("op,iterations,min_cycles,median_cycles,max_cycles,"
                  "min_instret,median_instret,max_instret\n");
    hqc_htif_puts("vect_mul,");
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

    /* Use poly_out to defeat dead-code elimination. */
    if (poly_out[0] == 0xdeadbeefdeadbeefULL)
        hqc_htif_puts("# (dead code marker)\n");

    hqc_htif_exit(0);
    return 0;
}
