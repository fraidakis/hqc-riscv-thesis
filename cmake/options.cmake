# cmake/options.cmake
# Three orthogonal CMake cache variables that independently control which
# implementation is compiled for each of the three optimizable HQC components.
#
# Usage:
#   cmake -B build -DHQC_KECCAK_IMPL=ref -DHQC_DECODE_IMPL=rvv  # Track B
#   cmake -B build -DHQC_KECCAK_IMPL=zvbb_x2_soa -DHQC_DECODE_IMPL=rvv  # Track C2
#
# All five POLYMUL_VARIANTS are always built regardless of these flags.
# The flags control which Keccak and Decoder sources are linked into every target.

# ── Phase 2 flag: Keccak / SHAKE implementation ───────────────────────────────
# ref         : vendor scalar fips202 (default; used for Track A and Track B)
# zvbb_x2_soa : Track C2 — 2-way SoA vector Keccak backend with paired-stream
#               wrappers; src/keccak/fips202_zvbb_x2.c
#               (includes keccak_xor_bytes RVV byte-streaming — promoted from
#               EXP-keccak-xor-bytes-rvv on 2026-05-22)
# Legacy keccak variants are archived under experiments/dropped/ and are not
# selectable in the default stable build.
set(HQC_KECCAK_IMPL "ref" CACHE STRING
    "Keccak/SHAKE backend: ref | zvbb_x2_soa")
set_property(CACHE HQC_KECCAK_IMPL PROPERTY STRINGS
    "ref" "zvbb_x2_soa")

# ── Phase 3 flag: RM/RS concatenated decoder ─────────────────────────────────
# ref : vendor scalar reed_muller.c + reed_solomon.c (default)
# rvv : RVV-vectorised 128-pt FHT + RVV log/exp gather RS syndromes (reed_solomon_rvv.c)
# zvbc : RVV RM + promoted Zvbc RS path (reed_solomon_zvbc.c)
# rs_zvbc_exp : legacy alias of zvbc (kept for backward compatibility)
set(HQC_DECODE_IMPL "ref" CACHE STRING
    "RM/RS decoder backend: ref | rvv | zvbc")
set_property(CACHE HQC_DECODE_IMPL PROPERTY STRINGS "ref" "rvv" "zvbc")

# ── Scalar ISA override (ablation experiments) ───────────────────────────────
# Overrides the -march for scalar-only variants (scalar_ref, scalar_zb).
# Default is the fully optimized baseline including bit-manipulation extensions.
# Set to e.g. "rv64gc_zbc" to strip zbb and measure auto-lowering impact.
#
# Note: vector variants (vector_rvv, vector_zvk) have their own -march
# strings set in CMakeLists.txt (rv64gcv_zbc_zbb_zba and rv64gcv_zbc_zbb_zba_zvbc_zvbb
# respectively).  Zba (address-generation: sh1add/sh2add/sh3add) was promoted
# on 2026-05-25 after ablation showed −5.6% polymul improvement on vector_zvk
# and −2.6% on vector_rvv (see docs/thesis-helper/optimizations/zba_ablation.md).
set(HQC_SCALAR_MARCH "rv64gc_zbc_zbb_zbs" CACHE STRING
    "ISA march for scalar variants (ablation: set to rv64gc_zbc to strip zbb)")

# ── Diagnostic messages ───────────────────────────────────────────────────────
message(STATUS "HQC configuration:")
message(STATUS "  POLYMUL  : matrix includes scalar_ref/vector_ref baselines and optimized variants")
message(STATUS "  KECCAK   : ${HQC_KECCAK_IMPL}")
message(STATUS "  DECODE   : ${HQC_DECODE_IMPL}")
if(HQC_KECCAK_IMPL STREQUAL "ref" AND HQC_DECODE_IMPL STREQUAL "ref")
    message(STATUS "  → Phase 1 build: polynomial multiplication head-to-head comparison")
elseif(HQC_KECCAK_IMPL STREQUAL "ref" AND HQC_DECODE_IMPL STREQUAL "rvv")
    message(STATUS "  → Track B build: RVV polymul/decoder with scalar reference Keccak")
elseif(HQC_KECCAK_IMPL STREQUAL "ref" AND (HQC_DECODE_IMPL STREQUAL "zvbc" OR HQC_DECODE_IMPL STREQUAL "rs_zvbc_exp"))
    message(STATUS "  → Track B+ build: RVV polymul with Zvbc RS decode backend")
elseif(HQC_KECCAK_IMPL STREQUAL "zvbb_x2_soa")
    message(STATUS "  → Track C2 build: Zvbb 2-way SoA vector backend (fips202_zvbb_x2.c)")
endif()

# ── RVV vector-bits assumption ────────────────────────────────────────────────
# HQC_VECTOR_BITS controls the -mrvv-vector-bits=N Clang flag applied to all
# RVV-compiled sources (Keccak zvbb_x2_soa, decode rvv/zvbc).
# Default is 256 (matches the reference hardware VLEN and the full-KEM build).
# The VLEN sweep sets this to the target VLEN per build dir so the same
# pairings (vector_rvv+ref+rvv, vector_zvk+zvbb+zvbc) produce correct
# results at every VLEN without sacrificing performance at VLEN=256.
set(HQC_VECTOR_BITS "256" CACHE STRING
    "RVV vector register width assumed by -mrvv-vector-bits= (default: 256)")
