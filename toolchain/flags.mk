# toolchain/flags.mk
# Common compiler/linker flags for GNU Make-based builds.
# Include this from sub-Makefiles: include $(ROOT)/toolchain/flags.mk

RISCV     ?= $(HOME)/Thesis/installed-tools
CROSS     := $(RISCV)/bin/riscv64-unknown-elf-
CC        := $(CROSS)gcc
CXX       := $(CROSS)g++
CLANG     := $(RISCV)/bin/clang
OBJDUMP   := $(CROSS)objdump
OBJCOPY   := $(CROSS)objcopy
SIZE      := $(CROSS)size

MARCH     := rv64gcv_zbc_zbb_zbkb_zvbc
MABI      := lp64d
TARGET    := riscv64-unknown-elf

CFLAGS    := -march=$(MARCH) -mabi=$(MABI) -O3 -flto \
             -ffreestanding -fno-builtin -fno-tree-vectorize \
             -ffunction-sections -fdata-sections \
             -mno-relax -Wall -Wextra

CLANGFLAGS := --target=$(TARGET) $(CFLAGS) \
              -menable-experimental-extensions

LDFLAGS   := -T$(ROOT)/sim/common/link.ld \
             -fuse-ld=lld \
             -Wl,--gc-sections \
             -nostartfiles -nostdlib

ASFLAGS   := -march=$(MARCH) -mabi=$(MABI)
