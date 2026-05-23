/* src/common/vlen_config.h
 *
 * VLEN-Adaptive LMUL Selection Header
 *
 * Centralizes all VLEN→LMUL mappings for the HQC-RISC-V implementation.
 * Provides type aliases and intrinsic wrapper macros that expand to the
 * correct RVV intrinsic calls for the compile-time-selected LMUL.
 *
 * The LMUL mapping ensures VL = VLEN × LMUL / 64 = 16 elements (the
 * Karatsuba threshold) at every supported VLEN for the GF2x leaf kernel:
 *
 *   VLEN=256  → LMUL=m4  (4×256/64 = 16)
 *   VLEN=512  → LMUL=m2  (2×512/64 = 16)
 *   VLEN=1024 → LMUL=m1  (1×1024/64 = 16)
 *   VLEN=2048 → LMUL=mf2 (0.5×2048/64 = 16)
 *
 * All selection is resolved at compile time via #if/#elif preprocessor
 * conditionals — zero runtime overhead.
 */

#ifndef HQC_VLEN_CONFIG_H
#define HQC_VLEN_CONFIG_H

#include <riscv_vector.h>

/* ══════════════════════════════════════════════════════════════════════════
 * VLEN Resolution
 * ══════════════════════════════════════════════════════════════════════════
 * Priority: HQC_VECTOR_BITS > HQC_VLEN > default (256)
 * This preserves backward compatibility: if neither is defined, we fall
 * back to LMUL=4 which matches the original hardcoded implementation.
 */
#if defined(HQC_VECTOR_BITS)
#  define HQC_VLEN_BITS  HQC_VECTOR_BITS
#elif defined(HQC_VLEN)
#  define HQC_VLEN_BITS  HQC_VLEN
#else
#  define HQC_VLEN_BITS  256   /* backward-compatible default */
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * Validation
 * ══════════════════════════════════════════════════════════════════════════ */

/* GF2x minimum: need VL >= 16 at SEW=64, max LMUL=8 → VLEN >= 128 */
#if HQC_VLEN_BITS < 128
#  error "VLEN < 128 is unsupported: cannot achieve VL>=16 at SEW=64 with any LMUL."
#endif

/* RS decoder minimum: requires VLEN >= 256 */
#if HQC_VLEN_BITS < 256
#  error "Minimum supported VLEN for RS_Decoder is 256."
#endif

/* Supported VLEN set */
#if HQC_VLEN_BITS != 256 && HQC_VLEN_BITS != 512 && \
    HQC_VLEN_BITS != 1024 && HQC_VLEN_BITS != 2048
#  error "Unsupported HQC_VLEN_BITS value. Must be 256, 512, 1024, or 2048."
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * GF2x Leaf LMUL Selection
 * ══════════════════════════════════════════════════════════════════════════
 * Requirement: VL >= 16 elements at SEW=64.
 * Formula: VL = VLEN × LMUL / SEW.  Minimum LMUL = 16 × 64 / VLEN.
 */

#if HQC_VLEN_BITS == 256

#  define GF2X_LMUL  4
   typedef vuint64m4_t  gf2x_vec_t;

#  define gf2x_vsetvl(n)          __riscv_vsetvl_e64m4(n)
#  define gf2x_vle64(ptr, vl)     __riscv_vle64_v_u64m4((ptr), (vl))
#  define gf2x_vse64(ptr, v, vl)  __riscv_vse64_v_u64m4((ptr), (v), (vl))
#  define gf2x_vmv_v_x(x, vl)    __riscv_vmv_v_x_u64m4((x), (vl))
#  define gf2x_vclmul_vv(a, b, vl)  __riscv_vclmul_vv_u64m4((a), (b), (vl))
#  define gf2x_vclmulh_vv(a, b, vl) __riscv_vclmulh_vv_u64m4((a), (b), (vl))
#  define gf2x_vxor_vv(a, b, vl)    __riscv_vxor_vv_u64m4((a), (b), (vl))

