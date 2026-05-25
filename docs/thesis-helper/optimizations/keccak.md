# 5.3 Vectorized Keccak / SHA-3 Acceleration

## 1. Baseline Architecture and Starting Point

The Keccak-f[1600] permutation constitutes the computational core of all
SHAKE-256 and SHA3 invocations within HQC. The reference implementation
(`vendor/hqc-ref/lib/fips202/fips202.c`, derived from PQClean) operates on a
25-lane state array of 64-bit words, applying 24 rounds of the five-step
mapping $\theta \to \rho \to \pi \to \chi \to \iota$. Each round is expressed
through scalar C operations: column-parity XOR reduction (Theta), fixed
rotations via the `ROL(a, offset)` macro expanding to a shift-pair
`(a << n) | (a >> (64-n))`, a NOT-AND pattern `(~b) & c` for Chi, and a
round-constant XOR for Iota. The implementation processes two rounds per loop
iteration (a standard "2-round unroll" from the Keer/Van Assche lineage),
maintaining two full copies of the 25-lane state (`A` and `E`) to avoid
write-after-read hazards.

Byte-level I/O is handled by `load64` and `store64` helper functions that
reconstruct 64-bit words from byte arrays via 8-iteration shift-OR loops — a
portable but instruction-heavy pattern that dominates absorb and squeeze phases
for long messages.

Profiling on Spike (instruction-accurate, `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb`)
established that Keccak accounts for a substantial and growing fraction of
total KEM retired-instruction count across all three implementation variants.
As polynomial multiplication was progressively optimised (from schoolbook to
Karatsuba with CLMUL acceleration), Keccak's absolute cost remained constant
while its relative share rose — from approximately 17% of total KEM cost in
the scalar-only `karatsuba_zbc` variant to over 33% in the fully-vectorized
`karatsuba_zvbc` variant. This Amdahl-law effect motivated a dedicated Keccak
optimisation campaign.

---

## 2. Optimization Progression (Qualitative)

The Keccak optimisation effort proceeded through four distinct phases, each
building on the insights and failures of its predecessor:

**Phase 1 — Scalar baseline with compiler auto-optimisation.** The unmodified
reference `fips202.c` was compiled with Zbb-enabled `-march` strings. The Clang
compiler automatically lowered `ROL` macros to single-cycle `rori` instructions
and `(~b) & c` to `andn`. An ablation study confirmed that explicit
`__builtin_rotateleft64` intrinsics produced identical machine code, establishing
that the compiler already achieved near-optimal scalar Keccak without manual
intervention.

**Phase 2 — Initial vectorization via LMUL=8 gather.** The first hand-written
Keccak backend (`fips202_zvbb_gather.c`) packed all 25 state lanes into a
single LMUL=8 vector register group and used `vrgather.vv` for the Pi
permutation, `vror.vv` (Zvbb) for Rho rotations, and `vandn.vv` (Zvbb) for
Chi. This achieved a modest improvement over the scalar reference but was
bottlenecked by four constant index arrays (`pi_src`, `ror_amounts`, `chi_x1`,
`chi_x2`) being reloaded from memory on every one of 24 rounds — 96 `vle64.v`
instructions per permutation call.

**Phase 3 — Register-wall discovery and SoA redesign.** An attempt to hoist
all four index vectors outside the round loop revealed a fundamental
architectural constraint: at VLEN=256 with LMUL=8, only four register groups
exist, and the state vector plus four index vectors require five. The compiler
responded by spilling the state to the stack every round, producing a net
regression. This insight motivated a complete architectural pivot: rather than
fighting the register wall with a single-instance LMUL=8 design, the
implementation was restructured as a 2-way Structure-of-Arrays (SoA) layout
operating at LMUL=1 with `vl=2`, where each vector register holds the same
lane from two independent Keccak instances. In this layout, Pi and Rho indices
are resolved at compile time (statically unrolled), eliminating all runtime
index loads entirely.

**Phase 4 — I/O streaming and production consolidation.** With the permutation
core stabilised, attention shifted to the absorb/squeeze I/O paths. The scalar
byte-by-byte XOR loop in `keccak_xor_bytes` was replaced with an RVV VLA
byte-streaming loop (`vle8.v`/`vxor.vv`/`vse8.v` at LMUL=8), and the
`load64_le`/`store64_le` helpers were converted to `__builtin_memcpy`-based
single-instruction loads. The final consolidated backend
(`src/keccak/fips202_zvbb_x2.c`) was promoted as the sole stable Keccak
implementation for the `karatsuba_zvbc` variant.

