/* src/keccak/rvv/fips202_rvv_x2.c
 *
 * Track B2: pure RVV 1.0 x2 SoA Keccak backend.
 *
 * This file keeps the full single-stream fips202 drop-in from the existing
 * rvv_only backend and adds a dedicated 2-way SoA permutation plus paired
 * SHAKE-256 absorb/squeeze helpers used by HQC keygen.
 *
 * No Vector Crypto intrinsics are used here (no Zvbb/Zvbc requirements).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>

#include "fips202.h"

/* Reuse the full single-stream API and incremental glue unchanged. */
#include "../fips202_rvv_only.c"

static const uint8_t rvv_x2_pi_src[25] = {
     0,  6, 12, 18, 24,
     3,  9, 10, 16, 22,
     1,  7, 13, 19, 20,
     4,  5, 11, 17, 23,
     2,  8, 14, 15, 21
};

static const uint8_t rvv_x2_rho_pi_rot[25] = {
     0, 44, 43, 21, 14,
    28, 20,  3, 45, 61,
     1,  6, 25,  8, 18,
    27, 36, 10, 15, 56,
    62, 55, 39, 41,  2
};

static inline size_t keccak_rvv_x2_setvl(void)
{
    size_t vl;
    __asm__ volatile("vsetivli %0, 2, e64, m1, ta, ma" : "=r"(vl));
    return vl;
}

/* Emulate a 64-bit rotate-left with standard RVV shifts + OR. */
static inline vuint64m1_t rvv_x2_rol_u64(vuint64m1_t v, uint32_t shift, size_t vl)
{
    if (shift == 0U) {
        return v;
    }
    return __riscv_vor_vv_u64m1(
        __riscv_vsll_vx_u64m1(v, shift, vl),
        __riscv_vsrl_vx_u64m1(v, (uint32_t)(64U - shift), vl),
        vl);
}

void KeccakF1600_StatePermute_rvv_x2(uint64_t *state1, uint64_t *state2)
{
    const size_t vl = keccak_rvv_x2_setvl();
    vu64m1_fixed_t A[25];
    vu64m1_fixed_t B[25];
    vu64m1_fixed_t C[5];
    vu64m1_fixed_t D[5];

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2] = {state1[i], state2[i]};
        A[i] = __riscv_vle64_v_u64m1(pair, vl);
    }

    for (int round = 0; round < NROUNDS; round++) {
        C[0] = __riscv_vxor_vv_u64m1(
            __riscv_vxor_vv_u64m1(A[0], A[5], vl),
            __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(A[10], A[15], vl), A[20], vl),
            vl);
        C[1] = __riscv_vxor_vv_u64m1(
            __riscv_vxor_vv_u64m1(A[1], A[6], vl),
            __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(A[11], A[16], vl), A[21], vl),
            vl);
        C[2] = __riscv_vxor_vv_u64m1(
            __riscv_vxor_vv_u64m1(A[2], A[7], vl),
            __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(A[12], A[17], vl), A[22], vl),
            vl);
        C[3] = __riscv_vxor_vv_u64m1(
            __riscv_vxor_vv_u64m1(A[3], A[8], vl),
            __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(A[13], A[18], vl), A[23], vl),
            vl);
        C[4] = __riscv_vxor_vv_u64m1(
            __riscv_vxor_vv_u64m1(A[4], A[9], vl),
            __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(A[14], A[19], vl), A[24], vl),
            vl);

        D[0] = __riscv_vxor_vv_u64m1(C[4], rvv_x2_rol_u64(C[1], 1, vl), vl);
        D[1] = __riscv_vxor_vv_u64m1(C[0], rvv_x2_rol_u64(C[2], 1, vl), vl);
        D[2] = __riscv_vxor_vv_u64m1(C[1], rvv_x2_rol_u64(C[3], 1, vl), vl);
        D[3] = __riscv_vxor_vv_u64m1(C[2], rvv_x2_rol_u64(C[4], 1, vl), vl);
        D[4] = __riscv_vxor_vv_u64m1(C[3], rvv_x2_rol_u64(C[0], 1, vl), vl);

        for (size_t x = 0; x < 5; x++) {
            A[x]      = __riscv_vxor_vv_u64m1(A[x], D[x], vl);
            A[x + 5]  = __riscv_vxor_vv_u64m1(A[x + 5], D[x], vl);
            A[x + 10] = __riscv_vxor_vv_u64m1(A[x + 10], D[x], vl);
            A[x + 15] = __riscv_vxor_vv_u64m1(A[x + 15], D[x], vl);
            A[x + 20] = __riscv_vxor_vv_u64m1(A[x + 20], D[x], vl);
        }

        for (size_t i = 0; i < 25; i++) {
            B[i] = rvv_x2_rol_u64(A[rvv_x2_pi_src[i]], rvv_x2_rho_pi_rot[i], vl);
        }

