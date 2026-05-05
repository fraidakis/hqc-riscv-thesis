/* src/poly_mul/fafft_zbc/ref/btfy_rvv.h
 *
 * RVV-accelerated 32-element inner-loop helpers for btfy_ffft.c.
 *
 * All helpers operate on exactly 32 uint32_t elements (one LMUL=4 group at
 * VLEN=256).  vsetvlmax_e32m4() returns 32 and is called once per invocation.
 *
 * Iter 2 — replace scalar 32-word XOR butterfly loops.
 * Iter 3 — replace scalar 32-word SWAPMOVE (bit-interleave) loops.
 */
#ifndef BTFY_RVV_H
#define BTFY_RVV_H

#include <riscv_vector.h>
#include <stdint.h>

/* ── Iter 2: XOR butterfly helpers ─────────────────────────────────────── */

/*
 * Forward butterfly, 1 source:
 *   lo[0..31] ^= t[0..31];
 *   hi[0..31] ^= lo[0..31];          (uses updated lo)
 */
static __attribute__((always_inline)) inline
void rvv_btf_xor32_1src(uint32_t * restrict lo, uint32_t * restrict hi,
                         const uint32_t * restrict t)
{
    size_t i = 0;
    while (i < 32) {
        size_t vl = __riscv_vsetvl_e32m1(32 - i);
        vuint32m1_t vlo = __riscv_vle32_v_u32m1(lo + i, vl);
        vuint32m1_t vt = __riscv_vle32_v_u32m1(t + i, vl);
        vlo = __riscv_vxor_vv_u32m1(vlo, vt, vl);
        __riscv_vse32_v_u32m1(lo + i, vlo, vl);
        vuint32m1_t vhi = __riscv_vle32_v_u32m1(hi + i, vl);
        vhi = __riscv_vxor_vv_u32m1(vhi, vlo, vl);
        __riscv_vse32_v_u32m1(hi + i, vhi, vl);
        i += vl;
    }
}

/*
 * Forward butterfly, 2 sources:
 *   lo[0..31] ^= t1[0..31] ^ t2[0..31];
 *   hi[0..31] ^= lo[0..31];          (uses updated lo)
 */
static __attribute__((always_inline)) inline
void rvv_btf_xor32_2src(uint32_t * restrict lo, uint32_t * restrict hi,
                          const uint32_t * restrict t1,
                          const uint32_t * restrict t2)
{
    size_t i = 0;
    while (i < 32) {
        size_t vl = __riscv_vsetvl_e32m1(32 - i);
        vuint32m1_t vlo = __riscv_vle32_v_u32m1(lo + i, vl);
        vuint32m1_t vt1 = __riscv_vle32_v_u32m1(t1 + i, vl);
        vuint32m1_t vt2 = __riscv_vle32_v_u32m1(t2 + i, vl);
        vuint32m1_t vt  = __riscv_vxor_vv_u32m1(vt1, vt2, vl);
        vlo = __riscv_vxor_vv_u32m1(vlo, vt, vl);
        __riscv_vse32_v_u32m1(lo + i, vlo, vl);
        vuint32m1_t vhi = __riscv_vle32_v_u32m1(hi + i, vl);
        vhi = __riscv_vxor_vv_u32m1(vhi, vlo, vl);
        __riscv_vse32_v_u32m1(hi + i, vhi, vl);
        i += vl;
    }
}

/*
 * Inverse butterfly, step A: hi[0..31] ^= lo[0..31]
 */
static __attribute__((always_inline)) inline
void rvv_ibtf_xor32_hi(const uint32_t * restrict lo, uint32_t * restrict hi)
{
    size_t i = 0;
    while (i < 32) {
        size_t vl = __riscv_vsetvl_e32m1(32 - i);
        vuint32m1_t vhi = __riscv_vle32_v_u32m1(hi + i, vl);
        vuint32m1_t vlo = __riscv_vle32_v_u32m1(lo + i, vl);
        vhi = __riscv_vxor_vv_u32m1(vhi, vlo, vl);
        __riscv_vse32_v_u32m1(hi + i, vhi, vl);
        i += vl;
    }
}

