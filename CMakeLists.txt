cmake_minimum_required(VERSION 3.21)
project(hqc-riscv C ASM)

# ── Fallback flags for direct GCC cross-configure (no toolchain file) ───────
# This keeps commands like
#   cmake -S . -B build/gcc-test -DCMAKE_C_COMPILER=riscv64-unknown-elf-gcc
# functional by injecting the ISA/ABI extensions required by Zbc/Zvbc sources.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_C_COMPILER MATCHES "riscv64-unknown-elf-gcc" AND
   NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Applying fallback RISC-V GCC flags for direct cross-compile configure")
    add_compile_options(
    -march=rv64gc_zbc_zbb_zba
        -mabi=lp64d
        -mcmodel=medany
        -mno-relax
        -ffreestanding
        -fno-tree-vectorize
        -ffunction-sections
        -fdata-sections
    )
    add_link_options(-nostartfiles -nostdlib -Wl,--gc-sections)
endif()

# ── compile_commands.json ─────────────────────────────────────────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── CTest ─────────────────────────────────────────────────────────────────────
enable_testing()

# ── Python interpreter (used by code generators) ─────────────────────────────
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ── Global include paths ──────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Startup assembly and linker script ───────────────────────────────────────
set(STARTUP_S    ${CMAKE_SOURCE_DIR}/sim/common/startup.S)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/sim/common/link.ld)

# ── Per-phase selection flags and invariant core OBJECT library ───────────────
include(cmake/options.cmake)
include(cmake/hqc_core.cmake)

# ── Simulator backend ─────────────────────────────────────────────────────────
# HQC_SIM=qemu  (default) — UART output + test-finisher (for qemu-system-riscv64 virt)
# HQC_SIM=spike           — HTIF tohost/fromhost output  (for Spike ISS)
set(HQC_SIM "qemu" CACHE STRING "Simulator backend: qemu or spike")
message(STATUS "Simulator backend: ${HQC_SIM}")

if(HQC_SIM STREQUAL "spike")
    set(SIM_DEFINES "HQC_SILENT=1") # Suppress verbose headers on Spike — HTIF traps per char are expensive
    set(BENCH_SIM_RUNNER "${CMAKE_SOURCE_DIR}/sim/spike/run.sh")
    set(BENCH_SIM_ARGS "--clean")
elseif(HQC_SIM STREQUAL "qemu")
    set(SIM_DEFINES "HQC_SIM_QEMU=1")
    set(BENCH_SIM_RUNNER "${CMAKE_SOURCE_DIR}/sim/qemu/run.sh")
    set(BENCH_SIM_ARGS "")
else()
    message(FATAL_ERROR "Unsupported HQC_SIM='${HQC_SIM}'. Use 'qemu' or 'spike'.")
endif()

set(HQC_TRACE_MODE 0 CACHE STRING "Compile trace/hist benchmark binaries without warm-up loops")
if(HQC_TRACE_MODE)
    list(APPEND SIM_DEFINES "HQC_TRACE_MODE=1")
    message(STATUS "HQC trace mode: ON (warm-up loops compiled out)")
else()
    message(STATUS "HQC trace mode: OFF")
endif()

# ── Benchmark iteration count ────────────────────────────────────────────────
# Controls N_ITER in benchmark harness targets.
# Spike runs in this repository are deterministic, so N=1 is sufficient for
# canonical instruction-count collection and keeps turnaround low.
set(BENCH_N_ITER 1 CACHE STRING "Iterations per benchmark operation (N_ITER)")
message(STATUS "Benchmark N_ITER: ${BENCH_N_ITER}")

# ── Upstream vendor root (registered as git submodule in Phase 0a) ────────────
set(HQC_REF ${CMAKE_SOURCE_DIR}/vendor/hqc-ref)

# ── Polynomial multiplication variants ───────────────────────────────────────
# All targets are declared but marked EXCLUDE_FROM_ALL — build explicitly:
#   cmake --build build --target hqc128_scalar_ref -j$(nproc)
# Scripts (scripts/test/run_kat.sh, scripts/bench/run_bench.sh) auto-skip missing ELFs.
# Different CMake build dirs control Keccak/Decode implementation via
# HQC_KECCAK_IMPL / HQC_DECODE_IMPL (set in cmake/options.cmake).
#
#   scalar_ref  : vendor scalar baseline, no RVV
#   scalar_zb   : scalar optimized champion (Zbc carry-less multiply)
#   vector_rvv  : RVV hybrid champion
#   vector_zvk  : full vector-crypto champion (Zvbc)
#   vector_rvv_ref  : RVV ablation baseline (no direct-write, helper calls)
#   vector_rvv_A    : RVV ablation + direct-write discipline
#   vector_rvv_AB   : RVV ablation + direct-write + overlap-save
#   vector_rvv_ABC  : RVV ablation + direct-write + overlap-save + inlined RVV
set(POLYMUL_VARIANTS
    scalar_ref
    scalar_zb
    scalar_zb_A
    scalar_zb_AB
    scalar_zb_AC
    scalar_zb_ACD
    scalar_zb_ABC_nosplit
    vector_rvv_D
    vector_rvv_V
    vector_rvv_VD
    vector_rvv_DV
    vector_rvv
    vector_zvk
)

# Variants with a real implementation (not __builtin_trap stubs).
# KAT targets for variants NOT listed here get EXCLUDE_FROM_ALL so the default
# build only produces ELFs for working code.  "All" scripts auto-build and run
# only these variants.
# Edit scripts/implemented_variants.txt (one variant per line) — loaded here
# and by scripts/variants.sh so both stay in sync automatically.
file(STRINGS "${CMAKE_SOURCE_DIR}/scripts/implemented_variants.txt" IMPLEMENTED_VARIANTS)

set(SECURITY_LEVELS 128 192 256)

