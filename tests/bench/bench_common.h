/* tests/bench/bench_common.h
 *
 * Shared header for all HQC C benchmark files.
 *
 * Provides platform-conditional includes and defaults, shared helper
 * functions, and anti-dead-code-elimination utilities.  Compile with
 * -DHQC_PLATFORM_HW=1 for hardware (perf_event_open), or without for
 * bare-metal simulation (Spike ISS via bench.h).
 *
 * Requirements: 5.3, 5.5, 5.6
 */

#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════
 * Platform-conditional includes and defaults
 * ══════════════════════════════════════════════════════════════════════════ */

#ifdef HQC_PLATFORM_HW
  /* ── Hardware (BPI-F3 / Linux userspace) ─────────────────────────────── */
  #include <stdio.h>
  #include <stdlib.h>
  #include "perf_hw.h"

  #ifndef N_ITER_DEFAULT
    #define N_ITER_DEFAULT 10000
  #endif

  #ifndef N_WARMUP_DEFAULT
    #define N_WARMUP_DEFAULT 100
  #endif

  #define BENCH_PRINTF printf

  /*
   * BENCH_RDCYCLE() — On hardware we cannot use a single inline rdcycle
   * because timing uses perf_event_open (start/stop/read pattern).
   * This macro is provided for compatibility in code that only needs a
   * simple cycle snapshot (e.g. quick sanity checks).  For real HW
   * benchmarks, use perf_hw_start/stop/read directly.
   */
  static inline uint64_t _bench_hw_rdcycle(void)
  {
      uint64_t c;
      __asm__ volatile ("rdcycle %0" : "=r"(c));
      return c;
  }
  #define BENCH_RDCYCLE() _bench_hw_rdcycle()

#else
  /* ── Simulation (Spike ISS / bare-metal) ─────────────────────────────── */
  #include "bench.h"

  #ifndef N_ITER_DEFAULT
    #define N_ITER_DEFAULT 1
  #endif

  #ifndef N_WARMUP_DEFAULT
    #define N_WARMUP_DEFAULT 0
  #endif

  /*
   * BENCH_PRINTF — Simulation does not have printf; use the HTIF puts
   * interface.  This macro accepts a single string literal only.
   * For formatted output on sim, use hqc_htif_puts + hqc_htif_putu64.
   */
  #define BENCH_PRINTF hqc_htif_puts

  #define BENCH_RDCYCLE() hqc_rdcycle()

#endif /* HQC_PLATFORM_HW */

/* ══════════════════════════════════════════════════════════════════════════
 * Shared helper functions (inline for header-only usage)
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * bench_init_random_poly() — Fill a polynomial buffer with pseudo-random data.
 *
 * Uses a deterministic pattern (Fibonacci hash constant XOR'd with index)
 * to produce a dense polynomial.  Mimics a public-key element without
 * requiring a PRNG.
 *
 * @param poly    Pointer to uint64_t array to fill
 * @param n_words Number of 64-bit words to write
 */
static inline void bench_init_random_poly(uint64_t *poly, size_t n_words)
{
    for (size_t i = 0; i < n_words; i++) {
        poly[i] = 0xA5A5A5A5A5A5A5A5ULL ^ ((uint64_t)i * 0x9e3779b97f4a7c15ULL);
    }
}

/**
 * bench_init_sparse_poly() — Create a sparse polynomial with a given weight.
 *
 * Sets exactly `weight` bits, evenly distributed across [0, n_bits).
 * Mimics an HQC error/r-vector with the correct Hamming weight.
 *
 * @param poly   Pointer to uint64_t array (must be zeroed or will be zeroed)
 * @param n_bits Total number of bit positions in the polynomial
 * @param weight Number of bits to set (Hamming weight)
 */
static inline void bench_init_sparse_poly(uint64_t *poly, size_t n_bits, int weight)
{
    /* Zero out the buffer first */
    size_t n_words = (n_bits + 63) / 64;
    memset(poly, 0, n_words * sizeof(uint64_t));

    /* Place bits evenly across [0, n_bits) */
    for (int i = 0; i < weight; i++) {
        size_t bit_pos = (uint64_t)i * n_bits / (size_t)weight;
        poly[bit_pos / 64] |= (uint64_t)1 << (bit_pos % 64);
    }
}

/**
 * bench_compare_u64() — qsort comparator for uint64_t values.
 *
 * Returns negative, zero, or positive for less-than, equal, greater-than.
 */
static inline int bench_compare_u64(const void *a, const void *b)
{
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

/**
 * bench_median() — Compute the median from an array of uint64_t values.
 *
 * Sorts the array in-place using insertion sort (suitable for typical
 * benchmark iteration counts up to ~10000), then returns the middle value.
 *
 * @param arr  Array of uint64_t samples (will be sorted in-place)
 * @param n    Number of elements
 * @return     Median value
 */
static inline uint64_t bench_median(uint64_t *arr, size_t n)
{
    /* Insertion sort — fine for N <= 10000 in a benchmark harness */
    for (size_t i = 1; i < n; i++) {
        uint64_t key = arr[i];
        size_t j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }

    if (n == 0) return 0;
    if (n % 2 == 1)
        return arr[n / 2];
    return (arr[n / 2 - 1] + arr[n / 2]) / 2;
}

/**
 * bench_consume() — Prevent dead-code elimination of benchmark results.
 *
 * Acts as a compiler barrier: the compiler must assume the pointed-to
 * memory may be read by an external observer, so it cannot optimize
 * away the computation that produced the data.
 *
 * @param ptr  Pointer to data that must not be optimized away
 * @param len  Size of the data in bytes (unused, for future extension)
 */
static inline void bench_consume(volatile void *ptr, size_t len)
{
    (void)len;
    /*
     * Inline asm barrier: tells the compiler that memory pointed to by
     * ptr may be observed.  The "memory" clobber prevents reordering.
     */
    __asm__ volatile("" : : "r"(ptr) : "memory");
}

#endif /* BENCH_COMMON_H */
