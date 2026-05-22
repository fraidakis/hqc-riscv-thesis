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
  if(DEFINED ENV{RISCV} AND EXISTS "$ENV{RISCV}/bin/clang")
    set(RISCV "$ENV{RISCV}")
  else()
    # Default: toolchains/installed-tools lives two levels above this file
    # (hqc-riscv/toolchain/ → hqc-riscv/ → Thesis/ → toolchains/installed-tools)
    get_filename_component(_RISCV_DEFAULT
        "${CMAKE_CURRENT_LIST_DIR}/../../toolchains/installed-tools" REALPATH)
    if(EXISTS "${_RISCV_DEFAULT}/bin/clang")
      set(RISCV "${_RISCV_DEFAULT}")
    elseif(DEFINED ENV{HOME} AND EXISTS "$ENV{HOME}/Thesis/installed-tools/bin/clang")
      set(RISCV "$ENV{HOME}/Thesis/installed-tools")
    elseif(DEFINED ENV{HOME} AND EXISTS "$ENV{HOME}/Thesis/toolchains/installed-tools/bin/clang")
      set(RISCV "$ENV{HOME}/Thesis/toolchains/installed-tools")
    elseif(EXISTS "/usr/bin/clang")
      set(RISCV "/usr")
    else()
      message(FATAL_ERROR
        "Unable to locate RISC-V LLVM toolchain. Set RISCV env var or pass -DRISCV=<toolchain-root>. "
        "Tried: '$ENV{RISCV}', '${_RISCV_DEFAULT}', '$ENV{HOME}/Thesis/installed-tools', '$ENV{HOME}/Thesis/toolchains/installed-tools', '/usr'.")
    endif()
  endif()
endif()

set(RISCV_TOOLCHAIN_PREFIX "${RISCV}/bin")
if(DEFINED ENV{RISCV} AND "$ENV{RISCV}" STREQUAL "/usr")
  set(RISCV_SYSROOT "/usr/riscv64-unknown-elf")
elseif(RISCV STREQUAL "/usr")
  set(RISCV_SYSROOT "/usr/riscv64-unknown-elf")
elseif(EXISTS "${RISCV}/riscv64-unknown-elf")
  set(RISCV_SYSROOT "${RISCV}/riscv64-unknown-elf")
else()
  set(RISCV_SYSROOT "")
endif()

function(_find_riscv_tool OUT_VAR)
  cmake_parse_arguments(FINDTOOL "" "TOOL" "CANDIDATES" ${ARGN})
  set(_resolved "")
  foreach(_cand IN LISTS FINDTOOL_CANDIDATES)
    if(EXISTS "${RISCV_TOOLCHAIN_PREFIX}/${_cand}")
      set(_resolved "${RISCV_TOOLCHAIN_PREFIX}/${_cand}")
      break()
    endif()
  endforeach()
  if(_resolved STREQUAL "")
    message(FATAL_ERROR "Unable to find required tool '${FINDTOOL_TOOL}' under ${RISCV_TOOLCHAIN_PREFIX}")
  endif()
  set(${OUT_VAR} "${_resolved}" PARENT_SCOPE)
endfunction()

_find_riscv_tool(CLANG_BIN TOOL clang CANDIDATES clang clang-18)
_find_riscv_tool(CLANGXX_BIN TOOL clang++ CANDIDATES clang++ clang++-18)
_find_riscv_tool(LLD_AR_BIN TOOL llvm-ar CANDIDATES llvm-ar llvm-ar-18)
_find_riscv_tool(LLD_RANLIB_BIN TOOL llvm-ranlib CANDIDATES llvm-ranlib llvm-ranlib-18)
_find_riscv_tool(LLD_OBJCOPY_BIN TOOL llvm-objcopy CANDIDATES llvm-objcopy llvm-objcopy-18)
_find_riscv_tool(LLD_OBJDUMP_BIN TOOL llvm-objdump CANDIDATES llvm-objdump llvm-objdump-18)
_find_riscv_tool(LLD_STRIP_BIN TOOL llvm-strip CANDIDATES llvm-strip llvm-strip-18)

# ── Compiler binaries ─────────────────────────────────────────────────────────
set(CMAKE_C_COMPILER   "${CLANG_BIN}")
set(CMAKE_CXX_COMPILER "${CLANGXX_BIN}")
set(CMAKE_ASM_COMPILER "${CLANG_BIN}")
set(CMAKE_AR           "${LLD_AR_BIN}")
set(CMAKE_RANLIB       "${LLD_RANLIB_BIN}")
set(CMAKE_OBJCOPY      "${LLD_OBJCOPY_BIN}")
set(CMAKE_OBJDUMP      "${LLD_OBJDUMP_BIN}")
set(CMAKE_STRIP        "${LLD_STRIP_BIN}")

