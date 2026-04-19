# Architecture Decision Records

This file records key design choices made during the thesis implementation.

---

## ADR-001: Bare-metal vs. Linux user-space

**Decision:** Target bare-metal newlib (riscv64-unknown-elf) for all simulation.

**Rationale:** Eliminates OS noise from cycle counts; Spike/QEMU virt machine
is simpler to configure for bare-metal; matches RTL co-simulation with Marian.

**Consequences:** Must provide startup.S, link.ld, and HTIF exit mechanism.
Cannot use standard `malloc`/`printf` without newlib stubs.

---

## ADR-002: Clang vs. GCC for vectorised code

**Decision:** Use Clang 21.1.1 as the primary compiler for all targets.

**Rationale:** Better Zvbc intrinsic support; `-menable-experimental-extensions`
flag; LLVM-MCA integration for throughput analysis.

**Consequences:** CMakeLists.txt uses `toolchain/riscv-clang.cmake`.
GCC sysroot (newlib) is still used as the C library/sysroot.

---

## ADR-003: Vector register width

**Decision:** Target VLEN=256 for QEMU simulation, VLEN=512 for Marian RTL.

**Rationale:** QEMU virt machine `-cpu rv64,vlen=256` is widely tested.
Ara/Marian hardware uses VLEN=512 (NrLanes=4).

**Consequences:** `custom_rvv.xml` uses bitsize=512 (Marian default).
QEMU run scripts set `vlen=256`.

---

## ADR-004: poly_mul implementation strategy

**Decision:** Use `vclmul.vv` / `vclmulh.vv` (Zvbc) for vectorised poly_mul.

**Rationale:** Direct hardware carry-less multiply over GF(2)[x] without
any algorithmic restructuring; maps cleanly to the binary polynomial ring
used in HQC.

**Consequences:** Requires `rv64gcv_zvbc` ISA string; not available on
all RISC-V cores (only Zvbc-capable cores like Ara/Marian custom).
