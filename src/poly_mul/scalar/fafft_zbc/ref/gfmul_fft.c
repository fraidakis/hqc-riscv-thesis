#include "parameters.h"
#include <stddef.h>
#include <stdint.h>
#include "encode.h"
#include "btfy_ffft.h"
#include "bitconversion.h"
#include "gfmul_fft.h"

#include <string.h>

void bmul2_4096_to_8192_prepare(uint32_t *a_out, const uint8_t *a_in)
{
    uint32_t a0[32768 / 32] = {0};
    memset(a_out, 0, (65536 / 32) * sizeof(uint32_t));
    memcpy(a0, a_in, VEC_N_SIZE_64 * sizeof(uint64_t));
    bc_1_32768(a0);

    uint32_t temp[16];
    for (int i = 0; i < ((65536 / 32) / 32); i++) {
        for (int j = 0; j < 16; j++) temp[j] = a0[((65536 / 32) / 32) * j + i];
        encode_trunc_fft(a_out + i * 32, temp);
    }
    btfy_65536(a_out);
}

void bmul2_4096_to_8192_mul(uint8_t *c, const uint32_t *a, const uint32_t *b)
{
    uint32_t a0[65536 / 32] = {0};
    uint32_t *c32 = (uint32_t *)c;
    memset(c, 0, 65536 / 8);

    for (int i = 0; i < (65536 / 32); i += 32) {
        gf232v_mul(a0 + i, a + i, b + i);
    }

    ibtfy_65536(a0);

    uint32_t temp[32];
    for (int i = 0; i < (65536 / 32); i += 32) {
        decode_trunc_fft(temp, a0 + i);
        for (int j = 0; j < 32; j++) c32[((65536 / 32) / 32) * j + i / 32] = temp[j];
    }

    ibc_1_65536(c32);
}

void bmul2_4096_to_8192(uint8_t *c, const uint8_t *a, const uint8_t *b)
{
    uint32_t a_u32[65536 / 32];
    uint32_t b_u32[65536 / 32];

    bmul2_4096_to_8192_prepare(a_u32, a);
    bmul2_4096_to_8192_prepare(b_u32, b);

    bmul2_4096_to_8192_mul(c, a_u32, b_u32);
}

void bmul2_8192_to_16384_prepare(uint32_t *a_out, const uint8_t *a_in)
{
    uint32_t a0[65536 / 32] = {0};
    memset(a_out, 0, (131072 / 32) * sizeof(uint32_t));
    memcpy(a0, a_in, VEC_N_SIZE_64 * 8);
    bc_1_65536(a0);

    uint32_t temp[16];
    for (int i = 0; i < ((131072 / 32) / 32); i++) {
        for (int j = 0; j < 16; j++) temp[j] = a0[((131072 / 32) / 32) * j + i];
        encode_trunc_fft(a_out + i * 32, temp);
    }
    btfy_131072(a_out);
}

void bmul2_8192_to_16384_mul(uint8_t *c, const uint32_t *a, const uint32_t *b)
{
    uint32_t a0[131072 / 32] = {0};
    uint32_t *c32 = (uint32_t *)c;
    memset(c, 0, 131072 / 8);

    for (int i = 0; i < (131072 / 32); i += 32) {
        gf232v_mul(a0 + i, a + i, b + i);
    }

    ibtfy_131072(a0);

    uint32_t temp[32];
    for (int i = 0; i < (131072 / 32); i += 32) {
        decode_trunc_fft(temp, a0 + i);
        for (int j = 0; j < 32; j++) c32[((131072 / 32) / 32) * j + i / 32] = temp[j];
    }

    ibc_1_131072(c32);
}

void bmul2_8192_to_16384(uint8_t *c, const uint8_t *a, const uint8_t *b)
{
    uint32_t a_u32[131072 / 32];
    uint32_t b_u32[131072 / 32];

    bmul2_8192_to_16384_prepare(a_u32, a);
    bmul2_8192_to_16384_prepare(b_u32, b);

    bmul2_8192_to_16384_mul(c, a_u32, b_u32);
}

/* ── Same-domain CRT kernels ─────────────────────────────────────────────────
 *
 * bmul2_4096_to_4096: compute the lower N_FAFFT bits of A*B (A*B mod x^32768).
 * Used by CRT_fft_HQC1 (HQC-128); the CRT reconstruction step then adds back
 * the high-degree correction to recover the full product.
 *
 * IMPLEMENTATION CONSTRAINT (HQC-128):  The FAFFT half-domain kernel that
 * would make this fast is bc_1_16384 + btfy_32768.  However, bc_1_16384
 * accepts at most 512-word inputs, and HQC-128 polynomials have 553 words.
 * bc_1_16384 cannot be swapped for bc_1_32768 because the valid bc/btfy
 * pairing is bc_1_N → encode → btfy_2N (bc_1_32768 pairs with btfy_65536).
 * Without a full-input bc_1_16384 the CRT correction cannot recover A*B;
 * therefore CRT_fft_HQC1 is not active and this function is a stub.
 */
void bmul2_4096_to_4096_prepare(uint32_t *a_out, const uint8_t *a_in)
{
    (void)a_out; (void)a_in;  /* prepare is fused into bmul2_4096_to_4096 */
}

void bmul2_4096_to_4096_mul(uint8_t *c, const uint32_t *a, const uint32_t *b)
{
    (void)c; (void)a; (void)b;
}

void bmul2_4096_to_4096(uint8_t *c, const uint8_t *a, const uint8_t *b)
{
    /* Full product; take the lower 32768 bits (1024 words). */
    uint32_t full[65536 / 32] = {0};
    bmul2_4096_to_8192((uint8_t *)full, a, b);
    memcpy(c, full, 32768 / 8);  /* lower 1024 words = 4096 bytes */
}

/* bmul2_8192_to_8192: lower 65536 bits of A*B (A*B mod x^65536), for CRT_HQC3. */
void bmul2_8192_to_8192_prepare(uint32_t *a_out, const uint8_t *a_in)
{
    (void)a_out; (void)a_in;
}

void bmul2_8192_to_8192_mul(uint8_t *c, const uint32_t *a, const uint32_t *b)
{
    (void)c; (void)a; (void)b;
}

void bmul2_8192_to_8192(uint8_t *c, const uint8_t *a, const uint8_t *b)
{
    uint32_t full[131072 / 32] = {0};
    bmul2_8192_to_16384((uint8_t *)full, a, b);
    memcpy(c, full, 65536 / 8);  /* lower 2048 words = 8192 bytes */
}
