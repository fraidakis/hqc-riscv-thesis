# Decode RVV Ablation Study

Generated: 2026-06-30 18:39
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_zb` (scalar RM + Zbc-optimized RS via src/common/gf.c)

Optimizations (layered incrementally):
- **E** — RVV-vectorized `expand_and_sum` (vid + vsrl + vand, SEW=32, LMUL=4)
- **H** — RVV-vectorized `hadamard` FHT (vlse16 strided loads, SEW=16, LMUL=4)
- **P** — RVV-vectorized `find_peaks` (vredmaxu + vmseq + vfirst, LMUL=8)
- **RS** — RVV-vectorized Reed-Solomon syndrome computation (vrgather log/exp)

Note: E, H, P steps use Zbc-optimized scalar RS. The final `rvv` step
replaces scalar RS with vectorized RS (reed_solomon_rvv.c).

## HQC-128

| Variant | Optimizations | median cycles | vs base | vs prev | instret | vs base | vs prev |
|---------|---------------|-------------:|--------:|--------:|--------:|--------:|--------:|
| `scalar_zb` | (none — scalar Zbc baseline) | 455,222 | ×1.0 | — | 613,806 | ×1.0 | — |
| `rm_rvv_E` | E | 387,397 | ×1.2 | ×1.2 | 316,951 | ×1.9 | ×1.9 |
| `rm_rvv_EH` | E+H | 275,160 | ×1.7 | ×1.4 | 255,000 | ×2.4 | ×1.2 |
| `rm_rvv_EHP` | E+H+P | 225,525 | ×2.0 | ×1.2 | 184,473 | ×3.3 | ×1.4 |
| `rvv` | E+H+P+RS (champion) | 204,690 | ×2.2 | ×1.1 | 141,148 | ×4.3 | ×1.3 |

## HQC-192

| Variant | Optimizations | median cycles | vs base | vs prev | instret | vs base | vs prev |
|---------|---------------|-------------:|--------:|--------:|--------:|--------:|--------:|
| `scalar_zb` | (none — scalar Zbc baseline) | 636,642 | ×1.0 | — | 874,421 | ×1.0 | — |
| `rm_rvv_E` | E | 511,337 | ×1.2 | ×1.2 | 401,148 | ×2.2 | ×2.2 |
| `rm_rvv_EH` | E+H | 373,036 | ×1.7 | ×1.4 | 323,313 | ×2.7 | ×1.2 |
| `rm_rvv_EHP` | E+H+P | 311,387 | ×2.0 | ×1.2 | 236,755 | ×3.7 | ×1.4 |
| `rvv` | E+H+P+RS (champion) | 279,469 | ×2.3 | ×1.1 | 179,401 | ×4.9 | ×1.3 |

## HQC-256

| Variant | Optimizations | median cycles | vs base | vs prev | instret | vs base | vs prev |
|---------|---------------|-------------:|--------:|--------:|--------:|--------:|--------:|
| `scalar_zb` | (none — scalar Zbc baseline) | 1,166,454 | ×1.0 | — | 1,611,069 | ×1.0 | — |
| `rm_rvv_E` | E | 953,036 | ×1.2 | ×1.2 | 806,630 | ×2.0 | ×2.0 |
| `rm_rvv_EH` | E+H | 728,473 | ×1.6 | ×1.3 | 691,695 | ×2.3 | ×1.2 |
| `rm_rvv_EHP` | E+H+P | 627,942 | ×1.9 | ×1.2 | 546,947 | ×2.9 | ×1.3 |
| `rvv` | E+H+P+RS (champion) | 547,224 | ×2.1 | ×1.1 | 378,751 | ×4.3 | ×1.4 |

## Incremental Optimization Contribution

| Transition | What changes | Vectorized sub-function |
|------------|--------------|-------------------------|
| scalar_zb→E | +RVV bit extraction | `expand_and_sum`: vid + vsrl.vv + vand (SEW=32, LMUL=4, vl=32) |
| E→EH | +RVV strided-load FHT | `hadamard`: vlse16 + vadd/vsub (SEW=16, LMUL=4, vl=64) |
| EH→EHP | +RVV max+argmax | `find_peaks`: vredmaxu + vmseq + vfirst (LMUL=8, vl=128) |
| EHP→rvv | +RVV RS syndromes | `reed_solomon`: vrgather log/exp table vectorization |
