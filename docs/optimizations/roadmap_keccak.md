# Optimization Roadmap: Keccak-f[1600] (SHAKE256 / SHA3)

> **Status:** All three production variants — `karatsuba_zbc`, `karatsuba_rvv`,
> `karatsuba_zvbc` — are bottlenecked by `KeccakF1600_StatePermute`. Polymul
> has been driven to a small fraction of total KEM cost; Keccak now accounts
> for 27–58 % of per-operation instret across security levels.
>
> **Profile date:** 2026-05-19  
> **Security level profiled:** HQC-128 (primary), scaling estimates for 192/256  
> **Simulator:** Spike (instruction-accurate; `instret` = retired instruction count)  
> **ISA baseline:** `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b`

---

## 0. Current State of Play

### 0.1 Per-Variant Keccak Instret (decaps L128)

| Variant | Keccak impl | `KeccakF1600` instret | % of decaps | Speedup vs ref |
|---------|-------------|----------------------|-------------|----------------|
| `karatsuba_zbc` | `fips202.c` (reference C) | 578,792 | ~16.6% | 1.00× |
| `karatsuba_rvv` | `fips202_rvv_only.c` | 578,792 | 27.3% | **1.00×** ← worse |
| `karatsuba_zvbc` | `fips202_zvbb_gather.c` | 521,424 | 33.5% | **1.11×** ← marginal |

Total KEM instret (keypair + encaps + decaps):

| Variant | Total KEM | Keccak share |
|---------|-----------|-------------|
| `karatsuba_zbc` | ~3,479,000 | ~16.6% |
| `karatsuba_rvv` | 2,106,717 | 27.5% |
| `karatsuba_zvbc` | 1,541,397 | 33.8% |

Keccak rises as a fraction of total cost precisely because polymul has been
optimised away. The absolute instruction count of `KeccakF1600` is identical
across all variants using the C reference — no optimisation has been applied
to the single-instance permutation path that HQC actually exercises.

### 0.2 Root Cause: `fips202_rvv_only.c` — Why No Improvement

`fips202_rvv_only.c` implements a **4-way SoA Keccak** in `shake256_x4()` but
keeps all single-stream functions (`shake256`, `keccak_inc_absorb`, etc.) on the
**unmodified scalar path** (identical to `fips202.c`). HQC never calls
`shake256_x4()` directly; every HQC call goes through the scalar fallback.
The result: identical instret to the reference, with minor overhead from the
larger binary (duplicate scalar + vector code).

### 0.3 Root Cause: `fips202_zvbb_gather.c` — Why Only 1.11×

Profiled line-level hotspots within `KeccakF1600_StatePermute`:

| Line | Source | instret | % of KEM |
|------|--------|---------|----------|
| L148 | `vle64.v` — Chi index arrays loaded each round | 194,256 | 12.5% |
| L143 | `vle64.v` — Rho+Pi index arrays loaded each round | 170,400 | 10.9% |

The four constant index tables (`pi_src[25]`, `ror_amounts[25]`, `chi_x1[25]`,
`chi_x2[25]`) are re-loaded from memory on **every one of 24 rounds** —
yielding 4 × 24 = 96 `vle64.v` instructions per permutation call, each
fetching 25 × 8 = 200 bytes from L1 cache. Index loads dominate the
instruction count; the `vrgather` permutations themselves are a secondary cost.

Additionally, **Theta is entirely scalar**: the 25-lane state vector is stored
to a 200-byte stack buffer, XOR-reduced across columns, and reloaded every
round. This adds 2 vector memory instructions + 35 scalar ops × 24 rounds =
~888 extra instructions per permutation call.

---

## 1. Optimization Tracks

The following tracks are ordered by expected payoff relative to implementation
effort, based on the PQRV (TCHES 2025) results and the profile above.

---

### Track A — Scalar Single-Instance: Hand-Scheduled Assembly

**Applicable to:** all variants (fixes the scalar path that every HQC call uses)  
**ISA requirements:** `rv64imb` (Zbb `rori` + `andn`; no V required)  
**Effort:** medium  
**Expected gain:** 2–3× reduction of `KeccakF1600` instret

