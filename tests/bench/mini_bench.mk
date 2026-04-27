# tests/bench/mini_bench.mk
#
# Standalone Makefile for the mini_bench.c simulator comparison benchmark.
# Deliberately avoids CMake so it can be run without reconfiguring the whole
# project.  All flags are copied from toolchain/flags.mk and riscv-clang.cmake.
#
# Usage:
#   make -f tests/bench/mini_bench.mk SIM=spike   # → build-mini/mini_bench_spike.elf
#   make -f tests/bench/mini_bench.mk SIM=qemu    # → build-mini/mini_bench_qemu.elf
#   make -f tests/bench/mini_bench.mk             # builds both

# ── Locate project root (the directory containing this Makefile) ────────────
ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../..

# ── Toolchain ────────────────────────────────────────────────────────────────
RISCV   ?= $(HOME)/Thesis/installed-tools
CLANG   := $(RISCV)/bin/clang
OBJDUMP := $(RISCV)/bin/llvm-objdump

# ── Architecture & ABI ───────────────────────────────────────────────────────
MARCH   := rv64gcv_zbc_zbb_zbkb_zvbc
MABI    := lp64d
TRIPLE  := riscv64-unknown-elf

# ── Compile flags (mirrors riscv-clang.cmake) ────────────────────────────────
CFLAGS  := \
    --target=$(TRIPLE)            \
    -march=$(MARCH)               \
    -mabi=$(MABI)                 \
    -menable-experimental-extensions \
    -mcmodel=medany               \
    -O3                           \
    -flto                         \
    -mno-relax                    \
    -ffreestanding                \
    -fno-builtin                  \
    -fno-tree-vectorize           \
    -ffunction-sections           \
    -fdata-sections               \
    -Wall -Wextra                 \
    -I$(ROOT)/src/common

# ── Linker flags ─────────────────────────────────────────────────────────────
LDFLAGS := \
    -T$(ROOT)/sim/common/link.ld  \
    -fuse-ld=lld                  \
    -Wl,--gc-sections             \
    -nostartfiles                 \
    -nostdlib

# ── Sources shared by both backends ──────────────────────────────────────────
COMMON_SRCS := \
    $(ROOT)/sim/common/startup.S  \
    $(ROOT)/sim/common/mem.c      \
    $(ROOT)/tests/bench/mini_bench.c

# ── Output directory ─────────────────────────────────────────────────────────
OUTDIR := $(ROOT)/build-mini
$(shell mkdir -p $(OUTDIR))

# ── Default: build both ───────────────────────────────────────────────────────
.PHONY: all spike qemu clean
all: spike qemu

# ── Spike target (HTIF backend, no -DHQC_SIM_QEMU) ───────────────────────────
SPIKE_ELF := $(OUTDIR)/mini_bench_spike.elf
SPIKE_SRCS := $(COMMON_SRCS) $(ROOT)/src/common/bench.c

spike: $(SPIKE_ELF)

$(SPIKE_ELF): $(SPIKE_SRCS)
	$(CLANG) $(CFLAGS) $(LDFLAGS) $^ -o $@
	@echo "[mini_bench] Built Spike ELF: $@"
	@$(RISCV)/bin/llvm-size $@ 2>/dev/null || true

# ── QEMU target (UART + test-finisher backend) ────────────────────────────────
QEMU_ELF  := $(OUTDIR)/mini_bench_qemu.elf
QEMU_SRCS := $(COMMON_SRCS) $(ROOT)/src/common/bench.c

qemu: $(QEMU_ELF)

$(QEMU_ELF): $(QEMU_SRCS)
	$(CLANG) $(CFLAGS) -DHQC_SIM_QEMU=1 $(LDFLAGS) $^ -o $@
	@echo "[mini_bench] Built QEMU ELF: $@"
	@$(RISCV)/bin/llvm-size $@ 2>/dev/null || true

# ── Disassembly helper ────────────────────────────────────────────────────────
.PHONY: dis-spike dis-qemu
dis-spike: $(SPIKE_ELF)
	$(OBJDUMP) -d $< | head -120

dis-qemu: $(QEMU_ELF)
	$(OBJDUMP) -d $< | head -120

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -f $(SPIKE_ELF) $(QEMU_ELF)
