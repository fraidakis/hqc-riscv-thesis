/* tests/bench/bench_poly_mul.c
 *
 * Polynomial-multiply micro-benchmark for HQC on bare-metal RISC-V.
 *
 * Benchmarks vect_mul() in isolation (one call, no keypair/encaps/decaps
 * overhead, no Keccak).  This is the dominant cost in HQC (>90% of
 * runtime), so it gives a fast iteration loop when tuning gf2x_mul:
 *
 *   QEMU KAT       — correctness, ~10-68 s total for all three levels
 *   Spike polymul  — instruction count for vect_mul alone, ~20-60 min
 *   Spike KEM      — full end-to-end baseline, used only at milestones, ~2-20 h
 *
 * Output CSV format:
 *   # level=<L>,polymul=<P>,n_iter=<N>
 *   op,iterations,min_cycles,median_cycles,max_cycles
 *   vect_mul,<N>,<min>,<median>,<max>
 *
 * Compile-time knobs (set by CMakeLists.txt):
 *   SECURITY_LEVEL  128 | 192 | 256
 *   POLYMUL_NAME    string literal e.g. "karatsuba_ref"
 *   N_ITER          (optional, default 1)
 *
 * Note: use on Spike only.  QEMU rdcycle is a software counter unrelated
 * to real instruction count — do not use for performance comparisons.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "gf2x.h"        /* vect_mul() */
#include "bench.h"        /* hqc_rdcycle, hqc_htif_puts, hqc_htif_putu64, hqc_htif_exit */
#include "parameters.h"  /* VEC_N_SIZE_64 */

#ifndef N_ITER
#  define N_ITER 1
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "karatsuba_ref"
#endif

/* ── Static buffers ──────────────────────────────────────────────────────── */
/* Input polynomials: fill with a non-trivial pattern so the compiler/optimiser
 * cannot constant-fold the multiply away.  Pattern 0xA5... is arbitrary. */
static uint64_t poly_a[VEC_N_SIZE_64];
static uint64_t poly_b[VEC_N_SIZE_64];
static uint64_t poly_out[VEC_N_SIZE_64];

/* ── Timing sample storage ───────────────────────────────────────────────── */
static uint64_t samples[N_ITER];

static void sort_samples(void)
{
    for (int i = 1; i < N_ITER; i++) {
        uint64_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && (int64_t)(samples[j] - key) > 0) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
}

static uint64_t min_sample(void)   { return samples[0]; }
static uint64_t max_sample(void)   { return samples[N_ITER - 1]; }
static uint64_t median_sample(void)
{
    if (N_ITER % 2 == 1)
        return samples[N_ITER / 2];
    return (samples[N_ITER / 2 - 1] + samples[N_ITER / 2]) / 2;
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Fill inputs with a non-trivial pattern.  Use a simple LFSR-like fill
     * so the data depends on VEC_N_SIZE_64 (prevents the compiler folding
     * the entire computation at link time). */
    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        poly_a[i] = (uint64_t)(0xA5A5A5A5A5A5A5A5ULL ^ (uint64_t)i * 0x9e3779b97f4a7c15ULL);
        poly_b[i] = (uint64_t)(0x5A5A5A5A5A5A5A5AULL ^ (uint64_t)i * 0x6c62272e07bb0142ULL);
    }

    /* Emit self-describing metadata header. */
    hqc_htif_puts("# level=");
    hqc_htif_putu64(SECURITY_LEVEL);
    hqc_htif_puts(",polymul=" POLYMUL_NAME ",n_iter=");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts("\n");

    /* Warm-up pass (discarded) — ensures any lazy initialisation inside
     * vect_mul (e.g. first-call setup) does not pollute the timed runs. */
    vect_mul(poly_out, poly_a, poly_b);

    /* Timed passes. */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0 = hqc_rdcycle();
        vect_mul(poly_out, poly_a, poly_b);
        uint64_t t1 = hqc_rdcycle();
        samples[i] = t1 - t0;
    }

    sort_samples();

    /* Emit CSV. */
    hqc_htif_puts("op,iterations,min_cycles,median_cycles,max_cycles\n");
    hqc_htif_puts("vect_mul,");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",");
    hqc_htif_putu64(min_sample());
    hqc_htif_puts(",");
    hqc_htif_putu64(median_sample());
    hqc_htif_puts(",");
    hqc_htif_putu64(max_sample());
    hqc_htif_puts("\n");

    /* Use poly_out to defeat dead-code elimination. */
    if (poly_out[0] == 0xdeadbeefdeadbeefULL)
        hqc_htif_puts("# (dead code marker)\n");

    hqc_htif_exit(0);
    return 0;
}
