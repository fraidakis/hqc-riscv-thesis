# Chapter 7: Security Analysis and Engineering Diagnostics

## 7.x Formal Constant-Time Model Under the RISC-V Zkt Profile

This section formalizes the constant-time claim for the accelerated HQC implementation under the RISC-V Data Independent Execution Time profile, denoted `Zkt`.

Let $\mathcal{I}_{\mathrm{Zkt}}$ denote the instruction subset whose execution latency is architecturally constrained to be independent of operand values on a compliant microarchitecture. In particular, for any instruction $i \in \mathcal{I}_{\mathrm{Zkt}}$, any two operand tuples $x,y$ of equal width, and any fixed microarchitectural context $\mu$ (pipeline configuration, cache state, branch predictor state, privilege mode), the execution-time observable satisfies

$$
T(i,x,\mu) = T(i,y,\mu).
$$

For an implementation trace $\tau = (i_1,\ldots,i_n)$, constant-time with respect to secrets follows when:

1. The dynamic control-flow graph is secret-independent.
2. Memory-access addresses are secret-independent (or access-equivalent by construction).
3. The instruction multiset in $\tau$ is contained in $\mathcal{I}_{\mathrm{Zkt}} \cup \mathcal{I}_{\mathrm{safe}}$, where $\mathcal{I}_{\mathrm{safe}}$ includes fixed-latency integer/vector instructions and register-local data movement not introducing secret-dependent timing channels.

Under these conditions, the total latency of the trace

$$
T(\tau, s, \mu) = \sum_{k=1}^{n} T(i_k, \mathrm{op}_k(s), \mu)
$$

is invariant with respect to the secret input $s$, hence eliminating first-order timing leakage at the instruction-latency layer.

### Applicability to This Thesis

The accelerated HQC kernels were engineered to remain within this model. Specifically, the hand-optimized scalar and vector routines for:

- carry-less polynomial arithmetic (GF2x),
- Keccak permutation and absorb/squeeze primitives,
- Reed-Solomon lane-packed finite-field processing,

were constrained to instruction forms and coding patterns compatible with `Zkt` timing discipline. The relevant instruction classes include carry-less multiply (`clmul`), rotate and bit-manipulation primitives (including `rori`-class operations), vector carry-less multiply (`vclmul`), and Boolean dataflow (`xor`, `and`, `or`, shifts, and vector analogues). In each case, implementation choices intentionally avoid secret-dependent branches and secret-dependent memory-indexing patterns.

Therefore, on a processor that correctly implements the `Zkt` profile, the thesis contribution termed "Pure Software Acceleration" is not merely performance-preserving but security-aligned: acceleration is obtained without relaxing timing-side-channel resistance assumptions.

## 7.x Engineering Diagnostics and Architectural Boundary Conditions

### Diagnostic Result 1: ISA-Level Speedups Must Be Mapped to the Active Entropy Path

A key engineering finding is that cryptographic acceleration opportunities are meaningful only when they intersect the execution path exercised by the benchmarked protocol. Extension-level experiments around AES acceleration demonstrated mathematically correct behavior and full KAT integrity, yet no macro-level KEM instruction-count impact when AES was outside the protocol-critical randomness path.

This establishes a practical diagnostic rule for co-design work: ISA extension exploration must be trace-mapped to the live call graph of the security primitive under evaluation, otherwise measured gains may be local but system-level neutral.

### Diagnostic Result 2: Verification Hierarchy Must Couple Correctness and Path Attribution

The evaluation pipeline used in this work implicitly defines a two-layer acceptance criterion:

1. Functional correctness: complete KAT conformance at all security levels.
2. Architectural relevance: demonstrated reduction in the instruction-cost surface of the target protocol path.

Passing (1) without satisfying (2) is necessary but insufficient for promotion. This distinction prevented overfitting to non-critical subcomponents and kept the stable tree aligned with thesis-level optimization objectives.

### Diagnostic Result 3: Constant-Time Claims Require Both Instruction Policy and Structural Discipline

`Zkt` compliance alone does not automatically imply end-to-end side-channel robustness if software introduces secret-dependent control or address behavior. The implemented kernels therefore enforce a structural constant-time discipline (fixed loop structure, masked/Boolean selection, deterministic access geometry) in addition to instruction-subset discipline. The security argument is thus compositional: architectural timing guarantees from `Zkt` plus software-level non-interference constraints.

## 7.x Final Security Statement

Given the enforced instruction-subset policy, secret-independent control and addressing structure, and successful cross-level validation, the accelerated HQC implementation maintains a defensible constant-time posture on `Zkt`-compliant RISC-V microarchitectures. Accordingly, the thesis concludes that software-only performance gains can be realized without violating timing-side-channel design goals, provided that optimization is governed by explicit architectural security profiles and validated with protocol-aware diagnostics.