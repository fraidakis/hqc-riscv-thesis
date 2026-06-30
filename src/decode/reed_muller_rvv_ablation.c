/* src/decode/reed_muller_rvv_ablation.c
 *
 * Parametric ablation file for the RVV Reed-Muller RM(1,7) decoder.
 *
 * Compile-time flags select which sub-functions use RVV vectorization:
 *
 *   ABLATION_RM_EXPAND_RVV  (E) : 1 = RVV-vectorized expand_and_sum (vid+vsrl)
 *                                 0 = scalar bit-extraction (reference)
 *
 *   ABLATION_RM_HADAMARD_RVV (H) : 1 = RVV-vectorized FHT (strided loads)
 *                                  0 = scalar butterfly loops (reference)
 *
 *   ABLATION_RM_PEAKS_RVV   (P) : 1 = RVV-vectorized find_peaks (vredmaxu+vfirst)
 *                                 0 = scalar linear scan (reference)
 *
 * Ablation variants (iresults/simulation/benchmarks/components/decode/sim_decode_scalar-zb_latest.csvn thesis presentation order):
 *   rm_ref       : E=0, H=0, P=0  — full scalar baseline (vendor reference)
 *   rm_rvv_E     : E=1, H=0, P=0  — + vectorized expand_and_sum only
 *   rm_rvv_EH    : E=1, H=1, P=0  — + vectorized FHT hadamard
 *   rm_rvv       : E=1, H=1, P=1  — full RVV (champion) = reed_muller_rvv.c
 *
 * Each variant is KAT-verifiable (identical functional output).
 *
 * Build:
 *   The Makefile.hw / CMakeLists.txt uses DECODE=rm_rvv_E, rm_rvv_EH, etc.
 *   with appropriate -D flags to select the active optimization set.
 *
 * Interface (unchanged from vendor):
 *   void reed_muller_encode(uint64_t *cdw, const uint64_t *msg);
 *   void reed_muller_decode(uint64_t *msg, const uint64_t *cdw);
 */

#include "reed_muller.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "data_structures.h"
#include "parameters.h"

#if ABLATION_RM_EXPAND_RVV || ABLATION_RM_HADAMARD_RVV || ABLATION_RM_PEAKS_RVV
#include <riscv_vector.h>
#endif

#define MULTIPLICITY CEIL_DIVIDE(PARAM_N2, 128)

typedef int16_t rm_expanded_cdw[128];

// clang-format off
#define BIT0MASK(x) (int32_t)(-((x) & 1))
// clang-format on

/* ── Default flags if not set at compile time ────────────────────────────── */
#ifndef ABLATION_RM_EXPAND_RVV
#define ABLATION_RM_EXPAND_RVV 0
#endif
#ifndef ABLATION_RM_HADAMARD_RVV
#define ABLATION_RM_HADAMARD_RVV 0
#endif
#ifndef ABLATION_RM_PEAKS_RVV
#define ABLATION_RM_PEAKS_RVV 0
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * expand_and_sum — bit extraction from MULTIPLICITY codeword copies
 * ═══════════════════════════════════════════════════════════════════════════*/

#if ABLATION_RM_EXPAND_RVV

/* RVV path: SEW=32, LMUL=4, vl=32.  vid provides shift amounts [0..31],
 * then vsrl+vand extracts all 32 bits of a uint32 word in one pass.
 * Narrow u32→u16 for accumulation into int16_t dest array. */