#define CHI_ROW(base) \
        do { \
            vu64m1_fixed_t n1 = __riscv_vxor_vx_u64m1(B[(base) + 1], UINT64_MAX, vl); \
            vu64m1_fixed_t n2 = __riscv_vxor_vx_u64m1(B[(base) + 2], UINT64_MAX, vl); \
            vu64m1_fixed_t n3 = __riscv_vxor_vx_u64m1(B[(base) + 3], UINT64_MAX, vl); \
            vu64m1_fixed_t n4 = __riscv_vxor_vx_u64m1(B[(base) + 4], UINT64_MAX, vl); \
            vu64m1_fixed_t n0 = __riscv_vxor_vx_u64m1(B[(base) + 0], UINT64_MAX, vl); \
            A[(base) + 0] = __riscv_vxor_vv_u64m1(B[(base) + 0], __riscv_vand_vv_u64m1(n1, B[(base) + 2], vl), vl); \
            A[(base) + 1] = __riscv_vxor_vv_u64m1(B[(base) + 1], __riscv_vand_vv_u64m1(n2, B[(base) + 3], vl), vl); \
            A[(base) + 2] = __riscv_vxor_vv_u64m1(B[(base) + 2], __riscv_vand_vv_u64m1(n3, B[(base) + 4], vl), vl); \
            A[(base) + 3] = __riscv_vxor_vv_u64m1(B[(base) + 3], __riscv_vand_vv_u64m1(n4, B[(base) + 0], vl), vl); \
            A[(base) + 4] = __riscv_vxor_vv_u64m1(B[(base) + 4], __riscv_vand_vv_u64m1(n0, B[(base) + 1], vl), vl); \
        } while (0)

        CHI_ROW(0);
        CHI_ROW(5);
        CHI_ROW(10);
        CHI_ROW(15);
        CHI_ROW(20);
#undef CHI_ROW

        A[0] = __riscv_vxor_vx_u64m1(A[0], keccak_round_constants[round], vl);
    }

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2];
        __riscv_vse64_v_u64m1(pair, A[i], vl);
        state1[i] = pair[0];
        state2[i] = pair[1];
    }
}

void shake256_x2_absorb(shake256ctx *ctx0,
                        shake256ctx *ctx1,
                        const uint8_t *in0,
                        const uint8_t *in1,
                        size_t inlen)
{
    uint64_t st0[25] = {0};
    uint64_t st1[25] = {0};
    size_t rem = inlen;

    while (rem >= SHAKE256_RATE) {
        for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
            st0[w] ^= load64_le(in0 + 8 * w);
            st1[w] ^= load64_le(in1 + 8 * w);
        }
        KeccakF1600_StatePermute_rvv_x2(st0, st1);
        in0 += SHAKE256_RATE;
        in1 += SHAKE256_RATE;
        rem -= SHAKE256_RATE;
    }

    uint8_t pad0[SHAKE256_RATE] = {0};
    uint8_t pad1[SHAKE256_RATE] = {0};
    memcpy(pad0, in0, rem);
    memcpy(pad1, in1, rem);
    pad0[rem] = 0x1FU;
    pad1[rem] = 0x1FU;
    pad0[SHAKE256_RATE - 1] |= 0x80U;
    pad1[SHAKE256_RATE - 1] |= 0x80U;

    for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
        st0[w] ^= load64_le(pad0 + 8 * w);
        st1[w] ^= load64_le(pad1 + 8 * w);
    }

    memcpy(ctx0->ctx, st0, sizeof(st0));
    memcpy(ctx1->ctx, st1, sizeof(st1));
}

void shake256_x2_squeezeblocks(uint8_t *out0,
                               uint8_t *out1,
                               size_t nblocks,
                               shake256ctx *ctx0,
                               shake256ctx *ctx1)
{
    uint64_t st0[25];
    uint64_t st1[25];
    memcpy(st0, ctx0->ctx, sizeof(st0));
    memcpy(st1, ctx1->ctx, sizeof(st1));

    while (nblocks > 0) {
        KeccakF1600_StatePermute_rvv_x2(st0, st1);

        for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
            store64_le(out0 + 8 * w, st0[w]);
            store64_le(out1 + 8 * w, st1[w]);
        }

        out0 += SHAKE256_RATE;
        out1 += SHAKE256_RATE;
        nblocks--;
    }

    memcpy(ctx0->ctx, st0, sizeof(st0));
    memcpy(ctx1->ctx, st1, sizeof(st1));
}