#### A1 — Hand-Written RV64IMB Assembly with `rori` + Dual-Issue Scheduling

**Problem.** The reference C scalar (2-round unrolled loop) compiles to 25+
variable registers, triggering register spills to stack inside the round loop.
Even with Zbb enabled, the compiler cannot guarantee that all 25 state lanes
stay in integer registers simultaneously; spills account for a significant
fraction of the `ld`/`sd` traffic already visible in the instruction histogram.

**Solution.** Write a fully-unrolled, hand-scheduled `KeccakF1600_StatePermute`
in RISC-V assembly — following the approach in PQRV's `fips202_rv64imb.S`
(Zhang et al., TCHES 2025). Key design decisions:

- Replace every `ROL(a, n)` macro with a single `rori rd, rs, (64-n)` (Zbb).
  The reference C produces `slli + srli + or` or `slli + xor` per rotation;
  `rori` halves the instruction count for the rotation-heavy ρ step.
- Replace `(~b) & c` (Chi NOT-AND) with `andn rd, c, b` (Zbb). Already
  present in the C reference if compiled with `-march=..._zbb`, but guaranteed
  in assembly.
- Unroll all 24 rounds: eliminates the inner loop counter, branch, and
  round-constant load from an array (constants can be embedded inline or
  pre-loaded into dedicated registers for the first few rounds).
- Schedule instruction pairs to fill both slots of a dual-issue in-order
  pipeline (e.g., the XuanTie C908 targeted by PQRV). The compiler cannot
  reliably achieve this across round boundaries; a hand-scheduled unroll can
  pair independent θ XORs with independent ρ rotations.

**Reference:** Zhang et al., "Optimized Software Implementation of Keccak, Kyber,
and Dilithium on RV{32,64}IM{B}{V}", TCHES 2025 (ePrint 2024/1515).
Artifact: `sha3/fips202_rv64imb.S` in the `ches2025` branch of
<https://github.com/Ji-Peng/PQRV>. Their implementation reports the best
single-instance Keccak-f[1600] on RV64IMB as of the paper date.

**Experiment slot:** `EXP-keccak-rv64imb-asm`

**Result (2026-05-20):** No instruction-count improvement on Spike for the
single-instance scalar path.

- Baseline (`--mode keccak ref --mul karatsuba_zbc`): **4,680 instret**
- A1/A3 assembly attempt (`--mode keccak opt --mul karatsuba_zbc`):
  **4,729 instret**

Observed interpretation: with `-O3` + Zbb, the compiler already reaches near
structural parity on the scalar permutation core for instruction-retirement
metrics. Rotations and Chi patterns are already lowered to efficient bitmanip
forms, so handwritten assembly did not unlock a lower-instret execution on this
simulator. The remaining expected A1 advantages (dual-issue scheduling across
round boundaries) are cycle-level effects and are not visible in Spike's
instruction-only accounting.

#### A2 — Lane Complementing (Eliminate Chi NOT Operations)

**Problem.** Each Chi step applies `st[i] ^= (~st[x+1]) & st[x+2]` across all
25 lanes. Even with `andn`, every round still requires the two-operand form
`vandn(C, B)` = `C & ~B`. On scalar code this is already the optimal 1-
instruction form (Zbb `andn`). On a vector implementation it is equally cheap
with Zvbb `vandn.vv`. The saving here is a *code-size* and *microarchitectural*
concern rather than pure instruction count.

**Solution.** Apply the *lane complementing* technique from XKCP:
select a fixed subset of the 25 lanes to maintain in complemented form
throughout all rounds; adjust θ and χ to account for the invariant. When a
lane is always stored as `~a`, the `~a` operand in Chi is already free — the
AND can be applied directly, and the remaining XOR patterns change. The
XKCP reference implementation (all platforms) demonstrates the full derivation.

Net effect: eliminates 2–5 explicit NOT (or vandn bias) operations per round on
carefully chosen lane subsets. Useful mainly when building a hand-written
assembly version (A1), where removing 2 instructions per round × 24 rounds = 48
instructions from a ~1 000-instruction body matters.

