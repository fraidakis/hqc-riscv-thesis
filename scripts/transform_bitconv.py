#!/usr/bin/env python3
"""
Transform bitconversion.c for Iter 2 (block-parallel basis conversion):
  1. Add <riscv_vector.h> and bc_rvv.h includes
  2. Replace bc_1_256 scalar loop → RVV strided-load version (LMUL=4)
  3. Replace ibc_1_256 scalar loop → RVV strided-load version (LMUL=4)
  4. Replace all forward-iteration inner loops (icvt_to_poly_256_* and
     irepr_s8_1_*) with RVV_XOR_OFFSET macro
"""

import re, sys

FILEPATH = '/home/fraidakisg/Thesis/hqc-riscv/src/poly_mul/fafft_zbc/ref/bitconversion.c'

# ────────────────────────────────────────────────────────────────────────────
# Helper: make one vec-register XOR expression
# ────────────────────────────────────────────────────────────────────────────

def V(dst, src_expr):
    return f'\t\t{dst} = {src_expr};\n'

def VXOR(a, b):    return f'__riscv_vxor_vv_u32m4({a}, {b}, vl)'
def VSRL(v, s):    return f'__riscv_vsrl_vx_u32m4({v}, {s}, vl)'
def VSLL(v, s):    return f'__riscv_vsll_vx_u32m4({v}, {s}, vl)'
def VAND(v, c):    return f'__riscv_vand_vx_u32m4({v}, {c}u, vl)'

# ────────────────────────────────────────────────────────────────────────────
# Build bc_1_256 RVV body
# ────────────────────────────────────────────────────────────────────────────

