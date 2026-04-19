# scripts/debug/.gdbinit
# GDB init for HQC-RISC-V bare-metal debugging via Spike or QEMU GDB stub.
#
# Usage (Spike):
#   spike -d --rbb-port=1234 <isa-string> <elf>
#   riscv64-unknown-elf-gdb -x scripts/debug/.gdbinit <elf>
#
# Usage (QEMU):
#   qemu-system-riscv64 -s -S ... <elf>
#   riscv64-unknown-elf-gdb -x scripts/debug/.gdbinit <elf>

set confirm off
set pagination off
set print pretty on
set print array on
set print array-indexes on

# Load the custom V-extension register description
set tdesc filename scripts/debug/custom_rvv.xml

# Connect to QEMU's default GDB stub port (change to :1234 for Spike OpenOCD)
target remote :1234

# Load symbols from the ELF (already specified on command line)
# Uncomment and set path if invoking GDB without the ELF argument:
# file build/hqc128_vector_zvbc.elf

# Useful breakpoints
# break main
# break hqc_pke_encrypt

# Helper: dump all vector registers as u64 arrays
define dump_vreg
  printf "v%d: ", $arg0
  print /x $v0
end

# Continue to main automatically (remove if you want to stop at _start)
# continue