**References:**  
- Bertoni et al., "The road from Panama to Keccak" (KeccakTools documentation)  
- XKCP `KeccakP-1600-opt64.c` and `KeccakP-1600-SnP.h` complement-bit tables  
- <https://github.com/XKCP/XKCP>

**Experiment slot:** `EXP-keccak-lane-complement`

#### A3 — Full 24-Round Unroll + Constant Embedding

**Problem.** The 2-round loop in the C reference executes 12 loop iterations;
each iteration includes a branch, a loop counter update, an array index, and a
64-bit constant load (`KeccakF_RoundConstants[round]`). These are 3–4
instructions of pure overhead per two rounds = 36–48 extra instructions per
permutation call on top of the round body.

**Solution.** Fully unroll all 24 rounds and embed the round constants as
immediate literals (or via `li` + `xori`). When combined with A1 (assembly),
this is a straightforward by-product of the unrolling. As a C-only change
(using `#pragma GCC unroll 24` or a `_Pragma`), the benefit is modest but
measurable on in-order cores because it also eliminates conditional branch
mispredictions.

**Experiment slot:** Bundled with `EXP-keccak-rv64imb-asm` (A1).

---

### Track B — Fix `fips202_zvbb_gather.c`: Constant Index Pre-Load

**Applicable to:** `karatsuba_zvbc` (and `karatsuba_zbc` if a vector Keccak
path is added there)  
**ISA requirements:** `Zvbb` (`vror.vv`, `vandn.vv`) — already present  
**Effort:** low–medium  
**Expected gain:** reduce `vle64.v` index loads by 96 instructions / call;
projected ~30–40% reduction in Keccak instret for the gather variant

#### B1 — Hoist Index Vectors Outside the Round Loop (Completed)

**Problem.** `pi_src[25]`, `ror_amounts[25]`, `chi_x1[25]`, `chi_x2[25]` are
constant arrays used identically in every round. They are currently re-loaded
from memory inside the 24-round loop, costing 4 × 24 = 96 `vle64.v` per call.

**Solution.** Load all four index vectors once before the loop and hold them in
dedicated LMUL=8 register groups throughout. With VLEN=256 and LMUL=8 we have
exactly 4 register groups (v0–v7, v8–v15, v16–v23, v24–v31):

```
Proposed register assignment:
  v0–v7  (group 0): main state         — live throughout
  v8–v15 (group 1): temporaries t/vB   — can alias with rounds
  v16–v23 (group 2): idx_pi + idx_ror  — pre-loaded, constant
  v24–v31 (group 3): idx_x1 + idx_x2  — pre-loaded, constant
```

Theta continues as scalar (store-XOR-reload), which is the only path still
using group 1 as a staging area. The two `vrgather` temporaries needed in Chi
can reuse group 1 after the Theta reload.

This eliminates all 96 index `vle64.v` from the hot path; only the
2 × 25-element `vle64.v` for the Theta store/reload remain, yielding an
estimated 30–40% reduction in total instruction count for this variant.

**Experiment slot:** `EXP-keccak-zvbb-gather-v2`

**Status:** Completed and promoted to `main`. The four constant index vectors are
hoisted outside the 24-round loop, KAT correctness was verified, and the
implementation was pushed as the stable Track B1 baseline.

#### B2 — Vectorize Theta: Eliminate Store-to-Stack

**Problem.** Theta currently uses a full store-to-stack round trip (200 bytes
written + 200 bytes read per round). Although VLEN=256/LMUL=8 makes these
single vector instructions, the round-trip has hidden latency on in-order
pipelines.

**Solution (option 1).** Use `vmv.x.s` + `vslidedown.vi` to extract the 5 columns
needed for C[x] into scalar registers, compute D[x] in scalars, then inject
back using `vslide1up.vx` or element-wise `vxor.vx` with a mask. This avoids
any memory traffic for Theta and keeps the state in v0 throughout.

**Solution (option 2).** Restructure state layout so columns are contiguous:
store the state in *column-major* order (5 columns × 5 rows instead of
row-major). Column sums C[x] then become a vl=5 reduce-XOR over consecutive
elements, expressible with LMUL=1 operations without gather. This requires a
one-time transpose at state init and inverse-transpose at the end; the cost
amortises over 24 rounds.

