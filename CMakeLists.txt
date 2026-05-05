cmake_minimum_required(VERSION 3.21)
project(hqc-riscv C ASM)

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
    set(SIM_DEFINES "")       # Spike HTIF is the default in bench.c
    set(BENCH_SIM_RUNNER "${CMAKE_SOURCE_DIR}/sim/spike/run_bench.sh")
elseif(HQC_SIM STREQUAL "qemu")
    set(SIM_DEFINES "HQC_SIM_QEMU=1")
    set(BENCH_SIM_RUNNER "${CMAKE_SOURCE_DIR}/sim/qemu/run_kat.sh")
else()
    message(FATAL_ERROR "Unsupported HQC_SIM='${HQC_SIM}'. Use 'qemu' or 'spike'.")
endif()

# ── Benchmark iteration count ────────────────────────────────────────────────
# Controls N_ITER in bench_kem.c / bench_poly_mul.c.
# Use a small value (e.g. 1) for a quick probe run; 10 for final results.
set(BENCH_N_ITER 10 CACHE STRING "Iterations per benchmark operation (N_ITER)")
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

# ── Upstream vendor root (registered as git submodule in Phase 0a) ────────────
set(HQC_REF ${CMAKE_SOURCE_DIR}/vendor/hqc-ref)

# ── Polynomial multiplication variants ───────────────────────────────────────
# All targets are declared but marked EXCLUDE_FROM_ALL — build explicitly:
#   cmake --build build --target hqc128_karatsuba_ref -j$(nproc)
# Scripts (run_kat_all.sh, run_spike_bench_variant.sh) auto-skip missing ELFs.
# Different CMake build dirs control Keccak/Decode implementation via
# HQC_KECCAK_IMPL / HQC_DECODE_IMPL (set in cmake/options.cmake).
#
#   Phase 0  — karatsuba_ref : vendor Karatsuba, unmodified (performance denominator)
#   Phase 1a — karatsuba_zbc : clmul base-case + lazy reduction + depth sweep
#   Phase 1b — fafft_zbc     : FAFFT with clmul GF(2^m) butterflies + RVV XOR
#   Phase 1c — hybrid        : Karatsuba outer levels → FAFFT inner (CRT)
#   Phase 1+ — fafft_zvbc    : vclmul.vv vectorised butterflies (Spike Zvbc only)
set(POLYMUL_VARIANTS
    karatsuba_ref
    karatsuba_zbc
    fafft_zbc
    hybrid
    fafft_zvbc
    karatsuba_zvbc
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
# Phase 2 replaces this with the RVV Keccak.  All polymul variants automatically
# pick up the new implementation when HQC_KECCAK_IMPL=rvv.
if(HQC_KECCAK_IMPL STREQUAL "rvv")
    # Phase 2: Zhang et al. RVV Keccak-f + HQC-specific SHAKE-256 wrapper
    set(KECCAK_SOURCES ${CMAKE_SOURCE_DIR}/src/keccak/rvv/fips202_rvv.c)
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
)
# Bench mode: randombytes.c provides its own stub; no AES overhead needed.
set(LOCAL_BENCH_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/randombytes.c
    ${CMAKE_SOURCE_DIR}/src/common/bench.c
    ${CMAKE_SOURCE_DIR}/sim/common/mem.c
)

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

        # ── Polynomial multiply sources per strategy ──────────────────────────
        if(POLYMUL STREQUAL "karatsuba_ref")
            # Vendor Karatsuba — never modified; this is the performance denominator
            set(POLYMUL_SOURCES ${HQC_REF}/src/ref/gf2x.c)
        elseif(POLYMUL STREQUAL "karatsuba_zbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/karatsuba_zbc/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "fafft_zbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/fafft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/fafft_v5.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/fafft_rvv.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/reduce.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/gfmul_fft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/bitconversion.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/btfy_consts_gft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/btfy_ffft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/encode.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/gft_mul_vi.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/CRT_fft_hqc1.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/CRT_fft_hqc3.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/radix16_base.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/radix16_81.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/radix16_193.c)
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
        elseif(POLYMUL STREQUAL "fafft_zvbc")
            # fafft_zvbc reuses all fafft_zbc ref sources, but replaces
            # gfmul_fft.c with fafft_vclmul.c which provides vect_mul +
            # bmul2_* functions (research finding: vclmul.vv does not help
            # tower-field FAFFT arithmetic; see fafft_vclmul.c for details).
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/vector/fafft_zvbc/fafft_vclmul.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/reduce.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/gfmul_fft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/bitconversion.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/btfy_consts_gft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/btfy_ffft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/encode.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc/ref/gft_mul_vi.c)
        elseif(POLYMUL STREQUAL "karatsuba_zvbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/vector/karatsuba_zvbc/gf2x_vclmul.c)
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
            ${KECCAK_SOURCES}
            ${DECODE_SOURCES}
            ${VENDOR_INVARIANT_SOURCES}
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
        )
        target_compile_definitions(${KAT_TARGET} PRIVATE
            HQC_KAT_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${SIM_DEFINES}
        )
        if(POLYMUL STREQUAL "fafft_zvbc" OR POLYMUL STREQUAL "karatsuba_zvbc")
            # Zvbc variants require --isa=rv64gcv_zvbc on Spike at runtime.
            # run_kat.sh detects this compile-time definition and adjusts accordingly.
            target_compile_definitions(${KAT_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        if(POLYMUL STREQUAL "fafft_zvbc")
            # fafft_vclmul.c uses #include "ref/gfmul_fft.h" etc.; the ref/ tree
            # lives under fafft_zbc/, so add that directory to the search path.
            target_include_directories(${KAT_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc)
        endif()
        if(POLYMUL STREQUAL "hybrid")
            target_compile_definitions(${KAT_TARGET} PRIVATE
                HYBRID_KARATSUBA_DEPTH=${HYBRID_KARATSUBA_DEPTH}
                V3_HYBRID_THRESH=${V3_HYBRID_THRESH}
                HQC_POLYMUL_HYBRID_KEYCACHE=1)
            # vect_mul_keycache.h is included by hqc.c via #ifdef guard
            target_include_directories(${KAT_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/hybrid_zbc)
        endif()
        set_target_properties(${KAT_TARGET} PROPERTIES
            SUFFIX ".elf"
            LINK_DEPENDS "${LINKER_SCRIPT}"
        )
        target_link_options(${KAT_TARGET} PRIVATE
            "-T${LINKER_SCRIPT}"
            "-Wl,-Map=${CMAKE_BINARY_DIR}/${KAT_TARGET}.map"
        )

        # CTest KAT gate — must pass before any branch is merged.
        # run_kat.sh(elf, level, polymul) drives QEMU and diffs against .rsp
        add_test(
            NAME "kat_${KAT_TARGET}"
            COMMAND ${CMAKE_SOURCE_DIR}/tests/kat/run_kat.sh
                    $<TARGET_FILE:${KAT_TARGET}> ${LEVEL} ${POLYMUL}
        )

        # BENCH target  (bench_hqc${LEVEL}_${POLYMUL}) ─────────────────────
        # Identical sources, but compiled with HQC_BENCH_MODE=1
        # so that AES-CTR DRBG overhead does not pollute rdcycle measurements.
        # Built with EXCLUDE_FROM_ALL — must be explicitly requested:
        #   cmake --build build --target bench_hqc128_karatsuba_ref
        set(BENCH_TARGET "bench_hqc${LEVEL}_${POLYMUL}")

        add_executable(${BENCH_TARGET} EXCLUDE_FROM_ALL
            ${STARTUP_S}
            ${CMAKE_SOURCE_DIR}/tests/bench/bench_kem.c
            ${POLYMUL_SOURCES}
            ${KECCAK_SOURCES}
            ${DECODE_SOURCES}
            ${VENDOR_INVARIANT_SOURCES}
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
        )
        target_compile_definitions(${BENCH_TARGET} PRIVATE
            HQC_BENCH_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${SIM_DEFINES}
            N_ITER=${BENCH_N_ITER}
            POLYMUL_NAME="${POLYMUL}"
        )
        if(POLYMUL STREQUAL "fafft_zvbc" OR POLYMUL STREQUAL "karatsuba_zvbc")
            target_compile_definitions(${BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        if(POLYMUL STREQUAL "fafft_zvbc")
            target_include_directories(${BENCH_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc)
        endif()
        if(POLYMUL STREQUAL "hybrid")
            target_compile_definitions(${BENCH_TARGET} PRIVATE
                HYBRID_KARATSUBA_DEPTH=${HYBRID_KARATSUBA_DEPTH}
                V3_HYBRID_THRESH=${V3_HYBRID_THRESH}
                HQC_POLYMUL_HYBRID_KEYCACHE=1)
            target_include_directories(${BENCH_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/hybrid_zbc)
        endif()
        set_target_properties(${BENCH_TARGET} PROPERTIES
            SUFFIX ".elf"
            LINK_DEPENDS "${LINKER_SCRIPT}"
        )
        target_link_options(${BENCH_TARGET} PRIVATE
            "-T${LINKER_SCRIPT}"
            "-Wl,-Map=${CMAKE_BINARY_DIR}/${BENCH_TARGET}.map"
        )
        add_custom_target(run_${BENCH_TARGET}
            COMMAND ${BENCH_SIM_RUNNER} $<TARGET_FILE:${BENCH_TARGET}>
            DEPENDS ${BENCH_TARGET}
            USES_TERMINAL
            COMMENT "Running ${BENCH_TARGET} with HQC_SIM=${HQC_SIM}"
        )

        # POLYMUL MICRO-BENCH target  (bench_polymul_hqc${LEVEL}_${POLYMUL}) ──
        # Benchmarks only vect_mul() — no keypair/encaps/decaps, no Keccak.
        # Use for fast iteration on the vect_mul hotspot.
        #   cmake --build build-spike-probe \
        #         --target bench_polymul_hqc128_karatsuba_ref
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
        )
        target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE
            HQC_BENCH_MODE=1
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
            ${SIM_DEFINES}
            N_ITER=${BENCH_N_ITER}
            POLYMUL_NAME="${POLYMUL}"
        )
        if(POLYMUL STREQUAL "fafft_zvbc" OR POLYMUL STREQUAL "karatsuba_zvbc")
            target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE HQC_POLYMUL_ZVBC=1)
        endif()
        if(POLYMUL STREQUAL "fafft_zvbc")
            target_include_directories(${POLYMUL_BENCH_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/fafft_zbc)
        endif()
        if(POLYMUL STREQUAL "hybrid")
            target_compile_definitions(${POLYMUL_BENCH_TARGET} PRIVATE
                HYBRID_KARATSUBA_DEPTH=${HYBRID_KARATSUBA_DEPTH}
                V3_HYBRID_THRESH=${V3_HYBRID_THRESH}
                HQC_POLYMUL_HYBRID_KEYCACHE=1)
            target_include_directories(${POLYMUL_BENCH_TARGET} BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/src/poly_mul/scalar/hybrid_zbc)
        endif()
        set_target_properties(${POLYMUL_BENCH_TARGET} PROPERTIES
            SUFFIX ".elf"
            LINK_DEPENDS "${LINKER_SCRIPT}"
        )
        target_link_options(${POLYMUL_BENCH_TARGET} PRIVATE
            "-T${LINKER_SCRIPT}"
            "-Wl,-Map=${CMAKE_BINARY_DIR}/${POLYMUL_BENCH_TARGET}.map"
        )
        add_custom_target(run_${POLYMUL_BENCH_TARGET}
            COMMAND ${BENCH_SIM_RUNNER} $<TARGET_FILE:${POLYMUL_BENCH_TARGET}>
            DEPENDS ${POLYMUL_BENCH_TARGET}
            USES_TERMINAL
            COMMENT "Running ${POLYMUL_BENCH_TARGET} with HQC_SIM=${HQC_SIM}"
        )

    endforeach()  # POLYMUL
endforeach()  # LEVEL


# ── Symlink compile_commands.json to project root ─────────────────────────────
add_custom_target(symlink_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Symlinking compile_commands.json from build dir to project root"
)

# ── objdump target: verify vclmul opcodes ────────────────────────────────────
find_program(OBJDUMP_BIN
    NAMES riscv64-unknown-elf-objdump llvm-objdump
    PATHS
        $ENV{RISCV}/bin
        "${CMAKE_SOURCE_DIR}/../toolchains/installed-tools/bin"
    NO_DEFAULT_PATH
)

if(OBJDUMP_BIN)
    add_custom_target(verify_vclmul
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/ci/check_opcodes.sh
                ${OBJDUMP_BIN}
                "$<TARGET_FILE:hqc128_fafft_zvbc>"
        DEPENDS hqc128_fafft_zvbc
        COMMENT "Verifying vclmul opcodes in hqc128_fafft_zvbc"
    )
endif()
