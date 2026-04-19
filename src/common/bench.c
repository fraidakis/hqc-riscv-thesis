/* src/common/bench.c
 *
 * Minimal cycle-counter and HTIF helper for bare-metal benchmarking on Spike.
 *
 * Two entry points:
 *   hqc_rdcycle()  — read the mcycle CSR directly (no syscall overhead)
 *   hqc_htif_exit(int code) — signal Spike to halt and return an exit code
 *
 * Used by tests/bench/bench_kem.c to wrap KeyGen / Encaps / Decaps.
 *
 * Constant-time note: rdcycle is a covert channel; its use here is strictly
 * confined to BENCH mode builds (HQC_BENCH_MODE=1).  KAT builds never call
 * these functions.
 */

#include "bench.h"
#include <stdint.h>

/* Read the machine-cycle counter.
 * On Spike, mcycle increments once per simulated cycle. */
uint64_t hqc_rdcycle(void)
{
    uint64_t cycles;
    __asm__ volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}

/* Request a clean halt via the RISC-V Front-End Server (HTIF).
 *
 * Spike polls the tohost symbol.  The convention used by libfemto / riscv-pk:
 *   write (code << 1 | 1) to tohost → Spike exits with code.
 *
 * The symbol is weakly defined in sim/common/startup.S; tests that provide
 * their own startup may override it.
 */
extern volatile uint64_t tohost   __attribute__((weak));
extern volatile uint64_t fromhost __attribute__((weak));

void hqc_htif_exit(int code)
{
    /* If tohost is not available (e.g. native host build), just return. */
    if (&tohost == (volatile uint64_t *)0) return;

    tohost = ((uint64_t)(unsigned int)code << 1) | 1ULL;
    /* Spin until Spike acknowledges. */
    while (1) {
        __asm__ volatile ("wfi");
    }
}

/* Formatted integer output over HTIF device 1 (console).
 *
 * Spike's HTIF console device: write a character c by setting
 *   tohost = (1ULL << 56) | (1ULL << 48) | c
 * and waiting for fromhost == 0.
 *
 * This avoids linking a full printf implementation.
 */
static void htif_putchar(char c)
{
    if (&tohost == (volatile uint64_t *)0) return;
    tohost = (1ULL << 56) | (1ULL << 48) | (uint8_t)c;
    while (fromhost == 0) { /* wait for echo */ }
    fromhost = 0;
}

void hqc_htif_puts(const char *s)
{
    while (*s) htif_putchar(*s++);
}

/* Print uint64_t as decimal string (no heap / no printf). */
void hqc_htif_putu64(uint64_t v)
{
    char buf[21];  /* max 20 decimal digits + NUL */
    int  pos = 20;
    buf[20] = '\0';
    if (v == 0) { htif_putchar('0'); return; }
    while (v > 0) {
        buf[--pos] = (char)('0' + (v % 10));
        v /= 10;
    }
    hqc_htif_puts(buf + pos);
}
