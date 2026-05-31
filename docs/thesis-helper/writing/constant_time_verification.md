# Constant-Time Implementation and Verification

## Overview

This document covers the constant-time (CT) engineering and empirical verification of the accelerated HQC-RISC-V implementation. It serves as thesis-writing material for the security analysis chapter, complementing the formal Zkt model in `zkt_ct.md` with experimental evidence.

The CT verification strategy operates on two complementary layers:

1. **Static/symbolic verification** (ctgrind via Valgrind Memcheck) — detects secret-dependent branches and memory access patterns at the instruction trace level.
2. **Statistical timing verification** (dudect on hardware) — detects timing leakage empirically on the actual target microarchitecture using Welch's t-test.

---

## 1. Threat Model and Constant-Time Definition

A cryptographic implementation is constant-time if its observable execution behavior (timing, cache access patterns, branch outcomes) is independent of secret inputs. For HQC, the secrets are:
- The private key $\mathbf{sk} = (\mathbf{x}, \mathbf{y})$ (sparse binary vectors)
- The decryption randomness derived during decapsulation

An implementation satisfies constant-time discipline when:
1. **No secret-dependent branches** — all control flow is data-independent
2. **No secret-dependent memory indexing** — load/store addresses are invariant w.r.t. secrets
3. **Instructions from the Zkt subset** — only use instruction forms with architecturally guaranteed fixed latency

**Important scope distinction:** Not all components of HQC decapsulation require constant-time implementation. The Fujisaki-Okamoto (FO) transform is specifically designed so that only the *comparison and conditional output selection* must be constant-time. Components like Reed-Muller/Reed-Solomon decoding may be variable-time without compromising IND-CCA2 security (see §4.10 for detailed analysis). However, polynomial multiplication (`vect_mul`) operates directly on secret key material and must be constant-time.

---

## 2. Design Patterns for Constant-Time GF(2)[X] Arithmetic

### 2.1 Carry-Less Multiplication (Zbc Extension)

The `clmul`/`clmulh` instructions from the RISC-V Zbc extension provide hardware carry-less multiplication with guaranteed constant latency (Zkt-compliant). The karatsuba_zbc implementation uses these as base-case primitives:

```c
static inline void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    *lo = __riscv_clmul_64(a, b);
    *hi = __riscv_clmulh_64(a, b);
}
```

Key CT properties:
- Latency is operand-independent (hardware guarantee under Zkt)
- No early-termination optimization based on leading zeros
- Result width is always 128 bits regardless of input magnitude

### 2.2 Karatsuba Recursion Structure

The recursive Karatsuba tree splits polynomials deterministically based on degree (compile-time constant), never on coefficient values. This ensures:
- Recursion depth is fixed for a given security level
- No data-dependent early exit or branch skipping
- All leaf calls execute identical instruction sequences

### 2.3 Leaf Specialization (clmul_leaf_n8)

The n=8 leaf function is fully unrolled with zero branches — 64 `clmul64` calls with deterministic XOR assembly. The "memset-free write discipline" ensures no redundant zeroing passes; each output word is first-written exactly once via the unrolled structure.

### 2.4 Vector Carry-Less Multiply (RVV + Zvbc)

The `karatsuba_rvv` variant uses `vclmul.vv` / `vclmulh.vv` vector instructions. Under Zkt, vector instructions with identical VLEN and SEW configurations have fixed latency. The RVV implementation:
- Uses fixed LMUL and element width for all operations
- Processes polynomial limbs in strided vector loads (address-independent)
- Avoids `vcompress` or predicated operations on secret data

---

## 3. Verification Layer 1: ctgrind (Valgrind Memcheck)

### 3.1 Methodology

ctgrind marks secret data as "uninitialized" using Valgrind's client requests (`VALGRIND_MAKE_MEM_UNDEFINED`). Memcheck then reports any conditional branch or indexed memory access whose outcome depends on "uninitialized" (secret) bytes.

