# toolchain/riscv-riscv64-unknown-elf-gcc.cmake
# CMake toolchain file for bare-metal RISC-V cross-compilation with Clang/LLVM.
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-riscv64-unknown-elf-gcc.cmake ..

# ── Critical bare-metal settings ──────────────────────────────────────────────
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# Prevent CMake from trying to link a test executable during compiler detection.
# Without this, the bare-metal linker will fail the try_compile check.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ── Toolchain prefix ──────────────────────────────────────────────────────────
if(NOT DEFINED RISCV)
  # Default: toolchains/installed-tools lives two levels above this file
  # (hqc-riscv/toolchain/ → hqc-riscv/ → Thesis/ → toolchains/installed-tools)
  get_filename_component(_RISCV_DEFAULT
      "${CMAKE_CURRENT_LIST_DIR}/../../toolchains/installed-tools" REALPATH)
  set(RISCV "${_RISCV_DEFAULT}")
endif()

set(RISCV_TOOLCHAIN_PREFIX "${RISCV}/bin")
set(RISCV_SYSROOT "${RISCV}/riscv64-unknown-elf")

# ── Compiler binaries ─────────────────────────────────────────────────────────
set(CMAKE_C_COMPILER   "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-gcc")
set(CMAKE_CXX_COMPILER "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-gcc++")
set(CMAKE_ASM_COMPILER "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-gcc")
set(CMAKE_AR           "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-ar")
set(CMAKE_RANLIB       "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-ranlib")
set(CMAKE_OBJCOPY      "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-objcopy")
set(CMAKE_OBJDUMP      "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-objdump")
set(CMAKE_STRIP        "${RISCV_TOOLCHAIN_PREFIX}/riscv64-unknown-elf-strip")

# ── Sysroot & target triple ───────────────────────────────────────────────────
set(CMAKE_SYSROOT "${RISCV_SYSROOT}")
set(RISCV_TARGET_TRIPLE "riscv64-unknown-elf")

# ── Aggressive bare-metal compile flags ──────────────────────────────────────
set(RISCV_MARCH "rv64gc_zbc_zbb")
set(RISCV_MABI  "lp64d")
set(RISCV_OPT_LEVEL "-O3" CACHE STRING "Optimization flag for GCC (e.g. -O2, -O3, -Os)")

if(NOT RISCV_OPT_LEVEL MATCHES "^-O([0-3]|s|z)$")
  message(FATAL_ERROR "Invalid RISCV_OPT_LEVEL='${RISCV_OPT_LEVEL}'. Expected one of -O0/-O1/-O2/-O3/-Os/-Oz.")
endif()

set(RISCV_COMMON_FLAGS
  "-march=${RISCV_MARCH}"
  "-mabi=${RISCV_MABI}"
  "-mcmodel=medany"
  "${RISCV_OPT_LEVEL}"
  "-g"
  "-fno-lto"
  "-mno-relax"
  "-ffreestanding"
  "-fno-tree-vectorize"
  "-ffunction-sections"
  "-fdata-sections"
)

string(JOIN " " RISCV_FLAGS_STR ${RISCV_COMMON_FLAGS})

set(CMAKE_C_FLAGS_INIT   "${RISCV_FLAGS_STR}")
set(CMAKE_CXX_FLAGS_INIT "${RISCV_FLAGS_STR} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "-march=${RISCV_MARCH} -mabi=${RISCV_MABI}")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -nostartfiles -nostdlib")

# ── Find root path for libraries ──────────────────────────────────────────────
set(CMAKE_FIND_ROOT_PATH "${RISCV_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
