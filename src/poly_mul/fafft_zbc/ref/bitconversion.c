#include "parameters.h"
#include "bitconversion.h"
#include <stddef.h>
#include <stdint.h>
#include <riscv_vector.h>
#include "bc_rvv.h"


void bc_1_256(uint32_t *poly, unsigned n_256bit)
{
	ptrdiff_t stride = 8 * sizeof(uint32_t);
	unsigned b = 0;
	while (b < n_256bit) {
		size_t vl = __riscv_vsetvl_e32m4(n_256bit - b);
		uint32_t *blk = poly + b * 8;
		vuint32m4_t vp0 = __riscv_vlse32_v_u32m4(blk + 0, stride, vl);
		vuint32m4_t vp1 = __riscv_vlse32_v_u32m4(blk + 1, stride, vl);
		vuint32m4_t vp2 = __riscv_vlse32_v_u32m4(blk + 2, stride, vl);
		vuint32m4_t vp3 = __riscv_vlse32_v_u32m4(blk + 3, stride, vl);
		vuint32m4_t vp4 = __riscv_vlse32_v_u32m4(blk + 4, stride, vl);
		vuint32m4_t vp5 = __riscv_vlse32_v_u32m4(blk + 5, stride, vl);
		vuint32m4_t vp6 = __riscv_vlse32_v_u32m4(blk + 6, stride, vl);
		vuint32m4_t vp7 = __riscv_vlse32_v_u32m4(blk + 7, stride, vl);
		//layer 13: [256] s4^8:(128,8) suggest unit:1
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(vp7, 24, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp7, 8, vl), __riscv_vsrl_vx_u32m4(vp6, 24, vl), vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp6, 8, vl), __riscv_vsrl_vx_u32m4(vp5, 24, vl), vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp5, 8, vl), __riscv_vsrl_vx_u32m4(vp4, 24, vl), vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp4, 8, vl), vl);
		//layer 14: [128] s4^4:(64,4) suggest unit:1
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(vp3, 28, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp3, 4, vl), __riscv_vsrl_vx_u32m4(vp2, 28, vl), vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp2, 4, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(vp7, 28, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp7, 4, vl), __riscv_vsrl_vx_u32m4(vp6, 28, vl), vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp6, 4, vl), vl);
		//layer 15: [64] s4^2:(32,2) suggest unit:1
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(vp1, 30, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(vp3, 30, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(vp5, 30, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(vp7, 30, vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp1, 2, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsll_vx_u32m4(vp3, 2, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp5, 2, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsll_vx_u32m4(vp7, 2, vl), vl);
		//layer 16: [32] s4^1:(16,1) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x80000000u, vl), 15, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x80000000u, vl), 15, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x80000000u, vl), 15, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x80000000u, vl), 15, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x80000000u, vl), 15, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x80000000u, vl), 15, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x80000000u, vl), 15, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x80000000u, vl), 15, vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x7fff0000u, vl), 15, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x7fff0000u, vl), 15, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x7fff0000u, vl), 15, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x7fff0000u, vl), 15, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x7fff0000u, vl), 15, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x7fff0000u, vl), 15, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x7fff0000u, vl), 15, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x7fff0000u, vl), 15, vl), vl);
		//layer 17: [256] s2^2:(128,32) suggest unit:16
		vp4 = __riscv_vxor_vv_u32m4(vp4, vp7, vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, vp6, vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, vp5, vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, vp4, vl);
		//layer 18: [128] s2^1:(64,16) suggest unit:16
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(vp3, 16, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp3, 16, vl), __riscv_vsrl_vx_u32m4(vp2, 16, vl), vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp2, 16, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(vp7, 16, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp7, 16, vl), __riscv_vsrl_vx_u32m4(vp6, 16, vl), vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp6, 16, vl), vl);
		//layer 19: [256] s1^1:(128,64) suggest unit:64
		vp5 = __riscv_vxor_vv_u32m4(vp5, vp7, vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, vp6, vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, vp5, vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, vp4, vl);
		//layer 20: [64] s1^1:(32,16) suggest unit:16
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(vp1, 16, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(vp3, 16, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(vp5, 16, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(vp7, 16, vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp1, 16, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsll_vx_u32m4(vp3, 16, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp5, 16, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsll_vx_u32m4(vp7, 16, vl), vl);
		//layer 21: [16] s2^2:(8,2) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0xc000c000u, vl), 6, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0xc000c000u, vl), 6, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0xc000c000u, vl), 6, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0xc000c000u, vl), 6, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0xc000c000u, vl), 6, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0xc000c000u, vl), 6, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0xc000c000u, vl), 6, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0xc000c000u, vl), 6, vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x3f003f00u, vl), 6, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x3f003f00u, vl), 6, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x3f003f00u, vl), 6, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x3f003f00u, vl), 6, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x3f003f00u, vl), 6, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x3f003f00u, vl), 6, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x3f003f00u, vl), 6, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x3f003f00u, vl), 6, vl), vl);
		//layer 22: [8] s2^1:(4,1) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x80808080u, vl), 3, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x80808080u, vl), 3, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x80808080u, vl), 3, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x80808080u, vl), 3, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x80808080u, vl), 3, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x80808080u, vl), 3, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x80808080u, vl), 3, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x80808080u, vl), 3, vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x70707070u, vl), 3, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x70707070u, vl), 3, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x70707070u, vl), 3, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x70707070u, vl), 3, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x70707070u, vl), 3, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x70707070u, vl), 3, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x70707070u, vl), 3, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x70707070u, vl), 3, vl), vl);
		//layer 23: [16] s1^1:(8,4) suggest unit:4
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0xf000f000u, vl), 4, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0xf000f000u, vl), 4, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0xf000f000u, vl), 4, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0xf000f000u, vl), 4, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0xf000f000u, vl), 4, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0xf000f000u, vl), 4, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0xf000f000u, vl), 4, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0xf000f000u, vl), 4, vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x0f000f00u, vl), 4, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x0f000f00u, vl), 4, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x0f000f00u, vl), 4, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x0f000f00u, vl), 4, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x0f000f00u, vl), 4, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x0f000f00u, vl), 4, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x0f000f00u, vl), 4, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x0f000f00u, vl), 4, vl), vl);
		//layer 24: [4] s1^1:(2,1) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x88888888u, vl), 1, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x88888888u, vl), 1, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x88888888u, vl), 1, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x88888888u, vl), 1, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x88888888u, vl), 1, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x88888888u, vl), 1, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x88888888u, vl), 1, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x88888888u, vl), 1, vl), vl);
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0x44444444u, vl), 1, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0x44444444u, vl), 1, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0x44444444u, vl), 1, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0x44444444u, vl), 1, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0x44444444u, vl), 1, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0x44444444u, vl), 1, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0x44444444u, vl), 1, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0x44444444u, vl), 1, vl), vl);
		__riscv_vsse32_v_u32m4(blk + 0, stride, vp0, vl);
		__riscv_vsse32_v_u32m4(blk + 1, stride, vp1, vl);
		__riscv_vsse32_v_u32m4(blk + 2, stride, vp2, vl);
		__riscv_vsse32_v_u32m4(blk + 3, stride, vp3, vl);
		__riscv_vsse32_v_u32m4(blk + 4, stride, vp4, vl);
		__riscv_vsse32_v_u32m4(blk + 5, stride, vp5, vl);
		__riscv_vsse32_v_u32m4(blk + 6, stride, vp6, vl);
		__riscv_vsse32_v_u32m4(blk + 7, stride, vp7, vl);
		b += vl;
	}
}

