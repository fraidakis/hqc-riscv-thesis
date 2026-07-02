/* src/common/bench.h
 *
 * Public API for the bare-metal benchmarking helpers (bench.c).
 */

#ifndef HQC_BENCH_H
#define HQC_BENCH_H

#include <stdint.h>

/* Read the current machine-cycle counter. */
uint64_t hqc_read_mcycle(void);

/* Read the machine instruction-retire counter. */
uint64_t hqc_read_minstret(void);

/* Halt the simulator with the given exit code (0 = success). */
void hqc_htif_exit(int code);

/* Print a NUL-terminated string to the HTIF console (no newline added). */
void hqc_htif_puts(const char *s);

/* Print a uint64_t as an unsigned decimal integer to the HTIF console. */
void hqc_htif_putu64(uint64_t v);

/* Print a uint64_t as 16-digit zero-padded hex (no 0x prefix). */
void hqc_htif_puthex(uint64_t v);

/* Trap reporter invoked from the assembly _trap_handler. Prints the trap
 * cause / faulting PC / faulting address over HTIF, then exits. */
void hqc_trap_report(uint64_t mcause, uint64_t mepc, uint64_t mtval, uint64_t ra);

#endif /* HQC_BENCH_H */
