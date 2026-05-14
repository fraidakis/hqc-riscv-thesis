# Phase 1b Scoreboard (fafft_zvc_v3)

Primary metric: median_instret from polymul_N1 CSV.

## Measurement notes

- Requested script scripts/bench/run_spike_bench_phase.sh is not present in this workspace.
- Equivalent direct Spike measurement used for HQC-128 fafft_zbc:
  - analysis/benchmarks/fafft_zbc/hqc128_fafft_zbc_polymul_N1.csv
  - median_instret = 1,651,224

## Deep-dive analysis (2026-05-04)

### Why Task A (gf2_32_mul_zbc) cannot be wired into the hot path

The existing `gf232v_mul` in ref/gfv_tower.h operates on 32 bit-sliced uint32_t
words representing 16 GF(2^32) elements in a tower-field representation:
  GF(2^32) = GF(2^16)[v]/(v^2 + v + 0x8000)  over  GF(2^16)

`gf2_32_mul_zbc` uses GF(2)[x]/(x^32 + x^22 + x^2 + x + 1) — a **different
GF(2^32)** with incompatible memory layout (single scalar uint32_t per element
vs 32 bit-sliced words per 16 elements). Direct substitution would corrupt the
field arithmetic in every FAFFT butterfly and all 2048 pointwise multiplications.

Wiring gf2_32_mul_zbc would require a complete rewrite of the FAFFT to use
monomial-basis GF(2^32) throughout (basis conversion, butterfly constants,
pointwise multiply). This is a separate research track, not a small change.

### Why Task C (bmul2_4096_to_4096 via bc_1_16384) cannot be wired into vect_mul

The CRT_fft_HQC1 path requires `bmul2_4096_to_4096` to compute **A * B mod
F_FAFFT** for the FULL 553-word (17,669-bit) HQC-128 polynomial. The natural
half-domain implementation is:
  bc_1_16384 (512-word input) + btfy_32768 (1024-word butterfly)

But `bc_1_16384` is limited to 512-word inputs (16,384 bits). HQC-128
polynomials have 553 words — 41 words (1,312 bits) over the limit.

The valid bc/btfy pairings in the FAFFT code are:
  bc_1_16384 (512 w)  → encode → btfy_32768  (1024 w)
  bc_1_32768 (1024 w) → encode → btfy_65536  (2048 w)   ← current full path
  bc_1_65536 (2048 w) → encode → btfy_131072 (4096 w)

bc_1_32768 accepts the full 553-word input but pairs only with btfy_65536 (the
existing full-product path), not with btfy_32768.

A working CRT path via bc_1_16384 would require `bmul2_4096_to_4096` to
compute A * B mod F_FAFFT (ALL 553 words), but bc_1_16384 sees only
A_lower[0..511]. The radix16_81 + CRT-lift step (81-word correction) cannot
recover the contribution of A[512..552] * B from the 41 high-degree words —
the error is too large (1312 × 1312 bits of cross terms in a 32768-bit ring).

CRT path attempt was built, tested, and yielded FAIL 0/100 KAT — then reverted.

### Why Tasks B and D have no performance impact

Task B (hqc128_crt_split/reconstruct): provides splitting helpers for the CRT
approach, but those helpers only help if bmul2_4096_to_4096 correctly processes
the FULL polynomial. Since it cannot (bc_1_16384 limit), Task B is blocked.

Task D (Cantor basis + Four Russians): standalone precomputation helpers with
no path to the active vect_mul hot path (which uses tower-field bc_1_32768).

## Hypotheses and outcomes

- Iter 1 hypothesis: clmul-based GF(2^32) CFB reduction can lower butterfly arithmetic cost.
  - Outcome: Incompatible GF(2^32) representation (monomial basis vs tower field
    bit-sliced). Cannot wire into gf232v_mul without a complete FAFFT rewrite.
    No measured change.

- Iter 2 hypothesis: explicit CRT split/reconstruct removes 553-word boundary friction.
  - Outcome: Blocked by the fundamental bc_1_16384 input-size limit (512 < 553).
    No measured change.

- Iter 3 hypothesis: branch-free BMUL2_4096 macro can improve same-domain PWM cost.
  - Outcome: bmul2_4096_to_4096 via bc_1_16384 + btfy_32768 was implemented and
    activated. KAT FAIL 0/100. The CRT correction (radix16_81 + 81-word lift)
    cannot compensate for the 41 truncated high-degree words.
    Reverted to full-domain path. No measured change.

- Iter 4 hypothesis: Cantor basis/Four Russians helper tables reduce setup overhead.
  - Outcome: Standalone helpers with no integration path into the tower-field FAFFT.
    No measured change.

## Scoreboard

| Iter | Technique                                | HQC-128 instret | Delta vs prev | Verdict |
|------|------------------------------------------|----------------:|--------------:|---------|
| 0    | phase0 baseline (karatsuba_ref)          |     18,578,007  |            -- | ref     |
| 1    | Task A (clmul CFB) — v3 standalone only  |      1,651,224  |      -91.11%  | keep*   |
| 2    | Task B (CRT split) — blocked by bc limit |      1,651,224  |        0.00%  | drop    |
| 3    | Task C (bmul2_4096 via bc_1_16384)       |      1,651,224  |        0.00%  | drop    |
| 4    | Task D (Cantor & Four Russians)          |      1,651,224  |        0.00%  | drop    |

*Iter-1 gain reflects the existing fafft_zbc path (full tower-field FAFFT), not the
v3 Task A helper. The v3 gf2_32_mul_zbc is compiled and present but not called
from any hot path.

## Root cause summary

The fafft_zbc variant at HQC-128 sizes is 4.7× slower than karatsuba_zbc
(1,651,224 vs 354,187 instret). The v3 Tasks were designed to close this gap via
a monomial-basis FAFFT with clmul butterflies and CRT splitting. Both approaches
are blocked by structural incompatibilities with the existing tower-field FAFFT:

1. **clmul wiring**: requires complete FAFFT rewrite to monomial-basis representation.
2. **CRT half-domain**: requires bc_1_16384 to process 553-word inputs (limit is 512).

The central thesis conclusion stands: at HQC-128 sizes, the FAFFT overhead
(tower-field basis conversion + butterfly constants + bit-sliced memory layout)
exceeds the savings from the FFT O(N log N) structure when the leaf multiply cost
has already been reduced to two instructions (clmul + clmulh) by karatsuba_zbc.
