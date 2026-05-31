# 5.2 Vectorized Polynomial Multiplication Acceleration

## 1. Baseline Architecture and Starting Point

The HQC key encapsulation mechanism requires carry-less polynomial multiplication
over $\text{GF}(2)[x]$ modulo $x^n - 1$, where $n \in \{17669, 35851, 57637\}$
corresponds to the three standardised security levels. Polynomials are represented
as dense bit-vectors packed into arrays of `uint64_t` words, yielding operand
sizes of $N = \lceil n/64 \rceil \in \{277, 561, 901\}$ words. The `vect_mul`
function is invoked multiple times per KEM operation (once in key generation,
twice in encapsulation, and up to five times in decapsulation), making it the
single most performance-critical subroutine in the entire scheme.

The reference implementation (`vendor/hqc-ref/src/ref/gf2x.c`) employs a
recursive Karatsuba decomposition with a schoolbook base case. At each recursion
level, the input of $n$ words is split into halves $n_0 = \lfloor n/2 \rfloor$
and $n_1 = n - n_0$, producing three sub-multiplications of size $\leq n_1$
and a linear-cost combine step. Recursion terminates at
`KARATSUBA_THRESHOLD = 16` words, where a schoolbook multiplier computes the
$O(n^2)$ product via nested loops over individual bits of each 64-bit word.

The schoolbook base case in the reference operates at bit granularity: for each
of the 64 bits of each word `a[i]`, a conditional XOR of the shifted operand
`b[]` is accumulated into the result buffer. This yields $64 \times n^2$
conditional shift-and-XOR operations per leaf call — an approach that is
portable but entirely agnostic to hardware carry-less multiply support.

The Karatsuba combine step (Step 4 in the reference) pre-zeroes the output
buffer with `memset`, then XOR-accumulates the three partial products ($z_0$,
$z_2$, $z_{\text{mid}}$) into their respective positions. This zero-then-accumulate
discipline is correct but introduces redundant memory traffic: every word of the
output buffer is written twice (once by `memset`, once by the XOR pass), and
recursive calls further cascade this redundancy across all recursion levels.

Three optimised implementation tracks were developed from this baseline, each
targeting a different subset of the RISC-V ISA extension landscape:

| Track | Label | ISA Extensions | Dominant Technique |
|-------|-------|----------------|-------------------|
| A | `karatsuba_zbc` | Zbc, Zbb | Scalar Karatsuba with `clmul`/`clmulh` leaf |
| B | `karatsuba_rvv` | RVV 1.0 (no crypto) | Direct-write combine with overlap-save, inlined RVV ops |
| C | `karatsuba_zvbc` | Zvbc + V | Vector carry-less multiply leaf |

---

## 2. Optimization Progression (Qualitative)

The evolution from the reference baseline to the promoted implementations
followed a systematic progression, with each stage validated against the full
Known Answer Test (KAT) suite (300 vectors across all three security levels)
before promotion to the stable source tree.

**Phase 1 — Algorithmic Restructuring (Track A foundation).**
The first transformation replaced the bit-granularity schoolbook with a
word-granularity schoolbook exploiting the Zbc extension's `clmul` and `clmulh`
instructions. Each pair of 64-bit input words produces a full 128-bit carry-less
product in exactly two instructions, collapsing the inner loop from 64 conditional
shift-XOR iterations to a single instruction pair. This single substitution
reduced the leaf cost by approximately two orders of magnitude.

**Phase 2 — Memory Discipline Reform.**
The redundant `memset` zeroing was eliminated across the entire recursion tree
by adopting an assign-on-first-write discipline. The combine step was restructured
so that the first writer to each output word uses direct assignment (`=`) rather
than XOR-accumulation (`^=`), making the prior zero unnecessary. This removed
the dominant hotspot (23% of total instructions in the initial Zbc profile).

**Phase 3 — Leaf Micro-Optimization.**
A two-way software pipeline was introduced in the schoolbook leaf, interleaving
independent `clmul64` calls to hide load-use latency. Adjacent products sharing
a common output word were fused ($\text{hi}_j \oplus \text{lo}_{j+1}$), reducing
the XOR and store count per pair. A subsequent four-way unroll of the inner loop
for rows $i \geq 1$ further halved branch overhead.

**Phase 4 — Fixed-Size Leaf Specialisation.**
A fully-unrolled `clmul_leaf_n8` function was introduced for the $n=8$ leaf
size that dominates the recursion tree at threshold $T=16$. This variant
eliminates all loop control (branches, counter increments) and embeds the
assign-on-first-write discipline directly into the unrolled structure, producing
64 `clmul64` calls with zero branches. A subsequent ablation study confirmed
that the $n=4$ specialisation was dead code for the HQC-128 recursion tree and
removed it along with its dispatch comparison.

**Phase 5 — Compile-Time Recursion Tree Generation (AP-2).**
A Python code generator (`gen_karatsuba_specialized.py`) was developed to
traverse the Karatsuba recursion tree for each HQC parameter size and emit
specialised C functions with all buffer offsets, loop bounds, and split sizes
resolved at compile time. The generated functions (`kmul_277`, `kmul_561`,
`kmul_901`) eliminate all runtime threshold checks, dynamic pointer arithmetic,
and recursion overhead. The `vect_mul` entry point dispatches directly to the
appropriate root via a compile-time `switch` on `VEC_N_SIZE_64`.

