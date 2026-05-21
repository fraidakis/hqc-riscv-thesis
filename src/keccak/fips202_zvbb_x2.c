/* src/keccak/fips202_zvbb_x2.c
 *
 * Track C2 (EXP-keccak-x2-soa): 2-way SoA vector Keccak helpers.
 *
 * This translation unit reuses the mature single-stream fips202 drop-in from
 * the zvbb gather backend, and adds a dedicated vl=2 SoA permutation core plus
 * paired-stream SHAKE-256 wrappers used by HQC call-site experiments.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>

#include "fips202.h"

/* Reuse the internal single-stream Zvbb core and incremental glue unchanged. */
#include "fips202_zvbb_core.c"

/* Clang requires fixed-width RVV aliases for arrays of vector values. */
typedef vuint64m1_t vu64m1_fixed_t __attribute__((riscv_rvv_vector_bits(256)));

static inline size_t keccak_x2_setvl(void)
{
    size_t vl;
    __asm__ volatile("vsetivli %0, 2, e64, m1, ta, ma" : "=r"(vl));
    return vl;
}

static inline uint64_t x2_load64_le(const uint8_t *x)
{
    uint64_t r = 0;
    for (size_t i = 0; i < 8; i++) {
        r |= (uint64_t)x[i] << (8 * i);
    }
    return r;
}

static inline void x2_store64_le(uint8_t *x, uint64_t u)
{
    for (size_t i = 0; i < 8; i++) {
        x[i] = (uint8_t)(u >> (8 * i));
    }
}

/* SoA permutation core: state[25], each vector lane is one independent stream. */
static void KeccakF1600_StatePermute_x2(vu64m1_fixed_t *state)
{
    const size_t vl = keccak_x2_setvl();
    vu64m1_fixed_t C[5];
    vu64m1_fixed_t D[5];
    vu64m1_fixed_t B[25];

    for (int round = 0; round < NROUNDS; round++) {
        C[0] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[0], state[5], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[10], state[15], vl), state[20], vl), vl);
        C[1] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[1], state[6], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[11], state[16], vl), state[21], vl), vl);
        C[2] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[2], state[7], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[12], state[17], vl), state[22], vl), vl);
        C[3] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[3], state[8], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[13], state[18], vl), state[23], vl), vl);
        C[4] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[4], state[9], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[14], state[19], vl), state[24], vl), vl);

        D[0] = __riscv_vxor_vv_u64m1(C[4], __riscv_vror_vx_u64m1(C[1], 63, vl), vl);
        D[1] = __riscv_vxor_vv_u64m1(C[0], __riscv_vror_vx_u64m1(C[2], 63, vl), vl);
        D[2] = __riscv_vxor_vv_u64m1(C[1], __riscv_vror_vx_u64m1(C[3], 63, vl), vl);
        D[3] = __riscv_vxor_vv_u64m1(C[2], __riscv_vror_vx_u64m1(C[4], 63, vl), vl);
        D[4] = __riscv_vxor_vv_u64m1(C[3], __riscv_vror_vx_u64m1(C[0], 63, vl), vl);

        for (int x = 0; x < 5; x++) {
            state[x]      = __riscv_vxor_vv_u64m1(state[x], D[x], vl);
            state[x + 5]  = __riscv_vxor_vv_u64m1(state[x + 5], D[x], vl);
            state[x + 10] = __riscv_vxor_vv_u64m1(state[x + 10], D[x], vl);
            state[x + 15] = __riscv_vxor_vv_u64m1(state[x + 15], D[x], vl);
            state[x + 20] = __riscv_vxor_vv_u64m1(state[x + 20], D[x], vl);
        }

        for (int i = 0; i < 25; i++) {
            B[i] = __riscv_vror_vx_u64m1(state[pi_src[i]], ror_amounts[i], vl);
        }

