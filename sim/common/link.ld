/* sim/common/link.ld
 * Bare-metal linker script for HQC-RISC-V targets.
 * Memory layout targets Spike (default) and QEMU virt machine.
 *
 * Memory map:
 *   0x8000_0000  DRAM base (Spike / QEMU virt)
 *   Stack grows down from top of DRAM region (128 MiB allocated)
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    . = ORIGIN(RAM);

    /* ── Startup code first ─────────────────────────────────────── */
    .text.startup : {
        KEEP(*(.text.startup))
    } > RAM

    /* ── Normal code ─────────────────────────────────────────────── */
    .text : {
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
    } > RAM

    /* ── Read-only data ──────────────────────────────────────────── */
    .rodata : {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
    } > RAM

    /* ── HTIF tohost/fromhost must be 64-byte aligned ────────────── */
    .tohost : ALIGN(64) {
        KEEP(*(.tohost))
    } > RAM

    /* ── Initialised data ────────────────────────────────────────── */
    .data : ALIGN(8) {
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
    } > RAM

    /* ── BSS ──────────────────────────────────────────────────────── */
    .bss (NOLOAD) : ALIGN(8) {
        _bss_start = .;
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM

    /* ── Stack (grows downward) ─────────────────────────────────── */
    .stack (NOLOAD) : ALIGN(16) {
        _stack_bottom = .;
        . += 64K;
        _stack_top = .;
    } > RAM

    /* ── Discard exception/unwinding tables ─────────────────────── */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}
