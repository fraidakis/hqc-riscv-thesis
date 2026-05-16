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
    int32_t peak_abs_value = 0;
    int32_t peak_value = 0;
    int32_t peak_pos = 0;
    for (int32_t i = 0; i < 128; i++) {
        int32_t t = (*transform)[i];
        int32_t pos_mask = -(t > 0);
        int32_t absolute = (pos_mask & t) | (~pos_mask & -t);
        peak_value = absolute > peak_abs_value ? t : peak_value;
        peak_pos = absolute > peak_abs_value ? i : peak_pos;
        peak_abs_value = absolute > peak_abs_value ? absolute : peak_abs_value;
    }
    peak_pos |= 128 * (peak_value > 0);
    return peak_pos;
}

static void expand_and_sum(rm_expanded_cdw *dest, rm_codeword_t src[]) {
    for (int32_t part = 0; part < 4; part++) {
        for (int32_t bit = 0; bit < 32; bit++) {
            (*dest)[part * 32 + bit] = src[0].u32[part] >> bit & 1;
        }
    }

    for (int32_t copy = 1; copy < MULTIPLICITY; copy++) {
        for (int32_t part = 0; part < 4; part++) {
            for (int32_t bit = 0; bit < 32; bit++) {
                (*dest)[part * 32 + bit] += src[copy].u32[part] >> bit & 1;
            }
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
