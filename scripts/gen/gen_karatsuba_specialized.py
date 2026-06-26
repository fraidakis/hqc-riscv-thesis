#!/usr/bin/env python3
"""Generate fixed-size Karatsuba kernel headers for HQC poly-mul variants.

Supports four backends via --mode:
  --mode zbc      Scalar zbc: clmul_leaf / clmul_leaf_n8, manual-loop recomposition
  --mode rvv      RVV: direct-write z0/z2 to r, inline RVV intrinsics, tuned LMUL
  --mode zvbc     Zvbc vector: vclmul_leaf, manual-loop recomposition
  --mode deferred Deferred two-phase: scalar leaves → flat buffer, then single RVV assembly pass

Split rule:
  n0 = ceil(n/2), n1 = floor(n/2)
Stop recursion at n <= 16.

Usage:
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zbc
  python3 scripts/gen/gen_karatsuba_specialized.py --mode rvv
  python3 scripts/gen/gen_karatsuba_specialized.py --mode zvbc
  python3 scripts/gen/gen_karatsuba_specialized.py --mode deferred
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT_NS = [277, 561, 901]
DEFERRED_ROOT_NS = [277]  # Requirement 9: HQC-128 only for initial scope

# Hybrid RVV threshold: functions with n <= this use scalar recomposition loops
# to avoid scalar↔vector pipeline transitions at the bottom recursion levels.
# At these sizes the RVV loops process ≤2 iterations and transition overhead dominates.
RVV_HYBRID_THRESHOLD = 18

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
    "deferred": {
        "out": REPO_ROOT / "src/mult/karatsuba_rvv_deferred/karatsuba_rvv_deferred_specialized_gen.h",
        "guard": "HQC_KARATSUBA_RVV_DEFERRED_SPECIALIZED_GEN_H",
        "leaf_fn": "scalar",
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


# ---------------------------------------------------------------------------
# Deferred mode: FlatBufferAllocator and tree traversal
# ---------------------------------------------------------------------------


class FlatBufferAllocator:
    """Bump allocator for flat buffer offset assignment.

    Each call to alloc(words) returns the current offset and advances
    the internal pointer by `words` positions.
    """

    def __init__(self) -> None:
        self.offset: int = 0

    def alloc(self, words: int) -> int:
        """Allocate `words` uint64_t slots and return the starting offset."""
        off = self.offset
        self.offset += words
        return off


@dataclass
class DeferredNode:
    """Represents one inner node of the Karatsuba recursion tree for deferred mode."""

    n: int  # Total size at this node
    n0: int  # ceil(n/2) — low-half size
    n1: int  # floor(n/2) — high-half size
    z0_off: int = 0  # Flat buffer offset for z0 product (2*n0 words)
    z2_off: int = 0  # Flat buffer offset for z2 product (2*n1 words)
    zmid_off: int = 0  # Flat buffer offset for zmid product (2*n0 words)
    out_off: int = 0  # Output offset in flat_buf (parent's z0/z2/zmid region)
    depth: int = 0  # Tree depth (0 = deepest leaves)
    is_root: bool = False
    descriptor_idx: int = -1  # Post-order (bottom-up) index in descriptor table
    children: list["DeferredNode"] = field(default_factory=list)


def build_deferred_tree(
    root_n: int, threshold: int
) -> tuple[list[DeferredNode], int]:
    """Walk the Karatsuba recursion tree and compute flat buffer offsets.

    Returns:
        A tuple of (nodes_in_bottom_up_order, total_flat_buf_words).
        Each node has z0_off, z2_off, zmid_off assigned via bump allocation,
        and descriptor_idx assigned in post-order (bottom-up) so that children
        always have lower indices than their parents.
    """
    allocator = FlatBufferAllocator()

    def _build(n: int) -> DeferredNode | None:
        """Recursively build the tree, allocating flat buffer regions."""
        if n <= threshold:
            return None  # Leaf — no descriptor entry needed

        n0, n1 = split_sizes(n)
        node = DeferredNode(n=n, n0=n0, n1=n1)

        # Allocate flat buffer regions for this node's products
        node.z0_off = allocator.alloc(2 * n0)
        node.z2_off = allocator.alloc(2 * n1)
        node.zmid_off = allocator.alloc(2 * n0)

        # Recurse into children (z0 child, z2 child, zmid child)
        z0_child = _build(n0)
        z2_child = _build(n1)
        zmid_child = _build(n0)

        if z0_child is not None:
            node.children.append(z0_child)
        if z2_child is not None:
            node.children.append(z2_child)
        if zmid_child is not None:
            node.children.append(zmid_child)

        return node

    root = _build(root_n)
    if root is None:
        return [], 0

    root.is_root = True

    # Assign descriptor indices in post-order (bottom-up) and compute depths
    all_nodes: list[DeferredNode] = []

    def _assign_postorder(node: DeferredNode) -> int:
        """Assign post-order indices and compute depth. Returns max child depth + 1."""
        max_child_depth = -1
        for child in node.children:
            child_depth = _assign_postorder(child)
            if child_depth > max_child_depth:
                max_child_depth = child_depth

        node.depth = max_child_depth + 1
        node.descriptor_idx = len(all_nodes)
        all_nodes.append(node)
        return node.depth

    _assign_postorder(root)

    # Assign out_off for non-root nodes: each child's output goes into
    # the parent's z0/z2/zmid region in flat_buf.
    def _assign_out_offsets(node: DeferredNode) -> None:
        """Set out_off for children based on parent's buffer regions."""
        n0, n1 = node.n0, node.n1
        child_idx = 0

        # z0 child (computes into parent's z0 region)
        if n0 > threshold:
            child = node.children[child_idx]
            child.out_off = node.z0_off
            _assign_out_offsets(child)
            child_idx += 1

        # z2 child (computes into parent's z2 region)
        if n1 > threshold:
            child = node.children[child_idx]
            child.out_off = node.z2_off
            _assign_out_offsets(child)
            child_idx += 1

        # zmid child (computes into parent's zmid region)
        if n0 > threshold:
            child = node.children[child_idx]
            child.out_off = node.zmid_off
            _assign_out_offsets(child)
            child_idx += 1

    _assign_out_offsets(root)

    return all_nodes, allocator.offset