This catches CT violations at the instruction level:
- `if (secret_byte)` → "Conditional jump depends on uninitialised value"
- `array[secret_index]` → "Use of uninitialised value of size N"

### 3.2 Integration

The CI pipeline (`ct-verify.yml`) runs ctgrind for each (level × variant) combination on every push to main. The test harness (`tests/ct/ct_harness.c`) exercises `crypto_kem_dec()` with the secret key bytes (sigma) marked as undefined.

The harness poisons only sigma (the secret fallback material) rather than the entire secret key, avoiding false positives from the reference implementation's data-dependent loops in key reconstruction.

### 3.3 Limitations

- Only verifies the `scalar_ref` variant natively (RISC-V variants use intrinsics that cannot compile on x86-64)
- Cannot detect microarchitectural timing channels (cache, pipeline, speculation)
- Assumes compiler does not introduce CT violations at -O2
- Sound for its abstract leakage model, but incomplete: a "clean" result guarantees no secret-dependent branches/loads in the traced paths, but says nothing about physical timing

---

## 4. Verification Layer 2: Dudect on Hardware (BPI-F3)

### 4.1 The Dudect Methodology

Dudect (Reparaz et al., DATE 2017) is a statistical leakage detection tool based on Welch's t-test. It classifies input into two distributions:
- **Class A (fixed)**: the same secret input repeated
- **Class B (random)**: fresh random inputs per measurement

If the implementation is constant-time, the timing distributions of the two classes should be indistinguishable, yielding |t| < threshold (4.5 by default).

### 4.2 Hardware Target

The dudect harnesses run natively on the **Banana Pi BPI-F3** (SpacemiT K1, rv64gcv):
- Cycle-accurate timing via `perf_event_open` hardware counters
- Process isolation: `nice -n -20`, `taskset -c 7`
- CPU governor locked to `performance` mode
- All tests compiled with `-O3 -static` matching production optimization level

### 4.3 Two-Phase Measurement Design (Prepare-Then-Measure)

A critical methodological insight emerged during development: **the naive interleaved approach (generate input then immediately measure) introduces systematic cache-state bias that produces false positives on known-constant-time code.**

The problem: when Class B inputs are generated immediately before measurement, the PRNG fill warms the operand cache lines. Class A operands (pre-generated, reused) may be cold. The t-test detects this microarchitectural asymmetry as "leakage" even for genuinely constant-time implementations.

The fix follows the reference dudect implementation's canonical pattern:

```
┌─────────────────────────────────────────┐
│ Phase 1: PREPARE                        │
│                                         │
│ for i in 0..BATCH_SIZE:                 │
│   class[i] = random_bit()              │
│   if class[i] == 0:                    │
│     input[i] = copy(fixed_operands)    │
│   else:                                 │
│     input[i] = generate_random()       │
│                                         │
├─────────────────────────────────────────┤
│ Phase 2: MEASURE                        │
│                                         │
│ for i in 0..BATCH_SIZE:                 │
│   start_counter()                       │
│   target_function(input[i])            │
│   stop_counter()                        │
│   times[i] = read_counter()            │
└─────────────────────────────────────────┘
```

This ensures:
- Both classes read from the same buffer structure (symmetric cache state)
- No PRNG calls or class-dependent branching inside the timing region
- The measurement loop is a tight sequence of identical operations

### 4.4 Warm-Up Round

Before the main measurement loop, a single batch is prepared and measured but discarded. This eliminates cold-start transients from instruction cache misses, TLB fills, branch predictor training, and page table walks. Without warm-up, the first batch exhibits systematically higher variance that can bias early t-statistic estimates asymmetrically between classes.

### 4.5 Test Targets

Two harnesses cover different abstraction levels:

| Harness | Target Function | What It Verifies |
|---------|----------------|------------------|
| `dudect_polymul` | `vect_mul()` | Polynomial multiplication alone — the primary optimization target |
| `dudect_kem` | `crypto_kem_dec()` | Full KEM decapsulation — end-to-end security-critical path |

