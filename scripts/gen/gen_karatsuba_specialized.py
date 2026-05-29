#!/usr/bin/env python3
"""Generate fixed-size Karatsuba kernel headers for HQC poly-mul variants.

Supports four backends via --mode:
  --mode zbc      Scalar zbc: clmul_leaf / clmul_leaf_n8, manual-loop recomposition
  --mode rvv      RVV base:   clmul_leaf / clmul_leaf_n8, rvv_copy_u64 / rvv_xor3_u64
  --mode rvv_opt  RVV optimized: direct-write z0/z2 to r, no copy loops
  --mode zvbc     Zvbc vector: vclmul_leaf, manual-loop recomposition

Split rule (AP-2):
  n0 = ceil(n/2), n1 = floor(n/2)
Stop recursion at n <= 16.

Usage:
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zbc
  python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv
  python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv_opt
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zvbc
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT_NS = [277, 561, 901]

REPO_ROOT = Path(__file__).resolve().parents[2]

_MODE_CONFIG: dict[str, dict] = {
    "zbc": {
        "out": REPO_ROOT / "src/mult/karatsuba_zbc/karatsuba_specialized_gen.h",
        "guard": "HQC_KARATSUBA_SPECIALIZED_GEN_H",
        "leaf_fn": "scalar",
    },
    "rvv": {
        "out": REPO_ROOT / "src/mult/karatsuba_rvv/karatsuba_rvv_specialized_gen.h",
        "guard": "HQC_KARATSUBA_RVV_SPECIALIZED_GEN_H",
        "leaf_fn": "scalar",
    },
    "rvv_opt": {
        "out": REPO_ROOT / "src/mult/karatsuba_rvv/karatsuba_rvv_specialized_gen.h",
        "guard": "HQC_KARATSUBA_RVV_SPECIALIZED_GEN_H",
        "leaf_fn": "scalar",
    },
    "zvbc": {
        "out": REPO_ROOT / "src/mult/karatsuba_zvbc/karatsuba_zvbc_specialized_gen.h",
        "guard": "HQC_KARATSUBA_ZVBC_SPECIALIZED_GEN_H",
        "leaf_fn": "zvbc",
    },
}


def validate_threshold(t: int) -> None:
    """Validate that threshold is a power of 2 in [8, 128]."""
    if t < 8 or t > 128 or (t & (t - 1)) != 0:
        sys.exit(f"Error: --threshold must be a power of 2 in [8, 128], got {t}")


def split_sizes(n: int) -> tuple[int, int]:
    return (n + 1) // 2, n // 2


def collect_sizes(n: int, out: set[int], threshold: int) -> None:
    if n <= threshold:
        return
    if n in out:
        return
    out.add(n)
    n0, n1 = split_sizes(n)
    collect_sizes(n0, out, threshold)
    collect_sizes(n1, out, threshold)


def child_call(dst: str, xa: str, xb: str, n: int, leaf_fn: str, threshold: int) -> str:
    if n <= threshold:
        if leaf_fn == "zvbc":
            return f"vclmul_leaf({dst}, {xa}, {xb}, {n}u);"
        # scalar / rvv
        if n == 8:
            return f"clmul_leaf_n8({dst}, {xa}, {xb});"
        return f"clmul_leaf({dst}, {xa}, {xb}, {n}u);"
    return f"kmul_{n}({dst}, {xa}, {xb}, child_buf);"


def _manual_loop_recomposition(lines: list[str], n0: int, n1: int) -> None:
    """Emit scalar manual-loop recomposition (used by zbc and zvbc modes)."""
    two_n0 = 2 * n0
    two_n1 = 2 * n1

    lines.append("    /* Loop 1: r[0..2*n0-1] = z0 */")
    lines.append("    {")
    lines.append("        uint64_t *r_p = r;")
    lines.append("        const uint64_t *z0_p = z0;")
    lines.append(f"        for (size_t i = 0; i + 4u <= {two_n0}u; i += 4u) {{")
    lines.append("            r_p[0] = z0_p[0];")
    lines.append("            r_p[1] = z0_p[1];")
    lines.append("            r_p[2] = z0_p[2];")
    lines.append("            r_p[3] = z0_p[3];")
    lines.append("            r_p += 4;")
    lines.append("            z0_p += 4;")
    lines.append("        }")
    lines.append(f"        for (size_t i = ({two_n0}u & ~3u); i < {two_n0}u; i++) {{")
    lines.append("            *r_p++ = *z0_p++;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    /* Loop 2: r[2*n0..2*n0+2*n1-1] = z2 */")
    lines.append("    {")
    lines.append(f"        uint64_t *rh_p = r + {two_n0}u;")
    lines.append("        const uint64_t *z2_p = z2;")
    lines.append(f"        for (size_t i = 0; i + 4u <= {two_n1}u; i += 4u) {{")
    lines.append("            rh_p[0] = z2_p[0];")
    lines.append("            rh_p[1] = z2_p[1];")
    lines.append("            rh_p[2] = z2_p[2];")
    lines.append("            rh_p[3] = z2_p[3];")
    lines.append("            rh_p += 4;")
    lines.append("            z2_p += 4;")
    lines.append("        }")
    lines.append(f"        for (size_t i = ({two_n1}u & ~3u); i < {two_n1}u; i++) {{")
    lines.append("            *rh_p++ = *z2_p++;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    /* Loop 3: r[n0+i] ^= zmid[i] ^ z0[i] ^ z2[i] (where z2 exists) */")
    lines.append("    {")
    lines.append(f"        uint64_t *rm_p = r + {n0}u;")
    lines.append("        const uint64_t *zm_p = zmid;")
    lines.append("        const uint64_t *z0_p = z0;")
    lines.append("        const uint64_t *z2_p = z2;")
    lines.append("")
    lines.append(f"        for (size_t i = 0; i + 4u <= {two_n1}u; i += 4u) {{")
    lines.append("            rm_p[0] ^= zm_p[0] ^ z0_p[0] ^ z2_p[0];")
    lines.append("            rm_p[1] ^= zm_p[1] ^ z0_p[1] ^ z2_p[1];")
    lines.append("            rm_p[2] ^= zm_p[2] ^ z0_p[2] ^ z2_p[2];")
    lines.append("            rm_p[3] ^= zm_p[3] ^ z0_p[3] ^ z2_p[3];")
    lines.append("            rm_p += 4;")
    lines.append("            zm_p += 4;")
    lines.append("            z0_p += 4;")
    lines.append("            z2_p += 4;")
    lines.append("        }")
    lines.append(f"        for (size_t i = ({two_n1}u & ~3u); i < {two_n1}u; i++) {{")
    lines.append("            *rm_p++ ^= *zm_p++ ^ *z0_p++ ^ *z2_p++;")
    lines.append("        }")
    lines.append(f"        for (size_t i = {two_n1}u; i < {two_n0}u; i++) {{")
    lines.append("            *rm_p++ ^= *zm_p++ ^ *z0_p++;")
    lines.append("        }")
    lines.append("    }")


def _rvv_recomposition(lines: list[str], n0: int, n1: int) -> None:
    """Emit RVV intrinsic recomposition (used by rvv mode)."""
    two_n0 = 2 * n0
    two_n1 = 2 * n1

    lines.append("    /* Loop 1: r[0..2*n0-1] = z0 */")
    lines.append(f"    rvv_copy_u64(r, z0, {two_n0}u);")
    lines.append("")
    lines.append("    /* Loop 2: r[2*n0..2*n0+2*n1-1] = z2 */")
    lines.append(f"    rvv_copy_u64(r + {two_n0}u, z2, {two_n1}u);")
    lines.append("")
    lines.append("    /* Loop 3A: r[n0+i] ^= zmid[i] ^ z0[i] ^ z2[i], i in [0, 2*n1) */")
    lines.append(f"    rvv_xor3_u64(r + {n0}u, zmid, z0, z2, {two_n1}u);")

    if two_n0 > two_n1:
        rem = two_n0 - two_n1
        rm_off = n0 + two_n1
        lines.append("")
        lines.append("    /* Loop 3B: odd-n tail where z2 is absent (at most one word) */")
        lines.append(
            f"    rvv_xor3_u64(r + {rm_off}u, zmid + {two_n1}u, z0 + {two_n1}u, (const uint64_t *)0, {rem}u);"
        )


def _rvv_opt_recomposition(lines: list[str], n0: int, n1: int) -> None:
    """Emit optimized RVV recomposition with direct-write discipline.

    In rvv_opt mode, z0 and z2 are written directly to r by child calls:
      - z0 lives at r[0..2*n0-1]
      - z2 lives at r[2*n0..2*n0+2*n1-1]
    No Loop 1 or Loop 2 needed. Only Loop 3 (XOR accumulation) remains,
    reading z0 from r[0..] and z2 from r[2*n0..].

    Buffer aliasing fix: dst (r+n0) overlaps with z0 (r) in the region
    r[n0..2*n0-1]. Forward processing of the first n0 elements is safe
    (z0 reads from r[0..n0-1] which is below dst). But for i >= n0,
    z0[i] = r[i] which was already overwritten by the first half.

    Solution: Save r[n0..2*n0-1] (n0 words) into ta (which is free after
    zmid computation) before Loop 3. Then:
      - First half (i=0..n0-1): z0[i] = r[i] (safe, below dst range)
      - Second half (i=n0..2*n1-1): z0[i] = ta[i-n0] (saved copy)
      - Loop 3B tail (i=2*n1..2*n0-1): z0[i] = ta[i-n0] (saved copy)

    Note: z2 reads (r[2*n0+i]) never overlap with dst writes (r[n0..n0+2*n1-1])
    because z2 starts at r[2*n0] which is at or above the dst upper bound.
    """
    two_n0 = 2 * n0
    two_n1 = 2 * n1
    second_half_count = two_n1 - n0  # elements in second half of Loop 3A

    # --- Save overlap region r[n0..2*n0-1] to ta using inline RVV (LMUL=m4) ---
    lines.append(f"    /* Save overlap region r[n0..2*n0-1] to ta — inline RVV copy (LMUL=m4) */")
    lines.append("    {")
    lines.append(f"        size_t _cp_rem = {n0}u;")
    lines.append(f"        const uint64_t *_cp_src = r + {n0}u;")
    lines.append(f"        uint64_t *_cp_dst = ta;")
    lines.append("        while (_cp_rem > 0) {")
    lines.append("            size_t _cp_vl = __riscv_vsetvl_e64m4(_cp_rem);")
    lines.append("            vuint64m4_t _cp_v = __riscv_vle64_v_u64m4(_cp_src, _cp_vl);")
    lines.append("            __riscv_vse64_v_u64m4(_cp_dst, _cp_v, _cp_vl);")
    lines.append("            _cp_src += _cp_vl; _cp_dst += _cp_vl; _cp_rem -= _cp_vl;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")

    # --- Loop 3A first half: inline XOR3 with LMUL=m2 ---
    lines.append("    /* Loop 3A first half: i in [0, n0) — z0[i]=r[i] is safe (below dst) */")
    lines.append("    {")
    lines.append(f"        size_t _rem = {n0}u;")
    lines.append(f"        uint64_t *_dst = r + {n0}u;")
    lines.append(f"        const uint64_t *_sm = zmid;")
    lines.append(f"        const uint64_t *_s0 = r;")
    lines.append(f"        const uint64_t *_s2 = r + {two_n0}u;")
    lines.append("        while (_rem > 0) {")
    lines.append("            size_t _vl = __riscv_vsetvl_e64m2(_rem);")
    lines.append("            vuint64m2_t _vd = __riscv_vle64_v_u64m2(_dst, _vl);")
    lines.append("            vuint64m2_t _vm = __riscv_vle64_v_u64m2(_sm, _vl);")
    lines.append("            vuint64m2_t _v0 = __riscv_vle64_v_u64m2(_s0, _vl);")
    lines.append("            vuint64m2_t _v2 = __riscv_vle64_v_u64m2(_s2, _vl);")
    lines.append("            vuint64m2_t _acc = __riscv_vxor_vv_u64m2(_vm, _v0, _vl);")
    lines.append("            _acc = __riscv_vxor_vv_u64m2(_acc, _v2, _vl);")
    lines.append("            _vd = __riscv_vxor_vv_u64m2(_vd, _acc, _vl);")
    lines.append("            __riscv_vse64_v_u64m2(_dst, _vd, _vl);")
    lines.append("            _dst += _vl; _sm += _vl; _s0 += _vl; _s2 += _vl; _rem -= _vl;")
    lines.append("        }")
    lines.append("    }")

    if second_half_count > 0:
        lines.append("")
        lines.append(f"    /* Loop 3A second half: i in [n0, 2*n1) — z0[i]=ta[i-n0] (saved copy) */")
        lines.append("    {")
        lines.append(f"        size_t _rem = {second_half_count}u;")
        lines.append(f"        uint64_t *_dst = r + {two_n0}u;")
        lines.append(f"        const uint64_t *_sm = zmid + {n0}u;")
        lines.append(f"        const uint64_t *_s0 = ta;")
        lines.append(f"        const uint64_t *_s2 = r + {3 * n0}u;")
        lines.append("        while (_rem > 0) {")
        lines.append("            size_t _vl = __riscv_vsetvl_e64m2(_rem);")
        lines.append("            vuint64m2_t _vd = __riscv_vle64_v_u64m2(_dst, _vl);")
        lines.append("            vuint64m2_t _vm = __riscv_vle64_v_u64m2(_sm, _vl);")
        lines.append("            vuint64m2_t _v0 = __riscv_vle64_v_u64m2(_s0, _vl);")
        lines.append("            vuint64m2_t _v2 = __riscv_vle64_v_u64m2(_s2, _vl);")
        lines.append("            vuint64m2_t _acc = __riscv_vxor_vv_u64m2(_vm, _v0, _vl);")
        lines.append("            _acc = __riscv_vxor_vv_u64m2(_acc, _v2, _vl);")
        lines.append("            _vd = __riscv_vxor_vv_u64m2(_vd, _acc, _vl);")
        lines.append("            __riscv_vse64_v_u64m2(_dst, _vd, _vl);")
        lines.append("            _dst += _vl; _sm += _vl; _s0 += _vl; _s2 += _vl; _rem -= _vl;")
        lines.append("        }")
        lines.append("    }")

    if two_n0 > two_n1:
        rem = two_n0 - two_n1
        rm_off = n0 + two_n1
        # For the tail, i in [2*n1, 2*n0), z0[i] = ta[i-n0] = ta[2*n1-n0..]
        ta_off = two_n1 - n0
        lines.append("")
        lines.append("    /* Loop 3B: odd-n tail where z2 is absent, z0 from saved ta */")
        lines.append("    {")
        lines.append(f"        size_t _rem = {rem}u;")
        lines.append(f"        uint64_t *_dst = r + {rm_off}u;")
        lines.append(f"        const uint64_t *_sm = zmid + {two_n1}u;")
        lines.append(f"        const uint64_t *_s0 = ta + {ta_off}u;")
        lines.append("        while (_rem > 0) {")
        lines.append("            size_t _vl = __riscv_vsetvl_e64m2(_rem);")
        lines.append("            vuint64m2_t _vd = __riscv_vle64_v_u64m2(_dst, _vl);")
        lines.append("            vuint64m2_t _vm = __riscv_vle64_v_u64m2(_sm, _vl);")
        lines.append("            vuint64m2_t _v0 = __riscv_vle64_v_u64m2(_s0, _vl);")
        lines.append("            vuint64m2_t _acc = __riscv_vxor_vv_u64m2(_vm, _v0, _vl);")
        lines.append("            _vd = __riscv_vxor_vv_u64m2(_vd, _acc, _vl);")
        lines.append("            __riscv_vse64_v_u64m2(_dst, _vd, _vl);")
        lines.append("            _dst += _vl; _sm += _vl; _s0 += _vl; _rem -= _vl;")
        lines.append("        }")
        lines.append("    }")


def _emit_inline_xor_prep(lines: list[str], n0: int, n1: int) -> None:
    """Emit inline RVV intrinsics for ta/tb XOR preparation (rvv_opt mode).

    Replaces rvv_xor_u64(ta, a, a+n0, n1) / rvv_xor_u64(tb, b, b+n0, n1)
    with inline vle64/vxor/vse64 loop. Uses LMUL=m8 for n1 >= 28, LMUL=m4 otherwise.
    Handles the scalar tail element when n0 > n1 (odd-n case).
    """
    # Select LMUL based on operand size
    if n1 >= 28:
        lmul = "m8"
    else:
        lmul = "m4"

    lines.append(f"    /* ta/tb preparation: inline RVV XOR (LMUL={lmul}, n1={n1}) */")
    lines.append("    {")
    lines.append(f"        size_t rem = {n1}u;")
    lines.append("        const uint64_t *sa = a, *sb = b;")
    lines.append(f"        const uint64_t *sa2 = a + {n0}u, *sb2 = b + {n0}u;")
    lines.append("        uint64_t *da = ta, *db = tb;")
    lines.append("        while (rem > 0) {")
    lines.append(f"            size_t vl = __riscv_vsetvl_e64{lmul}(rem);")
    lines.append(f"            vuint64{lmul}_t va1 = __riscv_vle64_v_u64{lmul}(sa, vl);")
    lines.append(f"            vuint64{lmul}_t va2 = __riscv_vle64_v_u64{lmul}(sa2, vl);")
    lines.append(f"            __riscv_vse64_v_u64{lmul}(da, __riscv_vxor_vv_u64{lmul}(va1, va2, vl), vl);")
    lines.append(f"            vuint64{lmul}_t vb1 = __riscv_vle64_v_u64{lmul}(sb, vl);")
    lines.append(f"            vuint64{lmul}_t vb2 = __riscv_vle64_v_u64{lmul}(sb2, vl);")
    lines.append(f"            __riscv_vse64_v_u64{lmul}(db, __riscv_vxor_vv_u64{lmul}(vb1, vb2, vl), vl);")
    lines.append("            sa += vl; sa2 += vl; da += vl;")
    lines.append("            sb += vl; sb2 += vl; db += vl;")
    lines.append("            rem -= vl;")
    lines.append("        }")
    if n0 > n1:
        lines.append(f"        ta[{n1}u] = a[{n1}u];")
        lines.append(f"        tb[{n1}u] = b[{n1}u];")
    lines.append("    }")


def emit_function(n: int, mode: str, threshold: int) -> str:
    n0, n1 = split_sizes(n)
    two_n0 = 2 * n0
    two_n1 = 2 * n1
    leaf_fn = _MODE_CONFIG[mode]["leaf_fn"]

    lines: list[str] = []
    lines.append(f"static void kmul_{n}(uint64_t * __restrict__ r,")
    lines.append("                    const uint64_t * __restrict__ a,")
    lines.append("                    const uint64_t * __restrict__ b,")
    lines.append("                    uint64_t * __restrict__ tmp)")
    lines.append("{")

    if mode == "rvv_opt":
        # Direct-write discipline: z0 → r[0..], z2 → r[2*n0..]
        # tmp layout: zmid + ta + tb + child_buf (no z0/z2 slots)
        lines.append("    uint64_t *zmid      = tmp;")
        lines.append(f"    uint64_t *ta        = zmid + {two_n0}u;")
        lines.append(f"    uint64_t *tb        = ta + {n0}u;")
        lines.append(f"    uint64_t *child_buf = tb + {n0}u;")
        lines.append("")
        lines.append(f"    /* Direct-write: z0 → r[0..{two_n0-1}], z2 → r[{two_n0}..{two_n0+two_n1-1}] */")
        lines.append(f"    {child_call('r', 'a', 'b', n0, leaf_fn, threshold)}")
        lines.append(f"    {child_call(f'r + {two_n0}u', f'a + {n0}u', f'b + {n0}u', n1, leaf_fn, threshold)}")
    else:
        # Original layout: z0 + z2 + zmid + ta + tb + child_buf in tmp
        lines.append("    uint64_t *z0        = tmp;")
        lines.append(f"    uint64_t *z2        = z0 + {two_n0}u;")
        lines.append(f"    uint64_t *zmid      = z2 + {two_n1}u;")
        lines.append(f"    uint64_t *ta        = zmid + {two_n0}u;")
        lines.append(f"    uint64_t *tb        = ta + {n0}u;")
        lines.append(f"    uint64_t *child_buf = tb + {n0}u;")
        lines.append("")
        lines.append(f"    {child_call('z0', 'a', 'b', n0, leaf_fn, threshold)}")
        lines.append(f"    {child_call('z2', f'a + {n0}u', f'b + {n0}u', n1, leaf_fn, threshold)}")
    lines.append("")

    # ta/tb preparation: ta[i] = a[i] ^ a[n0+i], tb[i] = b[i] ^ b[n0+i]
    # For the common part (0..n1-1), use rvv_xor_u64 (rvv/zvbc) or inline intrinsics (rvv_opt).
    # For the odd tail (n1..n0-1), use scalar copy (at most 1 element).
    if mode == "rvv_opt":
        _emit_inline_xor_prep(lines, n0, n1)
    elif mode in ("rvv", "zvbc"):
        lines.append(f"    rvv_xor_u64(ta, a, a + {n0}u, {n1}u);")
        lines.append(f"    rvv_xor_u64(tb, b, b + {n0}u, {n1}u);")
        if n0 > n1:
            # Odd split: copy the remaining element(s) from a[n1..n0-1]
            lines.append(f"    ta[{n1}u] = a[{n1}u];")
            lines.append(f"    tb[{n1}u] = b[{n1}u];")
    else:
        lines.append(f"    for (size_t i = 0; i < {n0}u; i++) {{")
        lines.append(f"        ta[i] = a[i] ^ (i < {n1}u ? a[{n0}u + i] : 0u);")
        lines.append(f"        tb[i] = b[i] ^ (i < {n1}u ? b[{n0}u + i] : 0u);")
        lines.append("    }")
    lines.append("")
    lines.append(f"    {child_call('zmid', 'ta', 'tb', n0, leaf_fn, threshold)}")
    lines.append("")

    if mode == "rvv_opt":
        _rvv_opt_recomposition(lines, n0, n1)
    elif mode in ("rvv", "zvbc"):
        _rvv_recomposition(lines, n0, n1)
    else:
        _manual_loop_recomposition(lines, n0, n1)

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_header(mode: str, threshold: int) -> str:
    cfg = _MODE_CONFIG[mode]
    guard = cfg["guard"]

    sizes: set[int] = set()
    for root in ROOT_NS:
        collect_sizes(root, sizes, threshold)
    ordered = sorted(sizes)

    out: list[str] = []
    out.append(f"#ifndef {guard}")
    out.append(f"#define {guard}")
    out.append("")
    out.append("#include <stddef.h>")
    out.append("#include <stdint.h>")
    out.append("")
    out.append(f"/* Auto-generated by scripts/gen/gen_karatsuba_specialized.py --mode {mode} --threshold {threshold}. */")
    out.append("/* Fixed-size recursion roots: 277, 561, 901. */")
    out.append("")

    for n in ordered:
        out.append(emit_function(n, mode, threshold))

    out.append("#endif")
    out.append("")
    return "\n".join(out)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate fixed-size Karatsuba kernel header for a specific ISA backend."
    )
    parser.add_argument(
        "--mode",
        choices=["zbc", "rvv", "rvv_opt", "zvbc"],
        required=True,
        help="Backend mode: zbc (scalar clmul), rvv (RVV base), rvv_opt (RVV direct-write), zvbc (Zvbc vclmul)",
    )
    parser.add_argument(
        "--threshold",
        type=int,
        default=16,
        help="Leaf threshold (power of 2, 8..128)",
    )
    parser.add_argument(
        "--output",
        type=str,
        default=None,
        help="Output file path (overrides mode default)",
    )
    args = parser.parse_args()

    validate_threshold(args.threshold)

    if args.output is not None:
        out_path = Path(args.output)
    else:
        out_path = _MODE_CONFIG[args.mode]["out"]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(generate_header(args.mode, args.threshold))
    print(f"Generated {out_path}")


if __name__ == "__main__":
    main()