---

## 3. Track A/B — Shared Baseline: Reference Keccak with Compiler Zbb Auto-Optimisation

The `karatsuba_zbc` (scalar polynomial multiplication) and `karatsuba_rvv`
(RVV-only polynomial multiplication) variants share an identical, unmodified
`fips202.c` as their Keccak backend. No custom Keccak code was developed for
these variants; the reference permutation is compiled as-is with the target
`-march` string including Zbb.

**Compiler auto-lowering.** When compiled at `-O3` with `_zbb` in the march
string, Clang performs three automatic transformations on the scalar Keccak:

1. The `ROL(a, n)` shift-OR macro is pattern-matched and lowered to a single
   `rori rd, rs, (64-n)` instruction (Zbb), replacing the two-shift-plus-OR
   sequence.
2. The Chi NOT-AND pattern `(~b) & c` is lowered to `andn rd, c, b` (Zbb),
   replacing a NOT plus AND pair.
3. The `load64`/`store64` byte-shift loops are partially optimised by the
   compiler's loop-idiom recogniser, though full 8-byte load/store emission
   depends on alignment guarantees.

**Zbb ablation result.** Experiment `EXP-keccak-scalar-zbb` measured the
consequence of removing Zbb from the compilation of `fips202.c`. On the
`ref_scalar` variant (schoolbook polymul), the overhead was a mere 0.12–0.30%
of total KEM instruction count — negligible because polynomial multiplication
dominates at that baseline. However, on the promoted `karatsuba_zbc` champion
(where CLMUL-accelerated polymul is approximately 34× faster than schoolbook),
Keccak's fractional share rises substantially, and removing Zbb imposes a
6–10% total KEM penalty. This Amdahl-tail effect confirms Zbb as a mandatory
ISA constraint for the scalar tier.

An explicit-builtin variant (`__builtin_rotateleft64` + `__builtin_memcpy`)
was also tested and produced binary-identical code to the auto-lowered
baseline (58 `rori` instructions per ELF, instruction-count delta within
measurement noise). The compiler already achieves near-optimal Zbb lowering
for scalar Keccak without manual source-level intervention.

**Consequence.** For the `karatsuba_zbc` and `karatsuba_rvv` variants, Keccak
remains the primary instruction-count bottleneck after polynomial
multiplication has been accelerated. This motivates the concentrated Track C
investment described below, which targets the `karatsuba_zvbc` variant where
the full vector ISA (RVV 1.0 + Zvbb) is available.

---

## 4. Track C — Zvbb Vectorized Keccak for `karatsuba_zvbc` (SoA Architecture)

All hand-engineered Keccak optimisation effort was concentrated on Track C,
targeting the `karatsuba_zvbc` variant compiled with
`rv64gcv_zbc_zbb_zvbc_zvbb_zvl256b`. The following sub-optimizations are
presented in method order: algorithm-level, ISA-level, micro-level, I/O-level,
and multi-instance.

### 4.1 C1 — Gather-Based Zvbb Variant (Initial Vectorization)

The first vectorized Keccak backend (`fips202_zvbb_gather.c`) represented the
25-lane state as a single LMUL=8 vector register group (`vuint64m8_t`,
`vl=25` at VLEN=256). The five permutation steps were mapped to RVV
operations:

- **Theta:** Scalar extraction via `vslidedown` + `vmv.x.s`, scalar D[x]
  computation, re-injection via `vslide1up` and `vrgather` with a
  column-broadcast index table.
- **Rho + Pi:** A single `vrgather.vv` with the Pi source-index table,
  followed by `vror.vv` (Zvbb) with the per-lane rotation-amount table.
- **Chi:** Two `vrgather.vv` operations to collect `A[x+1]` and `A[x+2]`
  using cyclic row-index tables, then `vandn.vv` (Zvbb) and `vxor.vv`.
- **Iota:** `vxor.vx` with `vl=1` to apply the round constant to lane 0.

This design achieved a measurable improvement over the scalar reference but
was limited by a critical bottleneck: the four constant index arrays
(`pi_src[25]`, `ror_amounts[25]`, `chi_x1[25]`, `chi_x2[25]`) were reloaded
from memory inside the 24-round loop, costing $4 \times 24 = 96$ `vle64.v`
instructions per permutation call. Profiling attributed over 23% of total
Keccak instruction count to these redundant index loads alone.

### 4.2 C2 — LMUL=8 Register Wall (Architectural Constraint Discovery)