def compute_ta_tb_buf_size(root_n: int, threshold: int) -> int:
    """Compute the ta_tb_buf size needed for the deepest recursion path.

    The scalar phase needs ta+tb space at each active recursion level.
    Since recursion is sequential (pre-order traversal), we need the sum
    of ta+tb along the deepest path from root to the last inner node
    before leaves.
    """
    total = 0
    n = root_n
    while n > threshold:
        n0, _ = split_sizes(n)
        # At this level, ta needs n0 words and tb needs n0 words
        total += 2 * n0
        # Follow the deepest path (always the z0/zmid child which has size n0)
        n = n0
    return total


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
        # All zbc modes + memset style: use generic clmul_leaf
        return f"clmul_leaf({dst}, {xa}, {xb}, {n}u);"
    return f"kmul_{n}({dst}, {xa}, {xb}, child_buf);"


def _manual_loop_recomposition(lines: list[str], n0: int, n1: int) -> None:
    """Emit scalar manual-loop recomposition with direct-write (noinit) discipline.

    Assign-on-first-write: Loop 1 and 2 use '=' (direct assign), Loop 3 uses '^='.
    No memset needed — every position in r[] is written before being read.
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
    lines.append(f"        const uint64_t *_cp_src = r + {n0}u;")
    lines.append(f"        uint64_t *_cp_dst = ta;")
    lines.append(f"        for (size_t _i = 0; _i < {n0}u; _i++)")
    lines.append("            _cp_dst[_i] = _cp_src[_i];")
    lines.append("    }")
    lines.append("")

    # --- Loop 3A first half: i in [0, n0) — z0[i]=r[i] is safe (below dst) ---
    lines.append("    /* Loop 3A first half: i in [0, n0) — scalar XOR3 */")
    lines.append("    {")
    lines.append(f"        uint64_t *_dst = r + {n0}u;")
    lines.append(f"        const uint64_t *_sm = zmid;")
    lines.append(f"        const uint64_t *_s0 = r;")
    lines.append(f"        const uint64_t *_s2 = r + {two_n0}u;")
    lines.append(f"        for (size_t _i = 0; _i < {n0}u; _i++)")
    lines.append("            _dst[_i] ^= _sm[_i] ^ _s0[_i] ^ _s2[_i];")
    lines.append("    }")

    if second_half_count > 0:
        lines.append("")
        lines.append(f"    /* Loop 3A second half: i in [n0, 2*n1) — z0[i]=ta[i-n0] (saved copy) */")
        lines.append("    {")
        lines.append(f"        uint64_t *_dst = r + {two_n0}u;")
        lines.append(f"        const uint64_t *_sm = zmid + {n0}u;")
        lines.append(f"        const uint64_t *_s0 = ta;")
        lines.append(f"        const uint64_t *_s2 = r + {3 * n0}u;")
        lines.append(f"        for (size_t _i = 0; _i < {second_half_count}u; _i++)")
        lines.append("            _dst[_i] ^= _sm[_i] ^ _s0[_i] ^ _s2[_i];")
        lines.append("    }")

    if two_n0 > two_n1:
        rem = two_n0 - two_n1
        rm_off = n0 + two_n1
        ta_off = two_n1 - n0
        lines.append("")
        lines.append("    /* Loop 3B: odd-n tail where z2 is absent, z0 from saved ta */")
        lines.append("    {")
        lines.append(f"        uint64_t *_dst = r + {rm_off}u;")
        lines.append(f"        const uint64_t *_sm = zmid + {two_n1}u;")
        lines.append(f"        const uint64_t *_s0 = ta + {ta_off}u;")
        lines.append(f"        for (size_t _i = 0; _i < {rem}u; _i++)")
        lines.append("            _dst[_i] ^= _sm[_i] ^ _s0[_i];")
        lines.append("    }")


def _scalar_inline_xor_prep(lines: list[str], n0: int, n1: int) -> None:
    """Emit scalar inline XOR preparation for ta/tb (hybrid mode).

    Same as _emit_inline_xor_prep but uses plain scalar loops instead of RVV.
    """
    lines.append(f"    /* ta/tb preparation: scalar XOR (n1={n1}) */")
    lines.append("    {")
    lines.append(f"        for (size_t _i = 0; _i < {n1}u; _i++) {{")
    lines.append(f"            ta[_i] = a[_i] ^ a[{n0}u + _i];")
    lines.append(f"            tb[_i] = b[_i] ^ b[{n0}u + _i];")
    lines.append("        }")
    if n0 > n1:
        lines.append(f"        ta[{n1}u] = a[{n1}u];")
        lines.append(f"        tb[{n1}u] = b[{n1}u];")
    lines.append("    }")


def _emit_inline_xor_prep(lines: list[str], n0: int, n1: int) -> None:
    """Emit inline RVV intrinsics for ta/tb XOR preparation (rvv mode).

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

    if mode == "rvv":
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
    # For the common part (0..n1-1), use inline intrinsics (rvv) or rvv_xor_u64 (zvbc).
    # For the odd tail (n1..n0-1), use scalar copy (at most 1 element).
    if mode == "rvv":
        # Hybrid: use scalar loops at bottom levels to avoid pipeline transitions
        if n <= RVV_HYBRID_THRESHOLD:
            _scalar_inline_xor_prep(lines, n0, n1)
        else:
            _emit_inline_xor_prep(lines, n0, n1)
    elif mode == "zvbc":
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
    lines.append(f"    {child_call('zmid', 'ta', 'tb', n0, leaf_fn, threshold, leaf_style, mode)}")
    lines.append("")

    if mode == "rvv":
        # Hybrid: use scalar recomposition at bottom levels
        if n <= RVV_HYBRID_THRESHOLD:
            _scalar_recomposition_directwrite(lines, n0, n1)
        else:
            _rvv_recomposition(lines, n0, n1)
    elif mode == "zvbc":
        _manual_loop_recomposition(lines, n0, n1)
    else:
        # zbc mode: select recomposition style based on leaf_style flag.
        # leaf_style="noinit" → direct-write (B optimization active)
        # leaf_style="memset" → memset + XOR-accumulate (B off, reference style)
        if leaf_style == "memset":
            _manual_loop_recomposition_memset(lines, n0, n1)
        else:
            _manual_loop_recomposition(lines, n0, n1)

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_header(mode: str, threshold: int, leaf_style: str = "noinit",
                    guard_override: str | None = None) -> str:
    cfg = _MODE_CONFIG[mode]
    guard = cfg["guard"]

    # Requirement 9: limit root sizes to [277] for deferred mode
    root_ns = DEFERRED_ROOT_NS if mode == "deferred" else ROOT_NS

    if mode == "deferred":
        return _generate_deferred_header(guard, threshold, root_ns)

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
    out.append("")

    for n in ordered:
        out.append(emit_function(n, mode, threshold, leaf_style))

    out.append("#endif")
    out.append("")
    return "\n".join(out)


