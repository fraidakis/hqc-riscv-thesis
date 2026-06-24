#!/usr/bin/env python3
"""
scripts/report/parse_isa_mix.py

Parse objdump disassembly and emit instruction-mix CSV.
Reads objdump output from stdin.

Usage:
    objdump -d ./binary | python3 parse_isa_mix.py
"""

import sys
import re
from collections import Counter

counts = Counter()
for line in sys.stdin:
    m = re.match(r'^\s+[0-9a-f]+:\s+[0-9a-f]+\s+(\S+)', line)
    if not m:
        continue
    mnemonic = m.group(1)
    counts[mnemonic] += 1

categories = {
    'zbc': ['clmul', 'clmulh', 'clmulr'],
    'zbb': ['andn', 'orn', 'xnor', 'clz', 'ctz', 'cpop', 'max', 'maxu', 'min', 'minu',
            'orc.b', 'rev8', 'rol', 'ror', 'rori', 'sext.b', 'sext.h', 'zext.h'],
    'zba': ['sh1add', 'sh2add', 'sh3add', 'add.uw', 'sh1add.uw', 'sh2add.uw',
            'sh3add.uw', 'slli.uw'],
    'zbs': ['bclr', 'bclri', 'bext', 'bexti', 'binv', 'binvi', 'bset', 'bseti'],
    'rvv_arith': ['vadd', 'vsub', 'vand', 'vor', 'vxor', 'vnot', 'vsll', 'vsrl', 'vsra',
                  'vmerge', 'vmv', 'vslideup', 'vslidedown', 'vslide1up', 'vslide1down',
                  'vrgather', 'vcompress', 'vredxor', 'vredand', 'vredor'],
    'rvv_load': ['vle8', 'vle16', 'vle32', 'vle64', 'vlse8', 'vlse16', 'vlse32', 'vlse64',
                 'vl1re8', 'vl1re16', 'vl1re32', 'vl1re64', 'vl2re8', 'vl4re8', 'vl8re8'],
    'rvv_store': ['vse8', 'vse16', 'vse32', 'vse64', 'vsse8', 'vsse16', 'vsse32', 'vsse64',
                  'vs1r', 'vs2r', 'vs4r', 'vs8r'],
    'rvv_config': ['vsetvli', 'vsetivli', 'vsetvl'],
    'zvbc': ['vclmul', 'vclmulh'],
    'scalar_mul': ['mul', 'mulh', 'mulhu', 'mulhsu', 'mulw', 'div', 'divu', 'rem', 'remu'],
    'branch': ['beq', 'bne', 'blt', 'bge', 'bltu', 'bgeu', 'jal', 'jalr'],
    'load': ['lb', 'lbu', 'lh', 'lhu', 'lw', 'lwu', 'ld', 'flw', 'fld'],
    'store': ['sb', 'sh', 'sw', 'sd', 'fsw', 'fsd'],
}

# Emit per-mnemonic counts grouped by category
for cat, mnemonics in categories.items():
    for mn in mnemonics:
        total = 0
        for key, cnt in counts.items():
            if key == mn or key.startswith(mn + '.'):
                total += cnt
        if total > 0:
            print(f'{cat},{mn},{total}')

# Total instruction count
total_insns = sum(counts.values())
print(f'total,all_instructions,{total_insns}')

# Category totals
for cat, mnemonics in categories.items():
    cat_total = 0
    for mn in mnemonics:
        for key, cnt in counts.items():
            if key == mn or key.startswith(mn + '.'):
                cat_total += cnt
    if cat_total > 0:
        print(f'summary,{cat}_total,{cat_total}')
