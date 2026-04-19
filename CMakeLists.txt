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

# ── Upstream vendor root (registered as git submodule in Phase 0a) ────────────
set(HQC_REF ${CMAKE_SOURCE_DIR}/vendor/hqc-ref)

# ── Polynomial multiplication variants ───────────────────────────────────────
# All five are built in every configure — head-to-head comparison is a core
# thesis deliverable.  Different CMake builds (different build dirs) control
# which Keccak/Decode implementation is linked via HQC_KECCAK_IMPL /
# HQC_DECODE_IMPL (set in cmake/options.cmake).
#
#   Phase 0  — karatsuba_ref : vendor Karatsuba, unmodified (performance denominator)
#   Phase 1a — karatsuba_zbc : clmul base-case + lazy reduction + depth sweep
#   Phase 1b — fafft_zbc     : FAFFT with clmul GF(2^m) butterflies + RVV XOR
#   Phase 1c — hybrid_zbc    : Karatsuba outer levels → FAFFT inner (CRT)
#   Phase 1+ — fafft_zvbc    : vclmul.vv vectorised butterflies (Spike Zvbc only)
set(POLYMUL_VARIANTS
    karatsuba_ref
    karatsuba_zbc
    fafft_zbc
    hybrid_zbc
    fafft_zvbc
)

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
    # Phase 3: RVV FHT decoder + vluxei8.v RS syndrome computation
    set(DECODE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/decode/rvv/reed_muller_rvv.c
        ${CMAKE_SOURCE_DIR}/src/decode/rvv/reed_solomon_rvv.c
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
set(LOCAL_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/randombytes.c
    ${CMAKE_SOURCE_DIR}/src/common/rng/aes_ctr_drbg.c
    ${CMAKE_SOURCE_DIR}/src/common/rng/tiny_aes.c
    ${CMAKE_SOURCE_DIR}/src/common/bench.c
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
                ${CMAKE_SOURCE_DIR}/src/poly_mul/karatsuba_zbc/gf2x_clmul.c)
        elseif(POLYMUL STREQUAL "fafft_zbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/fafft_zbc/fafft.c
                ${CMAKE_SOURCE_DIR}/src/poly_mul/fafft_zbc/fafft_rvv.c)
        elseif(POLYMUL STREQUAL "hybrid_zbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/hybrid_zbc/hybrid.c)
        elseif(POLYMUL STREQUAL "fafft_zvbc")
            set(POLYMUL_SOURCES
                ${CMAKE_SOURCE_DIR}/src/poly_mul/fafft_zvbc/fafft_vclmul.c)
        endif()

        # ── KAT target  (hqc${LEVEL}_${POLYMUL}) ─────────────────────────────
        # Linked against the invariant core OBJECT library.
        # Compiled with HQC_RANDOMBYTES_MODE_KAT=1 (AES-CTR DRBG for KAT vectors).
        set(KAT_TARGET "hqc${LEVEL}_${POLYMUL}")

        add_executable(${KAT_TARGET}
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
            ${HQC_REF}/src/ref/${VARIANT}
            ${HQC_REF}/src/ref
            ${HQC_REF}/lib/fips202
            ${CMAKE_SOURCE_DIR}/tests/kat
        )
        target_compile_definitions(${KAT_TARGET} PRIVATE
            HQC_RANDOMBYTES_MODE_KAT=1
        )
        if(POLYMUL STREQUAL "fafft_zvbc")
            # fafft_zvbc requires --isa=rv64gcv_zvbc on Spike at runtime.
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

        # CTest KAT gate — must pass before any branch is merged.
        # run_kat.sh(elf, level, polymul) drives QEMU and diffs against .rsp
        add_test(
            NAME "kat_${KAT_TARGET}"
            COMMAND ${CMAKE_SOURCE_DIR}/tests/kat/run_kat.sh
                    $<TARGET_FILE:${KAT_TARGET}> ${LEVEL} ${POLYMUL}
        )

        # ── BENCH target  (bench_hqc${LEVEL}_${POLYMUL}) ─────────────────────
        # Identical sources, but compiled with HQC_RANDOMBYTES_MODE_BENCH=1
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
            ${LOCAL_COMMON_SOURCES}
        )
        target_link_libraries(${BENCH_TARGET} PRIVATE
            $<TARGET_OBJECTS:hqc_core_${LEVEL}>)
        target_include_directories(${BENCH_TARGET} BEFORE PRIVATE
            ${HQC_REF}/src/common/${VARIANT}
            ${HQC_REF}/src/ref/${VARIANT}
            ${HQC_REF}/src/ref
            ${HQC_REF}/lib/fips202
        )
        target_compile_definitions(${BENCH_TARGET} PRIVATE
            HQC_RANDOMBYTES_MODE_BENCH=1
        )
        if(POLYMUL STREQUAL "fafft_zvbc")
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

    endforeach()  # POLYMUL
endforeach()  # LEVEL

# ── Symlink compile_commands.json to project root ─────────────────────────────
add_custom_target(symlink_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Symlinking compile_commands.json to project root"
)

# ── objdump target: verify vclmul opcodes ────────────────────────────────────
find_program(OBJDUMP_BIN
    NAMES riscv64-unknown-elf-objdump llvm-objdump
    PATHS $ENV{RISCV}/bin $ENV{HOME}/Thesis/installed-tools/bin
    NO_DEFAULT_PATH
)

if(OBJDUMP_BIN)
    add_custom_target(verify_vclmul
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/ci/check_opcodes.sh
                ${OBJDUMP_BIN}
                "$<TARGET_FILE:hqc128_vector_zvbc>"
        DEPENDS hqc128_vector_zvbc
        COMMENT "Verifying vclmul opcodes in hqc128_vector_zvbc"
    )
endif()