def _generate_deferred_header(guard: str, threshold: int, root_ns: list[int]) -> str:
    """Generate the deferred two-phase header.

    This builds the recursion tree, computes flat buffer offsets, and emits:
    - The deferred_descriptor_t typedef
    - Buffer size #defines and budget check
    - The static descriptor table
    - The scalar phase function
    - The vector assembly function
    """
    out: list[str] = []
    out.append(f"#ifndef {guard}")
    out.append(f"#define {guard}")
    out.append("")
    out.append("#include <stddef.h>")
    out.append("#include <stdint.h>")
    out.append("")
    out.append(f"/* Auto-generated by scripts/gen/gen_karatsuba_specialized.py --mode deferred --threshold {threshold}. */")
    out.append(f"/* Deferred two-phase Karatsuba: scalar leaves → flat buffer → single RVV assembly pass. */")
    out.append(f"/* Root sizes (HQC-128 only): {root_ns} */")
    out.append("")

    # Emit the deferred_descriptor_t typedef
    _emit_descriptor_typedef(out)

    for root_n in root_ns:
        nodes, total_flat_words = build_deferred_tree(root_n, threshold)
        ta_tb_words = compute_ta_tb_buf_size(root_n, threshold)

        # Emit buffer size defines and budget check
        _emit_buffer_defines(out, root_n, total_flat_words, ta_tb_words)

        # Emit the static descriptor table
        _emit_descriptor_table(out, root_n, nodes)

        # Emit the scalar phase function (task 1.3)
        _emit_scalar_phase(out, root_n, threshold)

        # Emit the vector assembly function (task 1.4)
        _emit_vector_assembly(out, root_n)

    out.append("#endif")
    out.append("")
    return "\n".join(out)


