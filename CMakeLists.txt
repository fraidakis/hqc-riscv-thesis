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
    -march=rv64gc_zbc_zbb_zbkb
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
# Controls N_ITER in bench_kem.c / bench_poly_mul.c.
# Spike runs in this repository are deterministic, so N=1 is sufficient for
# canonical instruction-count collection and keeps turnaround low.
set(BENCH_N_ITER 1 CACHE STRING "Iterations per benchmark operation (N_ITER)")
message(STATUS "Benchmark N_ITER: ${BENCH_N_ITER}")

# ── Hybrid Karatsuba depth (Phase 1c sweep) ───────────────────────────────────
# Sets HYBRID_KARATSUBA_DEPTH for all hybrid targets.
# 0 = pure FAFFT, 1 = one Karatsuba outer level (default), etc.
# Override at configure time: cmake -DHYBRID_KARATSUBA_DEPTH=2 ..
set(HYBRID_KARATSUBA_DEPTH 0 CACHE STRING "Karatsuba outer levels for hybrid (0=pure FAFFT, empirically optimal at HQC sizes)")
message(STATUS "Hybrid Karatsuba depth: ${HYBRID_KARATSUBA_DEPTH}")

# ── Hybrid v3 crossover threshold (Phase 1b v3 sweep) ────────────────────────
# Controls V3_HYBRID_THRESH in hybrid_v3.c: when sub-problem size ≤ threshold,
# the outer Karatsuba recursion ("FAFFT outer shell") exits and switches to the
# clmul+clmulh schoolbook leaf ("Karatsuba base case").
# Sweep: 32 → 64 → 128 to find the optimal crossover point.
# Override at configure time: cmake -DV3_HYBRID_THRESH=32 ..
set(V3_HYBRID_THRESH 32 CACHE STRING "Crossover threshold for hybrid_v3 (words): below this → clmul leaf")
message(STATUS "Hybrid v3 crossover threshold: ${V3_HYBRID_THRESH}")

# Karatsuba threshold for the karatsuba_zbc_exp experimental variant.
# Controls KARATSUBA_THRESHOLD in EXP-karatsuba-zbc-leaf-fixed/gf2x_clmul.c.
# Default 8 (V2-A1 sweep baseline); override: cmake -DHQC_KARATSUBA_THRESHOLD=4|8|16
set(HQC_KARATSUBA_THRESHOLD 8 CACHE STRING "Karatsuba leaf threshold for karatsuba_zbc_exp (V2-A1 sweep)")
message(STATUS "karatsuba_zbc_exp threshold: ${HQC_KARATSUBA_THRESHOLD}")

# ── Upstream vendor root (registered as git submodule in Phase 0a) ────────────
set(HQC_REF ${CMAKE_SOURCE_DIR}/vendor/hqc-ref)

# ── Polynomial multiplication variants ───────────────────────────────────────
# All targets are declared but marked EXCLUDE_FROM_ALL — build explicitly:
#   cmake --build build --target hqc128_ref_scalar -j$(nproc)
# Scripts (scripts/test/run_kat.sh, scripts/bench/run_benchmarks.sh) auto-skip missing ELFs.
# Different CMake build dirs control Keccak/Decode implementation via
# HQC_KECCAK_IMPL / HQC_DECODE_IMPL (set in cmake/options.cmake).
#
#   Phase 0a — ref_scalar    : vendor scalar baseline, strict rv64gc ISA
#   Phase 0b — ref_vector    : vendor scalar sources, compiled for base RVV auto-vectorization
#   Phase 1a — karatsuba_zbc : clmul base-case + lazy reduction + depth sweep
#   Phase 1b — fafft_zbc     : FAFFT with clmul GF(2^m) butterflies + RVV XOR
#   Phase 1c — hybrid        : Karatsuba outer levels → FAFFT inner (CRT)
#   Phase 1+ — fafft_zvbc    : vclmul.vv vectorised butterflies (Spike Zvbc only)
set(POLYMUL_VARIANTS
    ref_scalar
    ref_vector
    karatsuba_zbc
    karatsuba_zbc_exp
    karatsuba_zbc_ablation
    karatsuba_leaf_v2_micro
    karatsuba_v2_a5
    karatsuba_ap2_specialized
    toomcook3_zbc
    karatsuba_rvv
    karatsuba_zvbc
    hybrid
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
# Stable options: ref | rvv_only (Track B) | zvbb_gather (Track C)
#
# KECCAK_INCLUDE_DIRS  : extra include dirs required by the chosen Keccak impl
#                        (appended to every KAT / bench target)
set(KECCAK_INCLUDE_DIRS "")

if(HQC_KECCAK_IMPL STREQUAL "rvv_only")
    # Track B: Base V extension — Zhang et al. RVV Keccak-f + HQC SHAKE wrapper
    set(KECCAK_SOURCES ${CMAKE_SOURCE_DIR}/src/keccak/fips202_rvv_only.c)
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/keccak/fips202_rvv_only.c
        PROPERTIES COMPILE_FLAGS "-march=rv64gcv -mrvv-vector-bits=256"
    )

