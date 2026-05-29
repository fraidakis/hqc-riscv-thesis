# Marian Deployment Guide — HQC on Zvk

> **Target:** Run HQC bare-metal benchmarks on Marian (Ara+Zvk) for cycle-accurate profiling
>
> **Available Hardware:** Server with Linux, Xilinx Alveo U200 (UltraScale+ xcu200), Vivado

---

## Overview of Deployment Paths

| Path | Tool | Speed | Use Case |
|------|------|-------|----------|
| **A. Verilator** | Free, open-source | ~1-10 KHz | Kernel-level cycle profiling |
| **B. FPGA (Alveo U200)** | Vivado (have it) | ~50-100 MHz | Full HQC end-to-end benchmarks |

**Recommended approach:** Start with Verilator for quick validation, then move to FPGA
for full benchmarks.

---

## Phase 0: Prerequisites (One-Time Setup)

### 0.1 Check Server Environment

```bash
# Check OS
cat /etc/os-release

# Check available RAM (need ≥16 GB for Verilator build)
free -h

# Check disk space (need ≥50 GB free)
df -h .

# Check Vivado
which vivado
vivado -version

# Check if Verilator is installed
which verilator
verilator --version
```

### 0.2 Install Verilator v5.008+

If not installed or version is too old:

```bash
# Dependencies (Ubuntu/Debian)
sudo apt-get install git perl python3 make autoconf g++ flex bison ccache
sudo apt-get install libgoogle-perftools-dev numactl perl-doc
sudo apt-get install libfl2 libfl-dev zlib1g zlib1g-dev

# Dependencies (CentOS/RHEL)
sudo yum install git perl python3 make autoconf gcc-c++ flex bison ccache
sudo yum install gperftools-libs numactl perl-doc zlib-devel

# Build Verilator from source (pinned version)
cd /opt  # or wherever you want
git clone https://github.com/verilator/verilator.git
cd verilator
git checkout v5.008
autoconf
./configure --prefix=/opt/verilator-5.008
make -j$(nproc)
make install

# Add to PATH
export PATH=/opt/verilator-5.008/bin:$PATH
echo 'export PATH=/opt/verilator-5.008/bin:$PATH' >> ~/.bashrc
```

### 0.3 Install RISC-V GCC Cross-Compiler (≥ 13.2 with Zvk support)

```bash
# Option A: Build from source (recommended for Zvk support)
cd /opt
git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain

# Configure for bare-metal (newlib) with vector crypto extensions
./configure --prefix=/opt/riscv --with-arch=rv64gcv_zvk --with-abi=lp64d
make -j$(nproc)

# Add to PATH
export PATH=/opt/riscv/bin:$PATH
export RISCV=/opt/riscv
echo 'export PATH=/opt/riscv/bin:$PATH' >> ~/.bashrc
echo 'export RISCV=/opt/riscv' >> ~/.bashrc

# Verify
riscv64-unknown-elf-gcc --version
# Should show GCC 13.x or 14.x
```

**Note:** Building the toolchain takes 30-60 minutes and ~10 GB disk space.

### 0.4 Other Dependencies

```bash
# Python 3.10+
python3 --version

# GNU Make
make --version

# Device tree compiler (for FPGA flow)
sudo apt-get install device-tree-compiler  # Ubuntu
sudo yum install dtc                        # CentOS
```

---

## Phase 1: Verilator Simulation

### 1.1 Clone Marian

```bash
cd ~/Thesis  # or your preferred location
git clone --recursive https://github.com/soc-hub-fi/Marian.git
cd Marian
make repository_init
```

### 1.2 Verify Build Environment

```bash
# Check that submodules are populated
ls ips/
# Should see: axi_mem_if and possibly others

# Check RTL sources
ls src/
# Should see: rtl/, tb/, etc.
```

### 1.3 Build Verilated Model

```bash
# Set VERILATOR_ROOT if needed
export VERILATOR_ROOT=/opt/verilator-5.008

# Build the verilated model
make verilate
```

**Expected output:** A `build/` directory with the compiled C++ model.
**Expected time:** 15-30 minutes on a modern server.

### 1.4 Run Hello World Test

```bash
make simv TEST=hello_world
```