void ibc_1_256(uint32_t *poly, unsigned n_256bit)
{
	ptrdiff_t stride = 8 * sizeof(uint32_t);
	unsigned b = 0;
	while (b < n_256bit) {
		size_t vl = __riscv_vsetvl_e32m4(n_256bit - b);
		uint32_t *blk = poly + b * 8;
		vuint32m4_t vp0 = __riscv_vlse32_v_u32m4(blk + 0, stride, vl);
		vuint32m4_t vp1 = __riscv_vlse32_v_u32m4(blk + 1, stride, vl);
		vuint32m4_t vp2 = __riscv_vlse32_v_u32m4(blk + 2, stride, vl);
		vuint32m4_t vp3 = __riscv_vlse32_v_u32m4(blk + 3, stride, vl);
		vuint32m4_t vp4 = __riscv_vlse32_v_u32m4(blk + 4, stride, vl);
		vuint32m4_t vp5 = __riscv_vlse32_v_u32m4(blk + 5, stride, vl);
		vuint32m4_t vp6 = __riscv_vlse32_v_u32m4(blk + 6, stride, vl);
		vuint32m4_t vp7 = __riscv_vlse32_v_u32m4(blk + 7, stride, vl);
		//layer 7: [4] s1^1:(2,1) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0xccccccccu, vl), 1, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0xccccccccu, vl), 1, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0xccccccccu, vl), 1, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0xccccccccu, vl), 1, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0xccccccccu, vl), 1, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0xccccccccu, vl), 1, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0xccccccccu, vl), 1, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0xccccccccu, vl), 1, vl), vl);
		//layer 6: [16] s1^1:(8,4) suggest unit:4
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0xff00ff00u, vl), 4, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0xff00ff00u, vl), 4, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0xff00ff00u, vl), 4, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0xff00ff00u, vl), 4, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0xff00ff00u, vl), 4, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0xff00ff00u, vl), 4, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0xff00ff00u, vl), 4, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0xff00ff00u, vl), 4, vl), vl);
		//layer 5: [8] s2^1:(4,1) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0xf0f0f0f0u, vl), 3, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0xf0f0f0f0u, vl), 3, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0xf0f0f0f0u, vl), 3, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0xf0f0f0f0u, vl), 3, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0xf0f0f0f0u, vl), 3, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0xf0f0f0f0u, vl), 3, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0xf0f0f0f0u, vl), 3, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0xf0f0f0f0u, vl), 3, vl), vl);
		//layer 4: [16] s2^2:(8,2) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0xff00ff00u, vl), 6, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0xff00ff00u, vl), 6, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0xff00ff00u, vl), 6, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0xff00ff00u, vl), 6, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0xff00ff00u, vl), 6, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0xff00ff00u, vl), 6, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0xff00ff00u, vl), 6, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0xff00ff00u, vl), 6, vl), vl);
		//layer 3: [64] s1^1:(32,16) suggest unit:16
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp1, 16, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsll_vx_u32m4(vp3, 16, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp5, 16, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsll_vx_u32m4(vp7, 16, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(vp1, 16, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(vp3, 16, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(vp5, 16, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(vp7, 16, vl), vl);
		//layer 2: [256] s1^1:(128,64) suggest unit:64
		vp3 = __riscv_vxor_vv_u32m4(vp3, vp5, vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, vp4, vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, vp7, vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, vp6, vl);
		//layer 1: [128] s2^1:(64,16) suggest unit:16
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp2, 16, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp3, 16, vl), __riscv_vsrl_vx_u32m4(vp2, 16, vl), vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(vp3, 16, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp6, 16, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp7, 16, vl), __riscv_vsrl_vx_u32m4(vp6, 16, vl), vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(vp7, 16, vl), vl);
		//layer 0: [256] s2^2:(128,32) suggest unit:16
		vp1 = __riscv_vxor_vv_u32m4(vp1, vp4, vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, vp5, vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, vp6, vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, vp7, vl);
		//layer 16: [32] s4^1:(16,1) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp0, 0xffff0000u, vl), 15, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp1, 0xffff0000u, vl), 15, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp2, 0xffff0000u, vl), 15, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp3, 0xffff0000u, vl), 15, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp4, 0xffff0000u, vl), 15, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp5, 0xffff0000u, vl), 15, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp6, 0xffff0000u, vl), 15, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(__riscv_vand_vx_u32m4(vp7, 0xffff0000u, vl), 15, vl), vl);
		//layer 15: [64] s4^2:(32,2) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp1, 2, vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsll_vx_u32m4(vp3, 2, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp5, 2, vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsll_vx_u32m4(vp7, 2, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vsrl_vx_u32m4(vp1, 30, vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vsrl_vx_u32m4(vp3, 30, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vsrl_vx_u32m4(vp5, 30, vl), vl);
		vp7 = __riscv_vxor_vv_u32m4(vp7, __riscv_vsrl_vx_u32m4(vp7, 30, vl), vl);
		//layer 14: [128] s4^4:(64,4) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp2, 4, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp3, 4, vl), __riscv_vsrl_vx_u32m4(vp2, 28, vl), vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vsrl_vx_u32m4(vp3, 28, vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsll_vx_u32m4(vp6, 4, vl), vl);
		vp5 = __riscv_vxor_vv_u32m4(vp5, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp7, 4, vl), __riscv_vsrl_vx_u32m4(vp6, 28, vl), vl), vl);
		vp6 = __riscv_vxor_vv_u32m4(vp6, __riscv_vsrl_vx_u32m4(vp7, 28, vl), vl);
		//layer 13: [256] s4^8:(128,8) suggest unit:1
		vp0 = __riscv_vxor_vv_u32m4(vp0, __riscv_vsll_vx_u32m4(vp4, 8, vl), vl);
		vp1 = __riscv_vxor_vv_u32m4(vp1, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp5, 8, vl), __riscv_vsrl_vx_u32m4(vp4, 24, vl), vl), vl);
		vp2 = __riscv_vxor_vv_u32m4(vp2, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp6, 8, vl), __riscv_vsrl_vx_u32m4(vp5, 24, vl), vl), vl);
		vp3 = __riscv_vxor_vv_u32m4(vp3, __riscv_vxor_vv_u32m4(__riscv_vsll_vx_u32m4(vp7, 8, vl), __riscv_vsrl_vx_u32m4(vp6, 24, vl), vl), vl);
		vp4 = __riscv_vxor_vv_u32m4(vp4, __riscv_vsrl_vx_u32m4(vp7, 24, vl), vl);
		__riscv_vsse32_v_u32m4(blk + 0, stride, vp0, vl);
		__riscv_vsse32_v_u32m4(blk + 1, stride, vp1, vl);
		__riscv_vsse32_v_u32m4(blk + 2, stride, vp2, vl);
		__riscv_vsse32_v_u32m4(blk + 3, stride, vp3, vl);
		__riscv_vsse32_v_u32m4(blk + 4, stride, vp4, vl);
		__riscv_vsse32_v_u32m4(blk + 5, stride, vp5, vl);
		__riscv_vsse32_v_u32m4(blk + 6, stride, vp6, vl);
		__riscv_vsse32_v_u32m4(blk + 7, stride, vp7, vl);
		b += vl;
	}
}



