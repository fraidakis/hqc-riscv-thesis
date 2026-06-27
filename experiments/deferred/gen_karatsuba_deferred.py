#!/usr/bin/env python3
"""Generate deferred two-phase Karatsuba kernel header (REJECTED EXPERIMENT).

This is the extracted deferred mode from gen_karatsuba_specialized.py.
See README.md for why this approach was rejected.

Two-phase approach:
  Phase 1 (scalar): All clmul_leaf calls → flat buffer (zero RVV instructions)
  Phase 2 (vector): Single RVV pass iterates descriptor table bottom-up,
                    performing Karatsuba XOR assembly via rvv_copy_u64 / rvv_xor3_u64

Usage:
  python3 experiments/deferred/gen_karatsuba_deferred.py --threshold 16
  python3 experiments/deferred/gen_karatsuba_deferred.py --threshold 16 --output /tmp/header.h
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT_NS = [277]  # HQC-128 only (the only level this was implemented for)


def split_sizes(n: int) -> tuple[int, int]:
    """Split: n0 = ceil(n/2), n1 = floor(n/2)."""
    return (n + 1) // 2, n // 2


# ---------------------------------------------------------------------------
# FlatBufferAllocator and tree traversal
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


# ---------------------------------------------------------------------------
# Header generation
# ---------------------------------------------------------------------------


def generate_deferred_header(guard: str, threshold: int, root_ns: list[int]) -> str:
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
    out.append(f"/* Auto-generated by experiments/deferred/gen_karatsuba_deferred.py --threshold {threshold}. */")
    out.append(f"/* Deferred two-phase Karatsuba: scalar leaves → flat buffer → single RVV assembly pass. */")
    out.append(f"/* Root sizes (HQC-128 only): {root_ns} */")
    out.append("")
    out.append("/* STATUS: REJECTED EXPERIMENT — 25% slower than interleaved rvv mode. */")
    out.append("/* See experiments/deferred/README.md for analysis. */")
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

        # Emit the scalar phase function
        _emit_scalar_phase(out, root_n, threshold)

        # Emit the vector assembly function
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
# Scalar phase emission
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
    ta_tb_offsets: list[int] = []
    offset = 0
    n = root_n
    while n > threshold:
        ta_tb_offsets.append(offset)
        n0, _ = split_sizes(n)
        offset += 2 * n0
        n = n0

    out.append(f"/* Scalar phase: fully-unrolled pre-order traversal for n={root_n}. */")
    out.append(f"/* Executes all leaf multiplications writing to flat_buf. */")
    out.append(f"/* Contains ZERO RVV intrinsics or vector type declarations. */")
    out.append(f"static inline void scalar_phase_{root_n}(uint64_t *restrict flat_buf,")
    out.append(f"        const uint64_t *restrict a,")
    out.append(f"        const uint64_t *restrict b,")
    out.append(f"        uint64_t *restrict ta_tb_buf)")
    out.append("{")

    allocator = FlatBufferAllocator()
    leaf_count = [0]

    def _emit_node(n: int, a_expr: str, b_expr: str, depth: int, indent: str) -> None:
        if n <= threshold:
            raise RuntimeError("Leaf nodes should not be reached via _emit_node directly")

        n0, n1 = split_sizes(n)

        z0_off = allocator.alloc(2 * n0)
        z2_off = allocator.alloc(2 * n1)
        zmid_off = allocator.alloc(2 * n0)

        ta_tb_off = ta_tb_offsets[depth]
        ta_expr = f"ta_tb_buf + {ta_tb_off}u"
        tb_expr = f"ta_tb_buf + {ta_tb_off + n0}u"

        out.append(f"{indent}/* depth={depth}, n={n}, n0={n0}, n1={n1} */")
        out.append(f"{indent}/* z0_off={z0_off}, z2_off={z2_off}, zmid_off={zmid_off} */")

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
            out.append(f"{indent}    for (size_t i = 0; i < {n1}u; i++) {{")
            out.append(f"{indent}        ta[i] = a_lo[i] ^ a_hi[i];")
            out.append(f"{indent}        tb[i] = b_lo[i] ^ b_hi[i];")
            out.append(f"{indent}    }}")
        else:
            out.append(f"{indent}    for (size_t i = 0; i < {n1}u; i++) {{")
            out.append(f"{indent}        ta[i] = a_lo[i] ^ a_hi[i];")
            out.append(f"{indent}        tb[i] = b_lo[i] ^ b_hi[i];")
            out.append(f"{indent}    }}")
            out.append(f"{indent}    ta[{n1}u] = a_lo[{n1}u];")
            out.append(f"{indent}    tb[{n1}u] = b_lo[{n1}u];")

        out.append(f"{indent}}}")

        # z0 child
        _emit_child(n0, a_expr, b_expr, z0_off, depth + 1, indent)
        # z2 child
        a_hi_expr = _simplify_ptr_expr(a_expr, n0)
        b_hi_expr = _simplify_ptr_expr(b_expr, n0)
        _emit_child(n1, a_hi_expr, b_hi_expr, z2_off, depth + 1, indent)
        # zmid child
        _emit_child(n0, ta_expr, tb_expr, zmid_off, depth + 1, indent)

    def _emit_child(n: int, a_expr: str, b_expr: str, dest_off: int, depth: int, indent: str) -> None:
        if n <= threshold:
            leaf_count[0] += 1
            if n == 8:
                out.append(f"{indent}clmul_leaf_n8(flat_buf + {dest_off}u, {a_expr}, {b_expr});")
            else:
                out.append(f"{indent}clmul_leaf(flat_buf + {dest_off}u, {a_expr}, {b_expr}, {n}u);")
        else:
            _emit_node(n, a_expr, b_expr, depth, indent)

    _emit_node(root_n, "a", "b", 0, "    ")

    out.append("}")
    out.append(f"/* Total leaf calls emitted: {leaf_count[0]} */")
    out.append("")


def _simplify_ptr_expr(base_expr: str, offset: int) -> str:
    """Create a simplified pointer+offset expression."""
    if " + " in base_expr and base_expr.endswith("u"):
        parts = base_expr.rsplit(" + ", 1)
        if parts[1].endswith("u"):
            try:
                existing_off = int(parts[1][:-1])
                return f"{parts[0]} + {existing_off + offset}u"
            except ValueError:
                pass
    return f"{base_expr} + {offset}u"


# ---------------------------------------------------------------------------
# Vector assembly emission
# ---------------------------------------------------------------------------


def _emit_vector_assembly(out: list[str], root_n: int) -> None:
    """Emit the vector_assembly_<root_n>() function.

    Generates a loop-based function that iterates the descriptor table
    bottom-up, performing Karatsuba XOR assembly using RVV helpers.
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


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate deferred two-phase Karatsuba kernel header (REJECTED EXPERIMENT)."
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
        help="Output file path (default: karatsuba_rvv_deferred_specialized_gen.h in cwd)",
    )
    args = parser.parse_args()

    t = args.threshold
    if t < 8 or t > 128 or (t & (t - 1)) != 0:
        sys.exit(f"Error: --threshold must be a power of 2 in [8, 128], got {t}")

    guard = "HQC_KARATSUBA_RVV_DEFERRED_SPECIALIZED_GEN_H"
    header = generate_deferred_header(guard, t, ROOT_NS)

    if args.output:
        out_path = Path(args.output)
    else:
        out_path = Path("karatsuba_rvv_deferred_specialized_gen.h")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(header)
    print(f"Generated {out_path}")


if __name__ == "__main__":
    main()
