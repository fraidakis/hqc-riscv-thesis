# Constant-Time Static Verification (Valgrind/ctgrind)

## Status: NOT APPLICABLE to optimized variants

The `ct_harness.c` and `scripts/ci/check_ct.sh` implement a Valgrind Memcheck-based
CT verification approach (often called "ctgrind-style"): secret data is marked as
`VALGRIND_MAKE_MEM_UNDEFINED` and Memcheck reports any conditional branch or
indexed memory access that depends on secret bytes.

### Why This Does Not Work for Our Variants

1. **Cross-compilation barrier**: All optimized RISC-V variants (`scalar_zb`,
   `vector_rvv`, `vector_zvk`, `fafft_zbc`, etc.) use RISC-V intrinsics
   (`__riscv_clmul_64`, `<riscv_vector.h>`) that cannot compile on x86-64.
   The CI script (`check_ct.sh`) skips all RISC-V variants with `exit 0`.

2. **Only vendor reference runs**: The only variant that compiles natively is
   `scalar_ref` — the upstream HQC reference code. Verifying it provides no new
   information (its CT correctness is already established upstream).

3. **Valgrind RISC-V limitations**: Valgrind v3.25.0+ added RISC-V 64-bit support,
   but the instruction interpreter does not yet handle Zbc (`clmul`/`clmulh`) or
   RVV vector instructions. Binaries using these extensions crash with
   "unhandled instruction bytes."

### CT Verification Strategy (Actual)

The constant-time property of our implementations is documented through:

- **Architectural Zkt analysis**: All executed instructions belong to the
  data-independent timing subset (§5.4.2 of the thesis, `ssec:ct-design-patterns`).
- **Empirical dudect measurement**: Statistical timing verification on BPI-F3
  hardware for Tier 1/2 variants (see `tests/ct_dudect/`).

### Future: Making Valgrind Work for RISC-V Variants

A cross-compiled QEMU user-mode approach could enable Valgrind-based CT checks:

```bash
# Requires riscv64-linux-gnu toolchain (NOT bare-metal elf)
riscv64-linux-gnu-gcc -march=rv64gc_zbc -O2 \
    -I... tests/ct/ct_harness.c src/mult/karatsuba_zbc/gf2x_clmul.c ... \
    -o build/ct/ct_hqc128_scalar_zb_riscv

# Run via QEMU user-mode + Valgrind
qemu-riscv64 valgrind --tool=memcheck --error-exitcode=1 \
    ./build/ct/ct_hqc128_scalar_zb_riscv
```

Requirements:
- `gcc-riscv64-linux-gnu` or `clang --target=riscv64-linux-gnu` with sysroot
- QEMU user-mode (`qemu-riscv64`) with transparent binfmt registration
- Valgrind compiled for RISC-V (or running under QEMU's Valgrind passthrough)

This remains future work — the thesis CT claims rely on Zkt + dudect.