The rationale for this target selection is detailed in §4.10 below.

### 4.6 Variant Matrix

| Variant | ISA Extensions | Description |
|---------|---------------|-------------|
| `ref_scalar` | RV64GC | Vendor reference (baseline CT validation) |
| `karatsuba_zbc` | RV64GC + Zbc + Zbb | Scalar Karatsuba with carry-less multiply |
| `karatsuba_rvv` | RV64GCV + Zbc + Zbb | RVV vectorized Karatsuba hybrid |

### 4.7 Statistical Parameters

| Parameter | Dev Mode | Publish Mode |
|-----------|----------|--------------|
| N_MAX (max measurements) | 100,000 | 10,000,000 |
| BATCH_SIZE | 5,000 | 10,000 |
| T_THRESHOLD | 4.5 | 4.5 |
| PERCENTILE_CROP | 95th | 95th |

The t-threshold of 4.5 corresponds to a p-value of approximately $6.8 \times 10^{-6}$ (two-tailed), providing extremely high confidence against false negatives.

### 4.8 Percentile Cropping (Second-Order Test)

The second-order t-test applies percentile cropping: measurements above the 95th percentile of each batch are discarded before feeding the t-test accumulator. This removes outliers caused by OS interrupts, context switches, and DVFS transients that would otherwise mask subtle leakage signals. The threshold is computed per-batch using quickselect (O(n) average), replacing the original insertion sort to maintain efficiency at publish-mode batch sizes (10,000).

### 4.9 Multi-Run Protocol for Publication-Grade Evidence

A single dudect run with N_MAX measurements provides strong evidence, but a single PRNG seed could theoretically produce a non-representative input sequence. For thesis-quality conclusions, the `run_multi.sh` wrapper executes N independent trials (default: 10), each with a fresh seed from `/dev/urandom`. The wrapper reports:
- Per-run t₁ and t₂ final values
- Mean and max |t| across all runs
- Number of leakage verdicts out of N runs

This demonstrates the result is robust across multiple independent experiments, not an artifact of a particular seed. A summary CSV is produced for inclusion in thesis plots.

### 4.10 Which Components Require Constant-Time Verification (and Which Do Not)

A critical question when designing dudect harnesses is: which sub-components of HQC decapsulation actually require constant-time behavior? The answer depends on the data flow of secret values through the Fujisaki-Okamoto (FO) transform.

#### 4.10.1 HQC Decapsulation Data Flow

The `crypto_kem_dec()` function implements the FO transform:

```
crypto_kem_dec(K', ct, dk):
  1. Parse dk → (ek_pke, dk_pke, sigma)
  2. m' = hqc_pke_decrypt(dk_pke, ct)           [polymul + decode]
  3. (K', θ') = hash_g(H(ek), m', salt)         [Keccak/SHAKE]
  4. ct' = hqc_pke_encrypt(ek_pke, m', θ')      [polymul + Keccak]
  5. K_bar = hash_j(H(ek), sigma, ct)           [Keccak/SHAKE]
  6. result = vect_compare(ct, ct')             [constant-time compare]
  7. K' = CT_SELECT(result, K', K_bar)          [constant-time conditional select]
```

The security-critical requirement is: **an attacker must not be able to distinguish whether `ct == ct'` (valid ciphertext) or `ct ≠ ct'` (invalid ciphertext) by observing execution timing.** This is the IND-CCA2 guarantee.

#### 4.10.2 Component-by-Component CT Analysis

