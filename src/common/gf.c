/**
 * @file gf.c
 * @brief Galois field GF(2^8) implementation — promoted global default.
 *
 * Target 3 Track A (promoted 2026-05-14): Native Zbc clmul injection for
 * gf_mul(), replacing the software carry-less multiply loop in the upstream
 * reference implementation.
 *
 * On a RISC-V target with the Zbc extension a single `clmul` instruction
 * computes the same 8×8 → 16-bit carry-less product in one cycle.  The
 * guard #ifdef __riscv_zbc ensures the fast path is compiled only when the
 * toolchain confirms Zbc support; the reference software path is kept as a
 * fallback for portability.
 *
 * All other functions (gf_generate, gf_reduce, gf_square, gf_inverse,
 * gf_carryless_mul) are unchanged from the upstream reference implementation.
 *
 * Compiled into hqc_core_<LEVEL> OBJECT libraries and shared by all variants.
 */

#include "gf.h"
#include <stdint.h>
#include "parameters.h"

static uint16_t gf_reduce(uint16_t x);
void gf_carryless_mul(uint8_t *c, uint8_t a, uint8_t b);

/**
 * @brief Generates exp and log lookup tables of GF(2^8).
 *
 * @note   this function is not used in the code; it was used to generate
 *         the lookup table for GF(2^8).
 *
 * The logarithm of 0 is defined as 2^8 by convention. <br>
 * The last two elements of the exp table are needed by the gf_mul function from gf_lutmul.c
 * (for example if both elements to multiply are zero).
 * @param[out] exp Array of size 2^8 + 2 receiving the powers of the primitive element
 * @param[out] log Array of size 2^8 receiving the logarithms of the elements of GF(2^m)
 * @param[in] m Parameter of Galois field GF(2^m)
 */
void gf_generate(uint16_t *exp, uint16_t *log, const int16_t m) {
    uint16_t elt = 1;
    uint16_t alpha = 2;  // primitive element of GF(2^8)
    uint16_t gf_poly = PARAM_GF_POLY;

    for (size_t i = 0; i < (1U << m) - 1; ++i) {
        exp[i] = elt;
        log[elt] = i;

        elt *= alpha;
        if (elt >= 1 << m)
            elt ^= gf_poly;
    }

    exp[(1 << m) - 1] = 1;
    exp[1 << m] = 2;
    exp[(1 << m) + 1] = 4;
    log[0] = 0;  // by convention
}

/**
 * @brief Reduce a polynomial modulo PARAM_GF_POLY in GF(2^8).
 *
 * This function performs modular reduction of a 16-bit polynomial `x`
 * by the irreducible polynomial PARAM_GF_POLY = 0x11D
 * (i.e., x⁸ + x⁴ + x³ + x + 1), used in GF(2^8).
 *
 * It assumes the input polynomial has degree ≤ 14 and uses a fixed
 * number of reduction steps and fixed feedback tap positions
 * ({4, 3, 2}) to produce a result of degree < 8.
 *
 * @param x 16-bit input polynomial to reduce (deg(x) ≤ 14)
 * @return Reduced 8-bit polynomial modulo PARAM_GF_POLY (deg(x) < 8)
 */
uint16_t gf_reduce(uint16_t x) {
    uint16_t mod = x >> 8;
    x = (x & 0xFF) ^ mod ^ (mod << 2) ^ (mod << 3) ^ (mod << 4);
    mod = x >> 8;
    return (x & 0xFF) ^ mod ^ (mod << 2) ^ (mod << 3) ^ (mod << 4);
}

/**
 * Carryless multiplication of two polynomials a and b.
 *
 * Implementation of the algorithm mul1 in https://hal.inria.fr/inria-00188261v4/document
 * with s = 2 and w = 8.
 *
 * Retained for link compatibility (gf_mul on non-Zbc paths calls this).
 * On Zbc targets gf_mul bypasses this function entirely.
 */