**Phase 6 — RVV Direct-Write and Overlap-Save Combine (Track B divergence).**
Track B forked from the Track A scalar champion and introduced a direct-write
discipline: the $z_0$ and $z_2$ child multiplications write their results
directly into the output buffer $r$, eliminating the two full-array copy loops
that previously dominated the combine step. An overlap-save strategy resolves
the resulting aliasing hazard by saving the overlap region to scratch before
the combine XOR. All RVV operations are inlined directly into the generated
`kmul_N` functions (via `--mode rvv`) with per-operation LMUL tuning
(LMUL=m8 for ta/tb preparation, LMUL=m4 for overlap save, LMUL=m2 for the
four-source combine XOR). A vectorised modular reduction (`reduce_rvv`) at
LMUL=m8 replaces the scalar `reduce()` loop.

**Phase 7 — Vector Carry-Less Multiply Leaf (Track C divergence).**
Track C replaced the scalar `clmul_leaf` with a `vclmul_leaf` function that
uses the Zvbc extension's `vclmul.vv` and `vclmulh.vv` instructions. A single
vector instruction computes multiple 64×64 carry-less products simultaneously,
collapsing the $O(n^2)$ inner loop to $O(n^2 / \text{VLMAX})$ vector iterations.
A four-way outer-loop unroll keeps eight vector multiply results in flight per
inner iteration.

---

## 3. Variant I — Track A (`karatsuba_zbc`)

Track A targets the `rv64gc_zbc_zbb` ISA profile — a scalar-only
configuration with bit-manipulation and carry-less multiply extensions but
no vector unit. All optimizations in this track are applicable to the widest
range of RISC-V implementations.

### 3.1 Algorithm-Level Optimizations

#### 3.1.1 Word-Granularity Carry-Less Schoolbook (High Expected Impact)

**Extensions used:** Zbc (`clmul`, `clmulh`).

The reference schoolbook operates at bit granularity, iterating over all 64 bits
of each input word. The optimised leaf replaces this with a word-granularity
formulation: for each pair $(a[i], b[j])$, the full 128-bit carry-less product
is computed by a single `clmul`/`clmulh` instruction pair, yielding a low half
$\text{lo} = a[i] \otimes b[j] \bmod x^{64}$ and a high half
$\text{hi} = \lfloor a[i] \otimes b[j] / x^{64} \rfloor$. These half-products
are XOR-accumulated into the result at positions $r[i+j]$ and $r[i+j+1]$
respectively.

This transformation reduces the inner-loop iteration count from $64n$ to $n$
per outer row, eliminating the bit-extraction mask, the conditional branch, and
the variable-shift logic that dominated the reference implementation.

#### 3.1.2 Memset-Free Output Discipline (High Expected Impact)

**Extensions used:** None (pure algorithmic restructuring; base integer `sd`/`ld` only).

The assign-on-first-write discipline restructures the combine step so that no
output word is read before it is first written. The implementation proceeds in
three sequential passes:

- **Loop 1:** Direct-assign $r[0 \ldots 2n_0{-}1] = z_0[0 \ldots 2n_0{-}1]$.
- **Loop 2:** Direct-assign $r[2m \ldots 2m{+}2n_1{-}1] = z_2[0 \ldots 2n_1{-}1]$.
- **Loop 3:** XOR-accumulate $r[m{+}i] \mathrel{\oplus}= z_{\text{mid}}[i] \oplus z_0[i] \oplus z_2[i]$.

Since Loops 1 and 2 cover all positions that Loop 3 reads from $r$, the prior
`memset` is provably unnecessary. The same discipline extends to the leaf: the
first outer row ($i=0$) uses direct assignment, and subsequent rows use
XOR-accumulation into positions already initialised by row 0.

#### 3.1.3 Fixed-Size Recursion Tree Generation — AP-2 (High Expected Impact)

**Extensions used:** None (compile-time code generation; no runtime ISA dependency).

Since the HQC parameter sizes are compile-time constants, the entire Karatsuba
recursion tree is deterministic. A code generator traverses this tree and emits
a specialised function for each unique sub-problem size encountered. For
HQC-128 ($N=277$), the tree decomposes as:

$$277 \rightarrow (138, 139) \rightarrow (69, 69/70) \rightarrow (34, 35) \rightarrow (17, 18) \rightarrow (8, 9)$$

Each generated function (`kmul_277`, `kmul_138`, ..., `kmul_17`) contains:

- Compile-time constant buffer offsets for $z_0$, $z_2$, $z_{\text{mid}}$, $t_a$, $t_b$, and the child scratch buffer.
- Statically-dispatched child calls (no threshold comparison).
- Loop bounds resolved as integer literals, enabling the compiler to fully unroll small loops and apply constant-propagation optimisations.

The generator supports three emission modes (`--mode zbc`, `--mode rvv`,
`--mode zvbc`) to produce track-specific combine code while sharing the
recursion-tree traversal logic.

#### 3.1.4 Toom-Cook 3-Way — AP-1 (Rejected)

**Extensions used:** Zbc (`clmul`, `clmulh` at leaves); base integer for evaluation/interpolation XOR passes.

A Toom-Cook 3-way (TC3) implementation over $\text{GF}(2)[x]$ was investigated
as an algorithmic pivot. TC3 reduces the recursion from three sub-problems of
size $n/2$ to five sub-problems of size $n/3$, yielding asymptotic complexity
$O(n^{\log_3 5}) \approx O(n^{1.465})$ versus Karatsuba's $O(n^{1.585})$.
However, the evaluation and interpolation overhead at each non-leaf node —
approximately 20 XOR passes over size-$k$ arrays — dominated at the HQC
parameter sizes. The implementation passed KAT validation but was empirically
rejected after measurement showed it to be substantially slower than the
Karatsuba champion for HQC-128.

