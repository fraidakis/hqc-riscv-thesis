# Banana Pi BPI-F3 — Hardware Setup & HQC-RISC-V Run Guide

The BPI-F3 is based on the **SpacemiT K1** SoC: 8 × RV64GCV cores, dual-issue
in-order pipeline, VLEN=256 RVV, with hardware **Zbc** (scalar carry-less
multiply). It does **not** support the vector-crypto extensions Zvbc or Zvbb.
The supported ISA is `rv64gc_zbb_zba_zbc_zbkb_zvl256b` (no Zvbc/Zvbb).

---

## 1. Hardware Bill of Materials

### Required

| Item | Notes |
|---|---|
| **Banana Pi BPI-F3** board | 4 GB or 8 GB LPDDR4X variant recommended |
| **12 V / 3 A DC barrel-jack power supply** | 5.5 × 2.1 mm positive-centre plug; board accepts 12 V PD via Type-C as well |
| **microSD card ≥ 16 GB, Class 10 / A1** | For OS image; U3/A2 preferred for faster flashing |
| **USB-A to USB-A cable** (or USB-A to Type-C) | For eMMC flashing via Titan Flasher (FDL mode) |
| **Ethernet cable (RJ45)** | Headless SSH access; board has 2 × GbE ports |
| **Linux or Windows host machine** | For cross-compilation and image flashing |

### Strongly Recommended

| Item | Notes |
|---|---|
| **USB-to-UART TTL serial adapter (3.3 V)** | 3-pin JST or bare wires to the 40-pin header (UART0: TX=pin 8, RX=pin 10, GND=pin 6). Gives you a serial console before SSH is up and for boot debugging. Typical cables: CP2102, CH340, PL2303 |
| **Heatsink + small fan** | K1 runs warm under sustained crypto workloads. BPI-F3 has 4-pin PWM fan header |
| **USB 3.0 flash drive ≥ 8 GB** | Optional: used as staging area when flashing eMMC from a running SD-card system |

### Alternatives If Missing a Component

