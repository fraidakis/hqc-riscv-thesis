/* sim/common/startup.S
 * Bare-metal RISC-V startup for HQC targets.
 * Does NOT rely on proxy kernel (pk).
 *
 * Boot sequence:
 *   1. Set up a stack pointer
 *   2. If V is enabled at compile time: set mstatus.VS = Dirty (3)
 *   3. If V is enabled at compile time: execute vsetvli to configure vtype
 *   4. Zero the BSS segment
 *   5. Call main()
 *   6. Loop forever (or invoke a simulated exit via tohost)
 */

    .section .text.startup, "ax", @progbits
    .global _start
    .type   _start, @function

_start:
    /* ── 1. Stack pointer ──────────────────────────────────────────── */
    la   sp, _stack_top

    /* ── 2/3. Vector init (only when building with V enabled) ──────
     * If __riscv_vector is not defined, scalar targets skip this.
     */
#ifdef __riscv_vector
    li   t0, 0x600          /* mstatus.VS = Dirty */
    csrs mstatus, t0
    vsetvli t0, zero, e64, m1, ta, ma
#endif

    /* ── 4. Zero BSS ──────────────────────────────────────────────── */
    la   a0, _bss_start
    la   a1, _bss_end
    bgeu a0, a1, .Lbss_done
.Lbss_loop:
    sd   zero, 0(a0)
    addi a0, a0, 8
    bltu a0, a1, .Lbss_loop
.Lbss_done:

    /* ── 5. Call main ─────────────────────────────────────────────── */
    li   a0, 0              /* argc = 0 */
    li   a1, 0              /* argv = NULL */
    call main

    /* ── 6. Exit via tohost (HTIF) ───────────────────────────────── */
    /* Write (return_code << 1) | 1 to tohost to signal Spike/QEMU exit */
    slli a0, a0, 1
    ori  a0, a0, 1
    la   t0, tohost
    sd   a0, 0(t0)
.Lhalt:
    j    .Lhalt

    .size _start, . - _start

    /* ── HTIF symbols (Spike uses these) ──────────────────────────── */
    .section .tohost, "aw", @progbits
    .align 6
    .global tohost
tohost:   .dword 0
    .global fromhost
fromhost: .dword 0