### 3.2 ISA-Level Optimizations

#### 3.2.1 Native `clmul`/`clmulh` Injection (High Expected Impact)

**Extensions used:** Zbc (`clmul`, `clmulh`).

The Zbc extension provides `clmul rd, rs1, rs2` (carry-less multiply, low 64
bits) and `clmulh rd, rs1, rs2` (carry-less multiply, high 64 bits). These
instructions compute the full 128-bit product of two 64-bit polynomials in
$\text{GF}(2)[x]$ without any modular reduction. The leaf multiplier wraps
them in an `always_inline` helper:

```c
static inline void clmul64(uint64_t a, uint64_t b,
                           uint64_t *lo, uint64_t *hi) {
    asm volatile (
        "clmul  %0, %2, %3\n\t"
        "clmulh %1, %2, %3\n\t"
        : "=&r"(*lo), "=r"(*hi)
        : "r"(a), "r"(b)
    );
}
```

The early-clobber constraint on `lo` ensures the compiler does not allocate it
to the same register as an input, preserving correctness when the instruction
pair is scheduled back-to-back.

#### 3.2.2 Threshold Sweep — V2-A1 (Medium Expected Impact)

**Extensions used:** None (compile-time parameter tuning; no ISA dependency).

The optimal Karatsuba threshold $T^*$ minimises the total instruction count
$f(T) = N_{\text{leaves}}(n, T) \times I_{\text{leaf}}(T) + N_{\text{nodes}}(n, T) \times I_{\text{node}}$.
A compile-time sweep over $T \in \{4, 8, 16, 24, 32\}$ was conducted. The
sweep confirmed $T=16$ as the winner for the instruction-accurate simulator
model: smaller thresholds increase recursion depth and framework overhead
faster than they reduce per-leaf cost, while larger thresholds increase the
quadratic schoolbook work without sufficient recursion savings.

### 3.3 Micro-Level Optimizations

#### 3.3.1 Two-Way Software Pipeline with hi⊕lo Fusion (Medium Expected Impact)

**Extensions used:** Zbc (`clmul`, `clmulh`); base integer (`xor`, `ld`, `sd`). No additional extensions beyond Track A baseline.

The inner loop of the schoolbook leaf processes pairs of consecutive $b[j]$
values per iteration. Two independent `clmul64` calls are issued before
consuming either result, creating an instruction-level parallelism window that
hides the load-use latency of the carry-less multiply unit. The adjacent
products share a common output position: $\text{hi}(a[i], b[j])$ and
$\text{lo}(a[i], b[j{+}1])$ both target $r[i{+}j{+}1]$. Fusing them into a
single XOR-and-store ($r[i{+}j{+}1] \mathrel{\oplus}= \text{hi}_0 \oplus \text{lo}_1$)
eliminates one load-modify-store cycle per pair.

#### 3.3.2 Four-Way Inner-Loop Unroll — V2-A4 (Medium Expected Impact)

**Extensions used:** None beyond Zbc (pure loop-scheduling technique; compiler emits base integer instructions).

For rows $i \geq 1$, the inner $j$-loop was extended to a four-way unroll:
four consecutive `clmul64` calls are issued, and the hi⊕lo fusion extends
across all four pairs, converting eight half-products into five output XORs
(saving three `xor` instructions per four-pair group). A two-way tail handles
the remainder, and a scalar epilogue covers the final element for odd-$n$ leaves.

#### 3.3.3 Fixed-Size Leaf Specialisation — V2-A3 (Medium Expected Impact)

**Extensions used:** Zbc (`clmul`, `clmulh`); no additional extensions (full unrolling is a compile-time technique).

The `clmul_leaf_n8` function fully unrolls the $8 \times 8$ schoolbook product
into 64 sequential `clmul64` calls with zero loop control. Each row's write
pattern is statically determined: row 0 uses direct assignment for $r[0..8]$,
and rows 1–7 use XOR-accumulation for previously-written positions while
introducing one fresh assignment ($r[i{+}8]$) at the row's tail. This
eliminates all branch instructions and counter arithmetic from the leaf,
reducing it to a straight-line sequence of multiply-and-accumulate operations.

#### 3.3.4 Dead-Code Ablation (Low Expected Impact)

**Extensions used:** None (source-level dead-code removal; no ISA dependency).

An ablation study of the recursion tree for $N=277$ at $T=16$ revealed that
no leaf call ever produces $n=4$. The `clmul_leaf_n4` specialisation and its
`case 4:` dispatch arm were dead code. Removing them eliminated a superfluous
comparison on every leaf entry, yielding a small but measurable reduction.

#### 3.3.5 Pointer-Bump Addressing — V2-A5 (Rejected)

**Extensions used:** None (base integer addressing mode change only).

An attempt was made to replace index-based addressing (`r[i+j]`) with
pointer-bump addressing (`*rp++`) in the combine loops, hypothesising that
eliminating the `add` instruction for index computation would reduce overhead.
Empirical measurement showed a regression: pointer bumping prevented the
compiler from exploiting RISC-V's free 12-bit immediate offsets on `ld`/`sd`
instructions, introducing extra `addi` pointer-maintenance instructions that
offset the savings. The optimisation was rejected.

### 3.4 Data/Register-Layout-Level Optimizations

#### 3.4.1 Compile-Time Buffer Layout (Medium Expected Impact)

**Extensions used:** None (compile-time constant propagation; no ISA dependency).

