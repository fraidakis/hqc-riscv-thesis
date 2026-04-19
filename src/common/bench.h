/* src/common/bench.h
 *
 * Public API for the bare-metal benchmarking helpers (bench.c).
 */

#ifndef HQC_BENCH_H
#define HQC_BENCH_H

#include <stdint.h>

/* Read the current machine-cycle counter. */
uint64_t hqc_rdcycle(void);

/* Halt the simulator with the given exit code (0 = success). */
void hqc_htif_exit(int code);

/* Print a NUL-terminated string to the HTIF console (no newline added). */
void hqc_htif_puts(const char *s);

/* Print a uint64_t as an unsigned decimal integer to the HTIF console. */
void hqc_htif_putu64(uint64_t v);

#endif /* HQC_BENCH_H */