If this prints success and exits cleanly, the simulation infrastructure works.

### 1.5 Compile Your HQC Kernel

To run your own code, you need to:

1. Write bare-metal C code (no OS, no stdlib beyond what Marian provides)
2. Compile to ELF with the RISC-V cross-compiler
3. Convert ELF to `.hex` format
4. Place in `sw/hex/` directory

```bash
# Example compilation for a test kernel
riscv64-unknown-elf-gcc \
  -march=rv64gcv_zvk \
  -mabi=lp64d \
  -O2 \
  -nostdlib \
  -T sw/link.ld \
  -o build/hqc_test.elf \
  your_hqc_kernel.c sw/crt0.S

# Convert to hex (Marian uses $readmemh format)
riscv64-unknown-elf-objcopy -O verilog build/hqc_test.elf sw/hex/hqc_test.hex

# Run simulation
make simv TEST=hqc_test
```

**Note:** You'll need to adapt the linker script and startup code to Marian's memory
map (see README: BootRAM at 0x8000_0000, 1 MiB).

### 1.6 Extract Cycle Counts

Options for measuring cycles:

**Option A: CSR-based (if mcycle is accessible)**
```c
// In your bare-metal code
unsigned long start, end;
asm volatile("csrr %0, mcycle" : "=r"(start));
// ... your kernel ...
asm volatile("csrr %0, mcycle" : "=r"(end));
unsigned long cycles = end - start;
```

**Option B: Verilator simulation time**
```bash
# Verilator prints simulation time at end
# Or use --trace flag to generate VCD and count clock edges
make verilate VERILATOR_FLAGS="--trace"
make simv TEST=hqc_test
# Analyze resulting .vcd file
```

**Option C: Count from waveform**
```bash
# Generate FST trace (smaller than VCD)
# Then use GTKWave or script to count cycles between markers
```

---

## Phase 2: FPGA Emulation (Alveo U200)

### 2.1 Alveo U200 Compatibility Notes

The Marian FPGA flow was tested on VCU118 (xcvu9p). The Alveo U200 uses **xcu200**
(same UltraScale+ family, slightly different part). You will need to:

1. Change the target part in the Vivado project
2. Adapt pin constraints (Alveo has different I/O compared to VCU118)
3. Possibly adjust clock constraints

The Alveo U200 has:
- Xilinx UltraScale+ xcu200
- 64 GB DDR4 (4 channels)
- PCIe Gen3 x16 host interface
- No direct UART/JTAG — communication via PCIe or custom AXI-over-PCIe bridge

**Key difference from VCU118:** The Alveo is a PCIe accelerator card, not a
standalone dev board. You'll need a PCIe-based communication mechanism instead
of UART/JTAG.

### 2.2 FPGA Build Flow

```bash
cd fpga/

# Read the FPGA README first
cat README.md

# The flow likely uses Vivado TCL scripts
# You'll need to modify for xcu200 target
vivado -mode batch -source build.tcl
```

### 2.3 Adaptation Required for Alveo U200

Since Marian was designed for VCU118, you'll need to create an adapter:

```
VCU118 (original):          Alveo U200 (your board):
├── UART for console        ├── PCIe for host communication
├── JTAG for debug          ├── JTAG via PCIe (XSDB)
├── DDR4 (1 channel)        ├── DDR4 (4 channels)
└── GPIO/LEDs               └── No GPIO (PCIe card)
```

**Simplest approach:** Use Vivado's built-in JTAG-over-PCIe (XSDB) for loading
programs and use AXI BRAM for program memory instead of external DDR.

### 2.4 Alternative: Self-Contained FPGA Test

For benchmarking, you don't necessarily need host communication. You can:

1. Embed the test binary in Block RAM (BRAM) at synthesis time
2. Run the test autonomously on FPGA
3. Store results in a known memory location
4. Read back via JTAG/XSDB after completion

```tcl
# In Vivado, after synthesis:
# Use XSDB to read memory location where cycle count was stored
connect
targets
# Read result from BRAM address
mrd 0x80000100 4
```

---

## Phase 3: Integrating Your HQC Code

### 3.1 Bare-Metal HQC Port

Your HQC code needs to be adapted for bare-metal execution:

