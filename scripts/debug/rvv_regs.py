#!/usr/bin/env python3
"""scripts/debug/rvv_regs.py
GDB Python helper: dump all RVV vector registers (v0–v31) as hex words.

Load from GDB:
    (gdb) source scripts/debug/rvv_regs.py

Then use the new GDB commands:
    (gdb) vreg          # dump all v0–v31
    (gdb) vreg v5       # dump a single register
"""

import gdb


class DumpVregsCommand(gdb.Command):
    """Dump all RVV vector registers (v0–v31) as formatted hex."""

    def __init__(self):
        super().__init__("vreg", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, from_tty):
        arg = arg.strip()
        if arg:
            regs = [arg]
        else:
            regs = [f"v{i}" for i in range(32)]

        for reg in regs:
            try:
                val = gdb.parse_and_eval(f"${reg}")
                # Try to display as array of uint64
                t = val.type
                print(f"  {reg:4s} = {val}")
            except gdb.error as e:
                print(f"  {reg:4s} = <unavailable: {e}>")


DumpVregsCommand()
print("[rvv_regs] Command 'vreg' registered. Use 'vreg' or 'vreg v5'.")