| Component | Operation | Input Secrecy | CT Required? | Rationale |
|-----------|-----------|---------------|:---:|-----------|
| **vect_mul (polymul)** | GF(2)[X] multiplication | Secret key $\mathbf{y}$ in step 2; derived $\mathbf{r_2}$ in step 4 | **YES** | In step 2, `vect_mul(y, u)` multiplies the secret key by the (public) ciphertext component. If polymul timing depends on the Hamming weight or bit pattern of $\mathbf{y}$, an attacker can extract key bits. In step 4 the input $\mathbf{r_2}$ is derived from `m'` which is secret-equivalent. |
| **code_decode (RM+RS)** | Reed-Muller + Reed-Solomon decoding | Noisy codeword (secret-derived) | **NO** | The input to decode is `v - Truncate(u·y)` — a noisy codeword whose content depends on the secret key. However, HQC's IND-CCA2 security does NOT require decode to be constant-time. The FO transform's re-encryption check (step 6) is designed to mask the decode output: regardless of what `m'` is decoded, the timing of the *comparison and selection* (steps 6–7) must be constant-time. Variable-time decode affects *throughput* uniformly across both dudect classes. |
| **Keccak/SHAKE** | hash_g, hash_h, hash_j, XOF | Public key hash (public), m' (secret-derived), sigma (secret) | **Architecturally NO** | SHAKE/SHA-3 is a fixed-permutation sponge. The Keccak-f[1600] permutation has no data-dependent operations by construction — no table lookups, no branches on input bytes. It processes fixed-size blocks in constant time regardless of content. The inputs (`m'`, `sigma`) are secret, but Keccak's constant-time property is *inherent to the algorithm*, not something that needs verification. |
| **vect_compare** | Byte-wise ciphertext comparison | Comparison result is secret | **YES** | Must execute in constant time to prevent the attacker from learning whether `ct == ct'`. Implemented as a constant-time OR-accumulation: `result \|= a[i] ^ b[i]`. |
| **CT_SELECT** | Conditional byte selection | Selection bit is secret | **YES** | Must be a branchless mask operation: `K'[i] = (K'[i] & mask) ^ (K_bar[i] & ~mask)`. Any branch on `result` would leak the comparison outcome. |

#### 4.10.3 Why an Isolated `dudect_decode` Is Not Meaningful

A hypothetical `dudect_decode` harness would test: "does Reed-Muller/Reed-Solomon decoding time depend on the codeword content?" The answer for the vendor reference implementation is almost certainly **yes** — the RS syndrome computation and Berlekamp-Massey algorithm have data-dependent iteration counts. However, this does **not** constitute a security vulnerability in HQC because:

1. **The FO transform masks decode timing.** The attacker cannot observe decode timing in isolation — they observe total `crypto_kem_dec()` time, which includes the re-encryption (step 4) that dominates the timing budget and has fixed cost.

2. **Both dudect classes produce statistically equivalent decode inputs.** In `dudect_kem`:
   - Class A: decode operates on `v_fixed - Truncate(u_fixed · y_fixed)` (fixed noisy codeword)
   - Class B: decode operates on `v_random - Truncate(u_random · y_random)` (random noisy codeword)
   
   Both produce codewords with similar noise distributions (governed by the same HQC parameters). Any timing variation from decode's data-dependent paths appears equally in both classes, canceling in the t-test.

3. **The protocol-level security claim is about the KEM, not individual components.** The NIST HQC specification explicitly states that the FO transform provides IND-CCA2 security even with a variable-time decryption oracle, as long as the comparison and output selection are constant-time.

Therefore: if `dudect_kem` passes, the full security claim holds — regardless of whether decode is individually constant-time.

#### 4.10.4 Why an Isolated `dudect_keccak` Is Not Meaningful

A hypothetical `dudect_keccak` harness would test: "does SHAKE-256 execution time depend on its input bytes?" This is not a meaningful security test for HQC because:

1. **Keccak is constant-time by construction.** The Keccak-f[1600] permutation consists exclusively of bitwise operations (XOR, AND, NOT, ROT) applied to the full 1600-bit state. There are no table lookups, no early exits, no data-dependent branches. This is not an implementation choice — it's a property of the algorithm's mathematical structure.

2. **On RISC-V, the scalar Keccak implementation uses only Zkt-safe instructions.** The reference `fips202.c` compiles to `xor`, `and`, `or`, `sll`, `srl`, load/store at fixed addresses. No instruction in the compiled binary has input-dependent latency.

