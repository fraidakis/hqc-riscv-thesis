# HQC-RISCV: Hardware-Accelerated HQC on RISC-V

This repository is the implementation and evaluation framework for the thesis
**"Hardware-Accelerated HQC on RISC-V Vector Extension"**.

It benchmarks the Hamming Quasi-Cyclic (HQC) post-quantum KEM against
three implementation strategies on a RISC-V rv64gcv target:

| Implementation     | ISA extensions used            | Description                          |
|--------------------|-------------------------------|--------------------------------------|
| `baseline`         | rv64gc                        | Portable C reference                 |
| `scalar_zbc`       | rv64gc + Zbc/Zbb              | Carry-less multiply (CLMUL)          |
| `vector_zvbc`      | rv64gcv + Zvbc                | Vectorised CLMUL (vclmul/vclmulh)   |

---

## Source Layout

- Polynomial multiplication: `src/mult/`
	- `karatsuba_zbc/`
	- `karatsuba_rvv/`
	- `karatsuba_zvbc/`
- Decoder: `src/decode/`
	- `reed_muller_rvv.c`
	- `reed_solomon_rvv.c`
	- `reed_solomon_zvbc.c`
- Keccak: `src/keccak/`
- Archived experiments: `experiments/` (catalogued in `experiments/manifest.yaml`)

Repository layout and file-by-file documentation are maintained in:
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

Command workflows, rationale, and roadmap are maintained in:
- [docs/RUNBOOK.md](docs/RUNBOOK.md)
- [docs/METHODOLOGY.md](docs/METHODOLOGY.md)
- [docs/PLAN.md](docs/PLAN.md)

## Quick Start

```bash
# 1. Set up environment
source toolchain/toolchain.env

# 2. Configure QEMU and Spike build trees
cmake -S . -B build/qemu -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-gcc.cmake -DHQC_SIM=qemu
cmake -S . -B build/spike -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-gcc.cmake -DHQC_SIM=spike -DBENCH_N_ITER=1

# 3. Build all targets
cmake --build build/qemu -j$(nproc)

# 4. Run KAT under QEMU
./scripts/test/run_kat.sh --variant scalar_ref --level 128 --qemu
```

## Simulation Targets

- **Spike** — functional ISA simulation with `sim/spike/run.sh` (`--clean` for timing, `--profile` for traces)
- **QEMU** — system-mode emulation via `sim/qemu/run.sh`
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
| `karatsuba_zbc` | `rv64gc + zbc + zbb` | implemented |
| `fafft_zbc` | `rv64gcv + zbc + zbb` | implemented |
| `hybrid` | `rv64gcv + zbc + zbb` | implemented |
| `fafft_zvbc` | `rv64gcv + zbc + zbb + zvbc` | implemented |
| `karatsuba_zvbc` | `rv64gcv + zbc + zbb + zvbc` | implemented |

## Requirements

- CMake ≥ 3.21
- Python ≥ 3.10 (for analysis scripts)
- `device-tree-compiler` (dtc)
- Boost (for Spike)

## License

Academic thesis project. See individual source files for upstream licenses
(HQC reference: CC0 / public domain).
