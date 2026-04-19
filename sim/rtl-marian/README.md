# Marian / Ara RTL Co-simulation Setup

This directory contains helpers for running HQC binaries on the
[PULP Ara](https://github.com/pulp-platform/ara) / [Marian](https://github.com/soc-hub-fi/Marian)
open-source RISC-V vector processor RTL.

## Prerequisites

- **QuestaSim ≥ 2022.3** or **Verilator ≥ 5.008** (open-source)
- PULP Platform `bender` tool for IP dependency management
- Python ≥ 3.10

## Cloning the RTL

```bash
# Ara (PULP, MIT licence)
git clone https://github.com/pulp-platform/ara.git ~/Thesis/src/ara
cd ~/Thesis/src/ara
git submodule update --init --recursive

# Marian (SoC-Hub, Apache-2.0)
git clone https://github.com/soc-hub-fi/Marian.git ~/Thesis/src/Marian
cd ~/Thesis/src/Marian
git submodule update --init --recursive
```

## Building with Verilator

```bash
cd ~/Thesis/src/ara
# Follow ara/hardware/README.md for bender-based IP resolution
make -C hardware verilate
```

## Loading an HQC Binary

```bash
# Build the target ELF first
cmake --build ~/Thesis/hqc-riscv/build --target hqc128_vector_zvbc

# Run via the helper script
python3 sim/rtl-marian/load_binary.py \
    --elf build/hqc128_vector_zvbc.elf \
    --sim verilator \
    --ara-dir ~/Thesis/src/ara
```

## Notes

- Ara uses VLEN=512 by default; the `custom_rvv.xml` GDB description matches this.
- For QuestaSim, set `SIM=questasim` in `sim/rtl-marian/run_verilator.sh`.
- Waveforms are written to `*.fst` (Verilator) or `*.vcd` (QuestaSim); both are
  gitignored.
- The `cpu_vlen_patch.diff` in `sim/qemu/rave/` patches QEMU's VLEN to 512 to
  match Ara for RAVE-based evaluation.