elseif(HQC_KECCAK_IMPL STREQUAL "zvbb_gather")
    # Track C: Zvbb — custom LMUL=8 vrgather/vror/vandn single-state Keccak-f
    # Requires zvbb + zvl256b (vl=25 at e64/LMUL=8 needs VLEN≥256).
    set(KECCAK_SOURCES
        ${CMAKE_SOURCE_DIR}/src/keccak/fips202_zvbb_gather.c)
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/keccak/fips202_zvbb_gather.c
        PROPERTIES COMPILE_FLAGS "-march=rv64gcv_zvbb_zbkb"
    )

else()
    # Default (ref): vendor scalar fips202
    set(KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
endif()

# ── RM/RS Decoder sources — selected by HQC_DECODE_IMPL ──────────────────────
# Phase 3 replaces these.  The code.c / fft.c wrappers remain vendor-sourced
# regardless; only the leaf decode functions change.
if(HQC_DECODE_IMPL STREQUAL "rvv")
    # Phase 3a: RVV RM FHT decoder while keeping scalar RS for correctness.
    # Phase 3b can replace reed_solomon.c with src/decode/rvv/reed_solomon_rvv.c.
    set(DECODE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/decode/rvv/reed_muller_rvv.c
        ${HQC_REF}/src/ref/reed_solomon.c
    )
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/decode/rvv/reed_muller_rvv.c
        PROPERTIES COMPILE_FLAGS "-march=rv64gcv -mrvv-vector-bits=256"
    )
else()
    # Default (ref): vendor scalar reed_muller + reed_solomon
    set(DECODE_SOURCES
        ${HQC_REF}/src/ref/reed_muller.c
        ${HQC_REF}/src/ref/reed_solomon.c
    )
endif()

