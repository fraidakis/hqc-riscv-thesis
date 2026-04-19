# toolchain/riscv-clang.cmake
# CMake toolchain file for bare-metal RISC-V cross-compilation with Clang/LLVM.
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv-clang.cmake ..

# ── Critical bare-metal settings ──────────────────────────────────────────────
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# Prevent CMake from trying to link a test executable during compiler detection.
# Without this, the bare-metal linker will fail the try_compile check.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ── Toolchain prefix ──────────────────────────────────────────────────────────
if(NOT DEFINED RISCV)
  set(RISCV "$ENV{HOME}/Thesis/installed-tools")
endif()

set(RISCV_TOOLCHAIN_PREFIX "${RISCV}/bin")
set(RISCV_SYSROOT "${RISCV}/riscv64-unknown-elf")

# ── Compiler binaries ─────────────────────────────────────────────────────────
set(CMAKE_C_COMPILER   "${RISCV_TOOLCHAIN_PREFIX}/clang")
set(CMAKE_CXX_COMPILER "${RISCV_TOOLCHAIN_PREFIX}/clang++")
set(CMAKE_ASM_COMPILER "${RISCV_TOOLCHAIN_PREFIX}/clang")
set(CMAKE_AR           "${RISCV_TOOLCHAIN_PREFIX}/llvm-ar")
set(CMAKE_RANLIB       "${RISCV_TOOLCHAIN_PREFIX}/llvm-ranlib")
set(CMAKE_OBJCOPY      "${RISCV_TOOLCHAIN_PREFIX}/llvm-objcopy")
set(CMAKE_OBJDUMP      "${RISCV_TOOLCHAIN_PREFIX}/llvm-objdump")
set(CMAKE_STRIP        "${RISCV_TOOLCHAIN_PREFIX}/llvm-strip")

# ── Sysroot & target triple ───────────────────────────────────────────────────
set(CMAKE_SYSROOT "${RISCV_SYSROOT}")
set(RISCV_TARGET_TRIPLE "riscv64-unknown-elf")

# ── Aggressive bare-metal compile flags ──────────────────────────────────────
set(RISCV_MARCH "rv64gcv_zbc_zbb_zbkb_zvbc")
set(RISCV_MABI  "lp64d")

set(RISCV_COMMON_FLAGS
  "--target=${RISCV_TARGET_TRIPLE}"
  "-march=${RISCV_MARCH}"
  "-mabi=${RISCV_MABI}"
  "-menable-experimental-extensions"
  "-mcmodel=medany"
  "-O3"
  "-flto"
  "-mno-relax"
  "-ffreestanding"
  "-fno-tree-vectorize"
  "-ffunction-sections"
  "-fdata-sections"
)

string(JOIN " " RISCV_FLAGS_STR ${RISCV_COMMON_FLAGS})

set(CMAKE_C_FLAGS_INIT   "${RISCV_FLAGS_STR}")
set(CMAKE_CXX_FLAGS_INIT "${RISCV_FLAGS_STR} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "--target=${RISCV_TARGET_TRIPLE} -march=${RISCV_MARCH} -mabi=${RISCV_MABI} -menable-experimental-extensions")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -Wl,--gc-sections -nostartfiles -nostdlib")

# ── Find root path for libraries ──────────────────────────────────────────────
set(CMAKE_FIND_ROOT_PATH "${RISCV_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