def _emit_descriptor_typedef(out: list[str]) -> None:
    """Emit the deferred_descriptor_t struct typedef."""
    out.append("/* Descriptor for one inner node of the Karatsuba recursion tree. */")
    out.append("typedef struct {")
    out.append("    uint16_t out_off;   /* Output offset (words) into flat_buf, or 0 for root */")
    out.append("    uint16_t z0_off;    /* z0 product offset (words) into flat_buf */")
    out.append("    uint16_t z2_off;    /* z2 product offset (words) into flat_buf */")
    out.append("    uint16_t zmid_off;  /* zmid product offset (words) into flat_buf */")
    out.append("    uint8_t  n0;        /* ceil(n/2) — low-half size */")
    out.append("    uint8_t  n1;        /* floor(n/2) — high-half size */")
    out.append("    uint8_t  depth;     /* Tree depth (0=deepest, used for debugging) */")
    out.append("    uint8_t  is_root;   /* 1 if root node (output → unreduced[]) */")
    out.append("} deferred_descriptor_t;")
    out.append("")


def _emit_buffer_defines(out: list[str], root_n: int, total_flat_words: int, ta_tb_words: int) -> None:
    """Emit #define macros for buffer sizes and the compile-time budget check."""
    out.append(f"#define DEFERRED_FLAT_BUF_WORDS_{root_n}  {total_flat_words}u")
    out.append(f"#define DEFERRED_TA_TB_BUF_WORDS_{root_n} {ta_tb_words}u")
    out.append("")
    out.append(f"#if (DEFERRED_FLAT_BUF_WORDS_{root_n} + 2u * {root_n}u) * 8u > (512u * 1024u)")
    out.append(f'#error "Deferred flat buffer exceeds 512 KB stack budget for n={root_n}"')
    out.append("#endif")
    out.append("")


def _emit_descriptor_table(out: list[str], root_n: int, nodes: list[DeferredNode]) -> None:
    """Emit the static const descriptor table with all entries ordered bottom-up."""
    count = len(nodes)
    out.append(f"#define DESCRIPTOR_COUNT_{root_n} {count}u")
    out.append("")
    out.append(f"static const deferred_descriptor_t descriptor_table_{root_n}[{count}] = {{")

    for i, node in enumerate(nodes):
        is_root_val = 1 if node.is_root else 0
        trailing = "," if i < count - 1 else ""
        out.append(
            f"    /* [{i:3d}] n={node.n:3d} */ "
            f"{{ {node.out_off:5d}, {node.z0_off:5d}, {node.z2_off:5d}, {node.zmid_off:5d}, "
            f"{node.n0:3d}, {node.n1:3d}, {node.depth}, {is_root_val} }}{trailing}"
        )

    out.append("};")
    out.append("")