| Missing | Alternative |
|---|---|
| No 12 V barrel supply | Use a USB-C PD adapter that negotiates 12 V (20 V/3 A PD charger + PD trigger cable). **Do not use a 5 V phone charger — board will not boot** |
| No microSD card | Flash directly to eMMC from the host using Titan Flasher over USB Type-C (FDL mode) |
| No TTL serial cable | Use HDMI display + USB keyboard for first boot; set up SSH then work headlessly |
| No Ethernet cable | Use the onboard WiFi (`nmcli dev wifi connect "SSID" password "PASS"`) |
| 2 GB DDR version | The 2 GB variant has a known startup issue — see [BPI docs workaround](https://docs.banana-pi.org/en/BPI-F3/GettingStarted_BPI-F3) (change `ddr_cs_num` to 1 via Titan Tools mass-production mode) |

---

## 2. OS Image

Download the latest **Bianbu** image for BPI-F3 from the
[BPI-F3 image index](https://github.com/BPI-SINOVOIP/BPI-F3-bsp/releases).

- SD-card image: filename ends in `bianbu-k1-xxx.img.zip`
- eMMC image: filename ends in `bianbu-k1-xxx.zip`

**Do not mix these two formats.** Bianbu ships a GCC 13 RISC-V toolchain and
kernel headers — both are useful for on-board native compilation.

### Flash to microSD (recommended for first boot)

```bash
# On your host:
unzip bianbu-k1-*.img.zip
# Use balenaEtcher (GUI) or dd:
sudo dd if=bianbu-k1-*.img of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

Insert the card, connect power, and wait ~60 s. Connect via serial or HDMI to
complete the first-boot account-creation wizard, then enable SSH:

```bash
sudo systemctl enable --now ssh
```

---

## 3. Network & SSH

```bash
# Find the board's IP (from your router, or via serial console):
ip addr show eth0

# From your host:
ssh pi@<board-ip>          # default user on Bianbu is "pi"
```

Set up SSH key auth immediately to avoid typing passwords:

```bash
ssh-copy-id pi@<board-ip>
```

---

## 4. Cross-Compilation on the Host

The current project builds **bare-metal** ELFs (no OS, `sim/common/startup.S` +
`sim/common/link.ld`). Those binaries **cannot be exec'd on Linux** directly.
To run on the BPI-F3's Bianbu Linux you need a **Linux-ABI build** that links
against the standard C library.

### 4.1 Install a RISC-V Linux cross-compiler on your host

```bash
# Debian/Ubuntu:
sudo apt install gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu

# Verify:
riscv64-linux-gnu-gcc --version
```

Alternatively the existing LLVM toolchain at `~/Thesis/installed-tools` can
target Linux if built with the `riscv64-unknown-linux-gnu` sysroot. Check:

```bash
ls ~/Thesis/installed-tools/riscv64-unknown-linux-gnu 2>/dev/null || \
  echo "No Linux sysroot — use apt package above"
```

### 4.2 Build the K1-compatible ISA flags

The SpacemiT K1 supports: `rv64gc_zbb_zba_zbc_zbkb_zvl256b`.
Map those to GCC/Clang flags:

```
-march=rv64gc_zbb_zba_zbc_zbkb_zvl256b
-mabi=lp64d
```

**Note:** The K1 does **not** support Zvbc (`vclmul.vv`) or Zvbb (`vror.vv`,
`vandn.vv`). Only the `ref_scalar`, `ref_vector`, `karatsuba_zbc`, and
`karatsuba_rvv` variants can run on this board. The `karatsuba_zvbc` variant
and the Keccak `zvbb_x2_soa` backend require Marian RTL simulation.

**Important:** Bianbu's kernel enables the V extension (`CONFIG_RISCV_ISA_V=y`)
and the Linux ABI saves/restores vector state on context switches. You do not
need to configure anything extra — just compile with the flags above.

### 4.3 Example: build a Linux-linked benchmark

```bash
# From the hqc-riscv root on your host:
MARCH="rv64gc_zbb_zba_zbc_zbkb_zvl256b"
CC=riscv64-linux-gnu-gcc

$CC -O3 -march=$MARCH -mabi=lp64d \
    -I include -I src/common \
    src/common/bench.c src/common/randombytes.c \
    src/hqc.c src/vector.c \
    src/mult/karatsuba_zbc/gf2x_clmul.c \
    tests/bench/bench_hqc.c \
    -o bench_hqc128_k1 \
    -DHQC_SECURITY_LEVEL=128
```

Adjust source list and `-D` flags for the variant and security level you want.

### 4.4 Transfer and run

```bash
scp bench_hqc128_k1 pi@<board-ip>:~/
ssh pi@<board-ip> "./bench_hqc128_k1"
```

---

## 5. Native Compilation on the Board

Bianbu ships GCC 13 (`riscv64-linux-gnu-gcc` or just `gcc`). You can compile
directly on the board for smaller iterations:

```bash
ssh pi@<board-ip>
sudo apt install git cmake make gcc g++

# Clone / rsync your repo:
git clone https://github.com/your-fork/hqc-riscv.git
# or:
rsync -avz --exclude build/ ~/Thesis/hqc-riscv/ pi@<board-ip>:~/hqc-riscv/
```

Check the kernel's reported ISA to confirm all extensions are present:

```bash
cat /proc/cpuinfo | grep -E "^isa|uarch|processor" | head -10
# Expected output includes: rv64imafdcv_zic…_zbc…_zvbc…
```

---

## 6. Reading Hardware Performance Counters

Unlike Spike (where `rdcycle = instret`), on the K1 you get **real cycles**.
This is the "gold standard" measurement missing from the Spike ISS flow.

### 6.1 Using `perf`

```bash
sudo apt install linux-tools-$(uname -r) 2>/dev/null || sudo apt install perf
sudo perf stat -e cycles,instructions,cache-misses ./bench_hqc128_k1
```

This gives you:
- `cycles` — wall-clock CPU cycles (divide by `cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq` for seconds)
- `instructions` — retired instruction count (compare with Spike `instret`)
- `IPC = instructions / cycles` — reveals dual-issue utilisation

### 6.2 Reading CSR counters directly from C

```c
static inline uint64_t rdcycle(void) {
    uint64_t v;
    asm volatile ("rdcycle %0" : "=r"(v));
    return v;
}
static inline uint64_t rdinstret(void) {
    uint64_t v;
    asm volatile ("rdinstret %0" : "=r"(v));
    return v;
}
```

Add these helpers to your Linux bench harness (not the bare-metal `bench.c`
which targets HTIF/UART). On Bianbu the kernel enables `zicntr` so both CSRs
are accessible from userspace.

### 6.3 Validating against Spike numbers

| Metric | Spike | BPI-F3 (K1) | Interpretation |
|---|---|---|---|
| `instret` | X | ≈ X ± 1 % | Must match — instruction count is ISA-level |
| `cycles` | ≈ `instret` (ideal 1-CPI) | < `instret` | K1 is dual-issue; CPI < 1 is expected |
| `IPC` | 1.0 (Spike assumption) | > 1.0 | Confirms dual-issue benefit |

A large divergence in `instret` (> 5 %) between Spike and K1 indicates a
branch or loop count difference — check for conditional compilation flags.

---

## 7. Constant-Time Verification on Real Silicon

The `dudect` workflow (see `analysis/ct_verify/dudect/`) produces Linux
binaries. Build and run directly:

```bash
# On your host (cross-compile):
riscv64-linux-gnu-gcc -O2 -march=rv64gc_zbc_zbb -mabi=lp64d \
    -I vendor/dudect/include \
    analysis/ct_verify/dudect/dudect_hqc128.c \
    src/mult/karatsuba_zbc/gf2x_clmul.c \
    -o dudect_hqc128_k1

scp dudect_hqc128_k1 pi@<board-ip>:~/
ssh pi@<board-ip> "./dudect_hqc128_k1"
```

Running dudect on real silicon is stronger evidence than the ISS-based `ctgrind`
path because it measures actual timing variation on a real microarchitecture
(caches, branch predictor, pipeline stalls).

---

## 8. Checklist

- [ ] Board powered at 12 V, heatsink attached
- [ ] Bianbu image flashed and first-boot wizard completed
- [ ] SSH key auth working
- [ ] `cat /proc/cpuinfo` shows `zbc`, `v` in ISA string (Zvbc/Zvbb are NOT present)
- [ ] Cross-compiled Linux binary runs without `Illegal instruction`
- [ ] `perf stat` produces `cycles` and `instructions` (no `<not supported>`)
- [ ] `rdcycle` from C returns monotonically increasing values
- [ ] `instret` from hardware ≈ `instret` from Spike for the same variant

---

## 9. Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| Board does not boot, no UART output | Power supply < 12 V or wrong polarity | Use a proper 12 V 3 A barrel-jack supply |
| `Illegal instruction` when running binary | Binary compiled for unsupported ISA (e.g., Zvbc/Zvbb — K1 does not have these) | Remove Zvbc/Zvbb from `-march`; only `karatsuba_zbc` and `karatsuba_rvv` variants are supported on this board |
| `rdcycle` always returns 0 | Kernel has `zicntr` disabled or no userspace access | Check `CONFIG_RISCV_ISA_ZICNTR=y`; try `perf stat` as root instead |
| `perf stat` says `<not supported>` for `cycles` | PMU driver not loaded | `sudo modprobe riscv_pmu_sbi` or rebuild kernel with `CONFIG_RISCV_PMU_SBI=y` |
| eMMC flash via Titan Flasher fails with "initialization failure" | USB hub in the path | Connect Type-C directly to host USB port, not via hub |
| 2 GB DDR variant does not start | `ddr_cs_num` mismatch | Follow [BPI 2G DDR workaround](https://docs.banana-pi.org/en/BPI-F3/GettingStarted_BPI-F3) |
| SSH login refused | `sshd` not started | Use serial console: `sudo systemctl enable --now ssh` |

---

## 10. References

- [BPI-F3 Getting Started (official docs)](https://docs.banana-pi.org/en/BPI-F3/GettingStarted_BPI-F3)
- [Bianbu OS releases](https://github.com/BPI-SINOVOIP/BPI-F3-bsp/releases)
- [SpacemiT K1 datasheet](https://developer.spacemit.com)
- [Titan Flasher (eMMC flashing tool)](https://download.banana-pi.dev/d/ca025d76afd448aabc63/files/?p=%2FTools%2Fimage_download_tools%2F)
- [RISC-V ISA manual — Zbc extension](https://github.com/riscv/riscv-bitmanip)
- [RISC-V V extension spec](https://github.com/riscv/riscv-v-spec)
- [`docs/METHODOLOGY.md`](../METHODOLOGY.md) — explains why K1 real-silicon measurements complement Spike `instret` counts