/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////


static
void cvt_to_fft_256_16384(uint32_t *poly)
{
	const unsigned w = 16384;

	uint32_t *ptr;
	//layer 6: [16384] s4^2:(8192,512) suggest unit:256
#define DEG_H 8192
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 7: [8192] s4^1:(4096,256) suggest unit:256
#define DEG_H 4096
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 8: [16384] s1^1:(8192,4096) suggest unit:4096
#define DEG_H 8192
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 9: [4096] s2^2:(2048,512) suggest unit:256
#define DEG_H 2048
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 10: [2048] s2^1:(1024,256) suggest unit:256
#define DEG_H 1024
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 11: [4096] s1^1:(2048,1024) suggest unit:1024
#define DEG_H 2048
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 12: [1024] s1^1:(512,256) suggest unit:256
#define DEG_H 512
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

}


static
void icvt_to_poly_256_16384(uint32_t *poly)
{
	const unsigned w = 16384;

	uint32_t *ptr;

	//layer 12: [1024] s1^1:(512,256) suggest unit:256
#define DEG_H 512
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 11: [4096] s1^1:(2048,1024) suggest unit:1024
#define DEG_H 2048
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 10: [2048] s2^1:(1024,256) suggest unit:256
#define DEG_H 1024
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 9: [4096] s2^2:(2048,512) suggest unit:256
#define DEG_H 2048
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 8: [16384] s1^1:(8192,4096) suggest unit:4096
#define DEG_H 8192
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 7: [8192] s4^1:(4096,256) suggest unit:256
#define DEG_H 4096
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 6: [16384] s4^2:(8192,512) suggest unit:256
#define DEG_H 8192
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

}



static
void cvt_to_fft_256_32768(uint32_t *poly)
{
	const unsigned w = 32768;

	uint32_t *ptr;
	//layer 7: [32768] s4^4:(16384,1024) suggest unit:256
#define DEG_H 16384
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 8: [16384] s4^2:(8192,512) suggest unit:256
#define DEG_H 8192
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 9: [8192] s4^1:(4096,256) suggest unit:256
#define DEG_H 4096
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 10: [32768] s2^1:(16384,4096) suggest unit:4096
#define DEG_H 16384
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 11: [16384] s1^1:(8192,4096) suggest unit:4096
#define DEG_H 8192
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 12: [4096] s2^2:(2048,512) suggest unit:256
#define DEG_H 2048
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 13: [2048] s2^1:(1024,256) suggest unit:256
#define DEG_H 1024
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 14: [4096] s1^1:(2048,1024) suggest unit:1024
#define DEG_H 2048
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 15: [1024] s1^1:(512,256) suggest unit:256
#define DEG_H 512
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L
}


static
void icvt_to_poly_256_32768(uint32_t *poly)
{
	const unsigned w = 32768;

	uint32_t *ptr;

	//layer 15: [1024] s1^1:(512,256) suggest unit:256
#define DEG_H 512
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 14: [4096] s1^1:(2048,1024) suggest unit:1024
#define DEG_H 2048
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 13: [2048] s2^1:(1024,256) suggest unit:256
#define DEG_H 1024
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 12: [4096] s2^2:(2048,512) suggest unit:256
#define DEG_H 2048
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 11: [16384] s1^1:(8192,4096) suggest unit:4096
#define DEG_H 8192
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 10: [32768] s2^1:(16384,4096) suggest unit:4096
#define DEG_H 16384
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 9: [8192] s4^1:(4096,256) suggest unit:256
#define DEG_H 4096
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 8: [16384] s4^2:(8192,512) suggest unit:256
#define DEG_H 8192
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 7: [32768] s4^4:(16384,1024) suggest unit:256
#define DEG_H 16384
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

}


static
void cvt_to_fft_256_65536(uint32_t *poly)
{
	const unsigned w = 65536;

	uint32_t *ptr;
	//layer 8: [65536] s4^8:(32768,2048) suggest unit:256
#define DEG_H 32768
#define DEG_L 2048
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 9: [32768] s4^4:(16384,1024) suggest unit:256
#define DEG_H 16384
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 10: [16384] s4^2:(8192,512) suggest unit:256
#define DEG_H 8192
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 11: [8192] s4^1:(4096,256) suggest unit:256
#define DEG_H 4096
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 12: [65536] s2^2:(32768,8192) suggest unit:4096
#define DEG_H 32768
#define DEG_L 8192
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 13: [32768] s2^1:(16384,4096) suggest unit:4096
#define DEG_H 16384
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 14: [65536] s1^1:(32768,16384) suggest unit:16384
#define DEG_H 32768
#define DEG_L 16384
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 15: [16384] s1^1:(8192,4096) suggest unit:4096
#define DEG_H 8192
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 16: [4096] s2^2:(2048,512) suggest unit:256
#define DEG_H 2048
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 17: [2048] s2^1:(1024,256) suggest unit:256
#define DEG_H 1024
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 18: [4096] s1^1:(2048,1024) suggest unit:1024
#define DEG_H 2048
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 19: [1024] s1^1:(512,256) suggest unit:256
#define DEG_H 512
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L
}