# ── Sysroot & target triple ───────────────────────────────────────────────────
if(NOT RISCV_SYSROOT STREQUAL "")
  set(CMAKE_SYSROOT "${RISCV_SYSROOT}")
endif()
set(RISCV_TARGET_TRIPLE "riscv64-unknown-elf")

# ── Aggressive bare-metal compile flags ──────────────────────────────────────
# Allow external override for ablation studies (e.g. strip zbb)
if(DEFINED ENV{RISCV_MARCH_OVERRIDE} AND NOT "$ENV{RISCV_MARCH_OVERRIDE}" STREQUAL "")
  set(RISCV_MARCH "$ENV{RISCV_MARCH_OVERRIDE}")
  message(STATUS "[toolchain] RISCV_MARCH overridden via env: ${RISCV_MARCH}")
else()
  set(RISCV_MARCH "rv64gc_zbc_zbb")
endif()
set(RISCV_MABI  "lp64d")
set(RISCV_OPT_LEVEL "-O3" CACHE STRING "Optimization flag for Clang (e.g. -O2, -O3, -Os)")

if(NOT RISCV_OPT_LEVEL MATCHES "^-O([0-3]|s|z)$")
  message(FATAL_ERROR "Invalid RISCV_OPT_LEVEL='${RISCV_OPT_LEVEL}'. Expected one of -O0/-O1/-O2/-O3/-Os/-Oz.")
endif()

set(RISCV_COMMON_FLAGS
  "--target=${RISCV_TARGET_TRIPLE}"
  "-march=${RISCV_MARCH}"
  "-mabi=${RISCV_MABI}"
  "-menable-experimental-extensions"
  "-mcmodel=medany"
  "${RISCV_OPT_LEVEL}"
  "-g"
  "-gdwarf-4"
  "-fno-lto"
  "-mno-relax"
  "-ffreestanding"
  "-fno-tree-vectorize"
  "-ffunction-sections"
  "-fdata-sections"
)

if(RISCV STREQUAL "/usr")
  list(APPEND RISCV_COMMON_FLAGS
    "--sysroot=${RISCV_SYSROOT}"
    "--gcc-toolchain=/usr"
  )
endif()

string(JOIN " " RISCV_FLAGS_STR ${RISCV_COMMON_FLAGS})

# Emergency hard-append flags: do not use *_INIT variables here because CMake's
# compiler configuration may clobber them for cross builds.
set(RISCV_FORCE_FLAGS
  " --target=riscv64-unknown-elf"
  " --sysroot=${RISCV_SYSROOT}"
  " --gcc-toolchain=${RISCV}"
  " -isystem ${RISCV_SYSROOT}/include"
  " -march=${RISCV_MARCH}"
  " -mabi=lp64d"
  " -menable-experimental-extensions"
  " -mcmodel=medany"
  " ${RISCV_OPT_LEVEL}"
  " -g"
  " -gdwarf-4"
  " -ffreestanding"
  " -fno-lto"
  " -mno-relax"
  " -fno-tree-vectorize"
)
string(JOIN "" RISCV_FORCE_FLAGS_STR ${RISCV_FORCE_FLAGS})

# ── Find root path for libraries ──────────────────────────────────────────────
if(NOT RISCV_SYSROOT STREQUAL "")
  set(CMAKE_FIND_ROOT_PATH "${RISCV_SYSROOT}")
else()
  set(CMAKE_FIND_ROOT_PATH "${RISCV}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Append required target and ABI flags at the bottom so they survive CMake
# configuration passes and override earlier defaults.
string(APPEND CMAKE_C_FLAGS "${RISCV_FORCE_FLAGS_STR}")
string(APPEND CMAKE_CXX_FLAGS "${RISCV_FORCE_FLAGS_STR}")
string(APPEND CMAKE_ASM_FLAGS "${RISCV_FORCE_FLAGS_STR}")
string(APPEND CMAKE_CXX_FLAGS " -fno-exceptions -fno-rtti")

# Explicit linker flags for bare-metal sysroot and libraries.
string(APPEND CMAKE_EXE_LINKER_FLAGS
  " -fuse-ld=lld"
  " -Wl,--gc-sections"
  " -nostartfiles"
  " -nostdlib"
  " --sysroot=${RISCV_SYSROOT}"
  " -L${RISCV_SYSROOT}/lib"
)