**Experiment slot:** `EXP-keccak-zvbb-vectorized-theta`

#### B3 — Eliminate Chi vrgather with Row-Slice Shuffles

**Problem.** Chi uses two `vrgather.vv` to collect `A[x+1]` and `A[x+2]`
within each row. `vrgather` with LMUL=8 is an element-scatter/gather and is
not pipelined efficiently on most implementations.

**Solution.** Chi's x+1 and x+2 shifts are cyclic within a row of 5 and are
completely regular. Instead of vrgather, maintain three aligned *copies* of the
state, each rotated by one position within its row (using `vslide1up.vx` and
wrapping logic applied per-row). With three copies live, Chi becomes 2 ×
`vxor.vv` + 2 × `vandn.vv` and no gather. The register budget constraint
(LMUL=8: only 4 groups) is tight — this requires sharing groups with Theta or
reducing to LMUL=4 combined with VLEN-dependent loop pipelining, which limits
VLEN portability.

**Experiment slot:** `EXP-keccak-zvbb-no-gather-chi`  
**Note:** B1 is the prerequisite; measure B1 alone before attempting B3.

---

### Track C — Multi-Way Batched Keccak (N-Way Parallel Instances)

**Applicable to:** all variants once the underlying parallel SHAKE API is added  
**ISA requirements:** standard RVV 1.0 (`V` extension; Zvbb for `vror`, `vandn`)  
**Effort:** high (requires HQC caller changes + new API)  
**Expected gain:** up to N× throughput for N independent instances

#### C1 — Audit HQC's SHAKE Call Graph for Batching Opportunities

Before implementing a parallel API, profile which SHAKE instances are
independent (no data dependency between them). In the current HQC reference:

| Operation | SHAKE contexts | Data dependency |
|-----------|----------------|-----------------|
| `hqc_pke_keygen` | `dk_xof_ctx`, `ek_xof_ctx` | Independent after `hash_i` |
| `hqc_pke_encrypt` | `theta_xof_ctx` | Single context, sequential |
| `hqc_pke_decrypt` | `theta'_xof_ctx` + hash | Partially independent |

`keygen` presents a clear **2-way** batching opportunity: `dk_xof_ctx` and
`ek_xof_ctx` both derive from a shared seed but are then advanced independently
to generate `(x, y)` and `h` respectively. These could be processed as a
2-instance parallel SHAKE call once the seed derivation step is complete.

Further opportunities may exist in the generate-random-support functions
(`vect_generate_random_support1`, `vect_generate_random_support2`) which are
called sequentially with different contexts from the same KEM operation.

**Action:** instrument the KEM with lightweight call counters to measure
(a) how many active SHAKE contexts coexist at any point, and (b) their input
sizes, to select the optimal batch width N.

**Experiment slot:** `EXP-keccak-batch-audit`

#### C2 — N-Way shake256_xN() API + SHAKE State AoS→SoA

**Design (following Zhang et al., §3.3):**

- Add `shake256_x2()`, `shake256_x4()` APIs that accept N independent
  `(input_i, inlen_i, output_i, outlen_i)` tuples.
- Internally, pack N independent Keccak states in SoA layout:
  `vuint64m1_t st[25]` where `st[lane]` holds the same lane from all N
  instances. At `vl=N`, every vector instruction processes all N instances
  simultaneously.
- Use `vsetivli` (fixed-length) throughout: with VLEN=256, SEW=64, LMUL=1,
  `vsetivli zero, 4, e64, m1` gives exactly 4 lanes (N=4).
- Absorb: XOR input bytes into the state using byte-granularity gather or
  direct 64-bit loads at rate-aligned boundaries.
- Permute: the `KeccakF1600_x4` inner function runs 24 rounds with vl=4.
  θ column sums use 5 × `vredxor.vs` (or explicit vector add-chains); ρ/π use
  element-wise shift + OR (since `vror.vv` with Zvbb replaces two shifts);
  χ uses `vandn.vv` from Zvbb; ι uses `vxor.vx` with the round constant.

**Experiment slot:** `EXP-keccak-x4-rvv`