def bc_1_256_rvv_body():
    lines = []
    lines.append('\tptrdiff_t stride = 8 * sizeof(uint32_t);\n')
    lines.append('\tunsigned b = 0;\n')
    lines.append('\twhile (b < n_256bit) {\n')
    lines.append('\t\tsize_t vl = __riscv_vsetvl_e32m4(n_256bit - b);\n')
    lines.append('\t\tuint32_t *blk = poly + b * 8;\n')
    for k in range(8):
        lines.append(f'\t\tvuint32m4_t vp{k} = __riscv_vlse32_v_u32m4(blk + {k}, stride, vl);\n')

    # layer 13: [256] s4^8:(128,8) suggest unit:1
    lines.append('\t\t//layer 13: [256] s4^8:(128,8) suggest unit:1\n')
    lines.append(V('vp4', VXOR('vp4', VSRL('vp7', 24))))
    lines.append(V('vp3', VXOR('vp3', VXOR(VSLL('vp7', 8), VSRL('vp6', 24)))))
    lines.append(V('vp2', VXOR('vp2', VXOR(VSLL('vp6', 8), VSRL('vp5', 24)))))
    lines.append(V('vp1', VXOR('vp1', VXOR(VSLL('vp5', 8), VSRL('vp4', 24)))))
    lines.append(V('vp0', VXOR('vp0', VSLL('vp4', 8))))

    # layer 14: [128] s4^4:(64,4) suggest unit:1
    lines.append('\t\t//layer 14: [128] s4^4:(64,4) suggest unit:1\n')
    lines.append(V('vp2', VXOR('vp2', VSRL('vp3', 28))))
    lines.append(V('vp1', VXOR('vp1', VXOR(VSLL('vp3', 4), VSRL('vp2', 28)))))
    lines.append(V('vp0', VXOR('vp0', VSLL('vp2', 4))))
    lines.append(V('vp6', VXOR('vp6', VSRL('vp7', 28))))
    lines.append(V('vp5', VXOR('vp5', VXOR(VSLL('vp7', 4), VSRL('vp6', 28)))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp6', 4))))

    # layer 15: [64] s4^2:(32,2) suggest unit:1
    lines.append('\t\t//layer 15: [64] s4^2:(32,2) suggest unit:1\n')
    for k in [1,3,5,7]:
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(f'vp{k}', 30))))
    lines.append(V('vp0', VXOR('vp0', VSLL('vp1', 2))))
    lines.append(V('vp2', VXOR('vp2', VSLL('vp3', 2))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp5', 2))))
    lines.append(V('vp6', VXOR('vp6', VSLL('vp7', 2))))

    # layer 16: [32] s4^1:(16,1) suggest unit:1
    lines.append('\t\t//layer 16: [32] s4^1:(16,1) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x80000000'), 15))))
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x7fff0000'), 15))))

    # layer 17: [256] s2^2:(128,32) suggest unit:16
    lines.append('\t\t//layer 17: [256] s2^2:(128,32) suggest unit:16\n')
    lines.append(V('vp4', VXOR('vp4', 'vp7')))
    lines.append(V('vp3', VXOR('vp3', 'vp6')))
    lines.append(V('vp2', VXOR('vp2', 'vp5')))
    lines.append(V('vp1', VXOR('vp1', 'vp4')))

    # layer 18: [128] s2^1:(64,16) suggest unit:16
    lines.append('\t\t//layer 18: [128] s2^1:(64,16) suggest unit:16\n')
    lines.append(V('vp2', VXOR('vp2', VSRL('vp3', 16))))
    lines.append(V('vp1', VXOR('vp1', VXOR(VSLL('vp3', 16), VSRL('vp2', 16)))))
    lines.append(V('vp0', VXOR('vp0', VSLL('vp2', 16))))
    lines.append(V('vp6', VXOR('vp6', VSRL('vp7', 16))))
    lines.append(V('vp5', VXOR('vp5', VXOR(VSLL('vp7', 16), VSRL('vp6', 16)))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp6', 16))))

    # layer 19: [256] s1^1:(128,64) suggest unit:64
    lines.append('\t\t//layer 19: [256] s1^1:(128,64) suggest unit:64\n')
    lines.append(V('vp5', VXOR('vp5', 'vp7')))
    lines.append(V('vp4', VXOR('vp4', 'vp6')))
    lines.append(V('vp3', VXOR('vp3', 'vp5')))
    lines.append(V('vp2', VXOR('vp2', 'vp4')))

    # layer 20: [64] s1^1:(32,16) suggest unit:16
    lines.append('\t\t//layer 20: [64] s1^1:(32,16) suggest unit:16\n')
    for k in [1,3,5,7]:
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(f'vp{k}', 16))))
    lines.append(V('vp0', VXOR('vp0', VSLL('vp1', 16))))
    lines.append(V('vp2', VXOR('vp2', VSLL('vp3', 16))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp5', 16))))
    lines.append(V('vp6', VXOR('vp6', VSLL('vp7', 16))))

    # layer 21: [16] s2^2:(8,2) suggest unit:1
    lines.append('\t\t//layer 21: [16] s2^2:(8,2) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0xc000c000'), 6))))
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x3f003f00'), 6))))

    # layer 22: [8] s2^1:(4,1) suggest unit:1
    lines.append('\t\t//layer 22: [8] s2^1:(4,1) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x80808080'), 3))))
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x70707070'), 3))))

    # layer 23: [16] s1^1:(8,4) suggest unit:4
    lines.append('\t\t//layer 23: [16] s1^1:(8,4) suggest unit:4\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0xf000f000'), 4))))
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x0f000f00'), 4))))

    # layer 24: [4] s1^1:(2,1) suggest unit:1
    lines.append('\t\t//layer 24: [4] s1^1:(2,1) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x88888888'), 1))))
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0x44444444'), 1))))

    # stores
    for k in range(8):
        lines.append(f'\t\t__riscv_vsse32_v_u32m4(blk + {k}, stride, vp{k}, vl);\n')
    lines.append('\t\tb += vl;\n')
    lines.append('\t}\n')
    return ''.join(lines)


# ────────────────────────────────────────────────────────────────────────────
# Build ibc_1_256 RVV body
# ────────────────────────────────────────────────────────────────────────────