/*
 * Inverse butterfly, step B, 1 source: lo[0..31] ^= t[0..31]
 */
static __attribute__((always_inline)) inline
void rvv_ibtf_xor32_lo_1src(uint32_t * restrict lo,
                              const uint32_t * restrict t)
{
    size_t i = 0;
    while (i < 32) {
        size_t vl = __riscv_vsetvl_e32m1(32 - i);
        vuint32m1_t vlo = __riscv_vle32_v_u32m1(lo + i, vl);
        vuint32m1_t vt = __riscv_vle32_v_u32m1(t + i, vl);
        vlo = __riscv_vxor_vv_u32m1(vlo, vt, vl);
        __riscv_vse32_v_u32m1(lo + i, vlo, vl);
        i += vl;
    }
}

/*
 * Inverse butterfly, step B, 2 sources: lo[0..31] ^= t1[0..31] ^ t2[0..31]
 */
static __attribute__((always_inline)) inline
void rvv_ibtf_xor32_lo_2src(uint32_t * restrict lo,
                              const uint32_t * restrict t1,
                              const uint32_t * restrict t2)
{
    size_t i = 0;
    while (i < 32) {
        size_t vl = __riscv_vsetvl_e32m1(32 - i);
        vuint32m1_t vlo = __riscv_vle32_v_u32m1(lo + i, vl);
        vuint32m1_t vt1 = __riscv_vle32_v_u32m1(t1 + i, vl);
        vuint32m1_t vt2 = __riscv_vle32_v_u32m1(t2 + i, vl);
        vuint32m1_t vt = __riscv_vxor_vv_u32m1(vt1, vt2, vl);
        vlo = __riscv_vxor_vv_u32m1(vlo, vt, vl);
        __riscv_vse32_v_u32m1(lo + i, vlo, vl);
        i += vl;
    }
}

/* ── Iter 3: SWAPMOVE permutation helpers ───────────────────────────────── */

/*
 * RVV_SWAPMOVE32 — vectorised bit-interleave for 32 uint32_t elements.
 *
 * Implements the scalar SWAPMOVE_ASM macro on 32 elements simultaneously:
 *   tmp = (b ^ (a >> shift)) & mask;
 *   b ^= tmp;
 *   a ^= (tmp << shift);
 *
 * Used by btfy_layer_s0..s4 and ibtfy_layer_s0..s4 with five (mask, shift)
 * pairs: (0x55555555,1), (0x33333333,2), (0x0f0f0f0f,4), (0x00ff00ff,8),
 * (0x0000ffff,16).
 *
 * Spike ISS: replaces 32×5 = 160 scalar instructions with 9 vector instructions.
 */
#define RVV_SWAPMOVE32(a_ptr, b_ptr, mask_val, shift_val) do { \
    size_t _i = 0; \
    while (_i < 32) { \
        size_t _vl = __riscv_vsetvl_e32m1(32 - _i); \
        vuint32m1_t _va = __riscv_vle32_v_u32m1((a_ptr) + _i, _vl); \
        vuint32m1_t _vb = __riscv_vle32_v_u32m1((b_ptr) + _i, _vl); \
        vuint32m1_t _vt = __riscv_vand_vx_u32m1( \
            __riscv_vxor_vv_u32m1(_vb, \
                __riscv_vsrl_vx_u32m1(_va, (uint32_t)(shift_val), _vl), _vl), \
            (uint32_t)(mask_val), _vl); \
        _vb = __riscv_vxor_vv_u32m1(_vb, _vt, _vl); \
        _va = __riscv_vxor_vv_u32m1(_va, \
            __riscv_vsll_vx_u32m1(_vt, (uint32_t)(shift_val), _vl), _vl); \
        __riscv_vse32_v_u32m1((a_ptr) + _i, _va, _vl); \
        __riscv_vse32_v_u32m1((b_ptr) + _i, _vb, _vl); \
        _i += _vl; \
    } \
} while(0)

#endif /* BTFY_RVV_H */
