/* tests/bench/bench_keccak.c
 *
 * SHAKE-256 (Keccak) micro-benchmark for HQC on bare-metal RISC-V.
 *
 * Benchmarks one absorb + one squeezeblocks call in isolation to measure
 * the cost of the symmetric primitive that dominates decaps (Keccak is
 * ~30-40% of decaps instructions after polynomial-multiply is optimised).
 * Use this with bench_mul.c and bench_decode.c to build an Amdahl's Law
 * breakdown of the three bottlenecks.
 *
 * Operation benchmarked:
 *   shake256_absorb(&state, input, sizeof(input));
 *   shake256_squeezeblocks(output, 1, &state);
 *
 * Output CSV format (identical to bench_mul.c):
 *   # level=<L>,keccak=<KECCAK_NAME>,n_iter=<N>,codesize_text=<bytes>
 *   op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *   keccak,<N>,<min_c>,<med_c>,<max_c>,<min_i>,<med_i>,<max_i>
 *
 * Compile-time knobs (set by CMakeLists.txt):
 *   SECURITY_LEVEL  128 | 192 | 256
 *   KECCAK_NAME     string literal e.g. "ref" or "zvbb_x2_soa"
 *   N_ITER          (optional, default 1)
 *   HQC_SILENT      suppresses HTIF metadata headers
 */

#include <stdint.h>
#include <string.h>

#include "fips202.h"
#include "bench.h"
#include "parameters.h"

#ifndef N_ITER
#  define N_ITER 1
#endif

#ifndef KECCAK_NAME
#  define KECCAK_NAME "ref"
#endif

/* Linker symbols bracketing .text */
extern char _text_start[];
extern char _text_end[];

/* ── Static buffers ──────────────────────────────────────────────────────── */
/* Input: 32-byte seed (matches HQC's typical SHAKE-256 input width). */
static uint8_t keccak_input[SEED_BYTES];
/* Output: one full SHAKE-256 rate block (136 bytes). */
static uint8_t keccak_output[SHAKE256_RATE];

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
    /* Initialise input with a non-trivial pattern so the compiler cannot
     * constant-fold the Keccak permutation away. */
    for (size_t i = 0; i < SEED_BYTES; i++) {
        keccak_input[i] = (uint8_t)(0xA5U ^ (uint8_t)(i * 0x9eU));
    }

    /* ── Emit metadata header ────────────────────────────────────────────── */
#ifndef HQC_SILENT
    uint64_t text_size = (uint64_t)(_text_end - _text_start);
    hqc_htif_puts("# level=");
    hqc_htif_putu64(SECURITY_LEVEL);
    hqc_htif_puts(",keccak=" KECCAK_NAME ",n_iter=");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",codesize_text=");
    hqc_htif_putu64(text_size);
    hqc_htif_puts("\n");
#endif

    /* ── Warm-up call ────────────────────────────────────────────────────── */
#ifndef HQC_TRACE_MODE
    {
        shake256ctx warmup;
        shake256_absorb(&warmup, keccak_input, sizeof(keccak_input));
        shake256_squeezeblocks(keccak_output, 1, &warmup);
    }
#endif

    /* ── Timed passes ────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        shake256ctx state;
        uint64_t t0_cyc = hqc_rdcycle();
        uint64_t t0_ins = hqc_rdinstret();
        shake256_absorb(&state, keccak_input, sizeof(keccak_input));
        shake256_squeezeblocks(keccak_output, 1, &state);
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
    hqc_htif_puts("keccak,");
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

    /* Use keccak_output to defeat dead-code elimination. */
    if (keccak_output[0] == 0xde && keccak_output[1] == 0xad)
        hqc_htif_puts("# (dead code marker)\n");

    hqc_htif_exit(0);
    return 0;
}
