# Phase 1+ Benchmark Results — Vector Shootout (Zvbc)

## Variants Evaluated
- **karatsuba_zvbc** (Track A): Vectorized Karatsuba using `vclmul.vv`/`vclmulh.vv` RVV instructions.
- **fafft_zvbc** (Track B): FAFFT compiled for Zvbc — reuses `fafft_zbc/ref/` kernel unchanged.

## Polymul Microbench (Spike instret, N_ITER=1)

| Level   | karatsuba_zbc | karatsuba_zvbc | Speedup | fafft_zbc  | fafft_zvbc |
|---------|---------------|----------------|---------|------------|------------|
| HQC-128 | 354,187       | **187,074**    | 1.89×   | 1,462,167  | 1,462,167  |
| HQC-192 | 1,078,641     | **575,486**    | 1.87×   | 3,256,399  | 3,256,399  |
| HQC-256 | 2,257,476     | **816,419**    | 2.76×   | 3,258,453  | 3,258,453  |

Data source: `polymul_spike_instret.csv`

## Key Finding

`vclmul.vv` benefits **only** Karatsuba. FAFFT uses bit-sliced tower-field
GF(2^32) arithmetic (AND+XOR on 32-bit words), which is structurally
incompatible with the binary polynomial product semantics of `vclmul.vv`.
Therefore `fafft_zvbc` is identical to `fafft_zbc` at the instruction level.

## Verdict

Vector Karatsuba (`karatsuba_zvbc`) wins decisively:
- 1.87–2.76× faster than scalar Karatsuba (`karatsuba_zbc`)
- 4.0–7.8× faster than FAFFT for equivalent security levels

**karatsuba_zvbc retains the Phase 1+ crown.**

## KAT Validation

Run `scripts/test/run_kat.sh build/hqc<LEVEL>_<VARIANT>.elf <LEVEL> <VARIANT>` to re-validate.
Results: see kat_results.txt (generated after QEMU KAT completes).
