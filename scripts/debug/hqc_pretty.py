#!/usr/bin/env python3
"""scripts/debug/hqc_pretty.py
GDB Python helper: pretty-print HQC polynomial and ciphertext types.

Load from GDB:
    (gdb) source scripts/debug/hqc_pretty.py
"""

import gdb


class HqcPolyPrinter:
    """Pretty-printer for hqc_poly_t (sparse binary polynomial as uint64 array)."""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            n_words = int(self.val["n_words"])
            coeffs = self.val["coeffs"]
            nonzero = []
            for i in range(n_words):
                word = int(coeffs[i])
                if word:
                    for bit in range(64):
                        if word & (1 << bit):
                            nonzero.append(i * 64 + bit)
            return (
                f"hqc_poly_t [{n_words} words, "
                f"{len(nonzero)} nonzero bits]\n"
                f"  nonzero positions (first 16): {nonzero[:16]}"
            )
        except Exception as e:
            return f"hqc_poly_t <error: {e}>"


class HqcCiphertextPrinter:
    """Pretty-printer for hqc_ciphertext_t."""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return (
                f"hqc_ciphertext_t\n"
                f"  u = {self.val['u']}\n"
                f"  v = {self.val['v']}"
            )
        except Exception as e:
            return f"hqc_ciphertext_t <error: {e}>"


def register_printers(objfile):
    pp = gdb.printing.RegexpCollectionPrettyPrinter("hqc")
    pp.add_printer("hqc_poly_t",        r"^hqc_poly_t$",        HqcPolyPrinter)
    pp.add_printer("hqc_ciphertext_t",  r"^hqc_ciphertext_t$",  HqcCiphertextPrinter)
    if objfile is None:
        gdb.printing.register_pretty_printer(gdb, pp)
    else:
        gdb.printing.register_pretty_printer(objfile, pp)


register_printers(gdb.current_objfile())
print("[hqc_pretty] Pretty-printers registered for hqc_poly_t, hqc_ciphertext_t")
