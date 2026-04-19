cmake_minimum_required(VERSION 3.21)
project(hqc-riscv C ASM)

# ── compile_commands.json ─────────────────────────────────────────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── CTest ─────────────────────────────────────────────────────────────────────
enable_testing()

# ── Global include paths ──────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Startup assembly (common to all targets) ─────────────────────────────────
set(STARTUP_S ${CMAKE_SOURCE_DIR}/sim/common/startup.S)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/sim/common/link.ld)

# ── Target matrix ─────────────────────────────────────────────────────────────
# Implementations × security levels
set(IMPLEMENTATIONS baseline scalar_zbc vector_zvbc)
set(SECURITY_LEVELS 128 192 256)

foreach(IMPL ${IMPLEMENTATIONS})
    foreach(LEVEL ${SECURITY_LEVELS})
        set(TARGET_NAME "hqc${LEVEL}_${IMPL}")

        # Collect sources for this implementation
        if(IMPL STREQUAL "baseline")
            set(IMPL_SOURCES
                src/baseline/kem.c
                src/baseline/poly.c
                src/baseline/reed_muller.c
                src/baseline/fips202.c
                src/common/randombytes.c
            )
        elseif(IMPL STREQUAL "scalar_zbc")
            set(IMPL_SOURCES
                src/baseline/kem.c
                src/baseline/poly.c
                src/baseline/reed_muller.c
                src/baseline/fips202.c
                src/common/randombytes.c
                src/scalar_zbc/poly_clmul_zbc.c
            )
        elseif(IMPL STREQUAL "vector_zvbc")
            set(IMPL_SOURCES
                src/baseline/kem.c
                src/baseline/reed_muller.c
                src/baseline/fips202.c
                src/common/randombytes.c
                src/vector_zvbc/fips202_rvv.c
                src/vector_zvbc/hqc${LEVEL}/poly_mul_${LEVEL}.c
            )
        endif()

        add_executable(${TARGET_NAME}
            ${STARTUP_S}
            ${IMPL_SOURCES}
        )

        target_compile_definitions(${TARGET_NAME} PRIVATE
            SECURITY_LEVEL=${LEVEL}
            HQC_${LEVEL}
        )

        set_target_properties(${TARGET_NAME} PROPERTIES
            SUFFIX ".elf"
            LINK_FLAGS "-T${LINKER_SCRIPT} -Wl,-Map=${TARGET_NAME}.map"
        )

        # Register with CTest (runs under QEMU)
        add_test(
            NAME "kat_${TARGET_NAME}"
            COMMAND ${CMAKE_SOURCE_DIR}/tests/kat/run_kat.sh $<TARGET_FILE:${TARGET_NAME}>
        )
    endforeach()
endforeach()

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
