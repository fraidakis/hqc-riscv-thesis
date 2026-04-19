/* sim/common/startup.S
 * Bare-metal RISC-V startup for HQC targets.
 * Does NOT rely on proxy kernel (pk).
 *
 * Boot sequence:
 *   1. Set up a stack pointer
 *   2. Enable the V extension: set mstatus.VS = Dirty (3)
 *   3. Execute vsetvli to configure vtype (e64, m1, ta, ma)
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

    /* ── 2. Enable V extension: mstatus.VS = Dirty (0b11 << 9) ─────
     *  mstatus bit layout (RV64):
     *    [10:9] VS   – Vector context status
     *  We OR in (3 << 9) = 0x600 to set VS=Dirty so vsetvli doesn't trap.
     */
    li   t0, 0x600          /* VS = Dirty */
    csrs mstatus, t0

    /* ── 3. vsetvli: configure e64 lmul=m1 tail-agnostic mask-agnostic */
    vsetvli t0, zero, e64, m1, ta, ma

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