#elif HQC_VLEN_BITS == 512

#  define GF2X_LMUL  2
   typedef vuint64m2_t  gf2x_vec_t;

#  define gf2x_vsetvl(n)          __riscv_vsetvl_e64m2(n)
#  define gf2x_vle64(ptr, vl)     __riscv_vle64_v_u64m2((ptr), (vl))
#  define gf2x_vse64(ptr, v, vl)  __riscv_vse64_v_u64m2((ptr), (v), (vl))
#  define gf2x_vmv_v_x(x, vl)    __riscv_vmv_v_x_u64m2((x), (vl))
#  define gf2x_vclmul_vv(a, b, vl)  __riscv_vclmul_vv_u64m2((a), (b), (vl))
#  define gf2x_vclmulh_vv(a, b, vl) __riscv_vclmulh_vv_u64m2((a), (b), (vl))
#  define gf2x_vxor_vv(a, b, vl)    __riscv_vxor_vv_u64m2((a), (b), (vl))

#elif HQC_VLEN_BITS == 1024

#  define GF2X_LMUL  1
   typedef vuint64m1_t  gf2x_vec_t;

#  define gf2x_vsetvl(n)          __riscv_vsetvl_e64m1(n)
#  define gf2x_vle64(ptr, vl)     __riscv_vle64_v_u64m1((ptr), (vl))
#  define gf2x_vse64(ptr, v, vl)  __riscv_vse64_v_u64m1((ptr), (v), (vl))
#  define gf2x_vmv_v_x(x, vl)    __riscv_vmv_v_x_u64m1((x), (vl))
#  define gf2x_vclmul_vv(a, b, vl)  __riscv_vclmul_vv_u64m1((a), (b), (vl))
#  define gf2x_vclmulh_vv(a, b, vl) __riscv_vclmulh_vv_u64m1((a), (b), (vl))
#  define gf2x_vxor_vv(a, b, vl)    __riscv_vxor_vv_u64m1((a), (b), (vl))

#elif HQC_VLEN_BITS == 2048

/*
 * At VLEN=2048 with SEW=64, the minimum valid LMUL is m1 (since LMUL >=
 * SEW/ELEN = 64/64 = 1; vuint64mf2_t does not exist in the RVV ISA).
 * We use LMUL=m1 which gives VLMAX=32, but vsetvl(16) will return 16,
 * achieving the same VL=16 Karatsuba threshold with only 1 physical
 * register per group — equivalent register pressure to a hypothetical mf2.
 */
#  define GF2X_LMUL  1
#  define GF2X_LMUL_FRAC  2   /* logical mf2 intent for documentation */
   typedef vuint64m1_t  gf2x_vec_t;

#  define gf2x_vsetvl(n)          __riscv_vsetvl_e64m1(n)
#  define gf2x_vle64(ptr, vl)     __riscv_vle64_v_u64m1((ptr), (vl))
#  define gf2x_vse64(ptr, v, vl)  __riscv_vse64_v_u64m1((ptr), (v), (vl))
#  define gf2x_vmv_v_x(x, vl)    __riscv_vmv_v_x_u64m1((x), (vl))
#  define gf2x_vclmul_vv(a, b, vl)  __riscv_vclmul_vv_u64m1((a), (b), (vl))
#  define gf2x_vclmulh_vv(a, b, vl) __riscv_vclmulh_vv_u64m1((a), (b), (vl))
#  define gf2x_vxor_vv(a, b, vl)    __riscv_vxor_vv_u64m1((a), (b), (vl))

#endif /* HQC_VLEN_BITS GF2x selection */