static
void icvt_to_poly_256_65536(uint32_t *poly)
{
	const unsigned w = 65536;
	uint32_t *ptr;

	//layer 19: [1024] s1^1:(512,256) suggest unit:256
#define DEG_H 512
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 18: [4096] s1^1:(2048,1024) suggest unit:1024
#define DEG_H 2048
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 17: [2048] s2^1:(1024,256) suggest unit:256
#define DEG_H 1024
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 16: [4096] s2^2:(2048,512) suggest unit:256
#define DEG_H 2048
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 15: [16384] s1^1:(8192,4096) suggest unit:4096
#define DEG_H 8192
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 14: [65536] s1^1:(32768,16384) suggest unit:16384
#define DEG_H 32768
#define DEG_L 16384
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 13: [32768] s2^1:(16384,4096) suggest unit:4096
#define DEG_H 16384
#define DEG_L 4096
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 12: [65536] s2^2:(32768,8192) suggest unit:4096
#define DEG_H 32768
#define DEG_L 8192
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 11: [8192] s4^1:(4096,256) suggest unit:256
#define DEG_H 4096
#define DEG_L 256
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 10: [16384] s4^2:(8192,512) suggest unit:256
#define DEG_H 8192
#define DEG_L 512
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 9: [32768] s4^4:(16384,1024) suggest unit:256
#define DEG_H 16384
#define DEG_L 1024
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 8: [65536] s4^8:(32768,2048) suggest unit:256
#define DEG_H 32768
#define DEG_L 2048
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////


static
void repr_s8_1_16384(uint32_t *poly)
{
	uint32_t *ptr;
	//layer 0: [16384] s8^32:(8192,32) suggest unit:1
	for (int i = (8192 / 32) - 4; i >= 0; i -= 4) {
		uint32_t p0 = poly[i + (8192 / 32)];
		uint32_t p1 = poly[i + (8192 / 32) + 1];
		uint32_t p2 = poly[i + (8192 / 32) + 2];
		uint32_t p3 = poly[i + (8192 / 32) + 3];
		uint32_t q0 = poly[i + 1];
		uint32_t q1 = poly[i + 1 + 1];
		uint32_t q2 = poly[i + 1 + 2];
		uint32_t q3 = poly[i + 1 + 3];
		poly[i + 1] = q0 ^ p0;
		poly[i + 1 + 1] = q1 ^ p1;
		poly[i + 1 + 2] = q2 ^ p2;
		poly[i + 1 + 3] = q3 ^ p3;
	}
	//layer 1: [8192] s8^16:(4096,16) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 8192); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (4096 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (4096 / 32) + 3];
			uint32_t p2 = ptr[i + (4096 / 32) + 2];
			uint32_t p1 = ptr[i + (4096 / 32) + 1];
			uint32_t p0 = ptr[i + (4096 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 16) ^ (leftover << 16);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 16) ^ (p3 << 16);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 16) ^ (p2 << 16);
			ptr[i + 1] = q0 ^ (p0 >> 16) ^ (p1 << 16);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 16);
		ptr += (8192 / 32);
	}

	//layer 2: [4096] s8^8:(2048,8) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 4096); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (2048 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (2048 / 32) + 3];
			uint32_t p2 = ptr[i + (2048 / 32) + 2];
			uint32_t p1 = ptr[i + (2048 / 32) + 1];
			uint32_t p0 = ptr[i + (2048 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 24) ^ (leftover << 8);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 24) ^ (p3 << 8);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 24) ^ (p2 << 8);
			ptr[i + 1] = q0 ^ (p0 >> 24) ^ (p1 << 8);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 8);
		ptr += (4096 / 32);
	}

	//layer 3: [2048] s8^4:(1024,4) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 2048); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (1024 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (1024 / 32) + 3];
			uint32_t p2 = ptr[i + (1024 / 32) + 2];
			uint32_t p1 = ptr[i + (1024 / 32) + 1];
			uint32_t p0 = ptr[i + (1024 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 28) ^ (leftover << 4);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 28) ^ (p3 << 4);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 28) ^ (p2 << 4);
			ptr[i + 1] = q0 ^ (p0 >> 28) ^ (p1 << 4);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 4);
		ptr += (2048 / 32);
	}

	//layer 4: [1024] s8^2:(512,2) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 1024); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (512 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (512 / 32) + 3];
			uint32_t p2 = ptr[i + (512 / 32) + 2];
			uint32_t p1 = ptr[i + (512 / 32) + 1];
			uint32_t p0 = ptr[i + (512 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 30) ^ (leftover << 2);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 30) ^ (p3 << 2);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 30) ^ (p2 << 2);
			ptr[i + 1] = q0 ^ (p0 >> 30) ^ (p1 << 2);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 2);
		ptr += (1024 / 32);
	}

	//layer 5: [512] s8^1:(256,1) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 512); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (256 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (256 / 32) + 3];
			uint32_t p2 = ptr[i + (256 / 32) + 2];
			uint32_t p1 = ptr[i + (256 / 32) + 1];
			uint32_t p0 = ptr[i + (256 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 31) ^ (leftover << 1);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 31) ^ (p3 << 1);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 31) ^ (p2 << 1);
			ptr[i + 1] = q0 ^ (p0 >> 31) ^ (p1 << 1);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 1);
		ptr += (512 / 32);
	}
}


