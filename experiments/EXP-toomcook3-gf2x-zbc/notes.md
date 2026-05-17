# EXP-toomcook3-gf2x-zbc

## Goal

Implement Toom-Cook 3-way multiplication over GF(2)[x] with Zbc scalar CLMUL leaves, using GF(4) evaluation points:

- 0
- 1
- omega
- omega^2
- infinity

with GF(4) defined as GF(2)[omega]/(omega^2 + omega + 1).

## Split

For n words:

- k = ceil(n / 3)
- A(x) = A0 + x^k A1 + x^(2k) A2
- B(x) = B0 + x^k B1 + x^(2k) B2
- len(A0)=k, len(A1)=k, len(A2)=n-2k (zero padded to k)

## Evaluation

For each operand:

- E0 = A0
- E1 = A0 ^ A1 ^ A2
- E2 = A0 ^ omega*A1 ^ omega^2*A2
- E3 = A0 ^ omega^2*A1 ^ omega*A2
- E4 = A2

where omega^2*a = omega*a ^ a.

Packed GF(4) scaling by omega per 64-bit word:

- odd  = v & 0xAAAAAAAAAAAAAAAA
- even = v & 0x5555555555555555
- omega*v = (even << 1) ^ (odd >> 1) ^ odd

## Pointwise products

Compute recursively:

- p0 = E0(A) * E0(B)
- p1 = E1(A) * E1(B)
- p2 = E2(A) * E2(B)
- p3 = E3(A) * E3(B)
- p4 = E4(A) * E4(B)

Each p_i is handled as 2k words, with zero padding for p4 when A2/B2 are shorter than k.

## Interpolation

Given p0..p4:

1. t = p2 ^ p3
2. wt = omega*t
3. C3 = p0 ^ p1 ^ p2 ^ p3
4. C1 = p1 ^ p2 ^ p4 ^ wt
5. C2 = p1 ^ p3 ^ wt

and borders:

- C0 = p0
- C4 = p4

## Recomposition

C(x) = C0 + x^k*C1 + x^(2k)*C2 + x^(3k)*C3 + x^(4k)*C4

Assemble into output r[0..2n-1] with XOR at offsets:

- C0 -> r + 0*k
- C1 -> r + 1*k
- C2 -> r + 2*k
- C3 -> r + 3*k
- C4 -> r + 4*k

Only the valid prefix of C4 (2*(n-2k) words) is non-zero before padding.

## Base case and security constraints

- Base case uses the existing CLMUL leaves from stable scalar champion:
  - n==8 -> clmul_leaf_n8
  - n<8  -> clmul_leaf
- No vector instructions in this experiment path.
- Constant-time control flow: recursion shape depends only on n.

## Conclusion & Results

Status: rejected after full validation.

- KAT correctness: PASS 300/300 vectors across HQC-128, HQC-192, HQC-256.
- Isolated polymul benchmark (HQC-128, Spike): vect_mul instret = 474,326.
- Scalar champion reference (`karatsuba_zbc`): 274,131 instret.
- Delta: +200,195 instret (+73.03%), so AP-1 is empirically slower.

Root cause:

- The 6-way split follows O(n^log_3(6)) ≈ O(n^1.63), which is asymptotically
  worse than 2-way Karatsuba O(n^log_2(3)) ≈ O(n^1.58).
- The method also adds heavier O(k) evaluation/recomposition overhead at each
  internal node.
- At HQC-128 size n=277, these effects dominate and erase any benefit from a
  shallower recursion tree.