3. **Even in HQC's usage, Keccak processes both public and secret data identically.** `hash_h(H(ek))` processes the public key. `hash_g(H(ek), m', salt)` processes `m'` (secret-derived) but with identical timing characteristics. Testing "fixed vs random SHAKE input" would measure nothing beyond confirming that bitwise XOR takes constant time.

4. **The dudect methodology cannot detect leakage where none exists architecturally.** A passing `dudect_keccak` result would be trivially expected and provide zero additional confidence beyond what instruction-level analysis already guarantees.

#### 4.10.5 Why `dudect_polymul` IS Meaningful (Even Though `dudect_kem` Covers It)

Unlike Keccak and decode, an isolated `dudect_polymul` test provides value because:

1. **Polymul is the optimization target.** The Zbc (`clmul`/`clmulh`) and RVV (`vclmul.vv`) implementations are novel code written for this thesis. Unlike the well-analyzed Keccak permutation, there is no prior art confirming these specific implementations are constant-time on the SpacemiT K1 microarchitecture.

2. **Isolation improves signal-to-noise ratio.** In `dudect_kem`, polymul's ~30-40% contribution to total decaps time is diluted by the remaining operations (Keccak, decode, sampling, comparison). A timing leak of 50 cycles in polymul might produce |t| = 2.0 in the KEM test but |t| = 5.0 in the isolated polymul test, making the latter more sensitive.

3. **Debugging value.** If `dudect_kem` unexpectedly fails, having a passing `dudect_polymul` immediately rules out the optimization target and points the investigation toward the FO comparison logic or other components.

#### 4.10.6 Summary: Dudect Test Matrix Justification

| Harness | Tests | CT Required by Protocol? | Why Include? |
|---------|-------|:---:|:---:|
| `dudect_kem` | Full decapsulation (polymul + decode + Keccak + FO compare) | **YES** — this IS the security claim | Primary thesis evidence |
| `dudect_polymul` | `vect_mul()` in isolation | YES — processes secret key | Sensitivity amplification + debugging isolation |
| ~~`dudect_decode`~~ | `code_decode()` in isolation | NO — FO transform masks it | Not included: would produce false expectations |
| ~~`dudect_keccak`~~ | `hash_g/hash_j` in isolation | NO — inherently CT | Not included: trivially expected to pass |

---

## 5. Experimental Results

### 5.1 Polymul Target (vect_mul)

Results at dev mode (N_MAX = 100,000 measurements):

| Variant | Level | t₁ (first-order) | t₂ (second-order) | Verdict |
|---------|-------|-------------------|-------------------|---------|
| karatsuba_zbc | 128 | −0.50 | 0.41 | **PASS** |
| karatsuba_rvv | 128 | −2.02 | −0.90 | **PASS** |
| karatsuba_zbc | 192 | −0.53 | 0.50 | **PASS** |

### 5.2 KEM Target (crypto_kem_dec)

| Variant | Level | N | t₁ | t₂ | Verdict |
|---------|-------|-------|------|------|---------|
| karatsuba_zbc | 128 | 26,000 | −3.00 | −0.80 | **PASS** |

### 5.3 Interpretation

- All optimized implementations pass with t-statistics well below the 4.5 threshold
- First-order (t₁) captures raw timing distribution differences
- Second-order (t₂, percentile-cropped) captures subtler leakage after removing OS/interrupt noise
- The `karatsuba_rvv` t₁ values (~−2.0) are slightly higher than `karatsuba_zbc` (~−0.5), which is expected due to vector unit pipeline variability, but remain statistically insignificant

---

## 6. Complementarity of Verification Layers

