#!/usr/bin/env python3
"""Generate fixed-size Karatsuba kernel headers for HQC poly-mul variants.

Supports backends via --mode:
  --mode zbc      Scalar zbc: direct-write (noinit) or memset recomposition
  --mode rvv      RVV: direct-write z0/z2 to r, inline RVV intrinsics, tuned LMUL
  --mode rvv_noD  RVV vectorized recomposition without direct-write (ablation)
  --mode zvbc     Zvbc vector: vclmul_leaf, direct-write recomposition (same as rvv)

Split rule:
  n0 = ceil(n/2), n1 = floor(n/2)
Stop recursion at n <= 16.

Usage:
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zbc
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zbc --leaf-style memset
  python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zvbc
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT_NS = [277, 561, 901]

# Hybrid RVV threshold: functions with n <= this use scalar recomposition loops
# to avoid scalar↔vector pipeline transitions at the bottom recursion levels.
# At these sizes the RVV loops process ≤2 iterations and transition overhead dominates.
# Empirically validated via hardware sweep on BPI-F3 (SpacemiT X60, VLEN=256):
#   H=29 is optimal for HQC-256 (−2.1% vs H=18) and within noise for HQC-128/192.
RVV_HYBRID_THRESHOLD = 29

# RVV LMUL: flat m2 everywhere. Empirically validated on SpacemiT X60 (VLEN=256)
# to be optimal or within 1% of optimal at all HQC security levels. Lower LMUL
# avoids register pressure on in-order cores and reduces pipeline stalls.
_RVV_LMUL = "m2"

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
    "rvv_noD": {
        "out": REPO_ROOT / "src/mult/karatsuba_rvv/ablation/karatsuba_rvv_ablation_v_only_gen.h",
        "guard": "HQC_KARATSUBA_RVV_ABLATION_V_ONLY_GEN_H",
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


_SPLIT_MODE = "ap2"  # "ap2" = ceil/floor, "symmetric" = floor/ceil (both produce identical trees)


def split_sizes(n: int) -> tuple[int, int]:
    """Split: n0 = ceil(n/2), n1 = floor(n/2).

    The --split=symmetric mode was added for ablation testing and produces
    identical results because both ceil/floor and floor/ceil decompose into
    the same set of sub-problem sizes.
    """
    if _SPLIT_MODE == "symmetric":
        n0 = n // 2
        n1 = n - n0
        return max(n0, n1), min(n0, n1)
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


def child_call(dst: str, xa: str, xb: str, n: int, leaf_fn: str, threshold: int,
               leaf_style: str = "noinit", mode: str = "zbc") -> str:
    if n <= threshold:
        if leaf_fn == "zvbc":
            return f"vclmul_leaf({dst}, {xa}, {xb}, {n}u);"
        # scalar / rvv
        if leaf_style == "noinit" and mode == "rvv":
            if n == 8:
                return f"clmul_leaf_n8({dst}, {xa}, {xb});"
            if n == 9:
                return f"clmul_leaf_n9({dst}, {xa}, {xb});"
            if n == 14:
                return f"clmul_leaf_n14({dst}, {xa}, {xb});"
            if n == 15:
                return f"clmul_leaf_n15({dst}, {xa}, {xb});"
        # rvv_noD uses same specialized leaves as rvv
        if leaf_style == "noinit" and mode == "rvv_noD":
            if n == 8:
                return f"clmul_leaf_n8({dst}, {xa}, {xb});"
            if n == 9:
                return f"clmul_leaf_n9({dst}, {xa}, {xb});"
            if n == 14:
                return f"clmul_leaf_n14({dst}, {xa}, {xb});"
            if n == 15:
                return f"clmul_leaf_n15({dst}, {xa}, {xb});"
        # zbc noinit: use LEAF_CALL_N8/N9 macros for specialized leaves
        if leaf_style == "noinit" and mode == "zbc":
            if n == 8:
                return f"LEAF_CALL_N8({dst}, {xa}, {xb});"
            if n == 9:
                return f"LEAF_CALL_N9({dst}, {xa}, {xb});"
            if n == 14:
                return f"LEAF_CALL_N14({dst}, {xa}, {xb});"
            if n == 15:
                return f"LEAF_CALL_N15({dst}, {xa}, {xb});"
        # zbc memset: also use LEAF_CALL_N8/N9 macros (resolve to generic
        # clmul_leaf when ABLATION_NOINIT=0, or to specialized leaves otherwise)
        if leaf_style == "memset" and mode == "zbc":
            if n == 8:
                return f"LEAF_CALL_N8({dst}, {xa}, {xb});"
            if n == 9:
                return f"LEAF_CALL_N9({dst}, {xa}, {xb});"
        # All other modes: use generic clmul_leaf
        return f"clmul_leaf({dst}, {xa}, {xb}, {n}u);"
    return f"kmul_{n}({dst}, {xa}, {xb}, child_buf);"


def _manual_loop_recomposition(lines: list[str], n0: int, n1: int) -> None:
    """Emit scalar manual-loop recomposition with direct-write (noinit) discipline.

    Assign-on-first-write: Loop 1 and 2 use '=' (direct assign), Loop 3 uses '^='.
    No memset needed — every position in r[] is written before being read.
    Uses 4-wide manual unrolling to minimize loop overhead on in-order cores.
    """
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


def _manual_loop_recomposition_memset(lines: list[str], n0: int, n1: int) -> None:
    """Emit scalar manual-loop recomposition with memset + XOR-accumulate discipline.

    Zeroes r[] first, then uses '^=' for all three sub-products.
    This is the reference-style recomposition — orthogonal to B (noinit).
    """
    two_n0 = 2 * n0
    two_n1 = 2 * n1
    total_words = two_n0 + two_n1  # = 2*n total output words

    # Compute actual output size (same as 2*n where n = n0 + n1)
    n = n0 + n1
    lines.append(f"    memset(r, 0, {2 * n}u * sizeof(uint64_t));")
    lines.append("")
    lines.append("    /* Accumulate z0 into r[0..2*n0-1] */")
    lines.append("    {")
    lines.append("        uint64_t *r_p = r;")
    lines.append("        const uint64_t *z0_p = z0;")
    lines.append(f"        for (size_t i = 0; i + 4u <= {two_n0}u; i += 4u) {{")
    lines.append("            r_p[0] ^= z0_p[0];")
    lines.append("            r_p[1] ^= z0_p[1];")
    lines.append("            r_p[2] ^= z0_p[2];")
    lines.append("            r_p[3] ^= z0_p[3];")
    lines.append("            r_p += 4;")
    lines.append("            z0_p += 4;")
    lines.append("        }")
    lines.append(f"        for (size_t i = ({two_n0}u & ~3u); i < {two_n0}u; i++) {{")
    lines.append("            *r_p++ ^= *z0_p++;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    /* Accumulate z2 into r[2*n0..2*n0+2*n1-1] */")
    lines.append("    {")
    lines.append(f"        uint64_t *rh_p = r + {two_n0}u;")
    lines.append("        const uint64_t *z2_p = z2;")
    lines.append(f"        for (size_t i = 0; i + 4u <= {two_n1}u; i += 4u) {{")
    lines.append("            rh_p[0] ^= z2_p[0];")
    lines.append("            rh_p[1] ^= z2_p[1];")
    lines.append("            rh_p[2] ^= z2_p[2];")
    lines.append("            rh_p[3] ^= z2_p[3];")
    lines.append("            rh_p += 4;")
    lines.append("            z2_p += 4;")
    lines.append("        }")
    lines.append(f"        for (size_t i = ({two_n1}u & ~3u); i < {two_n1}u; i++) {{")
    lines.append("            *rh_p++ ^= *z2_p++;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    /* Accumulate zmid ^ z0 ^ z2 into r[n0..n0+2*n1-1] */")
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


def _rvv_recomposition_noD(lines: list[str], n0: int, n1: int) -> None:
    """Emit RVV recomposition with ref (non-direct-write) layout.

    In rvv_noD mode, z0 and z2 are in tmp (ref layout). We need:
      - Loop 1: copy z0 → r[0..2*n0-1] using RVV
      - Loop 2: copy z2 → r[2*n0..2*n0+2*n1-1] using RVV
      - Loop 3: r[n0+i] ^= zmid[i] ^ z0[i] ^ z2[i] using RVV

    No aliasing issues: z0/z2/zmid are in tmp, dst is in r.
    """
    two_n0 = 2 * n0
    two_n1 = 2 * n1

    # --- Loop 1: RVV copy z0 → r[0..2*n0-1] ---
    lines.append(f"    /* Loop 1: r[0..2*n0-1] = z0 — inline RVV copy (LMUL={_RVV_LMUL}) */")
    lines.append("    {")
    lines.append(f"        size_t rem = {two_n0}u;")
    lines.append("        const uint64_t *src = z0;")
    lines.append("        uint64_t *dst = r;")
    lines.append("        while (rem > 0) {")
    lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v = __riscv_vle64_v_u64{_RVV_LMUL}(src, vl);")
    lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(dst, v, vl);")
    lines.append("            src += vl; dst += vl; rem -= vl;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")

    # --- Loop 2: RVV copy z2 → r[2*n0..2*n0+2*n1-1] ---
    lines.append(f"    /* Loop 2: r[2*n0..2*n0+2*n1-1] = z2 — inline RVV copy (LMUL={_RVV_LMUL}) */")
    lines.append("    {")
    lines.append(f"        size_t rem = {two_n1}u;")
    lines.append("        const uint64_t *src = z2;")
    lines.append(f"        uint64_t *dst = r + {two_n0}u;")
    lines.append("        while (rem > 0) {")
    lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v = __riscv_vle64_v_u64{_RVV_LMUL}(src, vl);")
    lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(dst, v, vl);")
    lines.append("            src += vl; dst += vl; rem -= vl;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")

    # --- Loop 3: RVV XOR accumulation r[n0+i] ^= zmid[i] ^ z0[i] ^ z2[i] ---
    lines.append(f"    /* Loop 3: r[n0+i] ^= zmid[i] ^ z0[i] ^ z2[i] — inline RVV (LMUL={_RVV_LMUL}) */")
    lines.append("    {")
    lines.append(f"        size_t rem = {two_n1}u;")
    lines.append(f"        uint64_t *dst = r + {n0}u;")
    lines.append("        const uint64_t *zm_p = zmid;")
    lines.append("        const uint64_t *z0_p = z0;")
    lines.append("        const uint64_t *z2_p = z2;")
    lines.append("        while (rem > 0) {")
    lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_dst = __riscv_vle64_v_u64{_RVV_LMUL}(dst, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_zm = __riscv_vle64_v_u64{_RVV_LMUL}(zm_p, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_z0 = __riscv_vle64_v_u64{_RVV_LMUL}(z0_p, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_z2 = __riscv_vle64_v_u64{_RVV_LMUL}(z2_p, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_zm, v_z0, vl);")
    lines.append(f"            v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_acc, v_z2, vl);")
    lines.append(f"            v_dst = __riscv_vxor_vv_u64{_RVV_LMUL}(v_dst, v_acc, vl);")
    lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(dst, v_dst, vl);")
    lines.append("            dst += vl; zm_p += vl; z0_p += vl; z2_p += vl; rem -= vl;")
    lines.append("        }")
    if two_n0 > two_n1:
        # Odd-n tail: zmid[i] ^ z0[i] (no z2)
        rem = two_n0 - two_n1
        lines.append(f"        /* Tail: {rem} elements where z2 is absent */")
        lines.append(f"        rem = {rem}u;")
        lines.append("        while (rem > 0) {")
        lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_dst = __riscv_vle64_v_u64{_RVV_LMUL}(dst, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_zm = __riscv_vle64_v_u64{_RVV_LMUL}(zm_p, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_z0 = __riscv_vle64_v_u64{_RVV_LMUL}(z0_p, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_zm, v_z0, vl);")
        lines.append(f"            v_dst = __riscv_vxor_vv_u64{_RVV_LMUL}(v_dst, v_acc, vl);")
        lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(dst, v_dst, vl);")
        lines.append("            dst += vl; zm_p += vl; z0_p += vl; rem -= vl;")
        lines.append("        }")
    lines.append("    }")


def _rvv_recomposition(lines: list[str], n0: int, n1: int) -> None:
    """Emit optimized RVV recomposition with direct-write discipline.

    In rvv mode, z0 and z2 are written directly to r by child calls:
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
    lines.append(f"    /* Save overlap region r[n0..2*n0-1] to ta — inline RVV copy (LMUL={_RVV_LMUL}) */")
    lines.append("    {")
    lines.append(f"        size_t rem = {n0}u;")
    lines.append(f"        const uint64_t *cp_src = r + {n0}u;")
    lines.append(f"        uint64_t *cp_dst = ta;")
    lines.append("        while (rem > 0) {")
    lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v = __riscv_vle64_v_u64{_RVV_LMUL}(cp_src, vl);")
    lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(cp_dst, v, vl);")
    lines.append("            cp_src += vl; cp_dst += vl; rem -= vl;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")

    # --- Loop 3A first half: inline XOR3 with LMUL=m2 ---
    lines.append("    /* Loop 3A first half: i in [0, n0) — z0[i]=r[i] is safe (below dst) */")
    lines.append("    {")
    lines.append(f"        size_t rem = {n0}u;")
    lines.append(f"        uint64_t *dst = r + {n0}u;")
    lines.append(f"        const uint64_t *zm_p = zmid;")
    lines.append(f"        const uint64_t *z0_p = r;")
    lines.append(f"        const uint64_t *z2_p = r + {two_n0}u;")
    lines.append("        while (rem > 0) {")
    lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_dst = __riscv_vle64_v_u64{_RVV_LMUL}(dst, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_zm = __riscv_vle64_v_u64{_RVV_LMUL}(zm_p, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_z0 = __riscv_vle64_v_u64{_RVV_LMUL}(z0_p, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_z2 = __riscv_vle64_v_u64{_RVV_LMUL}(z2_p, vl);")
    lines.append(f"            vuint64{_RVV_LMUL}_t v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_zm, v_z0, vl);")
    lines.append(f"            v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_acc, v_z2, vl);")
    lines.append(f"            v_dst = __riscv_vxor_vv_u64{_RVV_LMUL}(v_dst, v_acc, vl);")
    lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(dst, v_dst, vl);")
    lines.append("            dst += vl; zm_p += vl; z0_p += vl; z2_p += vl; rem -= vl;")
    lines.append("        }")
    lines.append("    }")

    if second_half_count > 0:
        lines.append("")
        lines.append(f"    /* Loop 3A second half: i in [n0, 2*n1) — z0[i]=ta[i-n0] (saved copy) */")
        lines.append("    {")
        lines.append(f"        size_t rem = {second_half_count}u;")
        lines.append(f"        uint64_t *dst = r + {two_n0}u;")
        lines.append(f"        const uint64_t *zm_p = zmid + {n0}u;")
        lines.append(f"        const uint64_t *z0_p = ta;")
        lines.append(f"        const uint64_t *z2_p = r + {3 * n0}u;")
        lines.append("        while (rem > 0) {")
        lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_dst = __riscv_vle64_v_u64{_RVV_LMUL}(dst, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_zm = __riscv_vle64_v_u64{_RVV_LMUL}(zm_p, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_z0 = __riscv_vle64_v_u64{_RVV_LMUL}(z0_p, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_z2 = __riscv_vle64_v_u64{_RVV_LMUL}(z2_p, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_zm, v_z0, vl);")
        lines.append(f"            v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_acc, v_z2, vl);")
        lines.append(f"            v_dst = __riscv_vxor_vv_u64{_RVV_LMUL}(v_dst, v_acc, vl);")
        lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(dst, v_dst, vl);")
        lines.append("            dst += vl; zm_p += vl; z0_p += vl; z2_p += vl; rem -= vl;")
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
        lines.append(f"        size_t rem = {rem}u;")
        lines.append(f"        uint64_t *dst = r + {rm_off}u;")
        lines.append(f"        const uint64_t *zm_p = zmid + {two_n1}u;")
        lines.append(f"        const uint64_t *z0_p = ta + {ta_off}u;")
        lines.append("        while (rem > 0) {")
        lines.append(f"            size_t vl = __riscv_vsetvl_e64{_RVV_LMUL}(rem);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_dst = __riscv_vle64_v_u64{_RVV_LMUL}(dst, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_zm = __riscv_vle64_v_u64{_RVV_LMUL}(zm_p, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_z0 = __riscv_vle64_v_u64{_RVV_LMUL}(z0_p, vl);")
        lines.append(f"            vuint64{_RVV_LMUL}_t v_acc = __riscv_vxor_vv_u64{_RVV_LMUL}(v_zm, v_z0, vl);")
        lines.append(f"            v_dst = __riscv_vxor_vv_u64{_RVV_LMUL}(v_dst, v_acc, vl);")
        lines.append(f"            __riscv_vse64_v_u64{_RVV_LMUL}(dst, v_dst, vl);")
        lines.append("            dst += vl; zm_p += vl; z0_p += vl; rem -= vl;")
        lines.append("        }")
        lines.append("    }")


def _scalar_recomposition_directwrite(lines: list[str], n0: int, n1: int) -> None:
    """Emit scalar recomposition for RVV direct-write discipline (hybrid mode).

    Same logic as _rvv_recomposition but uses plain scalar C loops instead of
    RVV intrinsics. This avoids scalar↔vector pipeline transitions at small
    recursion sizes where the transition penalty dominates the XOR work.

    Like _rvv_recomposition, z0 is at r[0..2*n0-1] and z2 is at r[2*n0..].
    Only Loop 3 is needed (no Loop 1/2 copy).
    """
    two_n0 = 2 * n0
    two_n1 = 2 * n1
    second_half_count = two_n1 - n0  # elements in second half of Loop 3A

    # --- Save overlap region r[n0..2*n0-1] to ta (scalar copy) ---
    lines.append(f"    /* Save overlap region r[n0..2*n0-1] to ta — scalar copy */")
    lines.append("    {")
    lines.append(f"        const uint64_t *cp_src = r + {n0}u;")
    lines.append(f"        uint64_t *cp_dst = ta;")
    lines.append(f"        for (size_t i = 0; i < {n0}u; i++)")
    lines.append("            cp_dst[i] = cp_src[i];")
    lines.append("    }")
    lines.append("")

    # --- Loop 3A first half: i in [0, n0) — z0[i]=r[i] is safe (below dst) ---
    lines.append("    /* Loop 3A first half: i in [0, n0) — scalar XOR3 */")
    lines.append("    {")
    lines.append(f"        uint64_t *dst = r + {n0}u;")
    lines.append(f"        const uint64_t *zm_p = zmid;")
    lines.append(f"        const uint64_t *z0_p = r;")
    lines.append(f"        const uint64_t *z2_p = r + {two_n0}u;")
    lines.append(f"        for (size_t i = 0; i < {n0}u; i++)")
    lines.append("            dst[i] ^= zm_p[i] ^ z0_p[i] ^ z2_p[i];")
    lines.append("    }")

    if second_half_count > 0:
        lines.append("")
        lines.append(f"    /* Loop 3A second half: i in [n0, 2*n1) — z0[i]=ta[i-n0] (saved copy) */")
        lines.append("    {")
        lines.append(f"        uint64_t *dst = r + {two_n0}u;")
        lines.append(f"        const uint64_t *zm_p = zmid + {n0}u;")
        lines.append(f"        const uint64_t *z0_p = ta;")
        lines.append(f"        const uint64_t *z2_p = r + {3 * n0}u;")
        lines.append(f"        for (size_t i = 0; i < {second_half_count}u; i++)")
        lines.append("            dst[i] ^= zm_p[i] ^ z0_p[i] ^ z2_p[i];")
        lines.append("    }")

    if two_n0 > two_n1:
        rem = two_n0 - two_n1
        rm_off = n0 + two_n1
        ta_off = two_n1 - n0
        lines.append("")
        lines.append("    /* Loop 3B: odd-n tail where z2 is absent, z0 from saved ta */")
        lines.append("    {")
        lines.append(f"        uint64_t *dst = r + {rm_off}u;")
        lines.append(f"        const uint64_t *zm_p = zmid + {two_n1}u;")
        lines.append(f"        const uint64_t *z0_p = ta + {ta_off}u;")
        lines.append(f"        for (size_t i = 0; i < {rem}u; i++)")
        lines.append("            dst[i] ^= zm_p[i] ^ z0_p[i];")
        lines.append("    }")


def _helper_recomposition_directwrite(lines: list[str], n0: int, n1: int) -> None:
    """Emit direct-write recomposition using rvv_copy_u64 / rvv_xor3_u64 helper calls.

    Same logic as _rvv_recomposition but emits clean function calls instead of
    inline intrinsic loops. Identical performance (helpers are static inline).
    Much more readable generated code.

    z0 is at r[0..2*n0-1], z2 is at r[2*n0..].
    Steps:
      1. Save overlap region r[n0..2*n0-1] to ta (via rvv_copy_u64)
      2. Loop 3A first half: r[n0+i] ^= zmid[i] ^ r[i] ^ r[2*n0+i]
      3. Loop 3A second half: r[2*n0+i] ^= zmid[n0+i] ^ ta[i] ^ r[3*n0+i]
      4. Loop 3B tail (odd-n): r[n0+2*n1+i] ^= zmid[2*n1+i] ^ ta[2*n1-n0+i]
    """
    two_n0 = 2 * n0
    two_n1 = 2 * n1
    second_half_count = two_n1 - n0  # elements in second half of Loop 3A

    # --- Save overlap region r[n0..2*n0-1] to ta ---
    lines.append(f"    /* Save overlap region r[n0..2*n0-1] to ta */")
    lines.append(f"    rvv_copy_u64(ta, r + {n0}u, {n0}u);")
    lines.append("")

    # --- Loop 3A first half: i in [0, n0) ---
    lines.append(f"    /* Loop 3A: r[n0..2*n0-1] ^= zmid[0..n0-1] ^ z0[0..n0-1] ^ z2[0..n0-1] */")
    lines.append(f"    rvv_xor3_u64(r + {n0}u, zmid, r, r + {two_n0}u, {n0}u);")

    if second_half_count > 0:
        lines.append("")
        lines.append(f"    /* Loop 3A second half: z0 from saved ta, z2 from r[3*n0..] */")
        lines.append(f"    rvv_xor3_u64(r + {two_n0}u, zmid + {n0}u, ta, r + {3 * n0}u, {second_half_count}u);")

    if two_n0 > two_n1:
        rem = two_n0 - two_n1
        rm_off = n0 + two_n1
        ta_off = two_n1 - n0
        lines.append("")
        lines.append(f"    /* Loop 3B: odd-n tail where z2 is absent */")
        lines.append(f"    rvv_xor3_u64(r + {rm_off}u, zmid + {two_n1}u, ta + {ta_off}u, (const uint64_t *)0, {rem}u);")


def _scalar_helper_recomposition_directwrite(lines: list[str], n0: int, n1: int) -> None:
    """Emit direct-write recomposition using scalar_copy_u64 / scalar_xor3_u64 helper calls.

    Same logic as _helper_recomposition_directwrite but using scalar helpers.
    For the pure-scalar (zbc) variant — no vector instructions.
    """
    two_n0 = 2 * n0
    two_n1 = 2 * n1
    second_half_count = two_n1 - n0

    lines.append(f"    /* Save overlap region r[n0..2*n0-1] to ta */")
    lines.append(f"    scalar_copy_u64(ta, r + {n0}u, {n0}u);")
    lines.append("")

    lines.append(f"    /* Loop 3A: r[n0..2*n0-1] ^= zmid[0..n0-1] ^ z0[0..n0-1] ^ z2[0..n0-1] */")
    lines.append(f"    scalar_xor3_u64(r + {n0}u, zmid, r, r + {two_n0}u, {n0}u);")

    if second_half_count > 0:
        lines.append("")
        lines.append(f"    /* Loop 3A second half: z0 from saved ta, z2 from r[3*n0..] */")
        lines.append(f"    scalar_xor3_u64(r + {two_n0}u, zmid + {n0}u, ta, r + {3 * n0}u, {second_half_count}u);")

    if two_n0 > two_n1:
        rem = two_n0 - two_n1
        rm_off = n0 + two_n1
        ta_off = two_n1 - n0
        lines.append("")
        lines.append(f"    /* Loop 3B: odd-n tail where z2 is absent */")
        lines.append(f"    scalar_xor3_u64(r + {rm_off}u, zmid + {two_n1}u, ta + {ta_off}u, (const uint64_t *)0, {rem}u);")


def _scalar_inline_xor_prep(lines: list[str], n0: int, n1: int) -> None:
    """Emit scalar inline XOR preparation for ta/tb (hybrid mode).

    Same as _emit_inline_xor_prep but uses plain scalar loops instead of RVV.
    """
    lines.append(f"    /* ta/tb preparation: scalar XOR (n1={n1}) */")
    lines.append("    {")
    lines.append(f"        for (size_t i = 0; i < {n1}u; i++) {{")
    lines.append(f"            ta[i] = a[i] ^ a[{n0}u + i];")
    lines.append(f"            tb[i] = b[i] ^ b[{n0}u + i];")
    lines.append("        }")
    if n0 > n1:
        lines.append(f"        ta[{n1}u] = a[{n1}u];")
        lines.append(f"        tb[{n1}u] = b[{n1}u];")
    lines.append("    }")


def _helper_xor_prep(lines: list[str], n0: int, n1: int) -> None:
    """Emit ta/tb XOR preparation using rvv_xor_u64 helper function call.

    Cleaner than inline intrinsics — same performance (static inline).
    Handles the odd-tail element (n0 > n1) with a scalar assignment.
    """
    lines.append(f"    rvv_xor_u64(ta, a, a + {n0}u, {n1}u);")
    lines.append(f"    rvv_xor_u64(tb, b, b + {n0}u, {n1}u);")
    if n0 > n1:
        lines.append(f"    ta[{n1}u] = a[{n1}u];")
        lines.append(f"    tb[{n1}u] = b[{n1}u];")


def _scalar_helper_xor_prep(lines: list[str], n0: int, n1: int) -> None:
    """Emit ta/tb XOR preparation using scalar_xor_u64 helper function call.

    Clean single-line calls. Handles the odd-tail element with scalar assignment.
    """
    lines.append(f"    scalar_xor_u64(ta, a, a + {n0}u, {n1}u);")
    lines.append(f"    scalar_xor_u64(tb, b, b + {n0}u, {n1}u);")
    if n0 > n1:
        lines.append(f"    ta[{n1}u] = a[{n1}u];")
        lines.append(f"    tb[{n1}u] = b[{n1}u];")


def _emit_inline_xor_prep(lines: list[str], n0: int, n1: int) -> None:
    """Emit inline RVV intrinsics for ta/tb XOR preparation (rvv mode).

    Replaces rvv_xor_u64(ta, a, a+n0, n1) / rvv_xor_u64(tb, b, b+n0, n1)
    with inline vle64/vxor/vse64 loop. Uses LMUL policy function.
    Handles the scalar tail element when n0 > n1 (odd-n case).
    """
    # Use flat LMUL
    lmul = _RVV_LMUL

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


def emit_function(n: int, mode: str, threshold: int, leaf_style: str = "noinit") -> str:
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

    if mode in ("rvv", "zvbc"):
        # Direct-write discipline: z0 → r[0..], z2 → r[2*n0..]
        # tmp layout: zmid + ta + tb + child_buf (no z0/z2 slots)
        lines.append("    uint64_t *zmid      = tmp;")
        lines.append(f"    uint64_t *ta        = zmid + {two_n0}u;")
        lines.append(f"    uint64_t *tb        = ta + {n0}u;")
        lines.append(f"    uint64_t *child_buf = tb + {n0}u;")
        lines.append("")
        lines.append(f"    /* Direct-write: z0 → r[0..{two_n0-1}], z2 → r[{two_n0}..{two_n0+two_n1-1}] */")
        lines.append(f"    {child_call('r', 'a', 'b', n0, leaf_fn, threshold, leaf_style, mode)}")
        lines.append(f"    {child_call(f'r + {two_n0}u', f'a + {n0}u', f'b + {n0}u', n1, leaf_fn, threshold, leaf_style, mode)}")
    elif mode == "zbc" and leaf_style == "noinit":
        # Direct-write discipline for scalar: z0 → r[0..], z2 → r[2*n0..]
        # tmp layout: zmid + ta + tb + child_buf (no z0/z2 slots)
        lines.append("    uint64_t *zmid      = tmp;")
        lines.append(f"    uint64_t *ta        = zmid + {two_n0}u;")
        lines.append(f"    uint64_t *tb        = ta + {n0}u;")
        lines.append(f"    uint64_t *child_buf = tb + {n0}u;")
        lines.append("")
        lines.append(f"    /* Direct-write: z0 → r[0..{two_n0-1}], z2 → r[{two_n0}..{two_n0+two_n1-1}] */")
        lines.append(f"    {child_call('r', 'a', 'b', n0, leaf_fn, threshold, leaf_style, mode)}")
        lines.append(f"    {child_call(f'r + {two_n0}u', f'a + {n0}u', f'b + {n0}u', n1, leaf_fn, threshold, leaf_style, mode)}")
    else:
        # Original layout: z0 + z2 + zmid + ta + tb + child_buf in tmp
        lines.append("    uint64_t *z0        = tmp;")
        lines.append(f"    uint64_t *z2        = z0 + {two_n0}u;")
        lines.append(f"    uint64_t *zmid      = z2 + {two_n1}u;")
        lines.append(f"    uint64_t *ta        = zmid + {two_n0}u;")
        lines.append(f"    uint64_t *tb        = ta + {n0}u;")
        lines.append(f"    uint64_t *child_buf = tb + {n0}u;")
        lines.append("")
        lines.append(f"    {child_call('z0', 'a', 'b', n0, leaf_fn, threshold, leaf_style, mode)}")
        lines.append(f"    {child_call('z2', f'a + {n0}u', f'b + {n0}u', n1, leaf_fn, threshold, leaf_style, mode)}")
    lines.append("")

    # ta/tb preparation: ta[i] = a[i] ^ a[n0+i], tb[i] = b[i] ^ b[n0+i]
    if mode in ("rvv", "zvbc"):
        # Hybrid: use scalar loops at bottom levels to avoid pipeline transitions
        if n <= RVV_HYBRID_THRESHOLD:
            _scalar_inline_xor_prep(lines, n0, n1)
        else:
            _helper_xor_prep(lines, n0, n1)
    elif mode == "rvv_noD":
        # Same hybrid as rvv: RVV ta/tb prep at upper levels, scalar at bottom
        if n <= RVV_HYBRID_THRESHOLD:
            _scalar_inline_xor_prep(lines, n0, n1)
        else:
            _emit_inline_xor_prep(lines, n0, n1)
    elif mode == "zbc" and leaf_style == "noinit":
        _scalar_helper_xor_prep(lines, n0, n1)
    else:
        lines.append(f"    for (size_t i = 0; i < {n0}u; i++) {{")
        lines.append(f"        ta[i] = a[i] ^ (i < {n1}u ? a[{n0}u + i] : 0u);")
        lines.append(f"        tb[i] = b[i] ^ (i < {n1}u ? b[{n0}u + i] : 0u);")
        lines.append("    }")
    lines.append("")
    lines.append(f"    {child_call('zmid', 'ta', 'tb', n0, leaf_fn, threshold, leaf_style, mode)}")
    lines.append("")

    if mode in ("rvv", "zvbc"):
        # Hybrid: use scalar recomposition at bottom levels, helper calls at upper
        if n <= RVV_HYBRID_THRESHOLD:
            _scalar_recomposition_directwrite(lines, n0, n1)
        else:
            _helper_recomposition_directwrite(lines, n0, n1)
    elif mode == "rvv_noD":
        # Ref layout + RVV vectorized loops (V without D)
        if n <= RVV_HYBRID_THRESHOLD:
            _manual_loop_recomposition(lines, n0, n1)
        else:
            _rvv_recomposition_noD(lines, n0, n1)
    else:
        # zbc mode: select recomposition style based on leaf_style flag.
        # leaf_style="noinit" → direct-write via scalar helper calls
        # leaf_style="memset" → memset + XOR-accumulate (B off, reference style)
        if leaf_style == "memset":
            _manual_loop_recomposition_memset(lines, n0, n1)
        else:
            _scalar_helper_recomposition_directwrite(lines, n0, n1)

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_header(mode: str, threshold: int, leaf_style: str = "noinit",
                    guard_override: str | None = None) -> str:
    cfg = _MODE_CONFIG[mode]
    guard = cfg["guard"]

    root_ns = ROOT_NS

    sizes: set[int] = set()
    for root in root_ns:
        collect_sizes(root, sizes, threshold)
    ordered = sorted(sizes)

    # For memset leaf style, adjust guard to avoid collision with noinit header
    # (only when no explicit output path overrides the default file)
    if guard_override is not None:
        guard = guard_override
    elif leaf_style == "memset":
        guard = guard.replace("_GEN_H", "_MEMSET_GEN_H")

    out: list[str] = []
    out.append(f"#ifndef {guard}")
    out.append(f"#define {guard}")
    out.append("")
    out.append("#include <stddef.h>")
    out.append("#include <stdint.h>")
    if leaf_style == "memset" and mode in ("zbc",):
        out.append("#include <string.h>")
    out.append("")
    out.append(f"/* Auto-generated by scripts/gen/gen_karatsuba_specialized.py --mode {mode} --leaf-style {leaf_style} --threshold {threshold}. */")
    out.append("/* Fixed-size recursion roots: 277, 561, 901. */")
    if mode == "zbc" and leaf_style == "noinit":
        out.append("/* Leaf specializations (clmul_leaf_n8/n9) use assign-on-first-write discipline. */")
    out.append("")

    # Emit LEAF_CALL ablation dispatch macros for zbc mode (both noinit and memset)
    if mode == "zbc":
        out.append("/* Leaf dispatch: when ABLATION_NOINIT=1 (or not in ablation mode),")
        out.append(" * use fixed-size specialized leaves (part of the B assign-on-first-write policy).")
        out.append(" * In ablation mode with NOINIT=0, fall back to generic clmul_leaf. */")
        out.append("#if !defined(ABLATION_NOINIT) || ABLATION_NOINIT")
        out.append("#define LEAF_CALL_N9(r, a, b)    clmul_leaf_n9((r), (a), (b))")
        out.append("#define LEAF_CALL_N8(r, a, b)    clmul_leaf_n8((r), (a), (b))")
        if leaf_style == "noinit":
            out.append("#define LEAF_CALL_N14(r, a, b)   clmul_leaf_n14((r), (a), (b))")
            out.append("#define LEAF_CALL_N15(r, a, b)   clmul_leaf_n15((r), (a), (b))")
        out.append("#else")
        out.append("#define LEAF_CALL_N9(r, a, b)    clmul_leaf((r), (a), (b), 9u)")
        out.append("#define LEAF_CALL_N8(r, a, b)    clmul_leaf((r), (a), (b), 8u)")
        if leaf_style == "noinit":
            out.append("#define LEAF_CALL_N14(r, a, b)   clmul_leaf((r), (a), (b), 14u)")
            out.append("#define LEAF_CALL_N15(r, a, b)   clmul_leaf((r), (a), (b), 15u)")
        out.append("#endif")
        out.append("")

    for n in ordered:
        out.append(emit_function(n, mode, threshold, leaf_style))

    out.append("#endif")
    out.append("")
    return "\n".join(out)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate fixed-size Karatsuba kernel header for a specific ISA backend."
    )
    parser.add_argument(
        "--mode",
        choices=["zbc", "rvv", "rvv_noD", "zvbc"],
        required=True,
        help="Backend mode: zbc (scalar clmul), rvv (RVV direct-write), rvv_noD (RVV vectorized recomposition without direct-write), zvbc (Zvbc vclmul)",
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
    parser.add_argument(
        "--leaf-style",
        choices=["noinit", "memset"],
        default="noinit",
        help="Leaf function style: noinit (assign-on-first-write, uses clmul_leaf_n8/n9) "
             "or memset (zeroing leaf, uses generic clmul_leaf for all sizes)",
    )
    parser.add_argument(
        "--rvv-hybrid-threshold",
        type=int,
        default=None,
        help="RVV hybrid threshold: functions with n <= this use scalar loops (default: 29)",
    )
    parser.add_argument(
        "--split",
        choices=["ap2", "symmetric"],
        default="ap2",
        help="Split rule: ap2 (n0=ceil(n/2), n1=floor(n/2)) or symmetric (n0=floor(n/2), n1=n-n0). Both produce identical recursion trees.",
    )
    args = parser.parse_args()

    validate_threshold(args.threshold)

    # Allow overriding split mode from CLI
    global _SPLIT_MODE
    _SPLIT_MODE = args.split

    # Allow overriding RVV_HYBRID_THRESHOLD from CLI
    if args.rvv_hybrid_threshold is not None:
        global RVV_HYBRID_THRESHOLD
        RVV_HYBRID_THRESHOLD = args.rvv_hybrid_threshold

    if args.output is not None:
        out_path = Path(args.output)
        # Derive guard from filename when output is explicitly specified
        stem = out_path.stem.upper().replace("-", "_").replace(".", "_")
        guard_override = f"HQC_{stem}_H"
    else:
        out_path = _MODE_CONFIG[args.mode]["out"]
        guard_override = None
        # For memset leaf style in zbc mode, use a separate output file
        if args.leaf_style == "memset" and args.mode == "zbc":
            out_path = out_path.parent / "karatsuba_specialized_memset_gen.h"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(generate_header(args.mode, args.threshold, args.leaf_style, guard_override))
    print(f"Generated {out_path}")


if __name__ == "__main__":
    main()
