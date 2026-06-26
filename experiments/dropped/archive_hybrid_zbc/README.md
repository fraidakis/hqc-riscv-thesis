# archive_hybrid_zbc — Abandoned Hybrid Approach

## Status: Archived (superseded by specialized fixed-tree variants)

## What was tried

The `hybrid_zbc` track combined a partial Karatsuba decomposition at the top level
with schoolbook `clmul` leaves, targeting a balance between recursion overhead and
leaf efficiency without full compile-time specialization.

Files in this archive:
- `hybrid.c` — initial prototype
- `hybrid_v2.c` — second iteration with partial loop unrolling
- `hybrid_v3.c` / `hybrid_v3.h` — third iteration with split-size tuning
- `vect_mul_keycache.h` — keycache pre-computation header (shared)

## Why it was abandoned

The specialized fixed-tree Karatsuba (`karatsuba_zbc` and `karatsuba_rvv`)
generates fully unrolled static kernel trees via `scripts/gen/gen_karatsuba_specialized.py`
with `--mode zbc|rvv|zvbc`. This eliminates the function-call overhead, ABI stack
pressure, and compiler spilling that limited the hybrid approach.

Measured vect_mul instret at HQC-128:
- hybrid track: ~600k–900k (varied across v1–v3)
- karatsuba_zbc specialized: **211,841**
- karatsuba_rvv specialized: **157,377**

## Do not re-promote without benchmarking all three HQC security levels

See `experiments/manifest.yaml` and `docs/METHODOLOGY.md` for promotion criteria.