static void expand_and_sum(rm_expanded_cdw *dest, rm_codeword_t src[]) {
    size_t vl32 = __riscv_vsetvl_e32m4(32);
    vuint32m4_t shift_amounts = __riscv_vid_v_u32m4(vl32);

    /* Copy 0: assign (overwrite dest). */
    for (int32_t part = 0; part < 4; part++) {
        vuint32m4_t vword = __riscv_vmv_v_x_u32m4(src[0].u32[part], vl32);
        vuint32m4_t bits32 = __riscv_vand_vx_u32m4(
            __riscv_vsrl_vv_u32m4(vword, shift_amounts, vl32), 1, vl32);
        vuint16m2_t bits16 = __riscv_vncvt_x_x_w_u16m2(bits32, vl32);
        __riscv_vse16_v_u16m2(
            (uint16_t *)&(*dest)[part * 32], bits16, vl32);
    }

    /* Copies 1..MULTIPLICITY-1: accumulate. */
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

#else /* Scalar expand_and_sum (vendor reference) */

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

#endif /* ABLATION_RM_EXPAND_RVV */

/* ═══════════════════════════════════════════════════════════════════════════
 * hadamard — 7-pass Fast Hadamard Transform on 128 int16 elements
 * ═══════════════════════════════════════════════════════════════════════════*/

#if ABLATION_RM_HADAMARD_RVV

/* RVV path: VLEN=256, SEW=16, LMUL=4 → vl=64.  Each pass loads 64
 * even-indexed and 64 odd-indexed elements via strided loads (stride=4 bytes),
 * performs butterfly (add/sub), stores to contiguous halves. */
static void hadamard(rm_expanded_cdw *src, rm_expanded_cdw *dst) {
    rm_expanded_cdw *p1 = src;
    rm_expanded_cdw *p2 = dst;

    size_t vl = __riscv_vsetvl_e16m4(64);

    for (int32_t pass = 0; pass < 7; pass++) {
        int16_t *in  = (int16_t *)(*p1);
        int16_t *out = (int16_t *)(*p2);

        vint16m4_t even = __riscv_vlse16_v_i16m4(in,     (ptrdiff_t)(2 * sizeof(int16_t)), vl);
        vint16m4_t odd  = __riscv_vlse16_v_i16m4(in + 1, (ptrdiff_t)(2 * sizeof(int16_t)), vl);

        __riscv_vse16_v_i16m4(out,      __riscv_vadd_vv_i16m4(even, odd, vl), vl);
        __riscv_vse16_v_i16m4(out + 64, __riscv_vsub_vv_i16m4(even, odd, vl), vl);

        rm_expanded_cdw *p3 = p1;
        p1 = p2;
        p2 = p3;
    }
}

#else /* Scalar hadamard (vendor reference) */

static void hadamard(rm_expanded_cdw *src, rm_expanded_cdw *dst) {
    rm_expanded_cdw *p1 = src;
    rm_expanded_cdw *p2 = dst;
    for (int32_t pass = 0; pass < 7; pass++) {
        for (int32_t i = 0; i < 64; i++) {
            (*p2)[i] = (*p1)[2 * i] + (*p1)[2 * i + 1];
            (*p2)[i + 64] = (*p1)[2 * i] - (*p1)[2 * i + 1];
        }
        rm_expanded_cdw *p3 = p1;
        p1 = p2;
        p2 = p3;
    }
}

#endif /* ABLATION_RM_HADAMARD_RVV */

/* ═══════════════════════════════════════════════════════════════════════════
 * find_peaks — locate maximum absolute value in the 128-element transform
 * ═══════════════════════════════════════════════════════════════════════════*/

#if ABLATION_RM_PEAKS_RVV

/* RVV path: LMUL=8, SEW=16 → vl=128 (whole array in one pass).
 * Uses vredmaxu for maximum, vmseq+vfirst for position. */
static int32_t find_peaks(rm_expanded_cdw *transform) {
    size_t vl = __riscv_vsetvl_e16m8(128);
    int16_t *t = (int16_t *)(*transform);

    vint16m8_t v = __riscv_vle16_v_i16m8(t, vl);

    /* Absolute value: max(v, -v). */
    vint16m8_t neg_v  = __riscv_vrsub_vx_i16m8(v, 0, vl);
    vint16m8_t abs_v  = __riscv_vmax_vv_i16m8(v, neg_v, vl);

    /* Unsigned reduction to find the maximum absolute value. */
    vuint16m8_t uabs_v     = __riscv_vreinterpret_v_i16m8_u16m8(abs_v);
    vuint16m1_t identity   = __riscv_vmv_v_x_u16m1(0, 1);
    vuint16m1_t max_red    = __riscv_vredmaxu_vs_u16m8_u16m1(uabs_v, identity, vl);
    uint16_t    max_abs    = __riscv_vmv_x_s_u16m1_u16(max_red);

    /* Mask: first lane where |v[i]| == max_abs. */
    vbool2_t peak_mask = __riscv_vmseq_vx_u16m8_b2(uabs_v, max_abs, vl);
    long     peak_pos  = __riscv_vfirst_m_b2(peak_mask, vl);

    /* Set bit 7 if the peak value is positive. */
    int16_t peak_val = t[peak_pos];
    return (int32_t)peak_pos | (128 * (peak_val > 0));
}

#else /* Scalar find_peaks (vendor reference) */

static int32_t find_peaks(rm_expanded_cdw *transform) {
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

#endif /* ABLATION_RM_PEAKS_RVV */

/* ═══════════════════════════════════════════════════════════════════════════
 * encode — unchanged from vendor (not on the decode hot-path)
 * ═══════════════════════════════════════════════════════════════════════════*/

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

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════*/

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
        hadamard(&expanded, &transform);
        transform[0] -= 64 * MULTIPLICITY;

        message_array[i] = (uint8_t)find_peaks(&transform);
    }
}
