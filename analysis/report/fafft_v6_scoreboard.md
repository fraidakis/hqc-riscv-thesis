# v6 Scalar CRT-Karatsuba Hybrid — Scoreboard

## Implementation

**v6** (`src/poly_mul/fafft_zbc/v6/fafft_v6.c`)

### Algorithm: direct polynomial CRT split, all-scalar Zbc

Split at x^{16384} (256 uint64 = 512 uint32 words):

    A = A_L + A_H·x^16384,  A_L: 256 uint64,  A_H: 21 uint64
    B = B_L + B_H·x^16384

    A·B = C0 + cross·x^16384 + C2·x^32768

| Term  | Method                        | Size       |
|-------|-------------------------------|------------|
| C0    | Karatsuba(A_L(256), B_L(256)) | 511 words  |
| cross | schoolbook(A_L×B_H + A_H×B_L)| 277 words  |
| C2    | Karatsuba(A_H(21), B_H(21))   |  41 words  |

XOR into r64[0..552], then `reduce()`.
KAT: **PASS 100/100** (QEMU)

---

### Why bc_1_16384 FAFFT cannot be used for the "512-word main product"

The original request specified `bc_1_16384 → encode → btfy_32768 → scalar gf232v_mul
→ ibtfy → decode → ibc_1_16384` for the main product.  This is **mathematically
impossible** for HQC-128:

- A_L has degree ≤ 16383 bits; B_L similarly
- A_L × B_L has degree ≤ 32766 bits, which **overflows** the 16384-bit bc_1_16384 ring
- bc_1_16384 returns a ring-folded result, NOT the exact polynomial product
- The ref code documents this in a comment: *"bc_1_16384 accepts at most 512-word inputs,
  and HQC-128 polynomials have 553 words.  Without a full-input bc_1_16384 the CRT
  correction cannot recover A*B; therefore CRT_fft_HQC1 is not active."*

Also tested: calling `CRT_fft_HQC1` directly → **FAIL 100/100 KAT**.
The ring-based CRT (`radix16_81` + `bit_shift_xor_81_asm`) is also broken for HQC-128:
both paths operate on the same `r32[0..80]` input (from GF(2)[x] and the GF(2^4) ring),
making the correction term effectively zero and the output wrong.

For a bc_1_16384-based split to give an exact product, both inputs would need degree
< 8192 bits (≤ 8 uint32 = 4 uint64 words) — a useless split for 277-word polynomials.

---

## Benchmark Results (Spike, N=1)

| HQC Level | instret    |
|-----------|------------|
| HQC-128   | **490,423** |
| HQC-192   | 1,273,317  |
| HQC-256   | 2,559,357  |

---

## HQC-128 Comparison

| Variant                          | instret     | vs v6 CRT-Kara |
|---------------------------------|-------------|----------------|
| v4 RVV schoolbook (zvbc)         | 178,048     | −63.7%         |
| Phase 1a karatsuba_zbc           | 354,187     | −27.8%         |
| v3 FAFFT baseline (vect_mul_v3)  | 364,872     | −25.6%         |
| v6 pure Karatsuba+reduce (prev)  | 417,438     | −14.9%         |
| **v6 CRT-Karatsuba split (this)**| **490,423** | —              |

---

## Analysis

The CRT-Karatsuba split is **17.5% slower** than plain full-Karatsuba+reduce,
and **34.4% slower** than the v3 FAFFT baseline.

**Root cause**: The split at 256 uint64 words distributes the 277×277 schoolbook
into three independent products whose total complexity is identical:

    256² + 2·256·21 + 21² = 65536 + 10752 + 441 = 76729 = 277²

There are no savings.  The overhead from three separate loops, scratch buffers,
and XOR accumulation makes it **worse** than a unified Karatsuba on 277 words.

**Why v3 FAFFT is faster at scalar level**: The FAFFT uses the GF(2^32) tower-field
embedding to compute the convolution with O(n log n) complexity in that algebra
(via btfy_32768), whereas binary Karatsuba is O(n^{log_2 3}) ≈ O(n^{1.585}).
For HQC-128 (n=277), the FAFFT achieves ~35K fewer instructions.

**Path to beat v3**: A scalar implementation that replaces the GF(2^32)
pointwise multiplier (`gf232v_mul`) with Zbc clmul-based operations (rather than
integer tower-field arithmetic) would produce the "FAFFT-Zbc" hybrid targeted
by this research.  That optimization belongs in v7+.