```c
// hqc_bench.c - Minimal bare-metal HQC benchmark
#include <stdint.h>

// Marian memory-mapped UART (for printf-like output)
#define UART_BASE 0xC0000000
#define UART_TX   (*(volatile uint32_t*)(UART_BASE + 0x00))

void uart_putchar(char c) {
    UART_TX = c;
}

// Cycle counter
static inline uint64_t rdcycle(void) {
    uint64_t val;
    asm volatile("csrr %0, mcycle" : "=r"(val));
    return val;
}

// Your HQC functions (compiled with -march=rv64gcv_zvk)
extern void hqc_keygen(...);
extern void hqc_encaps(...);
extern void hqc_decaps(...);

int main(void) {
    uint64_t start, end;

    // Benchmark keygen
    start = rdcycle();
    hqc_keygen(...);
    end = rdcycle();
    // Store result somewhere readable
    volatile uint64_t *result = (volatile uint64_t*)0x80080000;
    result[0] = end - start;

    // Benchmark encaps
    start = rdcycle();
    hqc_encaps(...);
    end = rdcycle();
    result[1] = end - start;

    // Signal completion
    return 0;  // Triggers simulation end in Marian TB
}
```

### 3.2 Using Zvbc Instructions in Your Code

```c
#include <riscv_vector.h>

// Vector carry-less multiplication for GF(2^m)
// This uses the Zvbc vclmul instruction
vuint64m1_t gf2m_multiply(vuint64m1_t a, vuint64m1_t b, size_t vl) {
    return __riscv_vclmul_vv_u64m1(a, b, vl);
}

vuint64m1_t gf2m_multiply_high(vuint64m1_t a, vuint64m1_t b, size_t vl) {
    return __riscv_vclmulh_vv_u64m1(a, b, vl);
}
```

---

## Quick Reference: Key Commands

```bash
# === VERILATOR ===
make repository_init          # First time only
make verilate                 # Build verilated model (~20 min)
make simv TEST=hello_world    # Run test
make simv TEST=hqc_test       # Run your test

# === QUESTA (if available) ===
make compile elaborate        # Compile RTL
make simc TEST=hello_world    # Run in CLI
make sim TEST=hello_world     # Run in GUI

# === FPGA ===
cd fpga/
vivado -mode batch -source build.tcl  # Synthesize (~2-6 hours)

# === CROSS-COMPILER ===
riscv64-unknown-elf-gcc -march=rv64gcv_zvk -mabi=lp64d -O2 ...
riscv64-unknown-elf-objcopy -O verilog input.elf output.hex
```

---

## Estimated Timeline

| Task | Duration | Blocking? |
|------|----------|-----------|
| Install Verilator + RISC-V toolchain | 2-3 hours | Yes |
| Clone & build Marian (Verilator) | 30-60 min | Yes |
| Run hello_world validation | 5 min | Yes |
| Port HQC kernel to bare-metal | 1-2 days | Yes |
| Verilator cycle profiling of kernels | 1-2 days | No |
| Adapt FPGA flow for Alveo U200 | 2-3 days | Parallel |
| FPGA synthesis | 2-6 hours | No (batch) |
| Full HQC benchmarks on FPGA | 1 day | No |

**Total estimated time to first cycle count: ~1 week**

---

## Potential Issues & Solutions

| Issue | Solution |
|-------|----------|
| Verilator version mismatch | Pin to v5.008 or try latest (usually backward compatible) |
| GCC doesn't recognize `_zvk` | Need GCC ≥ 13.2; try `_zvbb_zvbc` individually |
| Marian submodules fail to clone | Check SSH keys; try HTTPS URLs |
| Alveo U200 pin constraints | Start with BRAM-only design (no external DDR) |
| Simulation too slow for full HQC | Profile only critical kernels; use FPGA for full runs |
| No UART on Alveo | Use ILA (Integrated Logic Analyzer) or XSDB memory reads |

---

## Next Steps

1. **Immediate:** Check server OS and install prerequisites
2. **Day 1:** Clone Marian, build Verilator model, run hello_world
3. **Day 2-3:** Port your existing HQC vclmul kernel to bare-metal
4. **Day 3-5:** Run cycle-accurate profiling on critical sections
5. **Parallel:** Start FPGA adaptation for Alveo U200