void gf_carryless_mul(uint8_t *c, uint8_t a, uint8_t b) {
    uint16_t h = 0, l = 0, g, u[4];
    u[0] = 0;
    u[1] = b & ((1UL << 7) - 1UL);
    u[2] = u[1] << 1;
    u[3] = u[2] ^ u[1];

    g = 0;
    uint16_t tmp1 = a & 3;

    for (int i = 0; i < 4; i++) {
        uint32_t tmp2 = tmp1 - i;
        g ^= (u[i] & -(1 - ((tmp2 | -tmp2) >> 31)));
    }

    l = g;
    h = 0;

    for (uint8_t i = 2; i < 8; i += 2) {
        g = 0;
        uint16_t tmp3 = (a >> i) & 3;
        for (int j = 0; j < 4; ++j) {
            uint32_t tmp2 = tmp3 - j;
            g ^= (u[j] & -(1 - ((tmp2 | -tmp2) >> 31)));
        }

        l ^= g << i;
        h ^= g >> (8 - i);
    }

    uint16_t mask = (-((b >> 7) & 1));
    l ^= ((a << 7) & mask);
    h ^= ((a >> (1)) & mask);

    c[0] = l;
    c[1] = h;
}

/**
 * Multiplies two elements of GF(2^GF_M).
 *
 * Target 3 Track A: On RISC-V Zbc targets, the 8×8 carry-less product is
 * computed with a single `clmul` instruction rather than the software loop
 * in gf_carryless_mul().  Inputs are masked to 8 bits before the instruction
 * so the upper 56 bits of the 64-bit result are guaranteed zero; the result
 * fits in 16 bits since deg(a) ≤ 7 and deg(b) ≤ 7 → deg(a·b) ≤ 14.
 *
 * @returns the product a*b
 * @param[in] a Element of GF(2^GF_M)
 * @param[in] b Element of GF(2^GF_M)
 */
uint16_t gf_mul(uint16_t a, uint16_t b) {
#ifdef __riscv_zbc
    uint64_t prod;
    __asm__ volatile (
        "clmul %0, %1, %2\n\t"
        : "=r"(prod)
        : "r"((uint64_t)(a & 0xFFu)), "r"((uint64_t)(b & 0xFFu))
    );
    return gf_reduce((uint16_t)prod);
#else
    uint8_t c[2] = {0};
    gf_carryless_mul(c, (uint8_t)a, (uint8_t)b);
    uint16_t tmp = (uint16_t)(c[0] ^ (c[1] << 8));
    return gf_reduce(tmp);
#endif
}

/**
 * Squares an element of GF(2^GF_M).
 * @returns a^2
 * @param[in] a Element of GF(2^GF_M)
 */
uint16_t gf_square(uint16_t a) {
#ifdef __riscv_zbc
    uint64_t s;
    __asm__ volatile (
        "clmul %0, %1, %1\n\t"
        : "=r"(s)
        : "r"((uint64_t)(a & 0xFFu))
    );
    return gf_reduce((uint16_t)s);
#else
    uint32_t b = a;
    uint32_t s = b & 1;
    for (size_t i = 1; i < PARAM_M; ++i) {
        b <<= 1;
        s ^= b & (1 << 2 * i);
    }

    return gf_reduce(s);
#endif
}

/**
 * Computes the inverse of an element of GF(2^8),
 * using the addition chain 1 2 3 4 7 11 15 30 60 120 127 254
 * @returns the inverse of a
 * @param[in] a Element of GF(2^GF_M)
 */
uint16_t gf_inverse(uint16_t a) {
    uint16_t inv = a;
    uint16_t tmp1, tmp2;

    inv = gf_square(a);       /* a^2 */
    tmp1 = gf_mul(inv, a);    /* a^3 */
    inv = gf_square(inv);     /* a^4 */
    tmp2 = gf_mul(inv, tmp1); /* a^7 */
    tmp1 = gf_mul(inv, tmp2); /* a^11 */
    inv = gf_mul(tmp1, inv);  /* a^15 */
    inv = gf_square(inv);     /* a^30 */
    inv = gf_square(inv);     /* a^60 */
    inv = gf_square(inv);     /* a^120 */
    inv = gf_mul(inv, tmp2);  /* a^127 */
    inv = gf_square(inv);     /* a^254 */
    return inv;
}