The natural response to the index-reload bottleneck was to hoist all four
index vectors outside the round loop, holding them as live register groups
throughout the 24 rounds. However, this approach encountered a fundamental
hardware constraint.

At VLEN=256 with LMUL=8, the 32-entry vector register file is partitioned
into exactly four register groups of eight registers each. The
`fips202_zvbb_gather.c` implementation requires the following simultaneous
allocations:

| Group | Contents |
|-------|----------|
| 0 (`v0–v7`) | State vector (25 lanes, live throughout) |
| 1 (`v8–v15`) | Round scratch: Rho+Pi result, Chi temporaries |
| 2 (`v16–v23`) | Hoisted `idx_pi` + `idx_ror` |
| 3 (`v24–v31`) | Hoisted `idx_x1` + `idx_x2` |

This allocation leaves zero free groups for the Theta scratch vector. The
compiler, unable to satisfy the five-group demand with four physical groups,
responded by spilling the state vector (`v0`) to the stack at the start of
every round and reloading it afterwards — 48 extra `vse64.v` + 48 extra
`vle64.v` over 24 rounds. The spill cost exceeded the 96 index loads it was
meant to eliminate, producing a net regression of approximately +47,000
retired instructions on the decaps path.

This result established a key architectural insight: at LMUL=8 on VLEN=256,
loop-invariant code motion for vector constants is counterproductive. The four
available register groups are fully occupied by (state, scratch0, scratch1,
scratch2), and any fifth live group forces a spill that costs more than the
computation it was intended to avoid.

### 4.3 C3 — Index Vector Elimination via SoA Redesign (Promoted)

The register-wall discovery motivated a fundamental architectural pivot. Rather
than persisting with the single-instance LMUL=8 gather design, the
implementation was restructured as a **2-way Structure-of-Arrays (SoA)**
layout:

- The state is represented as `vuint64m1_t state[25]`, where each LMUL=1
  vector register holds the same lane index from two independent Keccak
  instances, operating at fixed `vl=2`.
- Pi source indices and Rho rotation amounts are resolved entirely at compile
  time through static unrolling: each of the 25 Rho+Pi assignments is written
  as an explicit `state[dst] = vror_vx(state[src], amount, vl)` statement with
  compile-time-constant `src`, `dst`, and `amount`.
- Chi operates directly on adjacent array elements (`state[base+0]` through
  `state[base+4]`) using a row-local macro, requiring no gather at all.

This design eliminates all 96 runtime index loads from the permutation core.
The 25 LMUL=1 state registers consume 25 of the 32 physical vector registers,
leaving 7 registers available for temporaries — more than sufficient for the
`B[25]`, `C[5]`, and `D[5]` arrays used within each round. The compiler can
allocate these freely without spilling.

The Rho+Pi unrolling additionally enables the compiler to emit `vror.vi`
(vector rotate-right by immediate) for all rotation amounts $\leq 31$, and
`vror.vx` with a pre-loaded scalar immediate for amounts in the range 32–63.
This eliminates the `vror.vv` instruction (which requires a vector operand for
the rotation amount) and its associated index-vector load entirely.

### 4.4 C4 — Vectorized Theta Proposal (Pending)

The current Theta implementation in the LMUL=8 single-instance path uses a
hybrid scalar/vector approach: column parities $C[x]$ are extracted from the
state vector via `vslidedown` + `vmv.x.s` into scalar registers, the five
diffusion values $D[x] = C[x-1] \oplus \text{ROL}(C[x+1], 1)$ are computed
in scalar, and the result is re-injected into the full state via
`vslide1up` + `vrgather` with a column-broadcast index table.

A proposed improvement (Track B2 in the roadmap) would eliminate the
store-to-stack round trip by computing column sums entirely in-register using
either (a) `vmv.x.s` + `vslidedown.vi` extraction into scalars followed by
scalar D[x] computation and `vxor.vx` broadcast injection, or (b) a
column-major state layout where column sums become a `vl=5` reduce-XOR over
contiguous elements. This optimisation remains a pending experiment slot.

In the promoted 2-way SoA design, Theta is implemented with explicit
per-column XOR chains across the five rows (`state[0] ^ state[5] ^ state[10]
^ state[15] ^ state[20]` for column 0, etc.), followed by `vror_vx` for the
ROL-1 step and `vxor` for diffusion application. The D-application loop was
fully unrolled (25 explicit `vxor` statements), eliminating loop-carried
scalar index reloads.

### 4.5 C5 — Chi No-Gather Attempt (Dropped)

