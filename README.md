# HQC-RISCV: Hardware-Accelerated HQC on RISC-V

This repository is the implementation and evaluation framework for the thesis
**"Hardware-Accelerated HQC on RISC-V Vector Extension"**.

It benchmarks the Hamming Quasi-Cyclic (HQC) post-quantum KEM against
three implementation strategies on a RISC-V rv64gcv target:

| Implementation     | ISA extensions used            | Description                          |
|--------------------|-------------------------------|--------------------------------------|
| `baseline`         | rv64gc                        | Portable C reference                 |
| `scalar_zbc`       | rv64gc + Zbc/Zbkb             | Carry-less multiply (CLMUL)          |
| `vector_zvbc`      | rv64gcv + Zvbc                | Vectorised CLMUL (vclmul/vclmulh)   |

---

## Repository Layout

```
hqc-riscv/
├── toolchain/          CMake toolchain file + build scripts
├── include/            Public API and algorithm headers
├── src/                C/asm sources per implementation
├── sim/                Simulator runners (Spike, QEMU, Marian/Verilator)
├── tests/              KAT vectors, unit tests, benchmarks
├── analysis/           LLVM-MCA reports, cycle counts, CT verification
├── scripts/            GDB helpers, CI checks
└── docs/               Literature, figures, LaTeX thesis chapters
```

## Quick Start

```bash
# 1. Set up environment
source toolchain/toolchain.env

# 2. Configure (bare-metal Clang)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake

# 3. Build all targets
cmake --build build -j$(nproc)

# 4. Run KAT under QEMU
./sim/qemu/run_kat.sh build/hqc128_vector_zvbc.elf
```

## Simulation Targets

- **Spike** — functional ISA simulation with commit log (`--log-commits`)
- **QEMU** — system-mode emulation (`qemu-system-riscv64 -machine virt`)
- **Marian/Ara** — RTL co-simulation (see [sim/rtl-marian/README.md](sim/rtl-marian/README.md))

## Toolchain

| Tool        | Version   | Path                                      |
|-------------|-----------|-------------------------------------------|
| GCC         | 15.2.0    | `~/Thesis/installed-tools/bin/riscv64-unknown-elf-gcc` |
| Clang/LLVM  | 21.1.1    | `~/Thesis/installed-tools/bin/clang`      |
| Spike       | latest    | `~/Thesis/installed-tools/bin/spike`      |
| QEMU        | 10.2.2    | `~/Thesis/installed-tools/bin/qemu-system-riscv64` |

## Requirements

- CMake ≥ 3.21
- Python ≥ 3.10 (for analysis scripts)
- `device-tree-compiler` (dtc)
- Boost (for Spike)

## License

Academic thesis project. See individual source files for upstream licenses
(HQC reference: CC0 / public domain).
