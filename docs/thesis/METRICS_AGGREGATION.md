# Final Metrics Aggregation

This document freezes the thesis-facing performance summary at the end of the engineering phase.

## L128 Headline

| Configuration | `vect_mul` instret | `decaps` instret | Source |
| --- | ---: | ---: | --- |
| Original baseline (`karatsuba_ref`) | 18,527,412 | 56,442,183 | `analysis/benchmarks/isolated/scalar/karatsuba_ref/*.csv` |
| Final scalar champion (`karatsuba_zbc`, AP-2) | 158,928 | 1,407,786 | `analysis/benchmarks/isolated/scalar/karatsuba_zbc/*.csv` |
| Final vector champion (`karatsuba_zvbc`, AP-2) | 72,644 | 900,906 | `analysis/benchmarks/isolated/vector/karatsuba_zvbc/*.csv` |

## Isolated Polymul Comparison

| Level | Original baseline (`karatsuba_ref`) | Final scalar champion (`karatsuba_zbc`, AP-2) | Final vector champion (`karatsuba_zvbc`, AP-2) |
| --- | ---: | ---: | ---: |
| HQC-128 | 18,527,412 | 158,928 | 72,644 |
| HQC-192 | 56,315,140 | 483,283 | 219,338 |
| HQC-256 | 139,115,660 | 1,614,035 | 326,260 |

## Full KEM Decapsulation Comparison

| Level | Original baseline (`karatsuba_ref`) | Final scalar champion (`karatsuba_zbc`, AP-2) | Final vector champion (`karatsuba_zvbc`, AP-2) |
| --- | ---: | ---: | ---: |
| HQC-128 | 56,442,183 | 1,407,786 | 900,906 |
| HQC-192 | 170,368,590 | 3,048,493 | 1,851,145 |
| HQC-256 | 419,815,566 | 7,558,518 | 3,045,518 |

## Experimental Track Summary

- Target 1-4 micro-optimizations harvested the scalar low-hanging fruit: zero-free write discipline, direct support-bit scatter, native `clmul/clmulh` integration, and the final inner-loop unroll/hi-xor fusion sequence that established the strong pre-AP-2 scalar baseline.
- The LMUL=8 register wall discovery was decisive on the vector track: wider RVV configurations exposed register-pressure limits and forced a more disciplined use of LMUL, shaping the winning `vclmul.vv` implementations and ruling out several seemingly attractive full-width designs.
- The FAFFT `O(n \log n)` line was a negative result in this environment: asymptotics lost to constant factors, field-conversion overhead, and hardware mismatch, so FAFFT never displaced the Karatsuba/vector-schoolbook family in the final benchmark tables.
- The TC3 6-way / Toom-Cook track was also a negative result: despite the lower multiplication count on paper, interpolation/evaluation overhead dominated on Spike, and the implementation regressed versus the best Karatsuba baselines.
- The ultimate breakthrough was AP-2 fixed-tree specialization: replacing dynamic recursion with generated compile-time trees for roots `277/561/901` produced the final scalar champion at 158,928 instret and the final vector champion at 72,644 instret, enabling the project-ending 900,906-instret HQC-128 decapsulation result.

## Notes

- All counts above are Spike `instret` measurements captured from the repository's isolated benchmark harness.
- The final vector decapsulation milestone depends on two ingredients together: AP-2 specialized `karatsuba_zvbc` plus RVV Reed-Muller decode reactivation.