static
void irepr_s8_1_16384(uint32_t *poly)
{
	uint32_t *ptr;

	//layer 5: [512] s8^1:(256,1) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 512); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (256 / 32); i += 4) {
			uint32_t p3 = ptr[i + (256 / 32) + 3];
			uint32_t p2 = ptr[i + (256 / 32) + 2];
			uint32_t p1 = ptr[i + (256 / 32) + 1];
			uint32_t p0 = ptr[i + (256 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 31) ^ (p3 << 1);
			ptr[i + 2] = q2 ^ (p1 >> 31) ^ (p2 << 1);
			ptr[i + 1] = q1 ^ (p0 >> 31) ^ (p1 << 1);
			ptr[i] = q0 ^ (leftover >> 31) ^ (p0 << 1);
			leftover = p3;
		}
		ptr[(256 / 32)] ^= (leftover >> 31);
		ptr += (512 / 32);
	}

	//layer 4: [1024] s8^2:(512,2) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 1024); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (512 / 32); i += 4) {
			uint32_t p3 = ptr[i + (512 / 32) + 3];
			uint32_t p2 = ptr[i + (512 / 32) + 2];
			uint32_t p1 = ptr[i + (512 / 32) + 1];
			uint32_t p0 = ptr[i + (512 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 30) ^ (p3 << 2);
			ptr[i + 2] = q2 ^ (p1 >> 30) ^ (p2 << 2);
			ptr[i + 1] = q1 ^ (p0 >> 30) ^ (p1 << 2);
			ptr[i] = q0 ^ (leftover >> 30) ^ (p0 << 2);
			leftover = p3;
		}
		ptr[(512 / 32)] ^= (leftover >> 30);
		ptr += (1024 / 32);
	}

	//layer 3: [2048] s8^4:(1024,4) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 2048); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (1024 / 32); i += 4) {
			uint32_t p3 = ptr[i + (1024 / 32) + 3];
			uint32_t p2 = ptr[i + (1024 / 32) + 2];
			uint32_t p1 = ptr[i + (1024 / 32) + 1];
			uint32_t p0 = ptr[i + (1024 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 28) ^ (p3 << 4);
			ptr[i + 2] = q2 ^ (p1 >> 28) ^ (p2 << 4);
			ptr[i + 1] = q1 ^ (p0 >> 28) ^ (p1 << 4);
			ptr[i] = q0 ^ (leftover >> 28) ^ (p0 << 4);
			leftover = p3;
		}
		ptr[(1024 / 32)] ^= (leftover >> 28);
		ptr += (2048 / 32);
	}

	//layer 2: [4096] s8^8:(2048,8) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 4096); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (2048 / 32); i += 4) {
			uint32_t p3 = ptr[i + (2048 / 32) + 3];
			uint32_t p2 = ptr[i + (2048 / 32) + 2];
			uint32_t p1 = ptr[i + (2048 / 32) + 1];
			uint32_t p0 = ptr[i + (2048 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 24) ^ (p3 << 8);
			ptr[i + 2] = q2 ^ (p1 >> 24) ^ (p2 << 8);
			ptr[i + 1] = q1 ^ (p0 >> 24) ^ (p1 << 8);
			ptr[i] = q0 ^ (leftover >> 24) ^ (p0 << 8);
			leftover = p3;
		}
		ptr[(2048 / 32)] ^= (leftover >> 24);
		ptr += (4096 / 32);
	}

	//layer 1: [8192] s8^16:(4096,16) suggest unit:1
	ptr = poly;
	for (int j = (16384 / 8192); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (4096 / 32); i += 4) {
			uint32_t p3 = ptr[i + (4096 / 32) + 3];
			uint32_t p2 = ptr[i + (4096 / 32) + 2];
			uint32_t p1 = ptr[i + (4096 / 32) + 1];
			uint32_t p0 = ptr[i + (4096 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 16) ^ (p3 << 16);
			ptr[i + 2] = q2 ^ (p1 >> 16) ^ (p2 << 16);
			ptr[i + 1] = q1 ^ (p0 >> 16) ^ (p1 << 16);
			ptr[i] = q0 ^ (leftover >> 16) ^ (p0 << 16);
			leftover = p3;
		}
		ptr[(4096 / 32)] ^= (leftover >> 16);
		ptr += (8192 / 32);
	}

	//layer 0: [16384] s8^32:(8192,32) suggest unit:1
	for (int i = 0; i < (8192 / 32); i += 4) {
		uint32_t p0 = poly[i + (8192 / 32)];
		uint32_t p1 = poly[i + (8192 / 32) + 1];
		uint32_t p2 = poly[i + (8192 / 32) + 2];
		uint32_t p3 = poly[i + (8192 / 32) + 3];
		uint32_t q0 = poly[i + 1];
		uint32_t q1 = poly[i + 1 + 1];
		uint32_t q2 = poly[i + 1 + 2];
		uint32_t q3 = poly[i + 1 + 3];
		poly[i + 1] = q0 ^ p0;
		poly[i + 1 + 1] = q1 ^ p1;
		poly[i + 1 + 2] = q2 ^ p2;
		poly[i + 1 + 3] = q3 ^ p3;
	}

}

//////////////////


static
void repr_s8_1_32768(uint32_t *poly)
{
	const unsigned w = 32768;
	uint32_t *ptr;

	//layer 0: [32768] s8^64:(16384,64) suggest unit:1
#define DEG_H 16384
#define DEG_L 64
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 0: [16384] s8^32:(8192,32) suggest unit:1
#define DEG_H 8192
#define DEG_L 32
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 1: [8192] s8^16:(4096,16) suggest unit:1
	ptr = poly;
	for (int j = (w / 8192); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (4096 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (4096 / 32) + 3];
			uint32_t p2 = ptr[i + (4096 / 32) + 2];
			uint32_t p1 = ptr[i + (4096 / 32) + 1];
			uint32_t p0 = ptr[i + (4096 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 16) ^ (leftover << 16);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 16) ^ (p3 << 16);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 16) ^ (p2 << 16);
			ptr[i + 1] = q0 ^ (p0 >> 16) ^ (p1 << 16);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 16);
		ptr += (8192 / 32);
	}

	//layer 2: [4096] s8^8:(2048,8) suggest unit:1
	ptr = poly;
	for (int j = (w / 4096); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (2048 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (2048 / 32) + 3];
			uint32_t p2 = ptr[i + (2048 / 32) + 2];
			uint32_t p1 = ptr[i + (2048 / 32) + 1];
			uint32_t p0 = ptr[i + (2048 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 24) ^ (leftover << 8);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 24) ^ (p3 << 8);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 24) ^ (p2 << 8);
			ptr[i + 1] = q0 ^ (p0 >> 24) ^ (p1 << 8);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 8);
		ptr += (4096 / 32);
	}

	//layer 3: [2048] s8^4:(1024,4) suggest unit:1
	ptr = poly;
	for (int j = (w / 2048); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (1024 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (1024 / 32) + 3];
			uint32_t p2 = ptr[i + (1024 / 32) + 2];
			uint32_t p1 = ptr[i + (1024 / 32) + 1];
			uint32_t p0 = ptr[i + (1024 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 28) ^ (leftover << 4);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 28) ^ (p3 << 4);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 28) ^ (p2 << 4);
			ptr[i + 1] = q0 ^ (p0 >> 28) ^ (p1 << 4);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 4);
		ptr += (2048 / 32);
	}

	//layer 4: [1024] s8^2:(512,2) suggest unit:1
	ptr = poly;
	for (int j = (w / 1024); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (512 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (512 / 32) + 3];
			uint32_t p2 = ptr[i + (512 / 32) + 2];
			uint32_t p1 = ptr[i + (512 / 32) + 1];
			uint32_t p0 = ptr[i + (512 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 30) ^ (leftover << 2);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 30) ^ (p3 << 2);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 30) ^ (p2 << 2);
			ptr[i + 1] = q0 ^ (p0 >> 30) ^ (p1 << 2);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 2);
		ptr += (1024 / 32);
	}

	//layer 5: [512] s8^1:(256,1) suggest unit:1
	ptr = poly;
	for (int j = (w / 512); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (256 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (256 / 32) + 3];
			uint32_t p2 = ptr[i + (256 / 32) + 2];
			uint32_t p1 = ptr[i + (256 / 32) + 1];
			uint32_t p0 = ptr[i + (256 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 31) ^ (leftover << 1);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 31) ^ (p3 << 1);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 31) ^ (p2 << 1);
			ptr[i + 1] = q0 ^ (p0 >> 31) ^ (p1 << 1);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 1);
		ptr += (512 / 32);
	}
}


