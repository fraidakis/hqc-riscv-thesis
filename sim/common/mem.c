/* sim/common/mem.c
 *
 * Minimal bare-metal implementations of the C string/memory functions required
 * by the HQC reference code when targeting RISC-V with -nostdlib.
 *
 * These are compiled with the full ISA flags so the auto-vectoriser can
 * substitute RVV load/store sequences at -O3 if profitable.
 *
 * memmove handles the overlapping case that memcpy skips.
 */

#include <stddef.h>
#ifdef __riscv_v
#include <riscv_vector.h>
#endif

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s || d >= s + n) {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
#ifdef __riscv_v
    /* Fast-path: 8-byte aligned zero-fill using LMUL=8 (VLEN=256 → 256 B/iter).
     * All security-zeroization and implicit {0}-init buffers satisfy this:
     * they are uint64_t arrays, naturally 8-byte aligned, sizes divisible by 8. */
    if (c == 0 && ((uintptr_t)dst & 7) == 0 && (n & 7) == 0) {
        uint64_t *d = (uint64_t *)dst;
        size_t words = n >> 3;
        while (words > 0) {
            size_t vl = __riscv_vsetvl_e64m8(words);
            __riscv_vse64_v_u64m8(d, __riscv_vmv_v_x_u64m8(0, vl), vl);
            d     += vl;
            words -= vl;
        }
        return dst;
    }
#endif
    /* Scalar fallback: unaligned, non-zero fill, or byte-granularity. */
    unsigned char *d = (unsigned char *)dst;
    for (size_t i = 0; i < n; i++)
        d[i] = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i])
            return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