In the generated recursion tree (AP-2), all temporary buffer offsets are
resolved as integer literals. The pointer arithmetic that previously computed
$z_0$, $z_2$, $z_{\text{mid}}$, $t_a$, $t_b$, and `child_buf` at each
recursion entry is replaced by direct pointer declarations with constant
offsets from the `tmp` base. This eliminates approximately 12 pointer
computations per recursion node across the entire tree.

#### 3.4.2 Four-Way Unrolled Combine Loops (Low Expected Impact)

**Extensions used:** None (base integer loop unrolling; no ISA dependency).

The three combine loops (copy $z_0$, copy $z_2$, XOR-accumulate
$z_{\text{mid}}$) in the generated tree use four-way unrolled bodies with
explicit indexing (`r_p[0]`, `r_p[1]`, `r_p[2]`, `r_p[3]`), followed by a
scalar tail for the remainder. This reduces loop-control overhead (one branch
per four elements instead of one per element) while keeping the code within
the compiler's inlining budget.

---

## 4. Variant II — Track B (`karatsuba_rvv`)

Track B targets the `rv64gcv_zbc_zbb` ISA profile — the same scalar
extensions as Track A plus the base RVV 1.0 vector extension, but explicitly
excluding all vector-crypto sub-extensions (Zvbc, Zvbb). The scalar leaf
multiplier (`clmul_leaf`, `clmul_leaf_n8`) is identical to Track A; only the
Karatsuba combine and data-movement operations differ.

### 4.1 Algorithm-Level Optimizations

Same as Track A (Sections 3.1.1–3.1.3). The recursion structure, threshold,
memset-free discipline, and AP-2 fixed-tree generation are shared. The code
generator emits Track B combine code via `--mode rvv`.

#### 4.1.1 Direct-Write Discipline — Elimination of z0/z2 Copies (High Expected Impact)

**Extensions used:** None (pure algorithmic restructuring of the Karatsuba combine step).

In the original Track B implementation, the three child multiplications
($z_0$, $z_2$, $z_{\text{mid}}$) were computed into temporary buffers within
`tmp`, and then Loops 1 and 2 copied $z_0$ and $z_2$ into the output buffer
$r$ using `rvv_copy_u64`. The direct-write discipline eliminates these two
copy loops entirely by directing the child calls to write their results
directly into the final output positions:

- $z_0 = a_L \otimes b_L$ is written directly to $r[0 \ldots 2n_0{-}1]$.
- $z_2 = a_H \otimes b_H$ is written directly to $r[2n_0 \ldots 2n_0{+}2n_1{-}1]$.
- Only $z_{\text{mid}}$ remains in `tmp`, since it is consumed destructively
  during the combine step.

This restructuring eliminates two full-array RVV copy loops per recursion
node across the entire tree. For HQC-128 ($N=277$), the recursion tree
contains approximately 30 non-leaf nodes, each of which previously performed
two copies of size $\sim n$ words — the direct-write discipline removes all
of them.

**Temporary buffer reduction:** Since $z_0$ and $z_2$ no longer occupy space
in `tmp`, the per-level temporary footprint drops from $\sim 6n$ words
(zmid + z0 + z2 + ta + tb) to $\sim 4n_0$ words (zmid + ta + tb). Summing
the geometric series across recursion levels yields a total `tmp` requirement
of $< 4N$ words, down from the previous multiplier. The `TMP_BUFFER_WORDS`
constant is accordingly reduced to $4 \times \text{VEC\_N\_SIZE\_64}$.

Stack budget verification (unreduced[$2N$] + tmp\_buf[$4N$], in bytes):
- HQC-128 ($N=277$): $(554 + 1108) \times 8 = 13{,}296$ B $\approx 13.0$ KB
- HQC-192 ($N=561$): $(1122 + 2244) \times 8 = 26{,}928$ B $\approx 26.3$ KB
- HQC-256 ($N=901$): $(1802 + 3604) \times 8 = 43{,}248$ B $\approx 42.2$ KB

All well under the 512 KB bare-metal stack limit.

#### 4.1.2 Overlap-Save Combine Strategy (High Expected Impact)

**Extensions used:** V (RVV 1.0) — `vle64.v`, `vse64.v` for the overlap save; no additional extensions.

The direct-write discipline introduces an aliasing hazard in the combine step
(Loop 3A): the XOR-accumulation $r[n_0{+}i] \mathrel{\oplus}= z_{\text{mid}}[i]
\oplus z_0[i] \oplus z_2[i]$ reads $z_0[i]$ from $r[i]$ and $z_2[i]$ from
$r[2n_0{+}i]$, but writes to $r[n_0{+}i]$ — which overlaps with both source
regions when $i \geq n_0$ (for $z_0$) or when $n_0 + i \geq 2n_0$ (entering
the $z_2$ region).

The overlap-save strategy resolves this by:

1. **Saving the overlap region:** Before the combine step, the $n_0$ words at
   $r[n_0 \ldots 2n_0{-}1]$ (which contain the tail of $z_0$) are copied to
   the `ta` scratch buffer using an inline RVV copy at LMUL=m4.

2. **Splitting Loop 3A into two halves:**
   - **First half** ($i \in [0, n_0)$): Reads $z_0[i]$ from $r[i]$ (safe —
     source indices are strictly below the destination range $r[n_0 \ldots]$).
   - **Second half** ($i \in [n_0, 2n_1)$): Reads $z_0[i]$ from the saved
     `ta[i{-}n_0]` copy (safe — `ta` is a non-overlapping scratch buffer).

3. **Loop 3B (odd-$n$ tail):** For odd-sized splits where $n_0 > n_1$, the
   tail positions $r[n_0{+}2n_1 \ldots]$ are handled separately, reading
   $z_0$ from `ta` and omitting the absent $z_2$ contribution.

