# RM(1,7) Decode RVV Ablation Study

Generated: 2026-06-30 17:13
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `ref` (vendor scalar RM + scalar RS, no Zbc)

Optimizations (layered incrementally, all paired with RVV RS):
- **E** — RVV-vectorized `expand_and_sum` (vid + vsrl + vand, SEW=32, LMUL=4)
- **H** — RVV-vectorized `hadamard` FHT (vlse16 strided loads, SEW=16, LMUL=4)
- **P** — RVV-vectorized `find_peaks` (vredmaxu + vmseq + vfirst, LMUL=8)

Note: All rm_rvv_* variants also include the RVV Reed-Solomon vectorization
(Axis A: syndrome computation). The `ref` baseline uses pure scalar RS.

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `ref` | (none — scalar baseline) | 727,808 | ×1.0 | — | 1,113,200 | ×1.0 | — |
| `rm_rvv_E` | E | 367,411 | ×2.0 | ×2.0 | 273,626 | ×4.1 | ×4.1 |
| `rm_rvv_EH` | E+H | 253,455 | ×2.9 | ×1.4 | 211,675 | ×5.3 | ×1.3 |
| `rvv` | E+H+P (champion) | 204,318 | ×3.6 | ×1.2 | 141,148 | ×7.9 | ×1.5 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `ref` | (none — scalar baseline) | 977,833 | ×1.0 | — | 1,506,280 | ×1.0 | — |
| `rm_rvv_E` | E | 481,304 | ×2.0 | ×2.0 | 340,994 | ×4.4 | ×4.4 |
| `rm_rvv_EH` | E+H | 342,440 | ×2.9 | ×1.4 | 265,959 | ×5.7 | ×1.3 |
| `rvv` | E+H+P (champion) | 279,970 | ×3.5 | ×1.2 | 179,401 | ×8.4 | ×1.5 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `ref` | (none — scalar baseline) | 2,054,207 | ×1.0 | — | 3,232,692 | ×1.0 | — |
| `rm_rvv_E` | E | 870,166 | ×2.4 | ×2.4 | 638,434 | ×5.1 | ×5.1 |
| `rm_rvv_EH` | E+H | 646,914 | ×3.2 | ×1.3 | 517,839 | ×6.2 | ×1.2 |
| `rvv` | E+H+P (champion) | 546,974 | ×3.8 | ×1.2 | 378,751 | ×8.5 | ×1.4 |

## Incremental Optimization Contribution

| Transition | What changes | Vectorized sub-function |
|------------|--------------|-------------------------|
| ref→E | +RVV bit extraction | `expand_and_sum`: vid + vsrl.vv + vand (SEW=32, LMUL=4, vl=32) |
| E→EH | +RVV strided-load FHT | `hadamard`: vlse16 + vadd/vsub (SEW=16, LMUL=4, vl=64) |
| EH→rvv | +RVV max+argmax | `find_peaks`: vredmaxu + vmseq + vfirst (LMUL=8, vl=128) |