# ── Vendor sources invariant to polymul strategy ──────────────────────────────
# These change only in Phase 2 (symmetric.c → fips202 dependency) or Phase 3
# (code.c → RM/RS dependency), but at symbol level, not source level.
set(VENDOR_INVARIANT_SOURCES
    ${HQC_REF}/src/ref/hqc.c            # PKE encrypt/decrypt (calls gf2x + rs)
    ${HQC_REF}/src/common/code.c        # HQC encode/decode (wraps RM/RS)
    ${HQC_REF}/src/common/fft.c         # RS polynomial evaluation over GF(2^m)
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

# ── LTO + PGO helper for reference baseline targets (P-06) ───────────────────
# Applies -flto and/or PGO flags to a named target.
# Called for every ref_scalar/ref_vector executable (KAT, bench, polymul bench).
function(hqc_apply_ref_lto_pgo TARGET)
    if(HQC_REF_LTO)
        target_compile_options(${TARGET} PRIVATE -flto)
        target_link_options(${TARGET} PRIVATE -flto)
    endif()

    if(HQC_REF_PGO STREQUAL "generate")
        # LLVM IR-level instrumentation (works with Clang; Spike captures the
        # profile via the LLVM profile runtime's __llvm_profile_write_file
        # hook, which must be wired in via sim/common/bench.c or a wrapper).
        target_compile_options(${TARGET} PRIVATE -fprofile-instr-generate)
        target_link_options(${TARGET} PRIVATE -fprofile-instr-generate)
    elseif(HQC_REF_PGO STREQUAL "use")
        target_compile_options(${TARGET} PRIVATE
            -fprofile-instr-use=${HQC_REF_PGO_FILE})
        target_link_options(${TARGET} PRIVATE
            -fprofile-instr-use=${HQC_REF_PGO_FILE})
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
        # karatsuba_zbc_exp uses the same src/vector.c as all other variants.
        set(VECTOR_SOURCES ${CMAKE_SOURCE_DIR}/src/vector.c)
        set(VARIANT_KECCAK_SOURCES ${KECCAK_SOURCES})
        set(VARIANT_DECODE_SOURCES ${DECODE_SOURCES})
        set(VARIANT_COMPILE_OPTIONS "")
        set(VARIANT_REF_PROFILE OFF)

        # ── Polynomial multiply sources per strategy ──────────────────────────
        if(POLYMUL STREQUAL "ref_scalar")
            # Reference scalar baseline: vendor unoptimized polymul/keccak/decode.
            set(POLYMUL_SOURCES ${HQC_REF}/src/ref/gf2x.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gc
                -mabi=lp64d
                $<$<C_COMPILER_ID:GNU>:-mno-vector>
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                -fno-vectorize
                -fno-slp-vectorize)
            set(VARIANT_REF_PROFILE ON)
        elseif(POLYMUL STREQUAL "ref_vector")
            # Reference vector baseline: same vendor scalar sources, base V ISA.
            set(POLYMUL_SOURCES ${HQC_REF}/src/ref/gf2x.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv
                -mabi=lp64d
                -O3
                $<$<C_COMPILER_ID:GNU>:-ftree-vectorize>
                $<$<C_COMPILER_ID:Clang>:-fvectorize>
                $<$<C_COMPILER_ID:Clang>:-fslp-vectorize>)
            set(VARIANT_REF_PROFILE ON)
        elseif(POLYMUL STREQUAL "karatsuba_zbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c)
            set(VARIANT_KECCAK_SOURCES ${HQC_REF}/lib/fips202/fips202.c)
            set(VARIANT_DECODE_SOURCES
                ${HQC_REF}/src/ref/reed_muller.c
                ${HQC_REF}/src/ref/reed_solomon.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gc_zbc_zbb_zbkb
                -mabi=lp64d
                $<$<C_COMPILER_ID:GNU>:-mno-vector>
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                -fno-vectorize
                -fno-slp-vectorize)
        elseif(POLYMUL STREQUAL "karatsuba_zbc_exp")
            # EXP: V2-A3 fixed-size leaf specialization + V2-A1 threshold sweep.
            # Configure threshold: cmake -DHQC_KARATSUBA_THRESHOLD=4|8|16
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/experiments/EXP-karatsuba-zbc-leaf-fixed/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "karatsuba_zbc_ablation")
            # EXP: V2-A3 ablation — clmul_leaf_n4 removed (dead code at T=16).
            # Verifies that n=8 leaf alone explains the full 3.21% gain.
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/experiments/EXP-karatsuba-ablation/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "karatsuba_leaf_v2_micro")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/experiments/EXP-karatsuba-leaf-v2-micro/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "karatsuba_v2_a5")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/experiments/EXP-karatsuba-v2-a5/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "karatsuba_ap2_specialized")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/experiments/EXP-karatsuba-ap2-specialized/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "toomcook3_zbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/experiments/EXP-toomcook3-gf2x-zbc/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "fafft_zbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/fafft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/reduce.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zbkb
                -mabi=lp64d
                -O3)
        elseif(POLYMUL STREQUAL "hybrid")
            # Phase 1b v3: Scalar Hybrid Architecture.
            # hybrid.c wraps vect_mul_hybrid_v3 (v3/hybrid_v3.c):
            #   Recursive Karatsuba divide-and-conquer ("FAFFT outer shell")
            #   down to V3_HYBRID_THRESH words, then clmul+clmulh schoolbook
            #   leaf ("Karatsuba base case"). Pure scalar Zbc — no RVV, no
            #   tower-field FAFFT overhead.
            # V3_HYBRID_THRESH sweep: cmake -DV3_HYBRID_THRESH=32/64/128
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/hybrid_zbc/hybrid.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/hybrid_zbc/hybrid_v3.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gc_zbc_zbb_zbkb
                -mabi=lp64d
                $<$<C_COMPILER_ID:GNU>:-mno-vector>
                $<$<C_COMPILER_ID:Clang>:-mno-zvector>
                -fno-tree-vectorize
                -fno-vectorize
                -fno-slp-vectorize)
        elseif(POLYMUL STREQUAL "fafft_zvbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/vector/zvbc/fafft_zvbc/fafft_vclmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zbkb_zvbc_zvbb
                -mabi=lp64d
                -O3)
        elseif(POLYMUL STREQUAL "karatsuba_rvv")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/vector/rvv_only/karatsuba_rvv/gf2x_clmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zbkb
                -mabi=lp64d
                -O3)
        elseif(POLYMUL STREQUAL "karatsuba_zvbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/vector/zvbc/karatsuba_zvbc/gf2x_vclmul.c)
            list(APPEND VARIANT_COMPILE_OPTIONS
                -march=rv64gcv_zbc_zbb_zbkb_zvbc_zvbb
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
            ${CMAKE_SOURCE_DIR}/experiments/EXP-toomcook3-gf2x-zbc
            ${KECCAK_INCLUDE_DIRS}
        )
        target_compile_definitions(${KAT_TARGET} PRIVATE
            HQC_KAT_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${SIM_DEFINES}
        )
        if(VARIANT_COMPILE_OPTIONS)
            target_compile_options(${KAT_TARGET} PRIVATE ${VARIANT_COMPILE_OPTIONS})
        endif()
        if(POLYMUL STREQUAL "fafft_zvbc" OR POLYMUL STREQUAL "karatsuba_zvbc")
            # Zvbc variants require --isa=rv64gcv_zvbc on Spike at runtime.
            # run_kat.sh detects this compile-time definition and adjusts accordingly.
            target_compile_definitions(${KAT_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        if(POLYMUL STREQUAL "karatsuba_zbc_exp")
            target_compile_definitions(${KAT_TARGET} PRIVATE
                KARATSUBA_THRESHOLD=${HQC_KARATSUBA_THRESHOLD})
        endif()
        if(POLYMUL STREQUAL "karatsuba_zbc_ablation")
            target_compile_definitions(${KAT_TARGET} PRIVATE
                KARATSUBA_THRESHOLD=16)
        endif()
        if(POLYMUL STREQUAL "hybrid")
            target_compile_definitions(${KAT_TARGET} PRIVATE
                HYBRID_KARATSUBA_DEPTH=${HYBRID_KARATSUBA_DEPTH}
                V3_HYBRID_THRESH=${V3_HYBRID_THRESH}
                HQC_POLYMUL_HYBRID_KEYCACHE=1)
            # vect_mul_keycache.h is included by hybrid.c via #ifdef guard
            target_include_directories(${KAT_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/zvbc/hybrid)
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
            hqc_apply_ref_lto_pgo(${KAT_TARGET})
        endif()

        # CTest KAT gate — must pass before any branch is merged.
        # run_kat.sh(elf, level, polymul) drives simulator (Spike by default, QEMU with --sim) and diffs against .rsp
        add_test(
            NAME "kat_${KAT_TARGET}"
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/test/run_kat.sh
                    $<TARGET_FILE:${KAT_TARGET}> ${LEVEL} ${POLYMUL}
        )

        # BENCH target  (bench_hqc${LEVEL}_${POLYMUL}) ─────────────────────
        # Identical sources, but compiled with HQC_BENCH_MODE=1
        # so that AES-CTR DRBG overhead does not pollute rdcycle measurements.
        # Built with EXCLUDE_FROM_ALL — must be explicitly requested:
        #   cmake --build build --target bench_hqc128_ref_scalar
        set(BENCH_TARGET "bench_hqc${LEVEL}_${POLYMUL}")

        add_executable(${BENCH_TARGET} EXCLUDE_FROM_ALL
            ${STARTUP_S}
            ${CMAKE_SOURCE_DIR}/tests/bench/bench_kem.c
            ${POLYMUL_SOURCES}
            ${VARIANT_KECCAK_SOURCES}
            ${VARIANT_DECODE_SOURCES}
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
            ${CMAKE_SOURCE_DIR}/experiments/EXP-toomcook3-gf2x-zbc
            ${KECCAK_INCLUDE_DIRS}
        )
        target_compile_definitions(${BENCH_TARGET} PRIVATE
            HQC_BENCH_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${SIM_DEFINES}
            N_ITER=${BENCH_N_ITER}
            POLYMUL_NAME="${POLYMUL}"
        )
        if(VARIANT_COMPILE_OPTIONS)
            target_compile_options(${BENCH_TARGET} PRIVATE ${VARIANT_COMPILE_OPTIONS})
        endif()
        if(POLYMUL STREQUAL "fafft_zvbc" OR POLYMUL STREQUAL "karatsuba_zvbc")
            target_compile_definitions(${BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        if(POLYMUL STREQUAL "karatsuba_zbc_exp")
            target_compile_definitions(${BENCH_TARGET} PRIVATE
                KARATSUBA_THRESHOLD=${HQC_KARATSUBA_THRESHOLD})
        endif()
        if(POLYMUL STREQUAL "karatsuba_zbc_ablation")
            target_compile_definitions(${BENCH_TARGET} PRIVATE
                KARATSUBA_THRESHOLD=16)
        endif()

        if(POLYMUL STREQUAL "hybrid")
            target_compile_definitions(${BENCH_TARGET} PRIVATE
                HYBRID_KARATSUBA_DEPTH=${HYBRID_KARATSUBA_DEPTH}
                V3_HYBRID_THRESH=${V3_HYBRID_THRESH}
                HQC_POLYMUL_HYBRID_KEYCACHE=1)
            target_include_directories(${BENCH_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/zvbc/hybrid)
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
            hqc_apply_ref_lto_pgo(${BENCH_TARGET})
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
        #         --target bench_polymul_hqc128_ref_scalar
        set(POLYMUL_BENCH_TARGET "bench_polymul_hqc${LEVEL}_${POLYMUL}")

        add_executable(${POLYMUL_BENCH_TARGET} EXCLUDE_FROM_ALL
            ${STARTUP_S}
            ${CMAKE_SOURCE_DIR}/tests/bench/bench_poly_mul.c
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
            ${CMAKE_SOURCE_DIR}/experiments/EXP-toomcook3-gf2x-zbc
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
        if(POLYMUL STREQUAL "fafft_zvbc" OR POLYMUL STREQUAL "karatsuba_zvbc")
            target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        if(POLYMUL STREQUAL "karatsuba_zbc_exp")
            target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE
                KARATSUBA_THRESHOLD=${HQC_KARATSUBA_THRESHOLD})
        endif()
        if(POLYMUL STREQUAL "karatsuba_zbc_ablation")
            target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE
                KARATSUBA_THRESHOLD=16)
        endif()
        if(POLYMUL STREQUAL "hybrid")
            target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE
                HYBRID_KARATSUBA_DEPTH=${HYBRID_KARATSUBA_DEPTH}
                V3_HYBRID_THRESH=${V3_HYBRID_THRESH}
                HQC_POLYMUL_HYBRID_KEYCACHE=1)
            target_include_directories(${POLYMUL_BENCH_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/zvbc/hybrid)
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
            hqc_apply_ref_lto_pgo(${POLYMUL_BENCH_TARGET})
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
                set(VLEN_BENCH_TARGET "bench_polymul_hqc${LEVEL}_${POLYMUL}_vlen${VLEN}")

                add_executable(${VLEN_BENCH_TARGET} EXCLUDE_FROM_ALL
                    ${STARTUP_S}
                    ${CMAKE_SOURCE_DIR}/tests/bench/bench_poly_mul.c
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
                    "-mrvv-vector-bits=${VLEN}"
                )
                if(POLYMUL STREQUAL "fafft_zvbc" OR POLYMUL STREQUAL "karatsuba_zvbc")
                    target_compile_definitions(${VLEN_BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
                endif()
                if(POLYMUL STREQUAL "hybrid")
                    target_compile_definitions(${VLEN_BENCH_TARGET} PRIVATE
                        HYBRID_KARATSUBA_DEPTH=${HYBRID_KARATSUBA_DEPTH}
                        V3_HYBRID_THRESH=${V3_HYBRID_THRESH}
                        HQC_POLYMUL_HYBRID_KEYCACHE=1)
                    target_include_directories(${VLEN_BENCH_TARGET} BEFORE PRIVATE
                        ${CMAKE_SOURCE_DIR}/src/poly_mul/zvbc/hybrid)
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