This approach preserves correctness without requiring any additional temporary
buffers beyond the already-allocated `ta` (which is reused after the ta/tb
XOR preparation is consumed by the $z_{\text{mid}}$ child call).

### 4.2 ISA-Level Optimizations

#### 4.2.1 Inlined RVV Combine Operations with Tuned LMUL (Medium Expected Impact)

**Extensions used:** V (RVV 1.0) — `vsetvl`, `vle64.v`, `vse64.v`, `vxor.vv`. No Zvbc or Zvbb.

The previous Track B implementation used three shared helper functions
(`rvv_copy_u64`, `rvv_xor3_u64`, `rvv_xor_u64`) with fixed LMUL settings.
The `rvv` code generator now inlines all RVV operations directly into
each generated `kmul_N` function, with LMUL settings tuned per operation
based on the number of simultaneously-live vector register groups:

- **ta/tb XOR preparation** (LMUL=m8 for large nodes, LMUL=m4 for leaf-adjacent
  nodes): Computes $t_a = a_L \oplus a_H$ and $t_b = b_L \oplus b_H$
  simultaneously in a single loop, interleaving loads of both operand pairs.
  At LMUL=m8 with VLEN=256, each iteration processes 32 words — covering the
  full $n_1$ extent in 1–2 iterations for most tree levels.

- **Overlap save** (LMUL=m4): Copies $r[n_0 \ldots 2n_0{-}1]$ to `ta`. Only
  two register groups are live (source, destination), so LMUL=m4 maximises
  throughput without pressure.

- **Combine XOR (Loop 3A halves)** (LMUL=m2): Four source vectors are loaded
  simultaneously (destination $r$, $z_{\text{mid}}$, $z_0$, $z_2$), requiring
  four register groups. At LMUL=m2, each group occupies 2 registers (8 total),
  fitting within the 32-register file with headroom for the accumulator.

- **Loop 3B tail** (LMUL=m2): Same pattern as Loop 3A but without the $z_2$
  source (only three loads), handling the 1–2 word odd-$n$ remainder.

The inlining eliminates function-call overhead (argument setup, stack frame,
return) at every recursion node and enables the compiler to perform
cross-operation register allocation within each `kmul_N` body.

#### 4.2.2 Vectorised Modular Reduction — `reduce_rvv` (Low Expected Impact)

**Extensions used:** V (RVV 1.0) — `vsetvl`, `vle64.v`, `vse64.v`, `vsrl.vx`, `vsll.vx`, `vxor.vv`, `vmv.v.x`. No Zvbc or Zvbb.

The final modular reduction $\text{unreduced}[0 \ldots 2N{-}1] \bmod (x^n - 1)$
was previously performed by a scalar loop. The `reduce_rvv` function replaces
it with an RVV strip-mining loop at LMUL=m8:

For each strip of $\text{vl}$ words starting at position $i$:
1. Load $a[i]$ (low half of unreduced product).
2. Load $a[i + N - 1]$ and right-shift by $(n \bmod 64)$ bits → carries from
   the high half.
3. Load $a[i + N]$ and left-shift by $(64 - n \bmod 64)$ bits → continuation
   carries.
4. XOR all three vectors together and store to $o[i]$.
5. Mask the final word: $o[N{-}1] \mathrel{\&}= \text{LAST\_WORD\_MASK}$.

At VLEN=256 with LMUL=m8, each iteration processes 32 words, reducing the
$N=277$ output in $\lceil 277/32 \rceil = 9$ iterations versus 277 scalar
iterations.

#### 4.2.3 Scalar Leaf Unchanged

**Extensions used:** Zbc (`clmul`, `clmulh`) — identical to Track A; no vector instructions in the leaf path.

The `clmul`/`clmulh` leaf multiply cannot be vectorised without the Zvbc
extension. Track B retains the identical scalar `clmul_leaf` and
`clmul_leaf_n8` from Track A. This architectural constraint means that
Track B's improvement over Track A is bounded by the fraction of total
instructions spent in the combine and data-movement loops.

#### 4.2.4 Aligned Buffer Declarations (Low Expected Impact)

**Extensions used:** None (compiler attribute for memory alignment).

The `unreduced` and `tmp_buf` stack arrays are declared with
`__attribute__((aligned(32)))`, ensuring 256-bit alignment. This guarantees
that all RVV vector loads and stores operate on naturally-aligned addresses,
avoiding potential performance penalties from misaligned accesses on
implementations that do not support efficient unaligned vector memory
operations.

### 4.3 Micro-Level and Data-Layout Optimizations

Same as Track A (Sections 3.3.1–3.3.4, 3.4.1). The leaf micro-optimizations
and compile-time buffer layout are shared identically.

### 4.4 Track B Architectural Observation

The direct-write discipline with overlap-save significantly narrows the gap
between Track B and Track A by eliminating the two dominant copy loops that
previously constituted the bulk of Track B's non-leaf overhead. However, the
fundamental architectural constraint remains: the scalar `clmul`/`clmulh`
leaf is the binding constraint (consuming the majority of total instructions),
and vectorising only the linear-cost combine and data-movement operations
yields bounded returns. The combine loops are $O(n)$ per recursion level
while the leaf is $O(n^2)$ per call; at the HQC parameter sizes, the leaf's
quadratic cost dominates regardless of how efficiently the combines are
executed. This observation motivated the transition to Track C, where the
leaf itself is vectorised.

---

## 5. Variant III — Track C (`karatsuba_zvbc`)

