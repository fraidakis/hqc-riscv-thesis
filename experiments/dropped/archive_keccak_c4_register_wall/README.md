# Archived Experiment: Keccak C-4 — LMUL=8 Register Wall

**Status:** ABANDONED — all three variants measured and confirmed regressive.

---

## What Was Tried

Three micro-optimizations to `KeccakF1600_StatePermute` in
`src/keccak/fips202_zvbb_gather.c` (the `zvbb_gather` variant):

| Sub-target | Mechanism | Expected saving |
|---|---|---|
| C-4a | Hoist 4 constant index `vle64.v` loads outside the 24-round loop | ~50–80k instret |
| C-4b | Replace double `vsetvl` per Iota step with a lane-0 masked XOR | ~25k instret |
| C-4c | 2× manual loop unroll to eliminate 12 back-branches | ~12k instret |

---

## The Architectural Wall

The implementation runs on **rv64gcv with VLEN=256, LMUL=8**.

At LMUL=8 the entire 32-register file is consumed by a **single** vector
register group: 32 physical registers ÷ 8 registers per group = **4 groups
total**.

The `zvbb_gather` Keccak implementation needs the following simultaneously:

| Allocation | Purpose |
|---|---|
| Group 0 (`v0–v7`) | State vector `v0` (25 × u64 lanes) |
| Group 1 (`v8–v15`) | Scratch: `t` (Rho+Pi result), `vB` (Chi x+1) |
| Group 2 (`v16–v23`) | Scratch: `vC` (Chi x+2), or `idx_pi`/`idx_x1` |
| Group 3 (`v24–v31`) | `idx_ror` / `idx_x2` |

Only 4 groups exist. Hoisting all four index vectors (`idx_pi`, `idx_ror`,
`idx_x1`, `idx_x2`) requires them to **live simultaneously alongside `v0`**,
but that is 5 LMUL=8 groups — **one more than the hardware provides**.

The compiler responds by **spilling `v0` (the state vector) to the stack** at
the start of every round and **reloading it** afterwards, paying two
round-trip memory operations per round. Over 24 rounds that is 48 extra
`vse64.v` + 48 extra `vle64.v` spills — far more than the 96 `vle64.v`
index loads that were supposed to be eliminated.

---

## Measured Results (HQC-128, `karatsuba_zvbc`, `decode=rvv`)

| Configuration | `KeccakF` instret | `decaps` instret | Δ decaps |
|---|---:|---:|---:|
| **Baseline (C-5 committed)** | **521,424** | **953,889** | — |
| C-4a + C-4b + C-4c (full hoist + mask + unroll) | 609,322 | 1,001,552 | **+47,663** ❌ |
| C-4b only (masked Iota, no hoist/unroll) | 560,616 | 975,141 | **+21,252** ❌ |

Even C-4b in isolation regresses because the single `vid_v_u64m8` +
`vmseq_vx_u64m8_b8` mask-setup block displaces state vector registers,
triggering partial spilling that costs more than the 48 `vsetvl` pairs saved.

---

## Lesson Learned

At **LMUL=8 on VLEN=256**, "loop-invariant code motion" is a trap: every
hoisted vector constant that survives a round boundary forces a spill of the
state group. The correct model is:

> The 4 LMUL=8 register groups are fully occupied by (state, scratch0,
> scratch1, scratch2). Any fifth live group causes a spill that costs more
> than the computation it was meant to avoid.

The original code — 4 `vle64.v` index reloads per round plus 2 `vsetvl` for
Iota — is already the **register-pressure-optimal** implementation for this
ISA/VLEN/LMUL combination. **Do not attempt to hoist index vectors in this
function.**

The next productive Keccak optimization is **C-4d (Theta vectorization)**,
which eliminates the 25-lane scalar spill/reload per round. This is
high-risk and must be done on a separate branch with its own KAT validation.

---

## Reproduction

```bash
# Apply C-4a/b/c and measure:
git stash   # or implement the changes manually
cmake --build build/spike/kat --target hqc128_karatsuba_zvbc -j$(nproc)
./scripts/test/run_kat.sh --mul karatsuba_zvbc --decode rvv --level 128
./scripts/report/run_full_analysis.sh --mul karatsuba_zvbc --decode rvv --level 128
# Observe KeccakF instret increase and decaps regression.
git stash pop
```

---

## Related: `vclmul_leaf` True Assign-First-Write Failure

While attempting to eliminate the final `vzero` pre-store from `vclmul_leaf`
(the 1-instruction `vse64.v r+n, vmv.v.x(0)` upper-half initializer), a
`vslidedown_vx + vl=1 ASSIGN` approach was tried to write each first-touch
upper-half cell `r[i+n]` directly from the `hi` product vector.

**Result:** decaps 953,889 → 1,003,062 (+49k regression).

**Root cause:** At n=5 (actual leaf size for HQC-128), the outer loop runs 5
times. Each iteration needs `vsetvl(n-1)` + `vsetvl(1)` + `vslidedown_vx`
(3 extra instructions per outer iteration × 5 = 15 extra per leaf call).
The single `vmv.v.x(0)` + `vse64` vzero costs only 3 instructions total and
covers all n upper-half cells at once.

**Lesson:** The `vzero r+n` pre-store in `vclmul_leaf` is already the
register-optimal initialization pattern. The one RVV store covers all n
first-touch positions in a single instruction. Do not attempt per-element
extraction via `vslidedown+vl=1` — the overhead exceeds the benefit by
~5× at small n.

The committed C-3a code (vzero + i=0 lo-assign peel) is the correct and
optimal form. Current best: **decaps L128 = 953,889 instret**.