# ---------------------------------------------------------------------------
# Deferred mode: Scalar phase emission (Task 1.3)
# ---------------------------------------------------------------------------


def _emit_scalar_phase(out: list[str], root_n: int, threshold: int) -> None:
    """Emit the fully-unrolled scalar_phase_<root_n>() function.

    Performs a pre-order traversal of the recursion tree, emitting:
    - Scalar XOR loops computing ta/tb at each inner node
    - clmul_leaf / clmul_leaf_n8 calls at each leaf

    The ta_tb_buf is partitioned by recursion depth: each depth level
    gets a fixed region so that parent ta/tb remains valid while
    processing children.

    Zero RVV intrinsics or vector type declarations are emitted.
    """
    # First, compute the ta_tb_buf partition offsets per depth level.
    # At each depth, we need 2*n0 words (n0 for ta, n0 for tb).
    # We follow the deepest path to determine the maximum n0 at each level.
    ta_tb_offsets: list[int] = []  # ta_tb_offsets[depth] = offset into ta_tb_buf
    offset = 0
    n = root_n
    while n > threshold:
        ta_tb_offsets.append(offset)
        n0, _ = split_sizes(n)
        offset += 2 * n0  # ta needs n0, tb needs n0
        n = n0

    out.append(f"/* Scalar phase: fully-unrolled pre-order traversal for n={root_n}. */")
    out.append(f"/* Executes all leaf multiplications writing to flat_buf. */")
    out.append(f"/* Contains ZERO RVV intrinsics or vector type declarations. */")
    out.append(f"static inline void scalar_phase_{root_n}(uint64_t *restrict flat_buf,")
    out.append(f"        const uint64_t *restrict a,")
    out.append(f"        const uint64_t *restrict b,")
    out.append(f"        uint64_t *restrict ta_tb_buf)")
    out.append("{")

    # We need to rebuild the tree to walk it in pre-order and emit code.
    # Use a separate allocator to get the same offsets as build_deferred_tree.
    allocator = FlatBufferAllocator()
    leaf_count = [0]  # mutable counter for tracking leaf calls

    def _emit_node(n: int, a_expr: str, b_expr: str, depth: int, indent: str) -> None:
        """Recursively emit code for one node in pre-order.

        Args:
            n: Size of this subtree
            a_expr: C expression for the 'a' pointer at this node
            b_expr: C expression for the 'b' pointer at this node
            depth: Current recursion depth (0 = root)
            indent: Indentation string for emitted code
        """
        if n <= threshold:
            # Leaf node: emit clmul_leaf call
            # The leaf writes 2*n words into flat_buf at the current allocation offset.
            # But wait — for leaves, the offset is determined by the PARENT's z0/z2/zmid
            # allocation. The parent already allocated the region and we're writing into it.
            # So the offset is passed in via the caller context.
            # Actually, we need to think about this differently:
            # The flat buffer allocation happens in pre-order. For inner nodes, we allocate
            # z0, z2, zmid regions. Leaf calls write directly into those parent regions.
            # So the leaf's destination is the parent's z0_off, z2_off, or zmid_off.
            # This is handled by the caller passing the correct flat_buf offset.
            raise RuntimeError("Leaf nodes should not be reached via _emit_node directly")

        n0, n1 = split_sizes(n)

        # Allocate flat buffer regions for this node (must match build_deferred_tree order)
        z0_off = allocator.alloc(2 * n0)
        z2_off = allocator.alloc(2 * n1)
        zmid_off = allocator.alloc(2 * n0)

        # Get ta_tb_buf offset for this depth
        ta_tb_off = ta_tb_offsets[depth]
        ta_expr = f"ta_tb_buf + {ta_tb_off}u"
        tb_expr = f"ta_tb_buf + {ta_tb_off + n0}u"

        out.append(f"{indent}/* depth={depth}, n={n}, n0={n0}, n1={n1} */")
        out.append(f"{indent}/* z0_off={z0_off}, z2_off={z2_off}, zmid_off={zmid_off} */")

        # Emit ta/tb XOR computation
        # Compute simplified pointer expressions for a_hi and b_hi
        a_hi_full = _simplify_ptr_expr(a_expr, n0)
        b_hi_full = _simplify_ptr_expr(b_expr, n0)

        out.append(f"{indent}{{")
        out.append(f"{indent}    uint64_t *ta = {ta_expr};")
        out.append(f"{indent}    uint64_t *tb = {tb_expr};")
        out.append(f"{indent}    const uint64_t *a_lo = {a_expr};")
        out.append(f"{indent}    const uint64_t *a_hi = {a_hi_full};")
        out.append(f"{indent}    const uint64_t *b_lo = {b_expr};")
        out.append(f"{indent}    const uint64_t *b_hi = {b_hi_full};")

        if n0 == n1:
            # Even split: all elements have both halves
            out.append(f"{indent}    for (size_t i = 0; i < {n1}u; i++) {{")
            out.append(f"{indent}        ta[i] = a_lo[i] ^ a_hi[i];")
            out.append(f"{indent}        tb[i] = b_lo[i] ^ b_hi[i];")
            out.append(f"{indent}    }}")
        else:
            # Odd split: n0 = n1 + 1, last element has no high counterpart
            out.append(f"{indent}    for (size_t i = 0; i < {n1}u; i++) {{")
            out.append(f"{indent}        ta[i] = a_lo[i] ^ a_hi[i];")
            out.append(f"{indent}        tb[i] = b_lo[i] ^ b_hi[i];")
            out.append(f"{indent}    }}")
            out.append(f"{indent}    ta[{n1}u] = a_lo[{n1}u];")
            out.append(f"{indent}    tb[{n1}u] = b_lo[{n1}u];")

        out.append(f"{indent}}}")

        # Now emit the three child calls (z0, z2, zmid) in pre-order
        # z0 child: multiply a[0..n0-1] * b[0..n0-1] → flat_buf + z0_off
        _emit_child(n0, a_expr, b_expr, z0_off, depth + 1, indent)

        # z2 child: multiply a[n0..n-1] * b[n0..n-1] → flat_buf + z2_off
        a_hi_expr = _simplify_ptr_expr(a_expr, n0)
        b_hi_expr = _simplify_ptr_expr(b_expr, n0)
        _emit_child(n1, a_hi_expr, b_hi_expr, z2_off, depth + 1, indent)

        # zmid child: multiply ta * tb → flat_buf + zmid_off
        _emit_child(n0, ta_expr, tb_expr, zmid_off, depth + 1, indent)

    def _emit_child(n: int, a_expr: str, b_expr: str, dest_off: int, depth: int, indent: str) -> None:
        """Emit code for a child (either a leaf call or recurse into inner node)."""
        if n <= threshold:
            # Leaf: emit clmul_leaf or clmul_leaf_n8
            leaf_count[0] += 1
            if n == 8:
                out.append(f"{indent}clmul_leaf_n8(flat_buf + {dest_off}u, {a_expr}, {b_expr});")
            else:
                out.append(f"{indent}clmul_leaf(flat_buf + {dest_off}u, {a_expr}, {b_expr}, {n}u);")
        else:
            # Inner node: recurse
            _emit_node(n, a_expr, b_expr, depth, indent)

    # Start the pre-order traversal from the root
    _emit_node(root_n, "a", "b", 0, "    ")

    out.append("}")
    out.append(f"/* Total leaf calls emitted: {leaf_count[0]} */")
    out.append("")


