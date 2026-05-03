/* bc_rvv.h — RVV-accelerated inner-loop helper for bitconversion.c
 *
 * Replaces the scalar 4-element-per-iteration inner loops of the
 * cvt_to_fft_256 / icvt_to_poly_256 functions with VLEN=256 LMUL=4
 * vector loads/XOR/stores (32 uint32_t per vector group).
 *
 * The pattern to replace in each layer block is:
 *
 *   for (int i = (DEG_H/32)-4; i >= 0; i -= 4) {
 *       p0-3 = ptr[i + DEG_H/32 .. +3]
 *       q0-3 = ptr[i + DEG_L/32 .. +3]
 *       ptr[i + DEG_L/32 .. +3] = q ^ p
 *   }
 *
 * Replaced by:
 *
 *   RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32)
 *
 * Aliasing analysis: dst = ptr+DEG_L/32, src = ptr+DEG_H/32.
 * Writing dst[i] may alias src[i-(DEG_H-DEG_L)/32] (lower src index).
 * Forward iteration processes increasing i, so alias target is always
 * a src element that has already been consumed — safe.
 */

#ifndef _BC_RVV_H_
#define _BC_RVV_H_

#include <riscv_vector.h>
#include <stdint.h>
#include <stddef.h>

/* RVV_XOR_OFFSET(ptr, src_off, dst_off, n)
 *
 * Computes: ptr[dst_off .. dst_off+n-1] ^= ptr[src_off .. src_off+n-1]
 * using VLEN=256 LMUL=4 (32 uint32_t per vector group).
 * Works for any n >= 0, including n < 32 (tail handled by vsetvl).
 */
#define RVV_XOR_OFFSET(ptr, src_off, dst_off, n)                          \
    do {                                                                   \
        uint32_t *_src = (ptr) + (src_off);                               \
        uint32_t *_dst = (ptr) + (dst_off);                               \
        int _rem = (n);                                                    \
        while (_rem > 0) {                                                 \
            size_t _vl = __riscv_vsetvl_e32m4(_rem);                      \
            vuint32m4_t _vs = __riscv_vle32_v_u32m4(_src, _vl);          \
            vuint32m4_t _vd = __riscv_vle32_v_u32m4(_dst, _vl);          \
            __riscv_vse32_v_u32m4(_dst,                                   \
                __riscv_vxor_vv_u32m4(_vd, _vs, _vl), _vl);              \
            _src += _vl;                                                   \
            _dst += _vl;                                                   \
            _rem -= _vl;                                                   \
        }                                                                  \
    } while (0)

#endif /* _BC_RVV_H_ */