static
void irepr_s8_1_32768(uint32_t *poly)
{
	const unsigned w = 32768;
	uint32_t *ptr;

	//layer 5: [512] s8^1:(256,1) suggest unit:1
	ptr = poly;
	for (int j = (w / 512); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (256 / 32); i += 4) {
			uint32_t p3 = ptr[i + (256 / 32) + 3];
			uint32_t p2 = ptr[i + (256 / 32) + 2];
			uint32_t p1 = ptr[i + (256 / 32) + 1];
			uint32_t p0 = ptr[i + (256 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 31) ^ (p3 << 1);
			ptr[i + 2] = q2 ^ (p1 >> 31) ^ (p2 << 1);
			ptr[i + 1] = q1 ^ (p0 >> 31) ^ (p1 << 1);
			ptr[i] = q0 ^ (leftover >> 31) ^ (p0 << 1);
			leftover = p3;
		}
		ptr[(256 / 32)] ^= (leftover >> 31);
		ptr += (512 / 32);
	}

	//layer 4: [1024] s8^2:(512,2) suggest unit:1
	ptr = poly;
	for (int j = (w / 1024); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (512 / 32); i += 4) {
			uint32_t p3 = ptr[i + (512 / 32) + 3];
			uint32_t p2 = ptr[i + (512 / 32) + 2];
			uint32_t p1 = ptr[i + (512 / 32) + 1];
			uint32_t p0 = ptr[i + (512 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 30) ^ (p3 << 2);
			ptr[i + 2] = q2 ^ (p1 >> 30) ^ (p2 << 2);
			ptr[i + 1] = q1 ^ (p0 >> 30) ^ (p1 << 2);
			ptr[i] = q0 ^ (leftover >> 30) ^ (p0 << 2);
			leftover = p3;
		}
		ptr[(512 / 32)] ^= (leftover >> 30);
		ptr += (1024 / 32);
	}

	//layer 3: [2048] s8^4:(1024,4) suggest unit:1
	ptr = poly;
	for (int j = (w / 2048); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (1024 / 32); i += 4) {
			uint32_t p3 = ptr[i + (1024 / 32) + 3];
			uint32_t p2 = ptr[i + (1024 / 32) + 2];
			uint32_t p1 = ptr[i + (1024 / 32) + 1];
			uint32_t p0 = ptr[i + (1024 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 28) ^ (p3 << 4);
			ptr[i + 2] = q2 ^ (p1 >> 28) ^ (p2 << 4);
			ptr[i + 1] = q1 ^ (p0 >> 28) ^ (p1 << 4);
			ptr[i] = q0 ^ (leftover >> 28) ^ (p0 << 4);
			leftover = p3;
		}
		ptr[(1024 / 32)] ^= (leftover >> 28);
		ptr += (2048 / 32);
	}

	//layer 2: [4096] s8^8:(2048,8) suggest unit:1
	ptr = poly;
	for (int j = (w / 4096); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (2048 / 32); i += 4) {
			uint32_t p3 = ptr[i + (2048 / 32) + 3];
			uint32_t p2 = ptr[i + (2048 / 32) + 2];
			uint32_t p1 = ptr[i + (2048 / 32) + 1];
			uint32_t p0 = ptr[i + (2048 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 24) ^ (p3 << 8);
			ptr[i + 2] = q2 ^ (p1 >> 24) ^ (p2 << 8);
			ptr[i + 1] = q1 ^ (p0 >> 24) ^ (p1 << 8);
			ptr[i] = q0 ^ (leftover >> 24) ^ (p0 << 8);
			leftover = p3;
		}
		ptr[(2048 / 32)] ^= (leftover >> 24);
		ptr += (4096 / 32);
	}

	//layer 1: [8192] s8^16:(4096,16) suggest unit:1
	ptr = poly;
	for (int j = (w / 8192); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (4096 / 32); i += 4) {
			uint32_t p3 = ptr[i + (4096 / 32) + 3];
			uint32_t p2 = ptr[i + (4096 / 32) + 2];
			uint32_t p1 = ptr[i + (4096 / 32) + 1];
			uint32_t p0 = ptr[i + (4096 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 16) ^ (p3 << 16);
			ptr[i + 2] = q2 ^ (p1 >> 16) ^ (p2 << 16);
			ptr[i + 1] = q1 ^ (p0 >> 16) ^ (p1 << 16);
			ptr[i] = q0 ^ (leftover >> 16) ^ (p0 << 16);
			leftover = p3;
		}
		ptr[(4096 / 32)] ^= (leftover >> 16);
		ptr += (8192 / 32);
	}

	//layer 0: [16384] s8^32:(8192,32) suggest unit:1
#define DEG_H 8192
#define DEG_L 32
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 0: [32768] s8^64:(16384,64) suggest unit:1
#define DEG_H 16384
#define DEG_L 64
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

}


//////////////




