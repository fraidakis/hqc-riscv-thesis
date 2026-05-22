#!/usr/bin/env python3
"""Generate fixed-size Karatsuba kernel headers for HQC poly-mul variants.

Supports three backends via --mode:
  --mode zbc    Scalar zbc: clmul_leaf / clmul_leaf_n8, manual-loop recomposition
  --mode rvv    RVV base:   clmul_leaf / clmul_leaf_n8, rvv_copy_u64 / rvv_xor3_u64
  --mode zvbc   Zvbc vector: vclmul_leaf, manual-loop recomposition

Split rule (AP-2):
  n0 = ceil(n/2), n1 = floor(n/2)
Stop recursion at n <= 16.

Usage:
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zbc
  python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zvbc
"""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT_NS = [277, 561, 901]
THRESHOLD = 16

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
    "zvbc": {
        "out": REPO_ROOT / "src/mult/karatsuba_zvbc/karatsuba_zvbc_specialized_gen.h",
        "guard": "HQC_KARATSUBA_ZVBC_SPECIALIZED_GEN_H",
        "leaf_fn": "zvbc",
    },
}


def split_sizes(n: int) -> tuple[int, int]:
    return (n + 1) // 2, n // 2


def collect_sizes(n: int, out: set[int]) -> None:
    if n <= THRESHOLD:
        return
    if n in out:
        return
    out.add(n)
    n0, n1 = split_sizes(n)
    collect_sizes(n0, out)
    collect_sizes(n1, out)


def child_call(dst: str, xa: str, xb: str, n: int, leaf_fn: str) -> str:
    if n <= THRESHOLD:
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


def emit_function(n: int, mode: str) -> str:
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
    lines.append("    uint64_t *z0        = tmp;")
    lines.append(f"    uint64_t *z2        = z0 + {two_n0}u;")
    lines.append(f"    uint64_t *zmid      = z2 + {two_n1}u;")
    lines.append(f"    uint64_t *ta        = zmid + {two_n0}u;")
    lines.append(f"    uint64_t *tb        = ta + {n0}u;")
    lines.append(f"    uint64_t *child_buf = tb + {n0}u;")
    lines.append("")
    lines.append(f"    {child_call('z0', 'a', 'b', n0, leaf_fn)}")
    lines.append(f"    {child_call('z2', f'a + {n0}u', f'b + {n0}u', n1, leaf_fn)}")
    lines.append("")
    lines.append(f"    for (size_t i = 0; i < {n0}u; i++) {{")
    lines.append(f"        ta[i] = a[i] ^ (i < {n1}u ? a[{n0}u + i] : 0u);")
    lines.append(f"        tb[i] = b[i] ^ (i < {n1}u ? b[{n0}u + i] : 0u);")
    lines.append("    }")
    lines.append("")
    lines.append(f"    {child_call('zmid', 'ta', 'tb', n0, leaf_fn)}")
    lines.append("")

    if mode == "rvv":
        _rvv_recomposition(lines, n0, n1)
    else:
        _manual_loop_recomposition(lines, n0, n1)

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_header(mode: str) -> str:
    cfg = _MODE_CONFIG[mode]
    guard = cfg["guard"]

    sizes: set[int] = set()
    for root in ROOT_NS:
        collect_sizes(root, sizes)
    ordered = sorted(sizes)

    out: list[str] = []
    out.append(f"#ifndef {guard}")
    out.append(f"#define {guard}")
    out.append("")
    out.append("#include <stddef.h>")
    out.append("#include <stdint.h>")
    out.append("")
    out.append(f"/* Auto-generated by scripts/gen/gen_karatsuba_specialized.py --mode {mode}. */")
    out.append("/* Fixed-size recursion roots: 277, 561, 901. */")
    out.append("")

    for n in ordered:
        out.append(emit_function(n, mode))

    out.append("#endif")
    out.append("")
    return "\n".join(out)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate fixed-size Karatsuba kernel header for a specific ISA backend."
    )
    parser.add_argument(
        "--mode",
        choices=["zbc", "rvv", "zvbc"],
        required=True,
        help="Backend mode: zbc (scalar clmul), rvv (RVV base), zvbc (Zvbc vclmul)",
    )
    args = parser.parse_args()

    out_path: Path = _MODE_CONFIG[args.mode]["out"]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(generate_header(args.mode))
    print(f"Generated {out_path}")


if __name__ == "__main__":
    main()