/* ══════════════════════════════════════════════════════════════════════════
 * RS Decoder LMUL Selection
 * ══════════════════════════════════════════════════════════════════════════
 * Requirement: VL >= PARAM_DELTA elements at SEW=64.
 * PARAM_DELTA max = 29 (HQC-256).
 *
 * At VLEN 256/512/1024: same LMUL as GF2x (VL=16), two passes for DELTA=29.
 * At VLEN 2048: LMUL=1 gives VL=32, covering all PARAM_DELTA in one pass.
 *
 * Narrowing chain (u64→u32→u16) for vnsrl:
 *   VLEN=256:  u64m4  → u32m2  → u16m1
 *   VLEN=512:  u64m2  → u32m1  → u16mf2
 *   VLEN=1024: u64m1  → u32mf2 → u16mf4
 *   VLEN=2048: u64m1  → u32mf2 → u16mf4
 */

#if HQC_VLEN_BITS == 256

#  define RS_LMUL  4
   typedef vuint64m4_t   rs_vec_t;
   typedef vuint32m2_t   rs_vec_u32_t;
   typedef vuint16m1_t   rs_vec_u16_t;
   typedef vuint8mf2_t   rs_vec_u8_t;

   /* Blend loop (Loop 2) uses u16m4 for integer SIMD */
   typedef vuint16m4_t   rs_blend_vec_t;

#  define rs_vsetvl(n)              __riscv_vsetvl_e64m4(n)
#  define rs_blend_vsetvl(n)        __riscv_vsetvl_e16m4(n)
#  define rs_vclmul_vv(a, b, vl)    __riscv_vclmul_vv_u64m4((a), (b), (vl))
#  define rs_vzext_u16_to_u64(v, vl) __riscv_vzext_vf4_u64m4((v), (vl))
#  define rs_vzext_u8_to_u64(v, vl)  __riscv_vzext_vf8_u64m4((v), (vl))
#  define rs_vnsrl_u32(v, shift, vl) __riscv_vnsrl_wx_u32m2((v), (shift), (vl))
#  define rs_vnsrl_u16(v, shift, vl) __riscv_vnsrl_wx_u16m1((v), (shift), (vl))
#  define rs_vredxor(v, s, vl)       __riscv_vredxor_vs_u64m4_u64m1((v), (s), (vl))
#  define rs_vmv_v_x(x, vl)         __riscv_vmv_v_x_u64m4((x), (vl))
#  define rs_vxor_vv(a, b, vl)      __riscv_vxor_vv_u64m4((a), (b), (vl))
#  define rs_vsrl_vx(v, shift, vl)  __riscv_vsrl_vx_u64m4((v), (shift), (vl))
#  define rs_vand_vx(v, imm, vl)    __riscv_vand_vx_u64m4((v), (imm), (vl))
#  define rs_vsll_vx(v, shift, vl)  __riscv_vsll_vx_u64m4((v), (shift), (vl))
#  define rs_vle16(ptr, vl)         __riscv_vle16_v_u16m1((ptr), (vl))
#  define rs_vse16(ptr, v, vl)      __riscv_vse16_v_u16m1((ptr), (v), (vl))
#  define rs_vle8(ptr, vl)          __riscv_vle8_v_u8mf2((ptr), (vl))
#  define rs_vlse16(ptr, stride, vl) __riscv_vlse16_v_u16m1((ptr), (stride), (vl))

#elif HQC_VLEN_BITS == 512

#  define RS_LMUL  2
   typedef vuint64m2_t   rs_vec_t;
   typedef vuint32m1_t   rs_vec_u32_t;
   typedef vuint16mf2_t  rs_vec_u16_t;
   typedef vuint8mf4_t   rs_vec_u8_t;

   /* Blend loop (Loop 2) uses u16m4 for integer SIMD */
   typedef vuint16m4_t   rs_blend_vec_t;