Track C targets the `rv64gcv_zbc_zbb_zvbc` ISA profile, adding the Zvbc
vector carry-less multiply extension to the Track B baseline. The fundamental
architectural change is the replacement of the scalar schoolbook leaf with a
vectorised leaf that exploits `vclmul.vv` and `vclmulh.vv` to compute multiple
64×64 carry-less products per instruction.

### 5.1 Algorithm-Level Optimizations

Same as Track A (Sections 3.1.1–3.1.3), with one key difference: the AP-2
code generator emits `vclmul_leaf` calls at the recursion leaves instead of
`clmul_leaf`/`clmul_leaf_n8`. The recursion structure, threshold, and
fixed-tree generation are otherwise identical.

### 5.2 ISA-Level Optimizations

#### 5.2.1 Vector Carry-Less Multiply Leaf — `vclmul_leaf` (High Expected Impact)

**Extensions used:** Zvbc (`vclmul.vv`, `vclmulh.vv`); V (`vle64.v`, `vse64.v`, `vxor.vv`, `vmv.v.x`, `vsetvl`).

The `vclmul_leaf` function implements a splat-and-sweep pattern: for each outer
index $i$, the scalar value $a[i]$ is broadcast (splatted) across a vector
register using `vmv.v.x`, and the entire inner array $b[0 \ldots n{-}1]$ is
loaded as a vector. A single `vclmul.vv` instruction then computes all $n$
low-half products simultaneously, and `vclmulh.vv` computes all $n$ high-half
products. At VLEN=256 with LMUL=m4, the vector length is 16 elements — matching
the Karatsuba threshold exactly, so the entire inner loop collapses to one
vector iteration per outer index.

The accumulation into the result buffer uses vector load-XOR-store sequences:
the low-half vector is XOR'd into $r[i \ldots i{+}n{-}1]$ and the high-half
vector into $r[i{+}1 \ldots i{+}n]$. This replaces the scalar per-element
load-modify-store with a single vector load, vector XOR, and vector store per
strip.

#### 5.2.2 Assign-First-Write in Vector Leaf — C-3a (High Expected Impact)

**Extensions used:** V (`vse64.v`, `vmv.v.x` for the upper-half pre-zero); no additional extensions beyond the leaf's Zvbc usage.

The same assign-on-first-write discipline from Track A was adapted to the
vector context. The implementation splits the leaf into two phases:

1. **Peeled $i=0$ iteration:** The low-half products are stored with direct
   `vse64.v` (assignment, no prior load) since $r[0 \ldots n{-}1]$ are
   uninitialised. The high-half products are accumulated into $r[1 \ldots n]$,
   where positions $r[1 \ldots n{-}1]$ were just written by the low store and
   position $r[n]$ is pre-zeroed.

2. **Upper-half pre-zero:** A single RVV store (`vmv.v.x(0)` + `vse64.v`)
   zeroes $r[n \ldots 2n{-}1]$ in one instruction. This is necessary because
   the high-half accumulations for $i \geq 1$ read-modify-write these positions
   before any other writer reaches them.

This eliminates the full-buffer `memset` that previously dominated the leaf's
instruction count, replacing it with one vector zero-store covering only the
upper half.

### 5.3 Micro-Level Optimizations

#### 5.3.1 Four-Way Outer-Loop Unroll (Medium Expected Impact)

**Extensions used:** Zvbc (`vclmul.vv`, `vclmulh.vv`); V (`vmv.v.x`, `vle64.v`, `vse64.v`, `vxor.vv`). The unroll itself is a pure scheduling technique.

The outer loop over $i$ is unrolled by a factor of four. Each iteration loads
four scalar values $a[i], a[i{+}1], a[i{+}2], a[i{+}3]$, splats each into a
separate vector register, and issues $4 \times 2 = 8$ vector carry-less multiply
instructions against the same loaded $b$-vector. This amortises the cost of the
$b$-vector load across four outer iterations and keeps eight vector multiply
results in flight simultaneously, saturating the vector functional unit pipeline.

A scalar tail loop handles the remaining $n \bmod 4$ outer iterations using the
single-strip pattern.

#### 5.3.2 Upper-Half Initialisation Strategy (Low Expected Impact — Validated)

**Extensions used:** V (`vslidedown`, `vsetvl`, `vse64.v`). Rejected — the simpler `vmv.v.x` + `vse64.v` pattern was retained.

An alternative to the single-instruction upper-half pre-zero was investigated:
extracting each first-touch upper-half element directly from the high-product
vector using `vslidedown` with `vl=1` and storing it as a direct assignment.
This would eliminate the pre-zero entirely but requires three extra instructions
per outer iteration (`vsetvl(n-1)`, `vsetvl(1)`, `vslidedown`). At the actual
leaf sizes in the HQC-128 recursion tree ($n \leq 9$), the per-iteration
overhead exceeded the cost of the single wide zero-store. The approach was
measured and rejected; the committed implementation retains the one-instruction
`vmv.v.x(0)` + `vse64.v` pre-zero as the register-optimal pattern.

### 5.4 Data/Register-Layout-Level Optimizations

#### 5.4.1 LMUL Selection for Leaf vs. Combine (Medium Expected Impact)

**Extensions used:** V (LMUL configuration via `vsetvl`); Zvbc (leaf operations at LMUL=m4). No additional extensions.

The leaf uses LMUL=m4 for the carry-less multiply operations, consuming 4
register groups (16 physical registers) per vector operand. This leaves
sufficient register space for the splat register, the loaded $b$-vector, and
two result vectors (lo, hi) simultaneously. The combine helpers
(`rvv_copy_u64`, `rvv_xor3_u64`) use LMUL=m4 and LMUL=m2 respectively,
chosen to balance throughput against the number of simultaneously-live source
vectors in the three-way XOR pattern.

