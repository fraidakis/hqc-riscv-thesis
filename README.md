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

## Polynomial Source Layout

- Scalar variants: `src/poly_mul/scalar/`
	- `karatsuba_zbc/`
	- `fafft_zbc/`
	- `hybrid_zbc/`
- Vector variants: `src/poly_mul/vector/`
	- `karatsuba_zvbc/`
	- `fafft_zvbc/`
- Archived experiments: `experiments/` (catalogued in `experiments/manifest.yaml`)

## Repository Layout

```
hqc-riscv/
├── toolchain/          CMake toolchain file + build scripts
├── include/            Public API and algorithm headers
├── src/                C/asm sources (scalar + vector hierarchy)
├── experiments/        Historical/deprecated variant branches
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

# 2. Configure QEMU and Spike build trees
cmake -S . -B build/qemu -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=qemu
cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake -DHQC_SIM=spike -DBENCH_N_ITER=1

# 3. Build all targets
cmake --build build/qemu -j$(nproc)

# 4. Run KAT under QEMU
./sim/qemu/run_kat.sh build/qemu/hqc128_karatsuba_ref.elf
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

## Variant Index

The table below mirrors `scripts/implemented_variants.txt`.

| Variant | Required ISA Extensions | Status |
|---------|-------------------------|--------|
| `karatsuba_ref` | `rv64gc` | implemented |
| `karatsuba_zbc` | `rv64gc + zbc + zbkb` | implemented |
| `fafft_zbc` | `rv64gcv + zbc + zbkb` | implemented |
| `hybrid` | `rv64gcv + zbc + zbkb` | implemented |
| `fafft_zvbc` | `rv64gcv + zbc + zbkb + zvbc` | implemented |
| `karatsuba_zvbc` | `rv64gcv + zbc + zbkb + zvbc` | implemented |

## Requirements

- CMake ≥ 3.21
- Python ≥ 3.10 (for analysis scripts)
- `device-tree-compiler` (dtc)
- Boost (for Spike)

## License

Academic thesis project. See individual source files for upstream licenses
(HQC reference: CC0 / public domain).
