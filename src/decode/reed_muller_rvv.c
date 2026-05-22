/**
 * @file reed_muller_rvv.c
 * @brief RVV implementation of Reed-Muller code RM(1,7)
 */

#include "reed_muller.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include "data_structures.h"
#include "parameters.h"

#define MULTIPLICITY CEIL_DIVIDE(PARAM_N2, 128)

typedef int16_t rm_expanded_cdw[128];

// clang-format off
#define BIT0MASK(x) (int32_t)(-((x) & 1))
// clang-format on

static void hadamard_rvv(rm_expanded_cdw *src, rm_expanded_cdw *dst) {
    rm_expanded_cdw *p1 = src;
    rm_expanded_cdw *p2 = dst;

    // With VLEN=256, SEW=16, LMUL=4: vl = 256*4/16 = 64 elements (covers all
    // 64 butterfly pairs in a single vector instruction).
    size_t vl = __riscv_vsetvl_e16m4(64);

    for (int32_t pass = 0; pass < 7; pass++) {
        int16_t *in  = (int16_t *)(*p1);
        int16_t *out = (int16_t *)(*p2);

        // Load 64 even-indexed elements: p1[0], p1[2], ..., p1[126]
        // stride = 2 * sizeof(int16_t) = 4 bytes
        vint16m4_t even = __riscv_vlse16_v_i16m4(in,     (ptrdiff_t)(2 * sizeof(int16_t)), vl);
        // Load 64 odd-indexed elements: p1[1], p1[3], ..., p1[127]
        vint16m4_t odd  = __riscv_vlse16_v_i16m4(in + 1, (ptrdiff_t)(2 * sizeof(int16_t)), vl);

        // p2[0..63]   = even + odd  (butterfly sum)
        // p2[64..127] = even - odd  (butterfly difference)
        __riscv_vse16_v_i16m4(out,      __riscv_vadd_vv_i16m4(even, odd, vl), vl);
        __riscv_vse16_v_i16m4(out + 64, __riscv_vsub_vv_i16m4(even, odd, vl), vl);

        rm_expanded_cdw *p3 = p1;
        p1 = p2;
        p2 = p3;
    }
}

static int32_t find_peaks_rvv(rm_expanded_cdw *transform) {
    // VLEN=256, SEW=16, LMUL=8: vl = 256*8/16 = 128 — whole array in one pass.
    size_t vl = __riscv_vsetvl_e16m8(128);
    int16_t *t = (int16_t *)(*transform);

    vint16m8_t v = __riscv_vle16_v_i16m8(t, vl);

    // Absolute value: max(v, 0 - v).
    // RM values are bounded by MULTIPLICITY (≤46), so INT16_MIN is never seen.
    vint16m8_t neg_v  = __riscv_vrsub_vx_i16m8(v, 0, vl);
    vint16m8_t abs_v  = __riscv_vmax_vv_i16m8(v, neg_v, vl);

    // Unsigned reduction to find the maximum absolute value.
    vuint16m8_t uabs_v     = __riscv_vreinterpret_v_i16m8_u16m8(abs_v);
    vuint16m1_t identity   = __riscv_vmv_v_x_u16m1(0, 1);
    vuint16m1_t max_red    = __riscv_vredmaxu_vs_u16m8_u16m1(uabs_v, identity, vl);
    uint16_t    max_abs    = __riscv_vmv_x_s_u16m1_u16(max_red);

    // Mask: first lane where |v[i]| == max_abs.
    vbool2_t peak_mask = __riscv_vmseq_vx_u16m8_b2(uabs_v, max_abs, vl);
    long     peak_pos  = __riscv_vfirst_m_b2(peak_mask, vl);

    // Set bit 7 if the peak value is positive.
    int16_t peak_val = t[peak_pos];
    return (int32_t)peak_pos | (128 * (peak_val > 0));
}