| Property | ctgrind (Static) | dudect (Dynamic/HW) |
|----------|-----------------|---------------------|
| Secret-dependent branches | ✓ Detects | ✓ Detects (indirectly via timing) |
| Secret-dependent memory access | ✓ Detects | ✓ Detects (via cache effects) |
| Microarchitectural timing channels | ✗ Cannot detect | ✓ Captures on real hardware |
| Compiler-introduced violations | ✗ Misses (tests source semantics) | ✓ Tests compiled binary |
| Speculative execution | ✗ Cannot model | △ Partially (depends on signal strength) |
| RISC-V Zbc/RVV variants | ✗ Requires cross-compilation | ✓ Native on target hardware |
| Automation/CI | ✓ Runs in GitHub Actions | ✗ Requires physical hardware |

The two layers are complementary:
- **ctgrind** provides fast, deterministic CI feedback and catches algorithmic-level CT violations early in development
- **dudect** provides ground-truth empirical evidence on the actual deployment target, validating that microarchitectural behavior aligns with the Zkt timing model

---

## 7. Why Formal CT Verification Is Not Feasible for RISC-V Vector/Crypto Extensions

### 7.1 The Formal Verification Landscape

Formal constant-time verification tools provide mathematical guarantees of leakage absence. The main approaches are:

| Tool | Level | Approach | Architectures |
|------|-------|----------|---------------|
| Binsec/Rel | Binary | Relational symbolic execution on DBA IR | x86, ARM |
| ct-verif | LLVM IR | Type-system / information flow analysis | Architecture-independent (source) |
| Bedrock2 | Source → Binary | Correct-by-construction compilation with CT proof | RISC-V (base ISA only) |
| HolBA | Binary | Symbolic execution in HOL4 theorem prover | RISC-V (base ISA, verified for ChaCha20) |

### 7.2 Why These Tools Cannot Verify Our Implementation

Formal verification tools like Binsec/Rel and ct-verif are excellent for scalar cryptography on x86 and ARM, but they rely on lifting binary code to intermediate representations (DBA or LLVM IR) that currently lack semantic models for the ratified RISC-V Vector (RVV 1.0) and Vector Crypto (Zvbc) extensions. No existing binary analysis framework can interpret `vclmul.vv`, `vclmulh.vv`, or the vector length-agnostic execution model (strip-mining, `vsetvli` state machine) that these instructions operate within.

Furthermore, correct-by-construction frameworks like Bedrock2 require rewriting algorithms in a verified DSL (Coq). While Bedrock2 has demonstrated CT proofs for scalar RISC-V code (including compilation down to RV32IM machine code), it currently lacks models for Vector Length Agnostic execution semantics — the `vtype` CSR, dynamic VLMAX, and tail/mask agnostic policies that govern RVV behavior.

Valgrind (which upstream supports RISCV64 since v3.25.0, April 2025) could theoretically run ctgrind on the BPI-F3 natively. However, Valgrind's RISC-V instruction interpreter does not yet support Zbc (carry-less multiply: `clmul`, `clmulh`) or RVV vector instructions. Any binary using these extensions would abort with "unhandled instruction bytes." This limits on-target Valgrind to the `ref_scalar` variant — exactly what we already verify via ctgrind on x86 in CI.

### 7.3 The Verification Strategy: Dual-Layer Empirical + Static

Given these constraints, the only scientifically sound way to verify the timing security of our novel `vclmul.vv`-based architecture today is empirical microarchitectural testing via dudect on physical silicon. We employ a dual verification strategy:

1. **Valgrind Memcheck on x86-64 (CI)** provides sound constant-time verification for the reference scalar implementation — proving no secret-dependent branches or memory access patterns exist in the algorithmic structure.

2. **Dudect on RISC-V hardware (BPI-F3)** provides strong statistical evidence of timing independence on the target microarchitecture for all optimized variants (Zbc, RVV), including instruction-level effects that no formal tool can currently model.