def _simplify_ptr_expr(base_expr: str, offset: int) -> str:
    """Create a simplified pointer+offset expression.

    Handles cases like:
      "a" + 139 → "a + 139u"
      "a + 139u" + 70 → "a + 209u"
      "ta_tb_buf + 0u" + 70 → "ta_tb_buf + 70u"
    """
    # Try to detect "base + Xu" pattern and combine offsets
    if " + " in base_expr and base_expr.endswith("u"):
        # Pattern: "something + Nu"
        parts = base_expr.rsplit(" + ", 1)
        if parts[1].endswith("u"):
            try:
                existing_off = int(parts[1][:-1])
                return f"{parts[0]} + {existing_off + offset}u"
            except ValueError:
                pass
    # Simple case: just append offset
    return f"{base_expr} + {offset}u"


# ---------------------------------------------------------------------------
# Deferred mode: Vector assembly emission (Task 1.4)
# ---------------------------------------------------------------------------


def _emit_vector_assembly(out: list[str], root_n: int) -> None:
    """Emit the vector_assembly_<root_n>() function.

    Generates a loop-based function that iterates the descriptor table
    bottom-up (index 0 to DESCRIPTOR_COUNT-1), performing Karatsuba XOR
    assembly for each inner node using RVV helper functions:
      - rvv_copy_u64: vector memcpy (LMUL=m4)
      - rvv_xor3_u64: out[i] ^= zmid[i] ^ z0[i] ^ z2[i] (LMUL=m2)

    For each descriptor entry:
      1. Copy z0 to output[0..2*n0-1]
      2. Copy z2 to output[2*n0..2*n0+2*n1-1]
      3. XOR-accumulate zmid⊕z0⊕z2 into output[n0..n0+2*n1-1]
      4. If n0 > n1 (odd-n tail): XOR-accumulate zmid⊕z0 (z2=NULL) into
         output[n0+2*n1..n0+2*n0-1]

    Output routing:
      - Root node (is_root=1): output → unreduced parameter
      - All others: output → flat_buf + out_off
    """
    out.append(f"/* Vector assembly: bottom-up RVV Karatsuba recombination for n={root_n}. */")
    out.append(f"/* Iterates descriptor_table_{root_n}[0..DESCRIPTOR_COUNT-1] calling RVV helpers. */")
    out.append(f"/* Contains ZERO scalar clmul instructions — pure vector phase. */")
    out.append(f"static inline void vector_assembly_{root_n}(uint64_t *restrict unreduced,")
    out.append(f"        uint64_t *restrict flat_buf)")
    out.append("{")
    out.append(f"    for (size_t i = 0; i < DESCRIPTOR_COUNT_{root_n}; i++) {{")
    out.append(f"        const deferred_descriptor_t *d = &descriptor_table_{root_n}[i];")
    out.append("        const uint8_t n0 = d->n0;")
    out.append("        const uint8_t n1 = d->n1;")
    out.append("        const uint16_t two_n0 = 2u * n0;")
    out.append("        const uint16_t two_n1 = 2u * n1;")
    out.append("")
    out.append("        /* Determine output destination */")
    out.append("        uint64_t *out = d->is_root ? unreduced : (flat_buf + d->out_off);")
    out.append("")
    out.append("        const uint64_t *z0   = flat_buf + d->z0_off;")
    out.append("        const uint64_t *z2   = flat_buf + d->z2_off;")
    out.append("        const uint64_t *zmid = flat_buf + d->zmid_off;")
    out.append("")
    out.append("        /* Step 1: out[0..2*n0-1] = z0 (RVV copy) */")
    out.append("        rvv_copy_u64(out, z0, two_n0);")
    out.append("")
    out.append("        /* Step 2: out[2*n0..2*n0+2*n1-1] = z2 (RVV copy) */")
    out.append("        rvv_copy_u64(out + two_n0, z2, two_n1);")
    out.append("")
    out.append("        /* Step 3: out[n0+i] ^= zmid[i] ^ z0[i] ^ z2[i] for i in [0, 2*n1) */")
    out.append("        rvv_xor3_u64(out + n0, zmid, z0, z2, two_n1);")
    out.append("")
    out.append("        /* Step 4: odd-n tail where z2 is absent (n0 > n1) */")
    out.append("        if (two_n0 > two_n1) {")
    out.append("            rvv_xor3_u64(out + n0 + two_n1, zmid + two_n1, z0 + two_n1, NULL, two_n0 - two_n1);")
    out.append("        }")
    out.append("    }")
    out.append("}")
    out.append("")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate fixed-size Karatsuba kernel header for a specific ISA backend."
    )
    parser.add_argument(
        "--mode",
        choices=["zbc", "rvv", "zvbc", "deferred"],
        required=True,
        help="Backend mode: zbc (scalar clmul), rvv (RVV direct-write), zvbc (Zvbc vclmul), deferred (two-phase scalar+RVV)",
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
        help="RVV hybrid threshold: functions with n <= this use scalar loops (default: 18)",
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
