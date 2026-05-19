/* sim/common/mem.c
 *
 * Bare-metal C string/memory routines for HQC RISC-V -nostdlib targets.
 *
 * Constant-time intent:
 * - No branches depend on loaded memory contents.
 * - Control flow depends only on public parameters (n, pointer overlap,
 *   and alignment checks used by memset zero fast-path).
 */

#include <stddef.h>
#include <stdint.h>
#ifdef __riscv_v
#include <riscv_vector.h>
#endif

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

#ifdef __riscv_v
    while (n > 0) {
        size_t vl = __riscv_vsetvl_e8m8(n);
        vuint8m8_t v = __riscv_vle8_v_u8m8(s, vl);
        __riscv_vse8_v_u8m8(d, v, vl);
        s += vl;
        d += vl;
        n -= vl;
    }
    return dst;
#else
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
#endif
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s || d >= s + n) {
#ifdef __riscv_v
        while (n > 0) {
            size_t vl = __riscv_vsetvl_e8m8(n);
            vuint8m8_t v = __riscv_vle8_v_u8m8(s, vl);
            __riscv_vse8_v_u8m8(d, v, vl);
            s += vl;
            d += vl;
            n -= vl;
        }
#else
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
#endif
    } else {
        d += n;
        s += n;
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
#ifdef __riscv_v
    /* Fast-path: 8-byte aligned zero-fill using LMUL=8 (VLEN=256 -> 256 B/iter). */
    if (c == 0 && ((uintptr_t)dst & 7u) == 0 && (n & 7u) == 0) {
        uint64_t *d = (uint64_t *)dst;
        size_t words = n >> 3;
        while (words > 0) {
            size_t vl = __riscv_vsetvl_e64m8(words);
            __riscv_vse64_v_u64m8(d, __riscv_vmv_v_x_u64m8(0, vl), vl);
            d += vl;
            words -= vl;
        }
        return dst;
    }
#endif
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