An alternative Chi implementation was explored that replaced the two
`vrgather.vv` operations (for `A[x+1]` and `A[x+2]` collection) with
row-local slide and wrap sequences. The hypothesis was that `vrgather.vv` at
LMUL=8 incurs crossbar pressure on physical vector hardware, and that
row-boundary-respecting slides would be cheaper.

On Spike's instruction-accurate model, the replacement raised the
retired-instruction count: enforcing strict row boundaries in a flat `vl=25`
layout required additional `vslidedown`, wrap, and patch sequences whose
aggregate instruction cost exceeded the two gather operations they replaced.
The experiment was rolled back, and the gather-based Chi was retained as the
instruction-count-optimal form for the single-instance LMUL=8 path.

In the promoted 2-way SoA design, Chi operates on explicit array indices
(`B[base+0]` through `B[base+4]`) with a row-local macro, making the gather
question moot — no permutation instruction is needed when the row elements are
individually addressable.

### 4.6 C6 — RVV I/O Fast Paths (Promoted)

The incremental absorb and squeeze functions (`keccak_inc_absorb`,
`keccak_inc_squeeze`) were identified as a secondary hotspot: the original
byte-by-byte XOR loop in `keccak_xor_bytes` and the byte-extraction loop in
`keccak_extract_bytes` contributed measurably to total KEM instruction count,
particularly for the long absorb/squeeze phases in HQC keygen and decaps.

Two complementary I/O optimisations were applied:

**RVV byte-streaming `keccak_xor_bytes`.** The scalar word-aligned loop and
byte tail were replaced with an RVV 1.0 VLA (vector-length-agnostic)
byte-streaming loop using `vle8.v` / `vxor.vv` / `vse8.v` at LMUL=8. At
VLEN=256, `vlmax = 256` bytes — sufficient to cover any Keccak-rate block
(SHAKE-128: 168 bytes, SHAKE-256: 136 bytes) in a single vector iteration.
A scalar pre-alignment phase advances the byte pointer to an 8-byte boundary
before entering the vector loop, after which the flat `uint8_t*` view of the
state array is XORed directly. This exploits RISC-V's little-endian byte
ordering: a byte-level XOR into the state array is bit-for-bit equivalent to
the word-level `s[i] ^= load64(m + 8*i)` pattern.

Constant-time safety is preserved: all branch conditions depend only on the
public absorb position (0 to $r-1$) and the public chunk size, with no
secret-dependent branches or memory-access patterns introduced.

**Native `load64_le` / `store64_le`.** The 8-iteration byte-shift loops in
`load64_le` and `store64_le` were replaced with `__builtin_memcpy(&r, x, 8)`.
At `-O3` on little-endian `rv64` without strict-alignment constraints, Clang
lowers this compile-time-constant-size memcpy to a single native unaligned
`ld` or `sd` instruction, eliminating all scalar shifting and bitwise-OR
overhead. The use of the compiler builtin (rather than a library `memcpy` call)
is critical in the freestanding build environment, where a custom RVV-vectorized
`memcpy` symbol exists in `sim/common/mem.c` — a standard `memcpy` call would
dispatch to that function, incurring call overhead and defeating the
single-instruction lowering.

The combined I/O optimisation was promoted to stable after passing 300/300 KAT
vectors across all three security levels.

### 4.7 Theta D-Application and Pi+Rho Unrolling

Within the 2-way SoA permutation core (`KeccakF1600_StatePermute_x2`), two
loop-level unrolling passes were applied:

1. **Pi+Rho static resolution.** The 25 Rho+Pi assignments were written as
   explicit named statements with compile-time-constant source indices and
   rotation amounts, enabling the compiler to emit `vror.vi` (immediate ≤ 31)
   or `vror.vx` with a pre-loaded scalar (32–63). This eliminated 1,200
   loop-carried scalar loads per permutation call (25 source-index loads +
   25 rotation-amount loads, per round, over 24 rounds).

2. **Theta D-application unrolling.** The loop applying $D[x]$ across all
   five rows was fully unrolled into 25 explicit `vxor` statements, grouped
   by column. This eliminated the loop-driven `D[]` array index reload (5
   scalar `ld` per round) and enabled the compiler to keep all five `D[x]`
   values in registers simultaneously.

---

## 5. Track C (continued) — Multi-Instance Batching (C7–C8)

### 5.1 C7 — Call-Graph Audit (Concurrency Ceiling)