#  define rs_vsetvl(n)              __riscv_vsetvl_e64m2(n)
#  define rs_blend_vsetvl(n)        __riscv_vsetvl_e16m4(n)
#  define rs_vclmul_vv(a, b, vl)    __riscv_vclmul_vv_u64m2((a), (b), (vl))
#  define rs_vzext_u16_to_u64(v, vl) __riscv_vzext_vf4_u64m2((v), (vl))
#  define rs_vzext_u8_to_u64(v, vl)  __riscv_vzext_vf8_u64m2((v), (vl))
#  define rs_vnsrl_u32(v, shift, vl) __riscv_vnsrl_wx_u32m1((v), (shift), (vl))
#  define rs_vnsrl_u16(v, shift, vl) __riscv_vnsrl_wx_u16mf2((v), (shift), (vl))
#  define rs_vredxor(v, s, vl)       __riscv_vredxor_vs_u64m2_u64m1((v), (s), (vl))
#  define rs_vmv_v_x(x, vl)         __riscv_vmv_v_x_u64m2((x), (vl))
#  define rs_vxor_vv(a, b, vl)      __riscv_vxor_vv_u64m2((a), (b), (vl))
#  define rs_vsrl_vx(v, shift, vl)  __riscv_vsrl_vx_u64m2((v), (shift), (vl))
#  define rs_vand_vx(v, imm, vl)    __riscv_vand_vx_u64m2((v), (imm), (vl))
#  define rs_vsll_vx(v, shift, vl)  __riscv_vsll_vx_u64m2((v), (shift), (vl))
#  define rs_vle16(ptr, vl)         __riscv_vle16_v_u16mf2((ptr), (vl))
#  define rs_vse16(ptr, v, vl)      __riscv_vse16_v_u16mf2((ptr), (v), (vl))
#  define rs_vle8(ptr, vl)          __riscv_vle8_v_u8mf4((ptr), (vl))
#  define rs_vlse16(ptr, stride, vl) __riscv_vlse16_v_u16mf2((ptr), (stride), (vl))

#elif HQC_VLEN_BITS == 1024

#  define RS_LMUL  1
   typedef vuint64m1_t   rs_vec_t;
   typedef vuint32mf2_t  rs_vec_u32_t;
   typedef vuint16mf4_t  rs_vec_u16_t;
   typedef vuint8mf8_t   rs_vec_u8_t;

   /* Blend loop (Loop 2) uses u16m4 for integer SIMD */
   typedef vuint16m4_t   rs_blend_vec_t;

#  define rs_vsetvl(n)              __riscv_vsetvl_e64m1(n)
#  define rs_blend_vsetvl(n)        __riscv_vsetvl_e16m4(n)
#  define rs_vclmul_vv(a, b, vl)    __riscv_vclmul_vv_u64m1((a), (b), (vl))
#  define rs_vzext_u16_to_u64(v, vl) __riscv_vzext_vf4_u64m1((v), (vl))
#  define rs_vzext_u8_to_u64(v, vl)  __riscv_vzext_vf8_u64m1((v), (vl))
#  define rs_vnsrl_u32(v, shift, vl) __riscv_vnsrl_wx_u32mf2((v), (shift), (vl))
#  define rs_vnsrl_u16(v, shift, vl) __riscv_vnsrl_wx_u16mf4((v), (shift), (vl))
#  define rs_vredxor(v, s, vl)       __riscv_vredxor_vs_u64m1_u64m1((v), (s), (vl))
#  define rs_vmv_v_x(x, vl)         __riscv_vmv_v_x_u64m1((x), (vl))
#  define rs_vxor_vv(a, b, vl)      __riscv_vxor_vv_u64m1((a), (b), (vl))
#  define rs_vsrl_vx(v, shift, vl)  __riscv_vsrl_vx_u64m1((v), (shift), (vl))
#  define rs_vand_vx(v, imm, vl)    __riscv_vand_vx_u64m1((v), (imm), (vl))
#  define rs_vsll_vx(v, shift, vl)  __riscv_vsll_vx_u64m1((v), (shift), (vl))
#  define rs_vle16(ptr, vl)         __riscv_vle16_v_u16mf4((ptr), (vl))
#  define rs_vse16(ptr, v, vl)      __riscv_vse16_v_u16mf4((ptr), (v), (vl))
#  define rs_vle8(ptr, vl)          __riscv_vle8_v_u8mf8((ptr), (vl))
#  define rs_vlse16(ptr, stride, vl) __riscv_vlse16_v_u16mf4((ptr), (stride), (vl))