#### C3 — Hybrid 1+3 Scalar/Vector (PQRV Style)

**Background.** Zhang et al.'s headline result for RV64IMBV is a *hybrid*
implementation: 1 instance is computed using the fully-unrolled scalar path
(A1), and N−1 remaining instances are computed using the vector path (C2),
with scalar and vector instructions *interleaved* to fill both issue slots of
a dual-issue in-order CPU.

`fips202_rv64imbv_hybrid_x4.S` in the PQRV artifact demonstrates the technique
for N=4 (1 scalar + 3 vector). On the C908 (dual-issue in-order) this achieves
higher throughput per cycle than either pure scalar or pure vector alone, because
scalar Keccak instructions (integer ALU port) do not compete with vector Keccak
instructions (vector execution unit).

**Applicability to our target.** The approach is directly applicable if our
SPIKE/QEMU model or target board exposes a dual-issue pipeline. For
instruction-count-only profiling on Spike it will show no benefit; it should
be evaluated on real FPGA emulation or a cycle-accurate simulator.

**Experiment slot:** `EXP-keccak-hybrid-1plus3`  
**Prerequisite:** both A1 (scalar assembly) and C2 (N-way vector) must exist.

---

### Track D — Zvksh / Future ISA Extensions

**Note:** This track is speculative; no hardware exists today that our QEMU
target exposes.

#### D1 — Monitor Zvksh (SM3 / SHA-3 Vector Crypto)

The RISC-V Vector Cryptography extension (`Zvksh`) provides `vsm3me.vv` and
`vsm3c.vi` for SM3, and `vsm4k.vi` / `vsm4r.vv` for SM4. No Keccak-specific
vector instruction is currently standardised (unlike ARMv8.2's dedicated SHA3
FEAT_SHA3 instructions `eor3`, `rax1`, `xar`, `bcax`).

However, the RISC-V crypto working group (task group `zvkn`, `zvks`) is
active. Any future `Zvkk` or custom Keccak opcode would change the calculus
entirely and should be monitored at <https://github.com/riscv/riscv-crypto>.

#### D2 — ARM SHA3 Instruction Analogy (Cross-Architecture Reference)

On ARMv8.2+SHA3, the permutation maps to:

```
eor3  vD, vA, vB, vC   ; 3-way XOR            (θ column-sum step)
rax1  vD, vA, vB        ; rotate-accumulate XOR (θ D[x] = C[x-1] ^ ROL(C[x+1],1))
xar   vD, vA, vB, #imm  ; XOR + rotate         (ρ/π combined)
bcax  vD, vA, vB, vC    ; bitclear-and-XOR     (χ: a ^ ((~b) & c))
```

These 4 instructions replace approximately 10 scalar operations per step,
giving ~4–6× speedup over scalar on A55/A72. For RISC-V, the closest
analogues with current extensions are:
- `eor3`: no equivalent; needs 2 `xor` instructions
- `rax1`: can be fused with `rori` (Zbb) = 2 instructions
- `xar`: `rori` + `xor` = 2 instructions
- `bcax`: `vandn.vv` (Zvbb, vector only) or `andn + xor` (scalar Zbb)

Understanding this gap is useful context for the thesis chapter comparing
ISA suitability for Keccak across architectures.

---

## 2. Literature and Artifact References

