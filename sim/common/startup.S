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

    /* 1b. Trap vector (patch): install a machine-mode handler in DIRECT
     * mode so ANY exception (e.g. a corrupted return address after a stack
     * overflow) forces a clean HTIF exit with a nonzero code instead of
     * trapping to mtvec=0 (unmapped) and spinning forever. Without this a
     * fault shows up as an indefinite Spike hang rather than a reported
     * error. */
    la   t0, _trap_handler
    csrw mtvec, t0          /* low bits 00 => DIRECT mode (handler is 4B aligned) */

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

    /* Trap handler (patch): on any trap, signal a nonzero failure code to
     * HTIF tohost so the simulator exits promptly. Exit code 0xFF is
     * distinct from the success path (which writes (0<<1)|1). This turns
     * silent corruption-induced hangs into an observable failure. */
    .align 2
    .global _trap_handler
    .type   _trap_handler, @function
_trap_handler:
    /* Encode the trap cause into the HTIF exit code so the sweep script's rc
     * reveals WHAT faulted without a debugger:
     *   exit_code = 0x40 | (mcause & 0x3F)   (0x40 tag keeps it nonzero)
     * Decode on the host as:  mcause = rc - 64
     * Common RISC-V exception codes:
     *   1  instruction access fault   2  illegal instruction
     *   4  load address misaligned    5  load access fault
     *   6  store/AMO misaligned       7  store/AMO access fault
     * So rc=69 => load access fault, rc=71 => store access fault,
     *    rc=66 => illegal instruction, rc=65 => instruction access fault.
     * The handler also calls hqc_trap_report, which prints mcause/mepc/mtval
     * over HTIF, so a plain 'spike <elf>' run reveals the faulting PC directly
     * (no --log-commits needed). */
    la   sp, _trap_stack_top   /* fresh stack: the original sp may itself be the corrupted pointer */
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval
    mv   a3, ra                /* ra is still the memcpy/leaf caller at trap entry -> identifies the culprit */
    call hqc_trap_report       /* prints mcause/mepc/mtval/ra over HTIF, then exits via tohost */
.Ltrap_spin:
    j    .Ltrap_spin
    .size _trap_handler, . - _trap_handler

    /* ── HTIF symbols (Spike uses these) ──────────────────────────── */
    /* Dedicated trap stack so the trap handler can safely call C even if the
     * main stack pointer is what got corrupted (store access fault). */
    .section .bss
    .align 4
    .global _trap_stack
_trap_stack:
    .skip 4096
_trap_stack_top:

    .section .tohost, "aw", @progbits
    .align 6
    .global tohost
tohost:   .dword 0
    .global fromhost
fromhost: .dword 0