Before implementing a parallel SHAKE API, the HQC call graph was instrumented
with runtime context-tracking hooks to determine the maximum number of
simultaneously active, data-independent SHAKE instances within a single KEM
flow. The audit measured:

- **Maximum global concurrency:** $N = 2$ at all three security levels
  (HQC-128, HQC-192, HQC-256).
- **Keygen:** Two independent XOF contexts (`dk_xof_ctx` and `ek_xof_ctx`)
  coexist after the shared seed derivation step, generating the secret vector
  $\mathbf{x}$ and the public syndrome $\mathbf{h}$ respectively.
- **Encaps/Decaps:** At most two contexts coexist (the theta-expansion XOF
  and a hash context), but with data dependencies that limit true parallelism.
- **Absorb-size histogram:** All observed `shake256_inc_absorb` calls used
  input lengths of 1 or 32 bytes — short messages where permutation cost
  dominates over I/O.

The $N = 2$ ceiling established that a native `x4` batched path would be
under-utilised within a single KEM invocation. A 4-way backend remains viable
only with cross-request batching (processing multiple independent KEM
operations simultaneously), which requires caller-side restructuring beyond
the scope of a drop-in SHAKE replacement.

### 5.2 C8 — 2-Way SoA SHAKE API (Promoted)

Based on the $N = 2$ concurrency ceiling, a 2-way SoA SHAKE-256 API was
implemented:

- **State layout:** `vuint64m1_t state[25]` at fixed `vl=2`, where lane 0
  holds one Keccak instance and lane 1 holds the other.
- **Absorb:** `shake256_x2_absorb(ctx0, ctx1, in0, in1, inlen)` interleaves
  two input streams into the SoA state, applies padding independently, and
  invokes the 2-way permutation core.
- **Squeeze:** `shake256_x2_squeezeblocks(out0, out1, nblocks, ctx0, ctx1)`
  permutes and de-interleaves output blocks to two independent output buffers.
- **Keygen integration:** The `hqc_pke_keygen` call site was modified to pair
  the `dk_xof_ctx` and `ek_xof_ctx` contexts, processing both XOF expansions
  through the 2-way API in a single fused pass.

The 2-way SoA permutation core (`KeccakF1600_StatePermute_x2`) uses LMUL=1
throughout, consuming only 25 of 32 vector registers for the state array and
leaving ample room for the `B[25]`, `C[5]`, and `D[5]` temporaries. Every
vector instruction processes both instances simultaneously, amortising
instruction-fetch and decode overhead across two permutations.

### 5.3 x4 Batching Abandonment

A 4-way batched variant was prototyped using the PQRV (Zhang et al., TCHES
2025) hybrid assembly permutation (`KeccakF1600_StatePermute_RV64V_4x`). The
wrapper placed a single real state in one of the four interleaved slots,
zeroed the remaining three, and discarded their outputs. This approach was
abandoned for two reasons: (a) the $N = 2$ concurrency ceiling means three of
four slots are wasted in single-KEM flows, and (b) the interleaved state
layout (`S(j, i)` macro) requires element-wise packing/unpacking that adds
non-trivial overhead for the singleton use case.

### 5.4 PQRV Hybrid 1+(N-1) Design as Future Direction

The PQRV artifact (Zhang et al., TCHES 2025) demonstrates a hybrid scheduling
technique where one Keccak instance executes on the scalar integer pipeline
while $N-1$ instances execute on the vector pipeline, with scalar and vector
instructions interleaved to fill both issue slots of a dual-issue in-order
core. This approach is directly applicable to the `rv64gcv_zvbb` ISA but
requires (a) a hand-written scalar assembly permutation (Track A1) and (b) a
cycle-accurate simulator or real hardware to measure the dual-issue benefit —
Spike's instruction-retirement model cannot capture the ILP gain. This remains
a future direction contingent on hardware availability.

---

## 6. Intuition-Based Impact Ranking

The following ranking orders all Keccak optimisations by expected impact on
total KEM instruction count, assessed from scope and hotspot proximity:

### High Expected Impact

- **C3 — SoA redesign with compile-time index resolution.** Eliminated all 96
  runtime index loads per permutation call and resolved the LMUL=8 register
  wall; structural change affecting every round of every permutation invocation.
- **C6 — RVV I/O fast paths (`keccak_xor_bytes` + `load64_le`).** Replaced
  the dominant I/O-phase instruction cost with single-iteration vector
  operations; affects every absorb and squeeze call across all KEM operations.

### Medium Expected Impact