static void expand_and_sum(rm_expanded_cdw *dest, rm_codeword_t src[]) {
    // SEW=32, LMUL=4, vl=32: process all 32 bits of one uint32_t word in a
    // single vector pass.  shift_amounts = [0, 1, 2, ..., 31].
    size_t vl32 = __riscv_vsetvl_e32m4(32);
    vuint32m4_t shift_amounts = __riscv_vid_v_u32m4(vl32);

    // copy 0: assign (overwrite dest rather than accumulate).
    for (int32_t part = 0; part < 4; part++) {
        vuint32m4_t vword = __riscv_vmv_v_x_u32m4(src[0].u32[part], vl32);
        vuint32m4_t bits32 = __riscv_vand_vx_u32m4(
            __riscv_vsrl_vv_u32m4(vword, shift_amounts, vl32), 1, vl32);
        // Narrow uint32→uint16: values are 0 or 1, truncation is exact.
        vuint16m2_t bits16 = __riscv_vncvt_x_x_w_u16m2(bits32, vl32);
        __riscv_vse16_v_u16m2(
            (uint16_t *)&(*dest)[part * 32], bits16, vl32);
    }

    // copies 1..MULTIPLICITY-1: accumulate.
    for (int32_t copy = 1; copy < MULTIPLICITY; copy++) {
        for (int32_t part = 0; part < 4; part++) {
            vuint32m4_t vword = __riscv_vmv_v_x_u32m4(src[copy].u32[part], vl32);
            vuint32m4_t bits32 = __riscv_vand_vx_u32m4(
                __riscv_vsrl_vv_u32m4(vword, shift_amounts, vl32), 1, vl32);
            vuint16m2_t bits16 = __riscv_vncvt_x_x_w_u16m2(bits32, vl32);
            vint16m2_t  acc    = __riscv_vle16_v_i16m2(
                (int16_t *)&(*dest)[part * 32], vl32);
            acc = __riscv_vadd_vv_i16m2(
                acc, __riscv_vreinterpret_v_u16m2_i16m2(bits16), vl32);
            __riscv_vse16_v_i16m2(
                (int16_t *)&(*dest)[part * 32], acc, vl32);
        }
    }
}

static void encode(rm_codeword_t *word, int32_t message) {
    int32_t first_word;

    first_word = BIT0MASK(message >> 7);
    first_word ^= BIT0MASK(message >> 0) & 0xaaaaaaaa;
    first_word ^= BIT0MASK(message >> 1) & 0xcccccccc;
    first_word ^= BIT0MASK(message >> 2) & 0xf0f0f0f0;
    first_word ^= BIT0MASK(message >> 3) & 0xff00ff00;
    first_word ^= BIT0MASK(message >> 4) & 0xffff0000;

    word->u32[0] = first_word;

    first_word ^= BIT0MASK(message >> 5);
    word->u32[1] = first_word;
    first_word ^= BIT0MASK(message >> 6);
    word->u32[3] = first_word;
    first_word ^= BIT0MASK(message >> 5);
    word->u32[2] = first_word;
}

void reed_muller_encode(uint64_t *cdw, const uint64_t *msg) {
    uint8_t *message_array = (uint8_t *)msg;
    rm_codeword_t *codeArray = (rm_codeword_t *)cdw;

    for (size_t i = 0; i < VEC_N1_SIZE_BYTES; i++) {
        int32_t pos = i * MULTIPLICITY;
        encode(&codeArray[pos], message_array[i]);
        for (size_t copy = 1; copy < MULTIPLICITY; copy++) {
            memcpy(&codeArray[pos + copy], &codeArray[pos], sizeof(rm_codeword_t));
        }
    }
}

void reed_muller_decode(uint64_t *msg, const uint64_t *cdw) {
    uint8_t *message_array = (uint8_t *)msg;
    rm_codeword_t *codeArray = (rm_codeword_t *)cdw;
    rm_expanded_cdw expanded;

    for (size_t i = 0; i < VEC_N1_SIZE_BYTES; i++) {
        expand_and_sum(&expanded, &codeArray[i * MULTIPLICITY]);

        rm_expanded_cdw transform;
        hadamard_rvv(&expanded, &transform);
        transform[0] -= 64 * MULTIPLICITY;

        message_array[i] = (uint8_t)find_peaks_rvv(&transform);
    }
}