static
void repr_s8_1_65536(uint32_t *poly)
{
	const unsigned w = 65536;
	uint32_t *ptr;

	//layer 0: [65536] s8^128:(32768,128) suggest unit:1
#define DEG_H 32768
#define DEG_L 128
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 0: [32768] s8^64:(16384,64) suggest unit:1
#define DEG_H 16384
#define DEG_L 64
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 0: [16384] s8^32:(8192,32) suggest unit:1
#define DEG_H 8192
#define DEG_L 32
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		for (int i = (DEG_H / 32) - 4; i >= 0; i -= 4) {
			uint32_t p0 = ptr[i + (DEG_H / 32)];
			uint32_t p1 = ptr[i + (DEG_H / 32) + 1];
			uint32_t p2 = ptr[i + (DEG_H / 32) + 2];
			uint32_t p3 = ptr[i + (DEG_H / 32) + 3];
			uint32_t q0 = ptr[i + (DEG_L / 32)];
			uint32_t q1 = ptr[i + (DEG_L / 32) + 1];
			uint32_t q2 = ptr[i + (DEG_L / 32) + 2];
			uint32_t q3 = ptr[i + (DEG_L / 32) + 3];
			ptr[i + (DEG_L / 32)] = q0 ^ p0;
			ptr[i + (DEG_L / 32) + 1] = q1 ^ p1;
			ptr[i + (DEG_L / 32) + 2] = q2 ^ p2;
			ptr[i + (DEG_L / 32) + 3] = q3 ^ p3;
		}
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 1: [8192] s8^16:(4096,16) suggest unit:1
	ptr = poly;
	for (int j = (w / 8192); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (4096 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (4096 / 32) + 3];
			uint32_t p2 = ptr[i + (4096 / 32) + 2];
			uint32_t p1 = ptr[i + (4096 / 32) + 1];
			uint32_t p0 = ptr[i + (4096 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 16) ^ (leftover << 16);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 16) ^ (p3 << 16);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 16) ^ (p2 << 16);
			ptr[i + 1] = q0 ^ (p0 >> 16) ^ (p1 << 16);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 16);
		ptr += (8192 / 32);
	}

	//layer 2: [4096] s8^8:(2048,8) suggest unit:1
	ptr = poly;
	for (int j = (w / 4096); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (2048 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (2048 / 32) + 3];
			uint32_t p2 = ptr[i + (2048 / 32) + 2];
			uint32_t p1 = ptr[i + (2048 / 32) + 1];
			uint32_t p0 = ptr[i + (2048 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 24) ^ (leftover << 8);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 24) ^ (p3 << 8);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 24) ^ (p2 << 8);
			ptr[i + 1] = q0 ^ (p0 >> 24) ^ (p1 << 8);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 8);
		ptr += (4096 / 32);
	}

	//layer 3: [2048] s8^4:(1024,4) suggest unit:1
	ptr = poly;
	for (int j = (w / 2048); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (1024 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (1024 / 32) + 3];
			uint32_t p2 = ptr[i + (1024 / 32) + 2];
			uint32_t p1 = ptr[i + (1024 / 32) + 1];
			uint32_t p0 = ptr[i + (1024 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 28) ^ (leftover << 4);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 28) ^ (p3 << 4);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 28) ^ (p2 << 4);
			ptr[i + 1] = q0 ^ (p0 >> 28) ^ (p1 << 4);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 4);
		ptr += (2048 / 32);
	}

	//layer 4: [1024] s8^2:(512,2) suggest unit:1
	ptr = poly;
	for (int j = (w / 1024); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (512 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (512 / 32) + 3];
			uint32_t p2 = ptr[i + (512 / 32) + 2];
			uint32_t p1 = ptr[i + (512 / 32) + 1];
			uint32_t p0 = ptr[i + (512 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 30) ^ (leftover << 2);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 30) ^ (p3 << 2);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 30) ^ (p2 << 2);
			ptr[i + 1] = q0 ^ (p0 >> 30) ^ (p1 << 2);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 2);
		ptr += (1024 / 32);
	}

	//layer 5: [512] s8^1:(256,1) suggest unit:1
	ptr = poly;
	for (int j = (w / 512); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = (256 / 32) - 4; i >= 0; i -= 4) {
			uint32_t p3 = ptr[i + (256 / 32) + 3];
			uint32_t p2 = ptr[i + (256 / 32) + 2];
			uint32_t p1 = ptr[i + (256 / 32) + 1];
			uint32_t p0 = ptr[i + (256 / 32)];
			uint32_t q3 = ptr[i + 1 + 3];
			uint32_t q2 = ptr[i + 1 + 2];
			uint32_t q1 = ptr[i + 1 + 1];
			uint32_t q0 = ptr[i + 1];

			ptr[i + 1 + 3] = q3 ^ (p3 >> 31) ^ (leftover << 1);
			ptr[i + 1 + 2] = q2 ^ (p2 >> 31) ^ (p3 << 1);
			ptr[i + 1 + 1] = q1 ^ (p1 >> 31) ^ (p2 << 1);
			ptr[i + 1] = q0 ^ (p0 >> 31) ^ (p1 << 1);
			leftover = p0;
		}
		ptr[0] ^= (leftover << 1);
		ptr += (512 / 32);
	}
}



static
void irepr_s8_1_65536(uint32_t *poly)
{
	const unsigned w = 65536;
	uint32_t *ptr;

	//layer 5: [512] s8^1:(256,1) suggest unit:1
	ptr = poly;
	for (int j = (w / 512); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (256 / 32); i += 4) {
			uint32_t p3 = ptr[i + (256 / 32) + 3];
			uint32_t p2 = ptr[i + (256 / 32) + 2];
			uint32_t p1 = ptr[i + (256 / 32) + 1];
			uint32_t p0 = ptr[i + (256 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 31) ^ (p3 << 1);
			ptr[i + 2] = q2 ^ (p1 >> 31) ^ (p2 << 1);
			ptr[i + 1] = q1 ^ (p0 >> 31) ^ (p1 << 1);
			ptr[i] = q0 ^ (leftover >> 31) ^ (p0 << 1);
			leftover = p3;
		}
		ptr[(256 / 32)] ^= (leftover >> 31);
		ptr += (512 / 32);
	}

	//layer 4: [1024] s8^2:(512,2) suggest unit:1
	ptr = poly;
	for (int j = (w / 1024); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (512 / 32); i += 4) {
			uint32_t p3 = ptr[i + (512 / 32) + 3];
			uint32_t p2 = ptr[i + (512 / 32) + 2];
			uint32_t p1 = ptr[i + (512 / 32) + 1];
			uint32_t p0 = ptr[i + (512 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 30) ^ (p3 << 2);
			ptr[i + 2] = q2 ^ (p1 >> 30) ^ (p2 << 2);
			ptr[i + 1] = q1 ^ (p0 >> 30) ^ (p1 << 2);
			ptr[i] = q0 ^ (leftover >> 30) ^ (p0 << 2);
			leftover = p3;
		}
		ptr[(512 / 32)] ^= (leftover >> 30);
		ptr += (1024 / 32);
	}

	//layer 3: [2048] s8^4:(1024,4) suggest unit:1
	ptr = poly;
	for (int j = (w / 2048); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (1024 / 32); i += 4) {
			uint32_t p3 = ptr[i + (1024 / 32) + 3];
			uint32_t p2 = ptr[i + (1024 / 32) + 2];
			uint32_t p1 = ptr[i + (1024 / 32) + 1];
			uint32_t p0 = ptr[i + (1024 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 28) ^ (p3 << 4);
			ptr[i + 2] = q2 ^ (p1 >> 28) ^ (p2 << 4);
			ptr[i + 1] = q1 ^ (p0 >> 28) ^ (p1 << 4);
			ptr[i] = q0 ^ (leftover >> 28) ^ (p0 << 4);
			leftover = p3;
		}
		ptr[(1024 / 32)] ^= (leftover >> 28);
		ptr += (2048 / 32);
	}

	//layer 2: [4096] s8^8:(2048,8) suggest unit:1
	ptr = poly;
	for (int j = (w / 4096); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (2048 / 32); i += 4) {
			uint32_t p3 = ptr[i + (2048 / 32) + 3];
			uint32_t p2 = ptr[i + (2048 / 32) + 2];
			uint32_t p1 = ptr[i + (2048 / 32) + 1];
			uint32_t p0 = ptr[i + (2048 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 24) ^ (p3 << 8);
			ptr[i + 2] = q2 ^ (p1 >> 24) ^ (p2 << 8);
			ptr[i + 1] = q1 ^ (p0 >> 24) ^ (p1 << 8);
			ptr[i] = q0 ^ (leftover >> 24) ^ (p0 << 8);
			leftover = p3;
		}
		ptr[(2048 / 32)] ^= (leftover >> 24);
		ptr += (4096 / 32);
	}

	//layer 1: [8192] s8^16:(4096,16) suggest unit:1
	ptr = poly;
	for (int j = (w / 8192); j > 0; j--) {
		uint32_t leftover = 0;
		for (int i = 0; i < (4096 / 32); i += 4) {
			uint32_t p3 = ptr[i + (4096 / 32) + 3];
			uint32_t p2 = ptr[i + (4096 / 32) + 2];
			uint32_t p1 = ptr[i + (4096 / 32) + 1];
			uint32_t p0 = ptr[i + (4096 / 32)];
			uint32_t q3 = ptr[i + 3];
			uint32_t q2 = ptr[i + 2];
			uint32_t q1 = ptr[i + 1];
			uint32_t q0 = ptr[i];

			ptr[i + 3] = q3 ^ (p2 >> 16) ^ (p3 << 16);
			ptr[i + 2] = q2 ^ (p1 >> 16) ^ (p2 << 16);
			ptr[i + 1] = q1 ^ (p0 >> 16) ^ (p1 << 16);
			ptr[i] = q0 ^ (leftover >> 16) ^ (p0 << 16);
			leftover = p3;
		}
		ptr[(4096 / 32)] ^= (leftover >> 16);
		ptr += (8192 / 32);
	}

	//layer 0: [16384] s8^32:(8192,32) suggest unit:1
#define DEG_H 8192
#define DEG_L 32
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 0: [32768] s8^64:(16384,64) suggest unit:1
#define DEG_H 16384
#define DEG_L 64
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

	//layer 0: [65536] s8^128:(32768,128) suggest unit:1
#define DEG_H 32768
#define DEG_L 128
	ptr = poly;
	for (int j = (w / (DEG_H * 2)); j > 0; j--) {
		RVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);
		ptr += (DEG_H * 2 / 32);
	}