# ── Keccak / SHAKE sources — selected by HQC_KECCAK_IMPL ─────────────────────
# All polymul variants automatically pick up the chosen Keccak impl.
# Stable options: ref (Track A/B) | zvbb_x2_soa (Track C2, promoted 2026-05-22;
#   includes keccak_xor_bytes RVV byte-streaming from EXP-keccak-xor-bytes-rvv)
# Legacy variants are archived under experiments/dropped/ and not selectable by default.
#
# KECCAK_INCLUDE_DIRS  : extra include dirs required by the chosen Keccak impl
#                        (appended to every KAT / bench target)
set(KECCAK_INCLUDE_DIRS "")
set(KECCAK_COMPILE_DEFINES "")

if(HQC_KECCAK_IMPL STREQUAL "zvbb_x2_soa")
    # GCC 15 cannot compile the Zvbb x2 SoA backend due to two issues:
    #   1) __attribute__((riscv_rvv_vector_bits(N))) is Clang-only — GCC rejects
    #      arrays of sizeless vector types (vu64m1_fixed_t state[25]).
    #   2) LMUL=8 register pressure in KeccakF1600_StatePermute triggers an
    #      infinite-loop / ICE in GCC's register allocator.
    # Workaround: fall back to the scalar reference Keccak when compiling with GCC.
    # The Zvbc polynomial multiplier (gf2x_vclmul.c, LMUL=4) is unaffected.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(WARNING
            "GCC cannot compile zvbb_x2_soa Keccak (sizeless array / LMUL=8 regalloc). "
            "Falling back to ref Keccak.  Zvbc polymul remains fully vectorized.")
        set(KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
        set(KECCAK_COMPILE_DEFINES "")
    else()
        # Track C2: top-performing consolidated Zvbb backend (single TU).
        # Contains both single-stream fips202 API and paired-stream x2 wrappers.
        set(KECCAK_SOURCES
            ${CMAKE_SOURCE_DIR}/src/keccak/fips202_zvbb_x2.c)
        set_source_files_properties(
            ${CMAKE_SOURCE_DIR}/src/keccak/fips202_zvbb_x2.c
            PROPERTIES COMPILE_OPTIONS "-march=rv64gcv_zvbb_zbb;-mrvv-vector-bits=${HQC_VECTOR_BITS}"
                       COMPILE_DEFINITIONS "HQC_VECTOR_BITS=${HQC_VECTOR_BITS}"
        )
        set(KECCAK_COMPILE_DEFINES HQC_KECCAK_X2_SOA=1)
    endif()

else()
    # Default (ref): vendor scalar fips202
    set(KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
endif()

# ── RM/RS Decoder sources — selected by HQC_DECODE_IMPL ──────────────────────
# Phase 3 replaces these.  The code.c / fft.c wrappers remain vendor-sourced
# regardless; only the leaf decode functions change.
if(HQC_DECODE_IMPL STREQUAL "rvv")
    # Phase 3a: RVV RM FHT decoder (reed_muller_rvv.c).
    # Phase 3b baseline: RVV RS path with Axis A syndrome vectorisation only
    #                   (reed_solomon_rvv.c).
    set(DECODE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/decode/reed_muller_rvv.c
        ${CMAKE_SOURCE_DIR}/src/decode/reed_solomon_rvv.c
    )
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/decode/reed_muller_rvv.c
        PROPERTIES COMPILE_OPTIONS "-march=rv64gcv;$<$<C_COMPILER_ID:Clang>:-mrvv-vector-bits=${HQC_VECTOR_BITS}>;$<$<C_COMPILER_ID:GNU>:-mrvv-vector-bits=zvl>"
    )
    # reed_solomon_rvv.c uses RVV log/exp gather vectorisation (no Zvbc/Zvbb).
    # Scalar Zbc is included for consistency with the variant's ISA tier (rv64gcv_zbc).
    # Note: gf_mul() is linked from hqc_core (always compiled with Zbc), so the
    # TU-level flag has no measurable effect — confirmed via ablation (0 Δ instret).
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/decode/reed_solomon_rvv.c
        PROPERTIES COMPILE_OPTIONS "-march=rv64gcv_zbc;$<$<C_COMPILER_ID:Clang>:-mrvv-vector-bits=${HQC_VECTOR_BITS}>;$<$<C_COMPILER_ID:GNU>:-mrvv-vector-bits=zvl>"
    )
elseif(HQC_DECODE_IMPL STREQUAL "zvbc" OR HQC_DECODE_IMPL STREQUAL "rs_zvbc_exp")
    # Phase 3b+ promoted path: Zvbc vectorised BM + Forney RS implementation,
    # plus iterative Zvbc-vectorised Gao-Mateer additive FFT (Axis C).
    # fft_zvbc.c replaces vendor common/fft.c for this backend — see FFT_SOURCES
    # selection below.
    set(DECODE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/decode/reed_muller_rvv.c
        ${CMAKE_SOURCE_DIR}/src/decode/reed_solomon_zvbc.c
        ${CMAKE_SOURCE_DIR}/src/decode/fft_zvbc.c
    )
    # When HQC_DECODE_IMPL=zvbc, compile decode sources with Zvbc extensions.
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/decode/reed_muller_rvv.c
        PROPERTIES COMPILE_OPTIONS "-march=rv64gcv;$<$<C_COMPILER_ID:Clang>:-mrvv-vector-bits=${HQC_VECTOR_BITS}>;$<$<C_COMPILER_ID:GNU>:-mrvv-vector-bits=zvl>"
    )
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/decode/reed_solomon_zvbc.c
        PROPERTIES COMPILE_OPTIONS "-march=rv64gcv_zbc_zvbc;$<$<C_COMPILER_ID:Clang>:-mrvv-vector-bits=${HQC_VECTOR_BITS}>;$<$<C_COMPILER_ID:GNU>:-mrvv-vector-bits=zvl>"
    )
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/decode/fft_zvbc.c
        PROPERTIES COMPILE_OPTIONS "-march=rv64gcv_zbc_zvbc;$<$<C_COMPILER_ID:Clang>:-mrvv-vector-bits=${HQC_VECTOR_BITS}>;$<$<C_COMPILER_ID:GNU>:-mrvv-vector-bits=zvl>"
    )
