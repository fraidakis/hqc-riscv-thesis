# Vector RVV Ablation Study (Tier 2)

Generated: 2026-06-26 19:33

Baseline: `vector_rvv_ref` (scalar_zb champion + naive RVV combine with shared helper functions)

Optimizations:
- **A** — Direct-write discipline (children write to output buffer, eliminate copy loops)
- **B** — Overlap-save combine (resolve aliasing hazard from direct-write)
- **C** — Inlined RVV operations with per-operation LMUL tuning
- **D** — Hybrid scalar/vector recursion (scalar loops at lower tree levels)

## HQC-128

| Variant | Optimizations | instret | vs ref | vs prev |
|---------|---------------|--------:|-------:|--------:|
| `vector_rvv_ref` | (none) | 167,576 | ×1.0 | — |
| `vector_rvv_A` | A | 101,794 | ×1.6 | ×1.6 |
| `vector_rvv_AB` | A+B | 101,794 | ×1.6 | ×1.0 |
| `vector_rvv_ABC` | A+B+C | 101,794 | ×1.6 | ×1.0 |
| `vector_rvv` | A+B+C+D | 140,487 | ×1.2 | ×0.7 |

## HQC-192

| Variant | Optimizations | instret | vs ref | vs prev |
|---------|---------------|--------:|-------:|--------:|
| `vector_rvv_ref` | (none) | 534,711 | ×1.0 | — |
| `vector_rvv_A` | A | 309,985 | ×1.7 | ×1.7 |
| `vector_rvv_AB` | A+B | 309,985 | ×1.7 | ×1.0 |
| `vector_rvv_ABC` | A+B+C | 309,985 | ×1.7 | ×1.0 |
| `vector_rvv` | A+B+C+D | 439,516 | ×1.2 | ×0.7 |

## HQC-256

| Variant | Optimizations | instret | vs ref | vs prev |
|---------|---------------|--------:|-------:|--------:|
| `vector_rvv_ref` | (none) | 1,296,605 | ×1.0 | — |
| `vector_rvv_A` | A | 1,014,499 | ×1.3 | ×1.3 |
| `vector_rvv_AB` | A+B | 1,014,499 | ×1.3 | ×1.0 |
| `vector_rvv_ABC` | A+B+C | 1,014,499 | ×1.3 | ×1.0 |
| `vector_rvv` | A+B+C+D | 1,014,499 | ×1.3 | ×1.0 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +direct-write discipline | ×1.65 | ×1.72 | ×1.28 |
| A→AB | +overlap-save combine | ×1.00 | ×1.00 | ×1.00 |
| AB→ABC | +inlined RVV (tuned LMUL) | ×1.00 | ×1.00 | ×1.00 |
| ABC→ABCD | +hybrid scalar/vector recursion | ×0.72 | ×0.71 | ×1.00 |

## Comparison: Tier 2 champion vs Tier 1 champion

| Level | `scalar_zb` (Tier 1) | `vector_rvv` (Tier 2) | RVV speedup |
|-------|---------------------:|----------------------:|------------:|
| HQC-128 | 167,852 | 140,487 | ×1.19 |
| HQC-192 | 530,977 | 439,516 | ×1.21 |
| HQC-256 | 1,824,506 | 1,014,499 | ×1.80 |

