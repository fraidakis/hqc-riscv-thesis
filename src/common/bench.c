/* src/common/bench.c
 *
 * Cycle-counter and console I/O helper for bare-metal simulation.
 *
 * Two simulator backends, selected at compile time:
 *   HQC_SIM_QEMU=1  — QEMU virt: UART (ns16550a at 0x10000000) for output,
 *                      test-finisher at 0x100000 for halt/exit.
 *   (default)        — Spike:    HTIF tohost/fromhost for output and exit.
 *
 * Public API (declared in bench.h):
 *   hqc_rdcycle()          — read mcycle CSR
 *   hqc_htif_puts(s)       — print NUL-terminated string
 *   hqc_htif_putu64(v)     — print uint64_t as decimal
 *   hqc_htif_exit(code)    — halt the simulator with an exit code
 *
 * Constant-time note: rdcycle is a covert channel; its use here is strictly
 * confined to BENCH mode builds (HQC_BENCH_MODE=1).
 */

#include "bench.h"
#include <stdint.h>

/* ── Cycle counter (shared by both backends) ─────────────────────────────── */
/*
 * Read mcycle (CSR 0xb00) — always accessible in M-mode, no ISA extension
 * required.  Using the rdcycle pseudoinstruction (CSR 0xc00) would require
 * the Zicntr extension and traps on Spike when _zicntr is absent.
 */
uint64_t hqc_rdcycle(void)
{
    uint64_t cycles;
    __asm__ volatile ("csrr %0, 0xb00" : "=r"(cycles));   /* mcycle */
    return cycles;
}

/* ══════════════════════════════════════════════════════════════════════════ */
#if defined(HQC_SIM_QEMU)
/* ── QEMU virt backend ───────────────────────────────────────────────────── */
/*
 * QEMU virt UART: ns16550a mapped at 0x10000000.
 *   Offset 0x00 — THR (Transmit Holding Register): write byte to output
 *   Offset 0x05 — LSR (Line Status Register): bit 5 = THRE (TX ready)
 *
 * QEMU virt test-finisher: mapped at 0x100000.
 *   Write 0x5555 → PASS (exit 0)
 *   Write 0x3333 → FAIL (exit non-zero)
 */
#define UART_BASE    ((volatile uint8_t *)0x10000000UL)
#define UART_THR     (UART_BASE + 0)
#define UART_LSR     (UART_BASE + 5)
#define UART_THRE    (1u << 5)

#define QEMU_FINISHER ((volatile uint32_t *)0x100000UL)
#define QEMU_PASS    0x5555u
#define QEMU_FAIL    0x3333u

static void _putchar(char c)
{
    while (!(*UART_LSR & UART_THRE)) {}
    *UART_THR = (uint8_t)c;
}

void hqc_htif_exit(int code)
{
    *QEMU_FINISHER = (code == 0) ? QEMU_PASS : QEMU_FAIL;
    while (1) { __asm__ volatile ("wfi"); }
}

/* ══════════════════════════════════════════════════════════════════════════ */
#else
/* ── Spike / HTIF backend ────────────────────────────────────────────────── */
/*
 * Spike HTIF console device 1:
 *   tohost = (1ULL << 56) | (1ULL << 48) | char   → emit character
 *
 * Spike HTIF exit device 0:
 *   tohost = (code << 1) | 1                       → exit with code
 *
 * Each _putchar write must wait for Spike to clear tohost back to 0
 * before the next write.  Without this wait, consecutive tohost writes
 * overwrite each other before Spike ticks HTIF, and only the last
 * character of any burst survives.
 *
 * Spike clears tohost to 0 after processing (fesvr/htif.cc line ~304)
 * but does NOT set fromhost for BCD (character) commands — do not
 * spin-wait on fromhost.
 */
extern volatile uint64_t tohost   __attribute__((weak));
extern volatile uint64_t fromhost __attribute__((weak));

static void _putchar(char c)
{
    if (&tohost == (volatile uint64_t *)0) return;
    tohost = (1ULL << 56) | (1ULL << 48) | (uint8_t)c;
    while (tohost) {}   /* wait for Spike to clear tohost after processing */
}

void hqc_htif_exit(int code)
{
    if (&tohost == (volatile uint64_t *)0) return;
    tohost = ((uint64_t)(unsigned int)code << 1) | 1ULL;
    while (1) { __asm__ volatile ("wfi"); }
}

#endif /* HQC_SIM_QEMU */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Shared string/integer output (uses _putchar from selected backend) ──── */

void hqc_htif_puts(const char *s)
{
    while (*s) _putchar(*s++);
}

/* Print uint64_t as decimal string (no heap / no printf). */
void hqc_htif_putu64(uint64_t v)
{
    char buf[21];  /* max 20 decimal digits + NUL */
    int  pos = 20;
    buf[20] = '\0';
    if (v == 0) { _putchar('0'); return; }
    while (v > 0) {
        buf[--pos] = (char)('0' + (v % 10));
        v /= 10;
    }
    hqc_htif_puts(buf + pos);
}