else()
    # Default (ref): vendor scalar reed_muller + reed_solomon
    set(DECODE_SOURCES
        ${HQC_REF}/src/ref/reed_muller.c
        ${HQC_REF}/src/ref/reed_solomon.c
    )
endif()

# ── FFT source selection — selected by HQC_DECODE_IMPL ───────────────────────
# When HQC_DECODE_IMPL=zvbc, the Zvbc backend ships its own iterative FFT in
# src/decode/fft_zvbc.c (linked via DECODE_SOURCES above) and the vendor
# common/fft.c MUST be excluded from the link to avoid duplicate symbols.
# All other DECODE backends keep the vendor scalar fft.c.
if(HQC_DECODE_IMPL STREQUAL "zvbc" OR HQC_DECODE_IMPL STREQUAL "rs_zvbc_exp")
    set(FFT_SOURCES "")  # fft.c provided by fft_zvbc.c in DECODE_SOURCES
else()
    set(FFT_SOURCES ${HQC_REF}/src/common/fft.c)
endif()

# ── Vendor sources invariant to polymul strategy ──────────────────────────────
# These change only in Phase 2 (symmetric.c → fips202 dependency) or Phase 3
# (code.c → RM/RS dependency), but at symbol level, not source level.
# NOTE: common/fft.c is NOT included here — it is selected per-variant via
# VARIANT_FFT_SOURCES so that HQC_DECODE_IMPL=zvbc can substitute fft_zvbc.c
# while the scalar/auto-vectorised reference variants keep vendor fft.c.
set(VENDOR_INVARIANT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/hqc.c       # PKE encrypt/decrypt (calls gf2x + rs)
    ${HQC_REF}/src/common/code.c        # HQC encode/decode (wraps RM/RS)
    ${HQC_REF}/src/common/symmetric.c   # SHAKE-256 absorb/squeeze wrapper
)

# ── Our own common sources ────────────────────────────────────────────────────
# KAT mode: include AES-CTR DRBG for NIST-compatible random byte generation.
set(LOCAL_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/randombytes.c
    ${CMAKE_SOURCE_DIR}/src/common/rng/aes_ctr_drbg.c
    ${CMAKE_SOURCE_DIR}/src/common/rng/tiny_aes.c
    ${CMAKE_SOURCE_DIR}/src/common/bench.c
    ${CMAKE_SOURCE_DIR}/sim/common/mem.c        # bare-metal memcpy/memset/memcmp
    ${CMAKE_SOURCE_DIR}/sim/common/printf_stub.c
)
# Bench mode: randombytes.c provides its own stub; no AES overhead needed.
set(LOCAL_BENCH_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/randombytes.c
    ${CMAKE_SOURCE_DIR}/src/common/bench.c
    ${CMAKE_SOURCE_DIR}/sim/common/mem.c
    ${CMAKE_SOURCE_DIR}/sim/common/printf_stub.c
)

# ── PGO helper for reference baseline targets (P-06) ─────────────────────────
# Applies PGO flags to a named target.
# Called for every scalar_ref executable (KAT, bench, polymul bench).
# Supports both GCC (-fprofile-generate/-fprofile-use) and Clang
# (-fprofile-instr-generate/-fprofile-instr-use) PGO workflows.
function(hqc_apply_ref_pgo TARGET)
    if(HQC_REF_PGO STREQUAL "generate")
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${TARGET} PRIVATE -fprofile-generate=${HQC_REF_PGO_FILE})
            target_link_options(${TARGET} PRIVATE -fprofile-generate=${HQC_REF_PGO_FILE})
        else()
            # LLVM IR-level instrumentation (works with Clang; Spike captures the
            # profile via the LLVM profile runtime's __llvm_profile_write_file
            # hook, which must be wired in via sim/common/bench.c or a wrapper).
            target_compile_options(${TARGET} PRIVATE -fprofile-instr-generate)
            target_link_options(${TARGET} PRIVATE -fprofile-instr-generate)
        endif()
    elseif(HQC_REF_PGO STREQUAL "use")
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${TARGET} PRIVATE
                -fprofile-use=${HQC_REF_PGO_FILE} -fprofile-correction)
            target_link_options(${TARGET} PRIVATE
                -fprofile-use=${HQC_REF_PGO_FILE})
        else()
            target_compile_options(${TARGET} PRIVATE
                -fprofile-instr-use=${HQC_REF_PGO_FILE})
            target_link_options(${TARGET} PRIVATE
                -fprofile-instr-use=${HQC_REF_PGO_FILE})
        endif()
    endif()
endfunction()