#elif HQC_VLEN_BITS == 2048

/*
 * At VLEN=2048 with SEW=64, LMUL=1 gives VL=32 which covers all
 * PARAM_DELTA values (max 29) in a single pass.  The narrowing chain
 * is the same as VLEN=1024: u64m1 → u32mf2 → u16mf4.
 */
#  define RS_LMUL  1
   typedef vuint64m1_t   rs_vec_t;
   typedef vuint32mf2_t  rs_vec_u32_t;
   typedef vuint16mf4_t  rs_vec_u16_t;
   typedef vuint8mf8_t   rs_vec_u8_t;

   /* Blend loop (Loop 2) uses u16m4 for integer SIMD */
   typedef vuint16m4_t   rs_blend_vec_t;

#  define rs_vsetvl(n)              __riscv_vsetvl_e64m1(n)
#  define rs_blend_vsetvl(n)        __riscv_vsetvl_e16m4(n)
#  define rs_vclmul_vv(a, b, vl)    __riscv_vclmul_vv_u64m1((a), (b), (vl))
#  define rs_vzext_u16_to_u64(v, vl) __riscv_vzext_vf4_u64m1((v), (vl))
#  define rs_vzext_u8_to_u64(v, vl)  __riscv_vzext_vf8_u64m1((v), (vl))
#  define rs_vnsrl_u32(v, shift, vl) __riscv_vnsrl_wx_u32mf2((v), (shift), (vl))
#  define rs_vnsrl_u16(v, shift, vl) __riscv_vnsrl_wx_u16mf4((v), (shift), (vl))
#  define rs_vredxor(v, s, vl)       __riscv_vredxor_vs_u64m1_u64m1((v), (s), (vl))
#  define rs_vmv_v_x(x, vl)         __riscv_vmv_v_x_u64m1((x), (vl))
#  define rs_vxor_vv(a, b, vl)      __riscv_vxor_vv_u64m1((a), (b), (vl))
#  define rs_vsrl_vx(v, shift, vl)  __riscv_vsrl_vx_u64m1((v), (shift), (vl))
#  define rs_vand_vx(v, imm, vl)    __riscv_vand_vx_u64m1((v), (imm), (vl))
#  define rs_vsll_vx(v, shift, vl)  __riscv_vsll_vx_u64m1((v), (shift), (vl))
#  define rs_vle16(ptr, vl)         __riscv_vle16_v_u16mf4((ptr), (vl))
#  define rs_vse16(ptr, v, vl)      __riscv_vse16_v_u16mf4((ptr), (v), (vl))
#  define rs_vle8(ptr, vl)          __riscv_vle8_v_u8mf8((ptr), (vl))
#  define rs_vlse16(ptr, stride, vl) __riscv_vlse16_v_u16mf4((ptr), (stride), (vl))

#endif /* HQC_VLEN_BITS RS selection */

/* ══════════════════════════════════════════════════════════════════════════
 * Keccak Multi-Buffering Parallelism
 * ══════════════════════════════════════════════════════════════════════════
 * N = VLEN / 128 independent hash states processed per permutation call.
 * The existing x2 implementation uses VL=2 at VLEN=256 (2 of 4 available
 * lanes at LMUL=m1, SEW=64 → 50% utilization).  Scaling by VLEN/128
 * maintains ~50% lane utilization at all VLENs:
 *
 *   VLEN=256  → N=2  (x2,  2/4  lanes = 50%)
 *   VLEN=512  → N=4  (x4,  4/8  lanes = 50%)
 *   VLEN=1024 → N=8  (x8,  8/16 lanes = 50%)
 *   VLEN=2048 → N=16 (x16, 16/32 lanes = 50%)
 */
#define KECCAK_XN_PARALLELISM  (HQC_VLEN_BITS / 128)

#endif /* HQC_VLEN_CONFIG_H */
