#include <stdint.h>

void radix_16_basemul_asm(uint32_t out[2], uint32_t a, uint32_t b)
{
    const uint32_t mask32 = 0x11111111u;
    uint64_t acc;
    uint32_t t[8] = {0};

    uint32_t a0 = (a) & mask32;
    uint32_t a1 = (a >> 2) & mask32;
    uint32_t a2 = (a >> 1) & mask32;
    uint32_t a3 = (a >> 3) & mask32;
    uint32_t b0 = (b) & mask32;
    uint32_t b1 = (b >> 2) & mask32;
    uint32_t b2 = (b >> 1) & mask32;
    uint32_t b3 = (b >> 3) & mask32;

    acc = (uint64_t)a0 * b0;
    t[0] = (uint32_t)acc & mask32;
    t[1] = (uint32_t)(acc >> 32);

    acc = (uint64_t)a0 * b2;
    t[2] = (uint32_t)acc;
    t[3] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[1]) | ((uint64_t)t[2] << 32);
    acc += (uint64_t)a0 * b1;
    t[1] = (uint32_t)acc & mask32;
    t[2] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[1]) | ((uint64_t)t[2] << 32);
    acc += (uint64_t)a1 * b0;
    t[1] = (uint32_t)acc & mask32;
    t[2] = (uint32_t)(acc >> 32) & mask32;

    out[0] = t[0] ^ (t[1] << 2);

    acc = (uint64_t)a1 * b3;
    t[4] = (uint32_t)acc;
    t[5] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[3]) | ((uint64_t)t[4] << 32);
    acc += (uint64_t)a0 * b3;
    t[3] = (uint32_t)acc;
    t[4] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[2]) | ((uint64_t)t[3] << 32);
    acc += (uint64_t)a1 * b1;
    t[2] = (uint32_t)acc & mask32;
    t[3] = (uint32_t)(acc >> 32) & mask32;

    acc = ((uint64_t)t[3]) | ((uint64_t)t[4] << 32);
    acc += (uint64_t)a1 * b2;
    t[3] = (uint32_t)acc;
    t[4] = (uint32_t)(acc >> 32) & mask32;

    acc = ((uint64_t)t[2]) | ((uint64_t)t[3] << 32);
    acc += (uint64_t)a2 * b0;
    t[2] = (uint32_t)acc & mask32;
    t[3] = (uint32_t)(acc >> 32) & mask32;

    out[0] ^= (t[2] << 1);

    acc = ((uint64_t)t[3]) | ((uint64_t)t[4] << 32);
    acc += (uint64_t)a2 * b1;
    t[3] = (uint32_t)acc & mask32;
    t[4] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[4]) | ((uint64_t)t[5] << 32);
    acc += (uint64_t)a2 * b2;
    t[4] = (uint32_t)acc & mask32;
    t[5] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[3]) | ((uint64_t)t[4] << 32);
    acc += (uint64_t)a3 * b0;
    t[3] = (uint32_t)acc & mask32;
    t[4] = (uint32_t)(acc >> 32);

    out[0] ^= (t[3] << 3);

    acc = (uint64_t)a3 * b3;
    t[6] = (uint32_t)acc;
    t[7] = (uint32_t)(acc >> 32) & mask32;

    acc = ((uint64_t)t[5]) | ((uint64_t)t[6] << 32);
    acc += (uint64_t)a2 * b3;
    t[5] = (uint32_t)acc & mask32;
    t[6] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[4]) | ((uint64_t)t[5] << 32);
    acc += (uint64_t)a3 * b1;
    t[4] = (uint32_t)acc & mask32;
    t[5] = (uint32_t)(acc >> 32);

    acc = ((uint64_t)t[5]) | ((uint64_t)t[6] << 32);
    acc += (uint64_t)a3 * b2;
    t[5] = (uint32_t)acc & mask32;
    t[6] = (uint32_t)(acc >> 32) & mask32;

    out[1] = t[4] ^ (t[5] << 2);
    out[1] ^= (t[6] << 1);
    out[1] ^= (t[7] << 3);
}