#undef DEG_H
#undef DEG_L

}



////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////

void bc_1_16384(uint32_t *poly)
{
	repr_s8_1_16384(poly);

	cvt_to_fft_256_16384(poly);

	bc_1_256(poly, 16384 / 256);
}

void ibc_1_16384(uint32_t *poly)
{
	ibc_1_256(poly, 16384 / 256);

	icvt_to_poly_256_16384(poly);

	irepr_s8_1_16384(poly);
}



void bc_1_32768(uint32_t *poly)
{
	repr_s8_1_32768(poly);

	cvt_to_fft_256_32768(poly);

	bc_1_256(poly, 32768 / 256);
}

void ibc_1_32768(uint32_t *poly)
{
	ibc_1_256(poly, 32768 / 256);

	icvt_to_poly_256_32768(poly);

	irepr_s8_1_32768(poly);
}



void bc_1_65536(uint32_t *poly)
{
	repr_s8_1_65536(poly);

	cvt_to_fft_256_65536(poly);

	bc_1_256(poly, 65536 / 256);
}

void ibc_1_65536(uint32_t *poly)
{
	ibc_1_256(poly, 65536 / 256);

	icvt_to_poly_256_65536(poly);

	irepr_s8_1_65536(poly);
}

//new code block
//bc_1_131072
//65536=2^16, based on basis conversion algorithm
//k=16, h'(y)=h0(x)+h1(x)y where y=x^65536 + x
//then for h0(x) and h1(y), their basis conversion is bc_1_65536
//131072/32 = 4096


static
void repr_s16_1_131072(uint32_t *poly)
{
	uint32_t *ptr;
	uint32_t tmp = 0;
	//layer 0: [131072] s16^1:(65536,1) suggest unit:1
	//2048~4095
	ptr = poly;
	//first loop
	//ptr[4092]~ptr[4095] = p0~p3, ptr[2045]~ptr[2048] = q0~q3
	//ptr[2048] = ptr[2048] + ptr[4095]>>31
	//ptr[2047] = ptr[2047] + ptr[4094]>>31 + ptr[4095]<<1
	//ptr[2046] = ptr[2046] + ptr[4093]>>31 + ptr[4094]<<1
	//ptr[2045] = ptr[2045] + ptr[4092]>>31 + ptr[4093]<<1
	//tmp = ptr[4092]
	//ptr[0] = ptr[0] + ptr[2048]<<1
	//
	for (int i = 2048 - 4; i >= 0; i -= 4) {
		uint32_t p3 = ptr[i + 2048 + 3];
		uint32_t p2 = ptr[i + 2048 + 2];
		uint32_t p1 = ptr[i + 2048 + 1];
		uint32_t p0 = ptr[i + 2048];
		uint32_t q3 = ptr[i + 1 + 3];
		uint32_t q2 = ptr[i + 1 + 2];
		uint32_t q1 = ptr[i + 1 + 1];
		uint32_t q0 = ptr[i + 1];

		ptr[i + 1 + 3] = q3 ^ (p3 >> 31) ^ (tmp << 1);
		ptr[i + 1 + 2] = q2 ^ (p2 >> 31) ^ (p3 << 1);
		ptr[i + 1 + 1] = q1 ^ (p1 >> 31) ^ (p2 << 1);
		ptr[i + 1] = q0 ^ (p0 >> 31) ^ (p1 << 1);
		tmp = p0;
	}
	ptr[0] ^= (tmp << 1);
}

static
void irepr_s16_1_131072(uint32_t *poly)
{
	uint32_t *ptr;
	uint32_t tmp = 0;
	//layer 0: [131072] s16^1:(65536,1) suggest unit:1
	//2048~4095
	ptr = poly;
	//first loop
	//ptr[2048]~ptr[2051] = p0~p3, ptr[1]~ptr[4] = q0~q3
	//ptr[3] = ptr[3] + ptr[2051]<<1 + ptr[2050]>>31
	//ptr[2] = ptr[2] + ptr[2050]<<1 + ptr[2049]>>31
	//ptr[1] = ptr[1] + ptr[2049]<<1 + ptr[2048]>>31
	//ptr[0] = ptr[0] + ptr[2048]<<1
	//tmp = ptr[4095]
	//ptr[2048] = ptr[2048] + ptr[4095]>>31
	//
	for (int i = 0; i < 2048; i += 4) {
		uint32_t p3 = ptr[i + 2048 + 3];
		uint32_t p2 = ptr[i + 2048 + 2];
		uint32_t p1 = ptr[i + 2048 + 1];
		uint32_t p0 = ptr[i + 2048];
		uint32_t q3 = ptr[i + 3];
		uint32_t q2 = ptr[i + 2];
		uint32_t q1 = ptr[i + 1];
		uint32_t q0 = ptr[i];

		ptr[i + 3] = q3 ^ (p3 << 1) ^ (p2 >> 31);
		ptr[i + 2] = q2 ^ (p2 << 1) ^ (p1 >> 31);
		ptr[i + 1] = q1 ^ (p1 << 1) ^ (p0 >> 31);
		ptr[i] = q0 ^ (p0 << 1) ^ (tmp >> 31);
		tmp = p3;
	}
	ptr[2048] ^= (tmp >> 31);
}

void bc_1_131072(uint32_t *poly)
{
	repr_s16_1_131072(poly);

	bc_1_65536(poly);
	bc_1_65536(poly + 2048);
}

void ibc_1_131072(uint32_t *poly)
{
	ibc_1_65536(poly);
	ibc_1_65536(poly + 2048);

	irepr_s16_1_131072(poly);
}