def ibc_1_256_rvv_body():
    lines = []
    lines.append('\tptrdiff_t stride = 8 * sizeof(uint32_t);\n')
    lines.append('\tunsigned b = 0;\n')
    lines.append('\twhile (b < n_256bit) {\n')
    lines.append('\t\tsize_t vl = __riscv_vsetvl_e32m4(n_256bit - b);\n')
    lines.append('\t\tuint32_t *blk = poly + b * 8;\n')
    for k in range(8):
        lines.append(f'\t\tvuint32m4_t vp{k} = __riscv_vlse32_v_u32m4(blk + {k}, stride, vl);\n')

    # layer 7: [4] s1^1:(2,1) suggest unit:1
    lines.append('\t\t//layer 7: [4] s1^1:(2,1) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0xcccccccc'), 1))))

    # layer 6: [16] s1^1:(8,4) suggest unit:4
    lines.append('\t\t//layer 6: [16] s1^1:(8,4) suggest unit:4\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0xff00ff00'), 4))))

    # layer 5: [8] s2^1:(4,1) suggest unit:1
    lines.append('\t\t//layer 5: [8] s2^1:(4,1) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0xf0f0f0f0'), 3))))

    # layer 4: [16] s2^2:(8,2) suggest unit:1
    lines.append('\t\t//layer 4: [16] s2^2:(8,2) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0xff00ff00'), 6))))

    # layer 3: [64] s1^1:(32,16) suggest unit:16
    lines.append('\t\t//layer 3: [64] s1^1:(32,16) suggest unit:16\n')
    lines.append(V('vp0', VXOR('vp0', VSLL('vp1', 16))))
    lines.append(V('vp2', VXOR('vp2', VSLL('vp3', 16))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp5', 16))))
    lines.append(V('vp6', VXOR('vp6', VSLL('vp7', 16))))
    for k in [1,3,5,7]:
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(f'vp{k}', 16))))

    # layer 2: [256] s1^1:(128,64) suggest unit:64
    lines.append('\t\t//layer 2: [256] s1^1:(128,64) suggest unit:64\n')
    lines.append(V('vp3', VXOR('vp3', 'vp5')))
    lines.append(V('vp2', VXOR('vp2', 'vp4')))
    lines.append(V('vp5', VXOR('vp5', 'vp7')))
    lines.append(V('vp4', VXOR('vp4', 'vp6')))

    # layer 1: [128] s2^1:(64,16) suggest unit:16
    lines.append('\t\t//layer 1: [128] s2^1:(64,16) suggest unit:16\n')
    lines.append(V('vp0', VXOR('vp0', VSLL('vp2', 16))))
    lines.append(V('vp1', VXOR('vp1', VXOR(VSLL('vp3', 16), VSRL('vp2', 16)))))
    lines.append(V('vp2', VXOR('vp2', VSRL('vp3', 16))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp6', 16))))
    lines.append(V('vp5', VXOR('vp5', VXOR(VSLL('vp7', 16), VSRL('vp6', 16)))))
    lines.append(V('vp6', VXOR('vp6', VSRL('vp7', 16))))

    # layer 0: [256] s2^2:(128,32) suggest unit:16
    lines.append('\t\t//layer 0: [256] s2^2:(128,32) suggest unit:16\n')
    lines.append(V('vp1', VXOR('vp1', 'vp4')))
    lines.append(V('vp2', VXOR('vp2', 'vp5')))
    lines.append(V('vp3', VXOR('vp3', 'vp6')))
    lines.append(V('vp4', VXOR('vp4', 'vp7')))

    # layer 16: [32] s4^1:(16,1) suggest unit:1
    lines.append('\t\t//layer 16: [32] s4^1:(16,1) suggest unit:1\n')
    for k in range(8):
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(VAND(f'vp{k}', '0xffff0000'), 15))))

    # layer 15: [64] s4^2:(32,2) suggest unit:1
    lines.append('\t\t//layer 15: [64] s4^2:(32,2) suggest unit:1\n')
    lines.append(V('vp0', VXOR('vp0', VSLL('vp1', 2))))
    lines.append(V('vp2', VXOR('vp2', VSLL('vp3', 2))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp5', 2))))
    lines.append(V('vp6', VXOR('vp6', VSLL('vp7', 2))))
    for k in [1,3,5,7]:
        lines.append(V(f'vp{k}', VXOR(f'vp{k}', VSRL(f'vp{k}', 30))))

    # layer 14: [128] s4^4:(64,4) suggest unit:1
    lines.append('\t\t//layer 14: [128] s4^4:(64,4) suggest unit:1\n')
    lines.append(V('vp0', VXOR('vp0', VSLL('vp2', 4))))
    lines.append(V('vp1', VXOR('vp1', VXOR(VSLL('vp3', 4), VSRL('vp2', 28)))))
    lines.append(V('vp2', VXOR('vp2', VSRL('vp3', 28))))
    lines.append(V('vp4', VXOR('vp4', VSLL('vp6', 4))))
    lines.append(V('vp5', VXOR('vp5', VXOR(VSLL('vp7', 4), VSRL('vp6', 28)))))
    lines.append(V('vp6', VXOR('vp6', VSRL('vp7', 28))))

    # layer 13: [256] s4^8:(128,8) suggest unit:1
    lines.append('\t\t//layer 13: [256] s4^8:(128,8) suggest unit:1\n')
    lines.append(V('vp0', VXOR('vp0', VSLL('vp4', 8))))
    lines.append(V('vp1', VXOR('vp1', VXOR(VSLL('vp5', 8), VSRL('vp4', 24)))))
    lines.append(V('vp2', VXOR('vp2', VXOR(VSLL('vp6', 8), VSRL('vp5', 24)))))
    lines.append(V('vp3', VXOR('vp3', VXOR(VSLL('vp7', 8), VSRL('vp6', 24)))))
    lines.append(V('vp4', VXOR('vp4', VSRL('vp7', 24))))

    # stores
    for k in range(8):
        lines.append(f'\t\t__riscv_vsse32_v_u32m4(blk + {k}, stride, vp{k}, vl);\n')
    lines.append('\t\tb += vl;\n')
    lines.append('\t}\n')
    return ''.join(lines)


# ────────────────────────────────────────────────────────────────────────────
# Main transformation
# ────────────────────────────────────────────────────────────────────────────

with open(FILEPATH, 'r') as f:
    content = f.read()

# 1. Add RVV includes after <stdint.h>
content = content.replace(
    '#include <stdint.h>\n',
    '#include <stdint.h>\n#include <riscv_vector.h>\n#include "bc_rvv.h"\n',
    1   # only first occurrence
)

# 2. Replace bc_1_256 scalar loop body (the entire function body between the braces)
OLD_BC1 = (
    'void bc_1_256(uint32_t *poly, unsigned n_256bit)\n'
    '{\n'
    '\tfor (int i = n_256bit; i > 0; i--) {\n'
)
NEW_BC1 = (
    'void bc_1_256(uint32_t *poly, unsigned n_256bit)\n'
    '{\n'
    + bc_1_256_rvv_body()
)

# Find the extent of bc_1_256 (from function open to the matching close brace)
bc1_start = content.index(OLD_BC1)
# Walk to find the closing } of the outer for loop and the function }
# The function ends with '\n}\n' right before ibc_1_256
bc1_end = content.index('\nvoid ibc_1_256(uint32_t *poly, unsigned n_256bit)\n')
old_bc1_func = content[bc1_start:bc1_end]
new_bc1_func = (
    'void bc_1_256(uint32_t *poly, unsigned n_256bit)\n'
    '{\n'
    + bc_1_256_rvv_body()
    + '}\n'
)
content = content[:bc1_start] + new_bc1_func + content[bc1_end:]

# 3. Replace ibc_1_256 scalar loop body
OLD_IBC1_HDR = '\nvoid ibc_1_256(uint32_t *poly, unsigned n_256bit)\n{\n'
# ibc_1_256 ends before the static functions
ibc1_start = content.index(OLD_IBC1_HDR)
ibc1_func_start = ibc1_start + 1  # skip leading newline
ibc1_end = content.index('\n\n\n/////////////////////////////////////////////////////////////\n/////////////////////////////////////////////////////////////\n\n\nstatic\nvoid cvt_to_fft_256_16384')
old_ibc1 = content[ibc1_func_start:ibc1_end]
new_ibc1 = (
    'void ibc_1_256(uint32_t *poly, unsigned n_256bit)\n'
    '{\n'
    + ibc_1_256_rvv_body()
    + '}\n'
)
content = content[:ibc1_func_start] + new_ibc1 + content[ibc1_end:]

# 4. Replace all forward-iteration inner loops (icvt + irepr simple XOR patterns)
# Pattern: 16-line scalar inner loop doing ptr[i+DEG_L/32] ^= ptr[i+DEG_H/32]
ICVT_INNER = (
    '\t\tfor (int i = 0; i < (DEG_H / 32); i += 4) {\n'
    '\t\t\tuint32_t p0 = ptr[i + (DEG_H / 32)];\n'
    '\t\t\tuint32_t p1 = ptr[i + (DEG_H / 32) + 1];\n'
    '\t\t\tuint32_t p2 = ptr[i + (DEG_H / 32) + 2];\n'
    '\t\t\tuint32_t p3 = ptr[i + (DEG_H / 32) + 3];\n'
    '\t\t\tuint32_t q0 = ptr[i + (DEG_L / 32)];\n'
    '\t\t\tuint32_t q1 = ptr[i + (DEG_L / 32) + 1];\n'
    '\t\t\tuint32_t q2 = ptr[i + (DEG_L / 32) + 2];\n'
    '\t\t\tuint32_t q3 = ptr[i + (DEG_L / 32) + 3];\n'
    '\t\t\tptr[i + (DEG_L / 32)] = q0 ^ p0;\n'
    '\t\t\tptr[i + (DEG_L / 32) + 1] = q1 ^ p1;\n'
    '\t\t\tptr[i + (DEG_L / 32) + 2] = q2 ^ p2;\n'
    '\t\t\tptr[i + (DEG_L / 32) + 3] = q3 ^ p3;\n'
    '\t\t}\n'
)
ICVT_RVV = '\t\tRVV_XOR_OFFSET(ptr, DEG_H/32, DEG_L/32, DEG_H/32);\n'

n_replaced = content.count(ICVT_INNER)
content = content.replace(ICVT_INNER, ICVT_RVV)
print(f'Replaced {n_replaced} forward-iteration inner loops with RVV_XOR_OFFSET')

with open(FILEPATH, 'w') as f:
    f.write(content)

print('Done. bitconversion.c has been updated.')