- **C1 → C2 progression (gather-based vectorization + register-wall
  discovery).** The initial vectorization provided the foundation; the
  register-wall failure was the critical insight enabling the SoA pivot.
- **C8 — 2-way SoA batching for keygen.** Amortises permutation overhead
  across two instances; limited to keygen due to the $N = 2$ ceiling.
- **C4 — Vectorized Theta (pending).** Would eliminate the remaining
  scalar-vector boundary crossing in the LMUL=8 path; moderate impact on the
  single-instance permutation.
- **Pi+Rho and Theta D-application unrolling.** Eliminated 1,200+ scalar
  loads per permutation; incremental but measurable within the SoA core.

### Low Expected Impact

- **Track A/B Zbb auto-lowering.** Compiler already achieves optimal `rori`
  and `andn` emission; explicit builtins provide no additional benefit.
- **C5 — Chi no-gather (dropped).** Row-local slide sequences raised
  instruction count on the instruction-accurate model.
- **C7 — Call-graph audit.** Analysis-only; no direct instruction reduction,
  but established the $N = 2$ constraint that correctly scoped the batching
  effort.

---

## 7. Discarded Alternatives and Why They Were Archived

### 7.1 The Scalar Fallback Trap (`EXP-keccak-rvv-only-legacy`)

An early RVV-only Keccak backend (`fips202_rvv_only.c`) implemented a 4-way
SoA permutation in `shake256_x4()` but retained the unmodified scalar path for
all single-stream functions. Because HQC never calls `shake256_x4()` directly
— every SHAKE invocation goes through the incremental single-stream API — the
vector code was never exercised. The result was identical instruction count to
the reference (7,100 vs 4,680 per isolated permutation on Spike), with the
vector path serving only as dead code. This "scalar fallback trap" motivated
the requirement that any vectorized Keccak must replace the single-stream
permutation itself, not merely add an unused parallel API.

### 7.2 The LMUL=8 Register Wall (`archive_keccak_c4_register_wall`)

The attempt to hoist four constant index vectors outside the 24-round loop at
LMUL=8 on VLEN=256 required five simultaneously-live register groups but only
four exist. The compiler spilled the state vector to the stack every round,
adding 96 vector memory operations that exceeded the 96 index loads they were
meant to eliminate. The measured regression was +47,663 instructions on the
decaps path. This established the principle that at LMUL=8/VLEN=256, the
register file is fully saturated by (state + 3 scratch groups), and any
additional live vector constant forces a net-negative spill.

### 7.3 Chi No-Gather via Row-Slice Shuffles (Track B3)

Replacing the two `vrgather.vv` operations in Chi with row-local
`vslidedown`/wrap/patch sequences was hypothesised to reduce crossbar pressure
on physical vector hardware. On Spike's instruction-accurate model, the
replacement raised retired-instruction count because enforcing row boundaries
in a flat 25-element vector required additional slide and fixup instructions
whose aggregate cost exceeded the two gathers. The experiment was rolled back
with full KAT validation confirming no functional regression.

---

## 8. Benchmarking Plan for Next Phase

The following measurements are planned for the upcoming quantitative analysis
pass. This section defines what should be measured per optimisation without
reporting results:

1. **A1 — Hand-written RV64IMB assembly.** Isolated `KeccakF1600` instruction
   count on Spike; full KEM instruction count for all three security levels;
   cycle-level measurement on a cycle-accurate simulator or FPGA emulation to
   capture dual-issue scheduling benefits invisible to Spike.

2. **B2 — Vectorized Theta.** Isolated permutation instruction count before
   and after eliminating the scalar Theta round trip; full KEM delta for
   `karatsuba_zvbc` at all levels; register-pressure analysis to confirm no
   spill regression.

3. **C3 — Hybrid 1+(N-1) scheduling.** Requires a cycle-accurate model (not
   Spike). Measure throughput in permutations per cycle for the hybrid
   scalar+vector interleaved approach versus pure-vector and pure-scalar
   baselines. Evaluate at $N \in \{2, 3, 4\}$ to determine the optimal batch
   width for the target microarchitecture.

4. **Cross-variant Keccak share analysis.** Measure Keccak's percentage of
   total KEM cost at each security level for all three variants after all
   promoted optimisations, to quantify the remaining Amdahl headroom.

5. **VLEN sensitivity.** Evaluate the SoA 2-way backend at VLEN=128 (where
   `vl=2` at LMUL=1 still holds) and VLEN=512 (where wider vectors may enable
   higher-N batching without register pressure) to characterise portability.