#### 5.4.2 Combine Loops — Same as Track B (Pre-Direct-Write)

The Karatsuba combine loops use `rvv_copy_u64` and `rvv_xor3_u64`
helpers (the shared-function approach from Track B's earlier iteration).
The AP-2 generated tree for `--mode zvbc` emits these helpers at the combine
sites while dispatching `vclmul_leaf` at the recursion leaves. The direct-write
discipline introduced in Track B's `--mode rvv` has not yet been ported to
Track C.

---

## 6. Intuition-Based Impact Ranking

The following ranking orders all optimizations by their expected contribution
to overall polynomial multiplication performance, assessed by engineering
intuition based on hotspot proximity and algorithmic scope.

### High Expected Impact

| Optimization | Track(s) | Extensions Used | Rationale |
|---|---|---|---|
| Word-granularity `clmul`/`clmulh` leaf (§3.1.1) | A, B | Zbc | Eliminates the bit-level inner loop entirely; two-order-of-magnitude leaf reduction |
| Memset-free output discipline (§3.1.2) | A, B, C | None (algorithmic) | Removes the single largest hotspot (23% of baseline); affects every recursion level |
| Fixed-size recursion tree — AP-2 (§3.1.3) | A, B, C | None (code generation) | Eliminates all runtime dispatch, threshold checks, and dynamic pointer arithmetic |
| Direct-write discipline — z0/z2 copy elimination (§4.1.1) | B | None (algorithmic) | Eliminates two full-array copy loops per recursion node across the entire tree |
| Overlap-save combine strategy (§4.1.2) | B | V | Resolves aliasing from direct-write without additional buffer allocation; enables safe in-place combine |
| Vector carry-less multiply leaf — `vclmul.vv` (§5.2.1) | C | Zvbc, V | Collapses the $O(n^2)$ inner loop to $O(n^2/\text{VLMAX})$; vectorises the binding constraint |
| Assign-first-write in vector leaf — C-3a (§5.2.2) | C | V | Eliminates the dominant `memset` from the vector leaf path |

### Medium Expected Impact

| Optimization | Track(s) | Extensions Used | Rationale |
|---|---|---|---|
| Two-way software pipeline + hi⊕lo fusion (§3.3.1) | A, B | Zbc | Hides load-use latency; reduces store count per pair |
| Four-way inner-loop unroll (§3.3.2) | A, B | None (scheduling) | Halves branch count in the leaf's hottest loop |
| Fixed-size leaf specialisation — `clmul_leaf_n8` (§3.3.3) | A, B | Zbc | Eliminates all loop control from the dominant leaf size |
| Inlined RVV combine with tuned LMUL (§4.2.1) | B | V | Eliminates function-call overhead; per-operation LMUL tuning maximises throughput within register budget |
| Four-way outer-loop unroll in `vclmul_leaf` (§5.3.1) | C | Zvbc, V | Amortises $b$-vector load; saturates vector multiply pipeline |
| Threshold sweep — V2-A1 (§3.2.2) | A, B, C | None (parameter tuning) | Confirms optimal recursion depth for the target cost model |
| Compile-time buffer layout (§3.4.1) | A, B, C | None (code generation) | Eliminates per-node pointer arithmetic across the tree |
| LMUL selection strategy (§5.4.1) | B, C | V | Balances register pressure against vector throughput |

### Low Expected Impact

| Optimization | Track(s) | Extensions Used | Rationale |
|---|---|---|---|
| Dead-code ablation (§3.3.4) | A, B | None (source cleanup) | Removes one comparison per leaf call; marginal |
| Four-way unrolled combine loops in generated tree (§3.4.2) | A | None (loop unrolling) | Reduces branch count in linear-cost loops; dominated by leaf |
| Vectorised modular reduction — `reduce_rvv` (§4.2.2) | B | V | Replaces scalar reduction loop with LMUL=m8 vector strip-mining; bounded by single invocation per `vect_mul` call |
| Aligned buffer declarations (§4.2.4) | B | None (alignment attribute) | Ensures naturally-aligned vector accesses; marginal on implementations with efficient unaligned support |
| Upper-half initialisation validation (§5.3.2) | C | V | Confirmed existing pattern is optimal; no change applied |

---

## 7. Discarded Alternatives and Why They Were Archived

### 7.1 Toom-Cook 3-Way over GF(2)[x] — AP-1

**Hypothesis:** TC3 reduces asymptotic complexity from $O(n^{1.585})$ to
$O(n^{1.465})$, projecting fewer leaf calls and lower total schoolbook work.

**Outcome:** Rejected after empirical measurement. The five-point evaluation
and interpolation steps (each requiring multiple XOR passes over size-$\lceil n/3 \rceil$
arrays) introduced sufficient constant-factor overhead to negate the asymptotic
advantage at HQC parameter sizes. The implementation passed all 300 KAT vectors
but was substantially slower than the Karatsuba champion for HQC-128.

**Architectural reason:** At $n=277$, TC3 produces only 2–3 recursion levels
before reaching the leaf threshold, limiting the asymptotic advantage to a
narrow window. The evaluation/interpolation cost per level (~20 array-XOR
passes) exceeds the savings from reduced leaf count.

### 7.2 Pointer-Bump Addressing — V2-A5

**Hypothesis:** Replacing index-based addressing with pointer increments would
eliminate `add` instructions for address computation in the combine loops.

**Outcome:** Rejected. The RISC-V `ld`/`sd` instructions provide free 12-bit
signed immediate offsets. Index-based access (`r_p[0]`, `r_p[1]`, `r_p[2]`,
`r_p[3]`) compiles to loads with constant offsets from a single base register,
requiring no address arithmetic. Pointer bumping forced the compiler to emit
explicit `addi` instructions to maintain the pointer, producing a net increase
in instruction count.

### 7.3 FAFFT Tower-Field with Zvbc

**Hypothesis:** The additive FFT (FAFFT) algorithm over tower-field
$\text{GF}(2^{32})$ could exploit `vclmul.vv` for butterfly operations,
potentially achieving $O(n \log n)$ complexity.

**Outcome:** Deprecated. The tower-field bit-sliced representation is
semantically incompatible with `vclmul.vv`, which operates on polynomial
(not tower-field) elements. The implementation was measured at approximately
9× slower than the Karatsuba Zvbc winner for HQC-128.

### 7.4 Zvbc Schoolbook (Non-Recursive)

**Hypothesis:** A flat $O(n^2)$ schoolbook using `vclmul.vv` without Karatsuba
recursion might be competitive at HQC parameter sizes due to the elimination of
all recursion overhead.

**Outcome:** Deprecated. The quadratic growth dominates at $n=277$; the
non-recursive schoolbook was defeated by the Karatsuba-structured Zvbc
implementation.

### 7.5 Keccak Index-Vector Hoisting — C-4a/b/c

**Hypothesis:** Hoisting four loop-invariant index vectors outside the 24-round
Keccak permutation loop would eliminate 96 redundant `vle64.v` loads per
permutation call.

**Outcome:** Abandoned. At LMUL=8 on VLEN=256, the register file provides
exactly four register groups. The Keccak implementation requires all four groups
for (state, scratch0, scratch1, scratch2). Hoisting any index vector forces a
spill of the state group to memory, costing more than the loads it eliminates.
Even the isolated masked-Iota variant (C-4b) regressed due to partial register
spilling triggered by the mask-setup block.

**Architectural lesson:** At LMUL=8 on VLEN=256, loop-invariant code motion for
vector constants is counterproductive when the register file is fully occupied.
The per-round reload pattern is the register-pressure-optimal implementation.

### 7.6 Per-Element Upper-Half Assignment in `vclmul_leaf`

**Hypothesis:** Eliminating the final upper-half `vzero` pre-store by extracting
each first-touch element via `vslidedown` + `vl=1` assignment would save one
vector instruction per leaf call.

**Outcome:** Rejected. At the actual leaf sizes ($n \leq 9$), the three extra
instructions per outer iteration (`vsetvl`, `vsetvl`, `vslidedown`) exceeded
the cost of the single wide zero-store by approximately 5×. The one-instruction
`vmv.v.x(0)` + `vse64.v` pattern covers all $n$ upper-half positions
simultaneously and remains the optimal approach.

---

## 8. Benchmarking Plan for Next Phase

The following measurements are planned for the dedicated analytic benchmarking
pass. Each optimization will be isolated and measured independently to quantify
its individual contribution, avoiding confounding from stacked effects.

### 8.1 Per-Optimization Isolation Measurements

For each promoted optimization, measure:

- **Isolated `vect_mul` instruction count** at all three security levels
  (HQC-128, HQC-192, HQC-256) using the Spike instruction-accurate simulator.
- **Full KEM operation instruction counts** (keypair, encaps, decaps) to
  capture system-level impact including call-frequency amplification.
- **Instruction-mix histogram** (breakdown by instruction class: memory,
  arithmetic, branch, vector, crypto) to characterise the overhead-to-compute
  ratio before and after each optimization.
- **Function-level hotspot attribution** to verify that the targeted function's
  share decreases as expected and no secondary bottleneck emerges.

### 8.2 Ablation Study Design

- **Cumulative stacking:** Measure the promoted implementation with each
  optimization removed individually (leave-one-out) to assess interaction
  effects and confirm that gains are additive rather than redundant.
- **Threshold sensitivity:** Re-sweep $T \in \{4, 8, 12, 16, 20, 24\}$ on the
  final promoted implementations of all three tracks to verify that $T=16$
  remains optimal after all other optimizations are applied.
- **VLEN scaling:** For Tracks B and C, measure at VLEN $\in \{128, 256, 512, 1024\}$
  to characterise the vector-length sensitivity of each optimization.

### 8.3 Cross-Track Comparison

- **Track A vs. Track B:** Quantify the marginal value of RVV combine
  vectorisation when the scalar leaf is the binding constraint.
- **Track A vs. Track C:** Quantify the full benefit of vector carry-less
  multiply over the scalar-only baseline.
- **Track B vs. Track C:** Isolate the contribution of `vclmul.vv` leaf
  vectorisation from the shared RVV combine infrastructure.

### 8.4 Constant-Time Verification

- Run `ctgrind`-style timing leakage tests on all three track implementations
  to confirm that no secret-dependent branches or memory access patterns were
  introduced by the optimizations.
- Verify that all recursion splits, loop bounds, and buffer offsets depend only
  on public polynomial sizes (compile-time constants).

### 8.5 Scaling Analysis

- Plot `vect_mul` instruction count as a function of $N$ (polynomial word count)
  for each track to empirically verify the expected $O(N^{1.585})$ Karatsuba
  scaling and confirm that no optimization inadvertently worsens asymptotic
  behaviour.
- Compare the empirical scaling exponent against the theoretical prediction
  across the three HQC parameter sizes.