| Reference | Technique | Relevance |
|-----------|-----------|-----------|
| Zhang, Yan, Huang, Koç — "Optimized Software Implementation of Keccak, Kyber, and Dilithium on RV{32,64}IM{B}{V}", **TCHES 2025** (ePrint [2024/1515](https://eprint.iacr.org/2024/1515)) | Scalar RV64IMB assembly, lane-complementing, dual-issue scheduling, hybrid 1+(N-1) batched vector. Best published results for Keccak on RISC-V. | Primary reference for all A/C3 tracks. Artifact: <https://github.com/Ji-Peng/PQRV/tree/ches2025> (`sha3/` directory) |
| Bertoni, Daemen, Peeters, Van Assche — **KeccakTools + XKCP** | Lane complementing, in-place permutation, optimised 64-bit C | Reference for Track A2. <https://github.com/XKCP/XKCP> |
| Stoffelen — "Efficient cryptography on the RISC-V architecture", **CHES 2019** | RV32IM optimised Keccak; 32-bit bit-interleaving | Baseline for 32-bit; not directly applicable to our RV64 target but cited in PQRV |
| RISCV-Crypto working group — `sha3/` benchmark, `Zscrypto_rv64` | RV64IMB scalar Keccak using B-extension bitmanip | Included in PQRV comparison table (artifact: `sha3/riscvcrypto_rv64imb.c`) |
| Kannwischer, Rijneveld, Schwabe, Stoffelen — **pqm4** | ARMv7-M multi-instance Keccak for Kyber/Dilithium | Cross-platform baseline; shows batching gain on constrained cores |
| Neon / ARMv8.2-SHA3 implementations in **PQClean** | SHA3 instruction set analogy (D2) | Reference for ARM speedup magnitudes |

---

## 3. Prioritised Experiment Queue

Based on the profile data and literature survey, the recommended execution
order is:

| Priority | Experiment | Track | Prerequisite | Est. gain on Keccak instret |
|----------|-----------|-------|--------------|-----------------------------|
| 1 | `EXP-keccak-zvbb-gather-v2` (DONE: hoisted + KAT + merged to `main`) | B1 | none | ~30–40% |
| 2 | `EXP-keccak-rv64imb-asm` | A1+A3 | none | ~50–60% |
| 3 | `EXP-keccak-zvbb-vectorized-theta` | B2 | B1 | +5–15% on top of B1 |
| 4 | `EXP-keccak-batch-audit` | C1 | none (analysis only) | baseline for C2 |
| 5 | `EXP-keccak-x4-rvv` | C2 | C1 | up to 4× on batched path |
| 6 | `EXP-keccak-lane-complement` | A2 | A1 | +2–5% on top of A1 |
| 7 | `EXP-keccak-hybrid-1plus3` | C3 | A1 + C2 | hardware-dependent |
| 8 | `EXP-keccak-zvbb-no-gather-chi` | B3 | B1 + B2 | +5–10% on top of B1+B2 |

Items 1 and 2 are independent and can be developed in parallel branches.
Item 4 (the call-graph audit) can overlap with any other experiment as it is
read-only analysis.

---

## 4. Validation Checklist (per experiment)

All Keccak experiments must pass the following before promotion to `src/`:

- [ ] `ctest` KAT suite passes for all three security levels (HQC-128/192/256)
- [ ] `shake256` output matches reference for at least 10 000 input/output pairs
- [ ] Spike `instret` on `bench_keccak_f1600_isolated` (to be added) logged
- [ ] Spike `instret` for decaps L128/192/256 logged and compared to baseline table above
- [ ] Constant-time check: no input-dependent branch or memory address in the
      permutation core (see `analysis/ct_verify/`)
- [ ] Code size (`codesize` column in benchmark output) within 2× of reference

---

## 5. Open Questions

1. **Does the target simulator/board expose a dual-issue pipeline?**  
   Track C3 and the dual-issue scheduling benefit of A1 are only measurable
   on real hardware or a cycle-accurate model. Spike counts retired instructions,
   not wall-clock cycles.

2. **What is the minimal batch size N for HQC?**  
   The batch audit (C1) needs to determine whether HQC ever has ≥ 2 truly
   independent SHAKE instances active simultaneously. If the answer is "only
   sometimes", the x4 path needs a scalar fallback for the tail instances.

3. **Can we apply the hybrid approach to our ISA (rv64gcv_zbc_zbb_zbkb_zvbc_zvbb)?**  
   PQRV targets `rv64imbv` (B extension); our ISA additionally includes `Zvbc`
   and `Zvbb`. The hybrid assembly scheduling in C3 would need to be re-derived
   for our specific extension combination.

4. **Is `vrgather` latency the dominant cost, or is it `vle64.v` bandwidth?**  
   B1 eliminates index loads but keeps `vrgather`. If the gather itself is the
   bottleneck (not the loads), Track B3 becomes equally important to B1. This
   should be measured with Spike's memory access counters after implementing B1.
