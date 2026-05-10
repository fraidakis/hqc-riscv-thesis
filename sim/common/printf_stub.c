/* sim/common/printf_stub.c
 * Minimal stdio shim for bare-metal builds linked with -nostdlib.
 * HQC vendor sources contain debug-style printf calls; provide a weak no-op
 * symbol so link succeeds even when libc printf is unavailable.
 */

__attribute__((weak))
int printf(const char *fmt, ...) {
    (void)fmt;
    return 0;
}