#define CHI_ROW(base) \
        state[(base) + 0] = __riscv_vxor_vv_u64m1(B[(base) + 0], __riscv_vandn_vv_u64m1(B[(base) + 2], B[(base) + 1], vl), vl); \
        state[(base) + 1] = __riscv_vxor_vv_u64m1(B[(base) + 1], __riscv_vandn_vv_u64m1(B[(base) + 3], B[(base) + 2], vl), vl); \
        state[(base) + 2] = __riscv_vxor_vv_u64m1(B[(base) + 2], __riscv_vandn_vv_u64m1(B[(base) + 4], B[(base) + 3], vl), vl); \
        state[(base) + 3] = __riscv_vxor_vv_u64m1(B[(base) + 3], __riscv_vandn_vv_u64m1(B[(base) + 0], B[(base) + 4], vl), vl); \
        state[(base) + 4] = __riscv_vxor_vv_u64m1(B[(base) + 4], __riscv_vandn_vv_u64m1(B[(base) + 1], B[(base) + 0], vl), vl)

        CHI_ROW(0);
        CHI_ROW(5);
        CHI_ROW(10);
        CHI_ROW(15);
        CHI_ROW(20);
#undef CHI_ROW

        state[0] = __riscv_vxor_vx_u64m1(state[0], KeccakF_RoundConstants[round], vl);
    }
}

void shake256_x2_absorb(shake256ctx *ctx0,
                        shake256ctx *ctx1,
                        const uint8_t *in0,
                        const uint8_t *in1,
                        size_t inlen)
{
    const size_t vl = keccak_x2_setvl();
    vu64m1_fixed_t state[25];

    for (size_t i = 0; i < 25; i++) {
        state[i] = __riscv_vmv_v_x_u64m1(0, vl);
    }

    size_t rem = inlen;
    while (rem >= SHAKE256_RATE) {
        for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
            uint64_t pair[2];
            pair[0] = x2_load64_le(in0 + 8 * w);
            pair[1] = x2_load64_le(in1 + 8 * w);
            state[w] = __riscv_vxor_vv_u64m1(state[w], __riscv_vle64_v_u64m1(pair, vl), vl);
        }

        KeccakF1600_StatePermute_x2(state);
        in0 += SHAKE256_RATE;
        in1 += SHAKE256_RATE;
        rem -= SHAKE256_RATE;
    }

    uint8_t tail0[SHAKE256_RATE] = {0};
    uint8_t tail1[SHAKE256_RATE] = {0};
    memcpy(tail0, in0, rem);
    memcpy(tail1, in1, rem);
    tail0[rem] = 0x1F;
    tail1[rem] = 0x1F;
    tail0[SHAKE256_RATE - 1] |= 0x80;
    tail1[SHAKE256_RATE - 1] |= 0x80;

    for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
        uint64_t pair[2];
        pair[0] = x2_load64_le(tail0 + 8 * w);
        pair[1] = x2_load64_le(tail1 + 8 * w);
        state[w] = __riscv_vxor_vv_u64m1(state[w], __riscv_vle64_v_u64m1(pair, vl), vl);
    }

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2];
        __riscv_vse64_v_u64m1(pair, state[i], vl);
        ctx0->ctx[i] = pair[0];
        ctx1->ctx[i] = pair[1];
    }
}

void shake256_x2_squeezeblocks(uint8_t *out0,
                               uint8_t *out1,
                               size_t nblocks,
                               shake256ctx *ctx0,
                               shake256ctx *ctx1)
{
    const size_t vl = keccak_x2_setvl();
    vu64m1_fixed_t state[25];

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2] = {ctx0->ctx[i], ctx1->ctx[i]};
        state[i] = __riscv_vle64_v_u64m1(pair, vl);
    }

    while (nblocks > 0) {
        KeccakF1600_StatePermute_x2(state);
        for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
            uint64_t pair[2];
            __riscv_vse64_v_u64m1(pair, state[w], vl);
            x2_store64_le(out0 + 8 * w, pair[0]);
            x2_store64_le(out1 + 8 * w, pair[1]);
        }
        out0 += SHAKE256_RATE;
        out1 += SHAKE256_RATE;
        nblocks--;
    }

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2];
        __riscv_vse64_v_u64m1(pair, state[i], vl);
        ctx0->ctx[i] = pair[0];
        ctx1->ctx[i] = pair[1];
    }
}