message(STATUS "  HQC_VECTOR_BITS: ${HQC_VECTOR_BITS}")

# ── Multi-VLEN build matrix (P-02) ───────────────────────────────────────────
# VLEN_LIST: semicolon-separated VLEN values for which vector-variant bench
# targets are generated.  Each entry produces a distinct ELF compiled with
# -mvscale-min=N -mvscale-max=N (N = VLEN/64) so the compiler can use the
# guaranteed register width for constant-folding and vectorization decisions.
# Non-vector variants are always built once without a VLEN suffix.
#
# Example:  cmake -B build -DVLEN_LIST="128;256;512"
set(VLEN_LIST "256" CACHE STRING
    "VLEN values for vector-variant build matrix (semicolon-separated, e.g. 128;256;512)")
message(STATUS "  VLEN matrix: ${VLEN_LIST}")

# Variants that participate in the per-VLEN matrix.  These are the polymul
# strategies that contain RVV code whose performance is VLEN-sensitive.
set(VECTOR_POLYMUL_VARIANTS vector_zvk vector_rvv)

# ── LTO + PGO for reference baselines (P-06) ─────────────────────────────────
# These flags apply ONLY to scalar_ref/vector_ref targets (KAT + bench + polymul bench).
# They do NOT affect any other variant.
#
# HQC_REF_LTO    : ON (default) | OFF  — enable -flto on reference targets
#
# HQC_REF_PGO    : off (default) | generate | use
#   generate : compile with -fprofile-generate / -fprofile-instr-generate to
#              emit raw profile data when the ELF is run on Spike.
#              Run the ELF, convert the raw profile with llvm-profdata, then
#              rebuild with HQC_REF_PGO=use.
#   use      : compile with -fprofile-use / -fprofile-instr-use pointing at
#              HQC_REF_PGO_FILE (path to the merged .profdata file).
#
# Example workflow:
#   cmake -B build/qemu -DHQC_REF_LTO=ON -DHQC_REF_PGO=generate ...
#   cmake --build build/qemu --target bench_hqc128_scalar_ref
#   spike --isa=rv64gcv ... bench_hqc128_scalar_ref.elf   # emits default.profraw
#   llvm-profdata merge -output=hqc_ref.profdata default.profraw
#   cmake -B build/qemu -DHQC_REF_PGO=use -DHQC_REF_PGO_FILE=hqc_ref.profdata ...
#   cmake --build build/qemu --target bench_hqc128_scalar_ref

set(HQC_REF_LTO ON CACHE BOOL
    "Enable LTO (-flto) for scalar_ref baseline targets")

set(HQC_REF_PGO "off" CACHE STRING
    "PGO mode for scalar_ref: off | generate | use")
set_property(CACHE HQC_REF_PGO PROPERTY STRINGS "off" "generate" "use")

set(HQC_REF_PGO_FILE "" CACHE FILEPATH
    "Path to merged .profdata file (required when HQC_REF_PGO=use)")

# Validate PGO settings
if(HQC_REF_PGO STREQUAL "use" AND NOT HQC_REF_PGO_FILE)
    message(FATAL_ERROR
        "HQC_REF_PGO=use requires HQC_REF_PGO_FILE to point at a merged "
        ".profdata file.  Run with HQC_REF_PGO=generate first, then collect "
        "the profile and set HQC_REF_PGO_FILE accordingly.")
endif()
if(NOT HQC_REF_PGO STREQUAL "off" AND
   NOT HQC_REF_PGO STREQUAL "generate" AND
   NOT HQC_REF_PGO STREQUAL "use")
    message(FATAL_ERROR "HQC_REF_PGO must be 'off', 'generate', or 'use', got '${HQC_REF_PGO}'")
endif()

message(STATUS "  reference LTO : ${HQC_REF_LTO}")
message(STATUS "  reference PGO : ${HQC_REF_PGO}")

# ── Validation ────────────────────────────────────────────────────────────────
if(NOT HQC_KECCAK_IMPL STREQUAL "ref"         AND
   NOT HQC_KECCAK_IMPL STREQUAL "zvbb_x2_soa")
    message(FATAL_ERROR
        "HQC_KECCAK_IMPL must be one of: ref | zvbb_x2_soa, "
        "got '${HQC_KECCAK_IMPL}'")
endif()

if(NOT HQC_DECODE_IMPL STREQUAL "ref"         AND
   NOT HQC_DECODE_IMPL STREQUAL "rvv"         AND
   NOT HQC_DECODE_IMPL STREQUAL "zvbc"        AND
   NOT HQC_DECODE_IMPL STREQUAL "rs_zvbc_exp")
    message(FATAL_ERROR
    "HQC_DECODE_IMPL must be one of: ref | rvv | zvbc, "
        "got '${HQC_DECODE_IMPL}'")
endif()