# ── Target matrix: LEVEL × POLYMUL ───────────────────────────────────────────
foreach(LEVEL ${SECURITY_LEVELS})
    # Build the invariant core OBJECT library for this level (idempotent).
    hqc_add_core(${LEVEL})

    if(LEVEL EQUAL 128)
        set(VARIANT "hqc-1")
    elseif(LEVEL EQUAL 192)
        set(VARIANT "hqc-3")
    elseif(LEVEL EQUAL 256)
        set(VARIANT "hqc-5")
    endif()

    foreach(POLYMUL ${POLYMUL_VARIANTS})

        # ── vector.c source — promoted O(weight) scatter for all variants ──────
        # Target 2 (Track A, 2026-05-14): vect_write_support_to_vector rewritten
        # from O(VEC_N_SIZE_64 × weight) to O(weight) direct-scatter.
        # Validated correct across all 3 security levels via KAT (300/300 pass).
        # Performance: 20.99% → not in top 20 hotspots at HQC-128.
        set(VECTOR_SOURCES ${CMAKE_SOURCE_DIR}/src/vector.c)
        set(VARIANT_KECCAK_SOURCES ${KECCAK_SOURCES})
        set(VARIANT_DECODE_SOURCES ${DECODE_SOURCES})
        set(VARIANT_FFT_SOURCES ${FFT_SOURCES})
        set(VARIANT_COMPILE_OPTIONS "")
        set(VARIANT_REF_PROFILE OFF)

        # ── Polynomial multiply sources per strategy ──────────────────────────
        if(POLYMUL STREQUAL "scalar_ref")
            # Reference scalar baseline: vendor unoptimized polymul/keccak/decode.
            set(POLYMUL_SOURCES ${HQC_REF}/src/ref/gf2x.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            set(VARIANT_FFT_SOURCES ${HQC_REF}/src/common/fft.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=${HQC_SCALAR_MARCH}
                -mabi=lp64d
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                $<$<C_COMPILER_ID:Clang>:-fno-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fno-slp-vectorize>)
            set(VARIANT_REF_PROFILE ON)
        elseif(POLYMUL STREQUAL "scalar_zb")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_zbc/gf2x_clmul.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            set(VARIANT_FFT_SOURCES ${HQC_REF}/src/common/fft.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=${HQC_SCALAR_MARCH}
                -mabi=lp64d
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                $<$<C_COMPILER_ID:Clang>:-fno-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fno-slp-vectorize>)
        elseif(POLYMUL STREQUAL "scalar_zb_A")
            # Ablation: word-level only (no noinit, no specialized tree)
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_zbc/gf2x_clmul_ablation.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            set(VARIANT_FFT_SOURCES ${HQC_REF}/src/common/fft.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=${HQC_SCALAR_MARCH}
                -mabi=lp64d
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                $<$<C_COMPILER_ID:Clang>:-fno-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fno-slp-vectorize>
                -DABLATION_WORDLEVEL=1
                -DABLATION_NOINIT=0
                -DABLATION_AP2=0)
        elseif(POLYMUL STREQUAL "scalar_zb_AB")
            # Ablation: word-level + noinit (no specialized tree)
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_zbc/gf2x_clmul_ablation.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            set(VARIANT_FFT_SOURCES ${HQC_REF}/src/common/fft.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=${HQC_SCALAR_MARCH}
                -mabi=lp64d
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                $<$<C_COMPILER_ID:Clang>:-fno-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fno-slp-vectorize>
                -DABLATION_WORDLEVEL=1
                -DABLATION_NOINIT=1
                -DABLATION_AP2=0)
        elseif(POLYMUL STREQUAL "scalar_zb_AC")
            # Ablation: word-level + specialized tree only (no leaf unrolling, no noinit)
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_zbc/gf2x_clmul_ablation.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            set(VARIANT_FFT_SOURCES ${HQC_REF}/src/common/fft.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=${HQC_SCALAR_MARCH}
                -mabi=lp64d
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                $<$<C_COMPILER_ID:Clang>:-fno-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fno-slp-vectorize>
                -DABLATION_WORDLEVEL=1
                -DABLATION_NOINIT=0
                -DABLATION_AP2=1
                -DABLATION_LEAF_SPEC=0)
        elseif(POLYMUL STREQUAL "scalar_zb_ACD")
            # Ablation: word-level + specialized tree + leaf unrolling (no noinit)
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_zbc/gf2x_clmul_ablation.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            set(VARIANT_FFT_SOURCES ${HQC_REF}/src/common/fft.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=${HQC_SCALAR_MARCH}
                -mabi=lp64d
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                $<$<C_COMPILER_ID:Clang>:-fno-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fno-slp-vectorize>
                -DABLATION_WORDLEVEL=1
                -DABLATION_NOINIT=0
                -DABLATION_AP2=1
                -DABLATION_LEAF_SPEC=1)
        elseif(POLYMUL STREQUAL "scalar_zb_ABC_nosplit")
            # Ablation: word-level + noinit + symmetric-split specialized tree
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_zbc/gf2x_clmul_ablation.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            set(VARIANT_FFT_SOURCES ${HQC_REF}/src/common/fft.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=${HQC_SCALAR_MARCH}
                -mabi=lp64d
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                $<$<C_COMPILER_ID:Clang>:-fno-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fno-slp-vectorize>
                -DABLATION_WORDLEVEL=1
                -DABLATION_NOINIT=1
                -DABLATION_AP2=0
                -DABLATION_SYMMETRIC_TREE=1)
        elseif(POLYMUL STREQUAL "vector_rvv_D")
            # Ablation: direct-write discipline + scalar recomposition loops + scalar reduce.
            # Isolates the algorithmic gain of eliminating copy loops 1+2.
            # Header: direct-write tree with all-scalar XOR/copy (no RVV intrinsics in tree).
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_rvv/gf2x_clmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zba
                -mabi=lp64d
                -O3
                -fno-tree-vectorize
                "-DRVV_ABLATION_HEADER=\"karatsuba_rvv_ablation_dw_scalar_gen.h\""
                -DRVV_USE_SCALAR_REDUCE=1)
        elseif(POLYMUL STREQUAL "vector_rvv_V")
            # Ablation: ref layout + RVV-vectorized recomposition + scalar reduce.
            # Isolates V (vectorized loops) without D (direct-write discipline).
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_rvv/gf2x_clmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zba
                -mabi=lp64d
                -O3
                -fno-tree-vectorize
                "-DRVV_ABLATION_HEADER=\"karatsuba_rvv_ablation_v_only_gen.h\""
                -DRVV_USE_SCALAR_REDUCE=1)
        elseif(POLYMUL STREQUAL "vector_rvv_VD")
            # Ablation: V first, then D on top (alternate ordering: same binary as DV).
            # Direct-write + RVV-vectorized recomposition + scalar reduce.
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_rvv/gf2x_clmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zba
                -mabi=lp64d
                -O3
                -fno-tree-vectorize
                -DRVV_USE_SCALAR_REDUCE=1)
        elseif(POLYMUL STREQUAL "vector_rvv_DV")
            # Ablation: direct-write + RVV-vectorized recomposition + scalar reduce.
            # Isolates the benefit of vectorized data movement in the Karatsuba tree.
            # Header: the champion specialized gen with inline RVV (hybrid threshold).
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_rvv/gf2x_clmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zba
                -mabi=lp64d
                -O3
                -fno-tree-vectorize
                -DRVV_USE_SCALAR_REDUCE=1)
        elseif(POLYMUL STREQUAL "vector_rvv")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_rvv/gf2x_clmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zba
                -mabi=lp64d
                -O3
                -fno-tree-vectorize)
        elseif(POLYMUL STREQUAL "vector_zvk")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/mult/karatsuba_zvbc/gf2x_vclmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zba_zvbc_zvbb
                -mabi=lp64d
                -O3)
        endif()

        # ── KAT target  (hqc${LEVEL}_${POLYMUL}) ─────────────────────────────
        # Linked against the invariant core OBJECT library.
        # Compiled with HQC_RANDOMBYTES_MODE_KAT=1 (AES-CTR DRBG for KAT vectors).
        set(KAT_TARGET "hqc${LEVEL}_${POLYMUL}")

        if(NOT POLYMUL IN_LIST IMPLEMENTED_VARIANTS)
            set(KAT_EXCLUDE EXCLUDE_FROM_ALL)
        else()
            set(KAT_EXCLUDE "")
        endif()

        add_executable(${KAT_TARGET} ${KAT_EXCLUDE}
            ${STARTUP_S}
            ${CMAKE_SOURCE_DIR}/tests/kat/kat_gen.c
            ${POLYMUL_SOURCES}
            ${VARIANT_KECCAK_SOURCES}
            ${VARIANT_DECODE_SOURCES}
            ${VARIANT_FFT_SOURCES}
            ${VENDOR_INVARIANT_SOURCES}
            ${VECTOR_SOURCES}
            ${LOCAL_COMMON_SOURCES}
        )
        target_link_libraries(${KAT_TARGET} PRIVATE
            $<TARGET_OBJECTS:hqc_core_${LEVEL}>)
        target_include_directories(${KAT_TARGET} BEFORE PRIVATE
            ${HQC_REF}/src/common/${VARIANT}
            ${HQC_REF}/src/common
            ${HQC_REF}/src/ref/${VARIANT}
            ${HQC_REF}/src/ref
            ${HQC_REF}/lib/fips202
            ${CMAKE_SOURCE_DIR}/src/common
            ${CMAKE_SOURCE_DIR}/tests/kat
            ${KECCAK_INCLUDE_DIRS}
        )
        target_compile_definitions(${KAT_TARGET} PRIVATE
            HQC_KAT_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${KECCAK_COMPILE_DEFINES}
            ${SIM_DEFINES}
        )
        if(VARIANT_COMPILE_OPTIONS)
            target_compile_options(${KAT_TARGET} PRIVATE ${VARIANT_COMPILE_OPTIONS})
        endif()
        if(POLYMUL STREQUAL "vector_zvk")
            # Zvbc variants require --isa=rv64gcv_zvbc on Spike at runtime.
            # run_kat.sh detects this compile-time definition and adjusts accordingly.
            target_compile_definitions(${KAT_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        set_target_properties(${KAT_TARGET} PROPERTIES
            SUFFIX ".elf"
            LINK_DEPENDS "${LINKER_SCRIPT}"
        )
        target_link_options(${KAT_TARGET} PRIVATE
            "-T${LINKER_SCRIPT}"
            "-Wl,-Map=${CMAKE_BINARY_DIR}/${KAT_TARGET}.map"
        )
        if(VARIANT_REF_PROFILE)
            hqc_apply_ref_pgo(${KAT_TARGET})
        endif()

        # CTest KAT gate — must pass before any branch is merged.
        # run_kat.sh drives simulator (Spike by default, QEMU with --qemu) and diffs against .rsp
        add_test(
            NAME "kat_${KAT_TARGET}"
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/test/run_kat.sh
                    --variant ${POLYMUL} --level ${LEVEL}
        )

        # BENCH target  (bench_hqc${LEVEL}_${POLYMUL}) ─────────────────────
        # Identical sources, but compiled with HQC_BENCH_MODE=1
        # so that AES-CTR DRBG overhead does not pollute rdcycle measurements.
        # Built with EXCLUDE_FROM_ALL — must be explicitly requested:
        #   cmake --build build --target bench_hqc128_scalar_ref
        set(BENCH_TARGET "bench_hqc${LEVEL}_${POLYMUL}")

        add_executable(${BENCH_TARGET} EXCLUDE_FROM_ALL
            ${STARTUP_S}
            ${CMAKE_SOURCE_DIR}/tests/bench/bench_kem.c
            ${POLYMUL_SOURCES}
            ${VARIANT_KECCAK_SOURCES}
            ${VARIANT_DECODE_SOURCES}
            ${VARIANT_FFT_SOURCES}
            ${VENDOR_INVARIANT_SOURCES}
            ${VECTOR_SOURCES}
            ${LOCAL_BENCH_SOURCES}
        )
        target_link_libraries(${BENCH_TARGET} PRIVATE
            $<TARGET_OBJECTS:hqc_core_${LEVEL}>)
        target_include_directories(${BENCH_TARGET} BEFORE PRIVATE
            ${HQC_REF}/src/common/${VARIANT}
            ${HQC_REF}/src/common
            ${HQC_REF}/src/ref/${VARIANT}
            ${HQC_REF}/src/ref
            ${HQC_REF}/lib/fips202
            ${CMAKE_SOURCE_DIR}/src/common
            ${KECCAK_INCLUDE_DIRS}
        )
        target_compile_definitions(${BENCH_TARGET} PRIVATE
            HQC_BENCH_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${KECCAK_COMPILE_DEFINES}
            ${SIM_DEFINES}
            N_ITER=${BENCH_N_ITER}
            POLYMUL_NAME="${POLYMUL}"
        )
        if(VARIANT_COMPILE_OPTIONS)
            target_compile_options(${BENCH_TARGET} PRIVATE ${VARIANT_COMPILE_OPTIONS})
        endif()
        if(POLYMUL STREQUAL "vector_zvk")
            target_compile_definitions(${BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        set_target_properties(${BENCH_TARGET} PROPERTIES
            SUFFIX ".elf"
            LINK_DEPENDS "${LINKER_SCRIPT}"
        )
        target_link_options(${BENCH_TARGET} PRIVATE
            "-T${LINKER_SCRIPT}"
            "-Wl,-Map=${CMAKE_BINARY_DIR}/${BENCH_TARGET}.map"
        )
        if(VARIANT_REF_PROFILE)
            hqc_apply_ref_pgo(${BENCH_TARGET})
        endif()
        add_custom_target(run_${BENCH_TARGET}
            COMMAND ${BENCH_SIM_RUNNER} ${BENCH_SIM_ARGS} $<TARGET_FILE:${BENCH_TARGET}>
            DEPENDS ${BENCH_TARGET}
            USES_TERMINAL
            COMMENT "Running ${BENCH_TARGET} with HQC_SIM=${HQC_SIM}"
        )

        # POLYMUL MICRO-BENCH target  (bench_polymul_hqc${LEVEL}_${POLYMUL}) ──
        # Benchmarks only vect_mul() — no keypair/encaps/decaps, no Keccak.
        # Use for fast iteration on the vect_mul hotspot.
        #   cmake --build build-spike-probe \
        #         --target bench_polymul_hqc128_scalar_ref
        set(POLYMUL_BENCH_TARGET "bench_polymul_hqc${LEVEL}_${POLYMUL}")

        add_executable(${POLYMUL_BENCH_TARGET} EXCLUDE_FROM_ALL
            ${STARTUP_S}
            ${CMAKE_SOURCE_DIR}/tests/bench/bench_mul.c
            ${CMAKE_SOURCE_DIR}/src/common/bench.c
            ${POLYMUL_SOURCES}
            ${CMAKE_SOURCE_DIR}/sim/common/mem.c
        )
        target_include_directories(${POLYMUL_BENCH_TARGET} BEFORE PRIVATE
            ${HQC_REF}/src/common/${VARIANT}
            ${HQC_REF}/src/common
            ${HQC_REF}/src/ref/${VARIANT}
            ${HQC_REF}/src/ref
            ${CMAKE_SOURCE_DIR}/src/common
        )
        target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE
            HQC_BENCH_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${SIM_DEFINES}
            N_ITER=${BENCH_N_ITER}
            POLYMUL_NAME="${POLYMUL}"
        )
        if(VARIANT_COMPILE_OPTIONS)
            target_compile_options(${POLYMUL_BENCH_TARGET} PRIVATE ${VARIANT_COMPILE_OPTIONS})
        endif()
        if(POLYMUL STREQUAL "vector_zvk")
            target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        set_target_properties(${POLYMUL_BENCH_TARGET} PROPERTIES
            SUFFIX ".elf"
            LINK_DEPENDS "${LINKER_SCRIPT}"
        )
        target_link_options(${POLYMUL_BENCH_TARGET} PRIVATE
            "-T${LINKER_SCRIPT}"
            "-Wl,-Map=${CMAKE_BINARY_DIR}/${POLYMUL_BENCH_TARGET}.map"
        )
        if(VARIANT_REF_PROFILE)
            hqc_apply_ref_pgo(${POLYMUL_BENCH_TARGET})
        endif()
        add_custom_target(run_${POLYMUL_BENCH_TARGET}
            COMMAND ${BENCH_SIM_RUNNER} ${BENCH_SIM_ARGS} $<TARGET_FILE:${POLYMUL_BENCH_TARGET}>
            DEPENDS ${POLYMUL_BENCH_TARGET}
            USES_TERMINAL
            COMMENT "Running ${POLYMUL_BENCH_TARGET} with HQC_SIM=${HQC_SIM}"
        )

        # ── Per-VLEN vector-variant targets (P-02 multi-VLEN matrix) ─────────
        # For each variant that contains RVV code, generate a distinct
        # bench_polymul ELF per entry in VLEN_LIST.  The ELFs are compiled with
        # -mvscale-min=M -mvscale-max=M (M = VLEN/64) so the compiler assumes
        # a fixed register width and can constant-fold vsetvl results.
        # All targets are EXCLUDE_FROM_ALL; build explicitly or via
        #   cmake --build build --target vlen_matrix_hqc${LEVEL}
        if(POLYMUL IN_LIST VECTOR_POLYMUL_VARIANTS)
            foreach(VLEN ${VLEN_LIST})
                # ── Per-VLEN Karatsuba threshold (vector_zvk) ──
                if(POLYMUL STREQUAL "vector_zvk")
                    set(KARATSUBA_THRESHOLD 16)

                    # ── Per-VLEN header generation ─────────────────────────────
                    set(VLEN_GEN_DIR "${CMAKE_BINARY_DIR}/generated/vlen${VLEN}")
                    set(VLEN_GEN_HEADER "${VLEN_GEN_DIR}/karatsuba_zvbc_specialized_gen.h")

                    if(NOT TARGET gen_karatsuba_vlen${VLEN})
                        add_custom_command(
                            OUTPUT ${VLEN_GEN_HEADER}
                            COMMAND ${Python3_EXECUTABLE}
                                    ${CMAKE_SOURCE_DIR}/scripts/gen/gen_karatsuba_specialized.py
                                    --mode zvbc
                                    --threshold ${KARATSUBA_THRESHOLD}
                                    --output ${VLEN_GEN_HEADER}
                            DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen/gen_karatsuba_specialized.py
                            COMMENT "Generating Karatsuba tree for VLEN=${VLEN} (threshold=${KARATSUBA_THRESHOLD})"
                        )
                        add_custom_target(gen_karatsuba_vlen${VLEN}
                            DEPENDS ${VLEN_GEN_HEADER}
                        )
                    endif()
                endif()

                set(VLEN_BENCH_TARGET "bench_polymul_hqc${LEVEL}_${POLYMUL}_vlen${VLEN}")

                add_executable(${VLEN_BENCH_TARGET} EXCLUDE_FROM_ALL
                    ${STARTUP_S}
                    ${CMAKE_SOURCE_DIR}/tests/bench/bench_mul.c
                    ${CMAKE_SOURCE_DIR}/src/common/bench.c
                    ${POLYMUL_SOURCES}
                    ${CMAKE_SOURCE_DIR}/sim/common/mem.c
                )
                target_include_directories(${VLEN_BENCH_TARGET} BEFORE PRIVATE
                    ${HQC_REF}/src/common/${VARIANT}
                    ${HQC_REF}/src/common
                    ${HQC_REF}/src/ref/${VARIANT}
                    ${HQC_REF}/src/ref
                    ${CMAKE_SOURCE_DIR}/src/common
                )
                target_compile_definitions(${VLEN_BENCH_TARGET} PRIVATE
                    HQC_BENCH_MODE=1
                    SECURITY_LEVEL=${LEVEL}
                    HQC_${LEVEL}
                    ${SIM_DEFINES}
                    N_ITER=${BENCH_N_ITER}
                    POLYMUL_NAME="${POLYMUL}_vlen${VLEN}"
                    HQC_VLEN=${VLEN}
                )
                target_compile_options(${VLEN_BENCH_TARGET} PRIVATE
                    ${VARIANT_COMPILE_OPTIONS}
                    $<$<C_COMPILER_ID:Clang>:-mrvv-vector-bits=${VLEN}>
                    $<$<C_COMPILER_ID:GNU>:-mrvv-vector-bits=zvl>
                )
                if(POLYMUL STREQUAL "vector_zvk")
                    target_compile_definitions(${VLEN_BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
                    target_include_directories(${VLEN_BENCH_TARGET} BEFORE PRIVATE ${VLEN_GEN_DIR})
                    target_compile_definitions(${VLEN_BENCH_TARGET} PRIVATE KARATSUBA_THRESHOLD=${KARATSUBA_THRESHOLD})
                    add_dependencies(${VLEN_BENCH_TARGET} gen_karatsuba_vlen${VLEN})
                endif()
                set_target_properties(${VLEN_BENCH_TARGET} PROPERTIES
                    SUFFIX ".elf"
                    LINK_DEPENDS "${LINKER_SCRIPT}"
                )
                target_link_options(${VLEN_BENCH_TARGET} PRIVATE
                    "-T${LINKER_SCRIPT}"
                    "-Wl,-Map=${CMAKE_BINARY_DIR}/${VLEN_BENCH_TARGET}.map"
                )
                add_custom_target(run_${VLEN_BENCH_TARGET}
                    COMMAND ${BENCH_SIM_RUNNER} ${BENCH_SIM_ARGS} $<TARGET_FILE:${VLEN_BENCH_TARGET}>
                    DEPENDS ${VLEN_BENCH_TARGET}
                    USES_TERMINAL
                    COMMENT "Running ${VLEN_BENCH_TARGET} (VLEN=${VLEN}) with HQC_SIM=${HQC_SIM}"
                )
            endforeach()  # VLEN
        endif()  # POLYMUL IN VECTOR_POLYMUL_VARIANTS

    endforeach()  # POLYMUL

    # KECCAK MICRO-BENCH target  (bench_keccak_hqc${LEVEL}) ──────────────────
    # Replicates the FULL decaps Keccak/SHA-3 call pattern in isolation:
    # xof_init + vect_sample_fixed_weight1 (sk reconstruction),
    # hash_h (SHA3-256 over pk), hash_g (SHA3-512), re-encryption sampling
    # (xof_init + 3× vect_sample_fixed_weight2 + vect_set_random),
    # hash_j (SHA3-256 over ciphertext).
    # Use for Amdahl's Law analysis of the Keccak + sampling bottleneck.
    #   cmake --build build --target bench_keccak_hqc128
    set(KECCAK_BENCH_TARGET "bench_keccak_hqc${LEVEL}")

    add_executable(${KECCAK_BENCH_TARGET} EXCLUDE_FROM_ALL
        ${STARTUP_S}
        ${CMAKE_SOURCE_DIR}/tests/bench/bench_keccak.c
        ${CMAKE_SOURCE_DIR}/src/common/bench.c
        ${KECCAK_SOURCES}
        ${HQC_REF}/src/common/symmetric.c
        ${VECTOR_SOURCES}
        ${CMAKE_SOURCE_DIR}/sim/common/mem.c
        ${CMAKE_SOURCE_DIR}/sim/common/printf_stub.c
    )
    target_link_libraries(${KECCAK_BENCH_TARGET} PRIVATE
        $<TARGET_OBJECTS:hqc_core_${LEVEL}>)
    target_include_directories(${KECCAK_BENCH_TARGET} BEFORE PRIVATE
        ${HQC_REF}/src/common/${VARIANT}
        ${HQC_REF}/src/ref/${VARIANT}
        ${HQC_REF}/src/common
        ${HQC_REF}/src/ref
        ${HQC_REF}/lib/fips202
        ${CMAKE_SOURCE_DIR}/src/common
        ${KECCAK_INCLUDE_DIRS}
    )
    target_compile_definitions(${KECCAK_BENCH_TARGET} PRIVATE
        HQC_BENCH_MODE=1
        SECURITY_LEVEL=${LEVEL}
        HQC_${LEVEL}
        ${KECCAK_COMPILE_DEFINES}
        ${SIM_DEFINES}
        N_ITER=${BENCH_N_ITER}
        KECCAK_NAME="${HQC_KECCAK_IMPL}"
    )
    target_compile_options(${KECCAK_BENCH_TARGET} PRIVATE
        -O3
        -march=rv64gcv_zbc_zbb_zvbc_zvbb
        -mabi=lp64d
    )
    set_target_properties(${KECCAK_BENCH_TARGET} PROPERTIES
        SUFFIX ".elf"
        LINK_DEPENDS "${LINKER_SCRIPT}"
    )
    target_link_options(${KECCAK_BENCH_TARGET} PRIVATE
        "-T${LINKER_SCRIPT}"
        "-Wl,-Map=${CMAKE_BINARY_DIR}/${KECCAK_BENCH_TARGET}.map"
    )
    add_custom_target(run_${KECCAK_BENCH_TARGET}
        COMMAND ${BENCH_SIM_RUNNER} ${BENCH_SIM_ARGS} $<TARGET_FILE:${KECCAK_BENCH_TARGET}>
        DEPENDS ${KECCAK_BENCH_TARGET}
        USES_TERMINAL
        COMMENT "Running ${KECCAK_BENCH_TARGET} with HQC_SIM=${HQC_SIM}"
    )

    # DECODE MICRO-BENCH target  (bench_decode_hqc${LEVEL}) ──────────────────
    # Benchmarks reed_muller_decode + reed_solomon_decode in isolation.
    # Use for Amdahl's Law analysis of the decode bottleneck.
    #   cmake --build build --target bench_decode_hqc128
    set(DECODE_BENCH_TARGET "bench_decode_hqc${LEVEL}")

    add_executable(${DECODE_BENCH_TARGET} EXCLUDE_FROM_ALL
        ${STARTUP_S}
        ${CMAKE_SOURCE_DIR}/tests/bench/bench_decode.c
        ${CMAKE_SOURCE_DIR}/src/common/bench.c
        ${DECODE_SOURCES}
        ${FFT_SOURCES}
        ${CMAKE_SOURCE_DIR}/sim/common/mem.c
        ${CMAKE_SOURCE_DIR}/sim/common/printf_stub.c
    )
    target_link_libraries(${DECODE_BENCH_TARGET} PRIVATE
        $<TARGET_OBJECTS:hqc_core_${LEVEL}>)
    target_include_directories(${DECODE_BENCH_TARGET} BEFORE PRIVATE
        ${HQC_REF}/src/common/${VARIANT}
        ${HQC_REF}/src/ref/${VARIANT}
        ${HQC_REF}/src/ref
        ${HQC_REF}/src/common
        ${CMAKE_SOURCE_DIR}/src/common
    )
    target_compile_definitions(${DECODE_BENCH_TARGET} PRIVATE
        HQC_BENCH_MODE=1
        SECURITY_LEVEL=${LEVEL}
        HQC_${LEVEL}
        ${SIM_DEFINES}
        N_ITER=${BENCH_N_ITER}
        DECODE_NAME="${HQC_DECODE_IMPL}"
    )
    target_compile_options(${DECODE_BENCH_TARGET} PRIVATE
        -O3
        -march=rv64gcv_zbc_zbb_zvbc_zvbb
        -mabi=lp64d
    )
    set_target_properties(${DECODE_BENCH_TARGET} PROPERTIES
        SUFFIX ".elf"
        LINK_DEPENDS "${LINKER_SCRIPT}"
    )
    target_link_options(${DECODE_BENCH_TARGET} PRIVATE
        "-T${LINKER_SCRIPT}"
        "-Wl,-Map=${CMAKE_BINARY_DIR}/${DECODE_BENCH_TARGET}.map"
    )
    add_custom_target(run_${DECODE_BENCH_TARGET}
        COMMAND ${BENCH_SIM_RUNNER} ${BENCH_SIM_ARGS} $<TARGET_FILE:${DECODE_BENCH_TARGET}>
        DEPENDS ${DECODE_BENCH_TARGET}
        USES_TERMINAL
        COMMENT "Running ${DECODE_BENCH_TARGET} with HQC_SIM=${HQC_SIM}"
    )
endforeach()  # LEVEL

# ── VLEN matrix umbrella targets ──────────────────────────────────────────────
# Convenience targets to build the full VLEN matrix for all vector variants at
# a given security level:  cmake --build build --target vlen_matrix_hqc128
foreach(LEVEL ${SECURITY_LEVELS})
    set(_VLEN_DEPS "")
    foreach(POLYMUL ${VECTOR_POLYMUL_VARIANTS})
        foreach(VLEN ${VLEN_LIST})
            list(APPEND _VLEN_DEPS "bench_polymul_hqc${LEVEL}_${POLYMUL}_vlen${VLEN}")
        endforeach()
    endforeach()
    add_custom_target(vlen_matrix_hqc${LEVEL} DEPENDS ${_VLEN_DEPS}
        COMMENT "Building VLEN={${VLEN_LIST}} matrix for HQC-${LEVEL} vector variants")
endforeach()

# ── Symlink compile_commands.json to project root ─────────────────────────────
add_custom_target(symlink_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Symlinking compile_commands.json from build dir to project root"
)

# ── objdump target: verify vclmul opcodes ────────────────────────────────────
set(HQC_TOOLCHAIN_BIN_HINT "$ENV{RISCV}/bin" CACHE PATH
    "Preferred RISC-V toolchain bin directory (defaults to $ENV{RISCV}/bin)")

find_program(OBJDUMP_BIN
    NAMES riscv64-unknown-elf-objdump llvm-objdump
    PATHS
        ${HQC_TOOLCHAIN_BIN_HINT}
        "${CMAKE_SOURCE_DIR}/../toolchains/installed-tools/bin"
    NO_DEFAULT_PATH
)