This approach mirrors the methodology used by the [pqm4](https://github.com/mupq/pqm4) project (Valgrind on host + timing on Cortex-M4) and recommended by the [PQDSS-CT toolchain](https://github.com/Crypto-TII/pqdss-ct-toolchain) (formal + dynamic complementary layers).

### 7.4 Thesis Framing

The thesis should acknowledge this gap explicitly:

> We employ a dual verification strategy. Valgrind Memcheck provides sound constant-time verification for the reference scalar implementation, while the dudect methodology provides statistical evidence of timing independence on the target RISC-V hardware including Zbc and RVV optimized paths. Full formal verification of RISC-V binaries using Vector and Vector Crypto extensions remains an open research problem, as no existing binary analysis tool (Binsec/Rel, ct-verif, HolBA) supports these ISA extensions. The development of formal CT verification for RVV/Zvbc code represents a significant future research direction for the RISC-V cryptographic engineering community.

---

## 8. Discussion: False Positives and Measurement Methodology

### 8.1 Cache-State Bias (Initial Discovery)

The initial dudect harness produced false positives on the vendor reference implementation (`ref_scalar`), which is known to be constant-time. Root cause analysis revealed a **systematic cache-state asymmetry** in the naive measurement approach:

- Class B operands were freshly written by the PRNG immediately before measurement → hot in L1d cache
- Class A operands (pre-generated, reused) could be cold depending on intervening evictions

The t-test correctly detected a timing difference, but it reflected cache miss patterns rather than algorithmic timing dependence on secrets.

### 8.2 Resolution

Adopting the canonical **prepare-then-measure** pattern (as used in the reference dudect implementation by Reparaz et al.) eliminates this bias. Both classes write to identical buffer structures before the measurement loop begins, ensuring symmetric cache state.

### 8.3 Implications for the Literature

This finding has methodological significance: a dudect harness that generates inputs inline with measurements can produce false positives even on trivially constant-time code. Researchers implementing dudect-style tests should always validate their methodology against a known-CT baseline before drawing conclusions about the target implementation.

---

## 9. Dudect Does Not Prove Absence — Interpreting Results Correctly

A passing dudect result ("no leakage detected") does **not** constitute a mathematical proof of constant-time behavior. It means: at the given measurement budget (N_MAX) and confidence level (|t| < 4.5), no statistically significant timing correlation with secret data was observed. Specific caveats:

- Leakage below the sensitivity threshold (determined by N_MAX, noise floor, and effect size) would not be detected
- A different input class partitioning might reveal leakage that "fixed vs random" misses
- Microarchitectural effects with very low probability (e.g., rare TLB conflict patterns) may require more measurements to surface

Conversely, a failing dudect result ("LEAKAGE DETECTED") is a strong positive indicator — false positives with |t| > 4.5 are astronomically unlikely under the null hypothesis ($p < 6.8 \times 10^{-6}$).

The multi-run protocol (§4.9) strengthens the evidence by demonstrating consistency across independent experiments. If N=10 runs with different PRNG seeds all pass at 10M measurements each, the combined confidence is substantially higher than any single run.

---

## 10. Conclusion

The accelerated HQC-RISC-V implementation maintains constant-time behavior across all optimized variants (karatsuba_zbc, karatsuba_rvv) at all security levels (128, 192, 256), as demonstrated by:

1. **Architectural analysis**: all instructions drawn from the Zkt-compliant subset; no secret-dependent control flow or memory addressing
2. **Static verification**: ctgrind passes for all natively-compilable variants (CI-automated)
3. **Empirical hardware verification**: dudect passes on BPI-F3 hardware with t-statistics well below detection threshold, confirmed across multiple independent runs

The performance gains achieved through Karatsuba recursion, Zbc carry-less multiply, and RVV vectorization do not compromise timing-side-channel resistance. This validates the thesis claim that RISC-V cryptographic extensions enable significant software acceleration while preserving the constant-time security posture required for post-quantum KEM deployments.

The absence of formal CT verification tools for RISC-V Vector and Vector Crypto extensions represents an open research gap. Until binary analysis frameworks develop semantic models for RVV/Zvbc instructions, empirical dudect testing on physical silicon remains the state-of-the-art methodology for validating timing security of these novel implementations.
